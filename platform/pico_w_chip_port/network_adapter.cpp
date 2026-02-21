/*
 * network_adapter.cpp
 * Network adapter implementation for Pico W (CYW43439 WiFi chip)
 * Supports Station (STA) mode for WiFi connectivity
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"
#include "lwip/dhcp.h"

// Forward declaration of storage functions
extern "C" {
    int storage_adapter_has_wifi_credentials(void);
    int storage_adapter_load_wifi_credentials(char *ssid, size_t ssid_buffer_len,
                                              char *password, size_t password_buffer_len);
    int storage_adapter_save_wifi_credentials(const char *ssid, const char *password);
}

// Network mode
typedef enum {
    NETWORK_MODE_NONE = 0,
    NETWORK_MODE_STA      // Station mode (client)
} network_mode_t;

static bool wifi_connected = false;
static bool wifi_initialized = false;
static network_mode_t current_mode = NETWORK_MODE_NONE;

extern "C" {

int network_adapter_init(void) {
    printf("[NetworkAdapter] network_adapter_init() called (initialized=%d, connected=%d, mode=%d)\n",
           wifi_initialized ? 1 : 0, wifi_connected ? 1 : 0, (int) current_mode);

    if (wifi_initialized) {
        printf("[NetworkAdapter] CYW43 already initialized, skipping init\n");
        return 0;
    }

    printf("Initializing CYW43439 WiFi adapter...\n");
    const uint32_t init_start_ms = to_ms_since_boot(get_absolute_time());
    printf("[NetworkAdapter] About to call cyw43_arch_init() at %lu ms since boot\n",
           (unsigned long) init_start_ms);
    fflush(stdout);

    const int cyw43_result = cyw43_arch_init();
    const uint32_t init_end_ms = to_ms_since_boot(get_absolute_time());
    if (cyw43_result != 0) {
        printf("[NetworkAdapter] ERROR: cyw43_arch_init() failed with %d after %lu ms\n",
               cyw43_result, (unsigned long) (init_end_ms - init_start_ms));
        return -1;
    }
    printf("[NetworkAdapter] cyw43_arch_init() succeeded after %lu ms\n",
           (unsigned long) (init_end_ms - init_start_ms));

    wifi_initialized = true;
    printf("WiFi adapter initialized\n");
    return 0;
}

int network_adapter_connect(const char *ssid, const char *password) {
    if (!wifi_initialized) {
        printf("[NetworkAdapter] ERROR: WiFi not initialized. Call network_adapter_init() first\n");
        return -1;
    }

    // Try to load credentials from storage if not provided
    char stored_ssid[33] = {0};
    char stored_password[65] = {0};

    if (!ssid && !password) {
        printf("No WiFi credentials provided, checking flash storage...\n");
        if (storage_adapter_has_wifi_credentials()) {
            if (storage_adapter_load_wifi_credentials(stored_ssid, sizeof(stored_ssid),
                                                     stored_password, sizeof(stored_password)) == 0) {
                ssid = stored_ssid;
                password = stored_password;
                printf("Using WiFi credentials from flash\n");
            } else {
                printf("[NetworkAdapter] ERROR: Failed to load WiFi credentials from flash\n");
                return -1;
            }
        } else {
            printf("No WiFi credentials in flash storage\n");
            return -1;
        }
    }

    if (!ssid || strlen(ssid) == 0) {
        printf("[NetworkAdapter] ERROR: No SSID provided\n");
        return -1;
    }

    printf("Connecting to WiFi SSID: %s\n", ssid);

    // Enable station mode
    cyw43_arch_enable_sta_mode();

    int result = cyw43_arch_wifi_connect_timeout_ms(
        ssid,
        password,
        CYW43_AUTH_WPA2_AES_PSK,
        30000  // 30 second timeout
    );

    if (result != 0) {
        printf("[NetworkAdapter] ERROR: Failed to connect to WiFi (error %d)\n", result);
        wifi_connected = false;
        current_mode = NETWORK_MODE_NONE;
        return result;
    }

    // Get IP address
    struct netif *netif = netif_default;
    if (netif) {
        printf("WiFi connected successfully\n");
        printf("IP Address: %s\n", ip4addr_ntoa(netif_ip4_addr(netif)));
        printf("Netmask: %s\n", ip4addr_ntoa(netif_ip4_netmask(netif)));
        printf("Gateway: %s\n", ip4addr_ntoa(netif_ip4_gw(netif)));
    }

    current_mode = NETWORK_MODE_STA;
    wifi_connected = true;
    
    return 0;
}

int network_adapter_save_and_connect(const char *ssid, const char *password) {
    if (!ssid || !password) {
        printf("[NetworkAdapter] ERROR: Invalid SSID or password\n");
        return -1;
    }

    // Save credentials to flash
    printf("Saving WiFi credentials to flash...\n");
    if (storage_adapter_save_wifi_credentials(ssid, password) != 0) {
        printf("[NetworkAdapter] ERROR: Failed to save WiFi credentials\n");
        return -1;
    }

    // Connect to WiFi
    return network_adapter_connect(ssid, password);
}

bool network_adapter_is_connected(void) {
    return wifi_connected && wifi_initialized;
}

network_mode_t network_adapter_get_mode(void) {
    return current_mode;
}

void network_adapter_get_ip_address(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return;
    }

    if (!wifi_initialized) {
        snprintf(buffer, buffer_len, "0.0.0.0");
        return;
    }

    struct netif *netif = NULL;
    
    if (current_mode == NETWORK_MODE_STA) {
        // Get STA interface IP
        netif = netif_default;
    }

    if (netif && netif_is_up(netif)) {
        const char *ip_str = ip4addr_ntoa(netif_ip4_addr(netif));
        snprintf(buffer, buffer_len, "%s", ip_str);
    } else {
        snprintf(buffer, buffer_len, "0.0.0.0");
    }
}

void network_adapter_get_mac_address(uint8_t *mac_addr) {
    if (!mac_addr || !wifi_initialized) {
        return;
    }

    // Get MAC address from CYW43 (station mode MAC)
    cyw43_hal_get_mac(0, mac_addr);
}

void network_adapter_deinit(void) {
    if (!wifi_initialized) {
        return;
    }

    cyw43_arch_deinit();
    wifi_initialized = false;
    wifi_connected = false;
    current_mode = NETWORK_MODE_NONE;
    printf("WiFi adapter deinitialized\n");
}

} // extern "C"
