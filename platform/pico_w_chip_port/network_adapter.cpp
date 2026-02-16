/*
 * network_adapter.cpp
 * Network adapter implementation for Pico W (CYW43439 WiFi chip)
 * Supports both Station (STA) and Access Point (AP) modes for WiFi commissioning
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

// SoftAP configuration
#define SOFTAP_SSID "VikingBio-Setup"
#define SOFTAP_CHANNEL 1
#define SOFTAP_TIMEOUT_MS 1800000  // 30 minutes in milliseconds

// Network mode
typedef enum {
    NETWORK_MODE_NONE = 0,
    NETWORK_MODE_STA,      // Station mode (client)
    NETWORK_MODE_AP        // Access Point mode (SoftAP)
} network_mode_t;

static bool wifi_connected = false;
static bool wifi_initialized = false;
static network_mode_t current_mode = NETWORK_MODE_NONE;
static uint32_t softap_start_time = 0;  // Timestamp when SoftAP was started

extern "C" {

int network_adapter_init(void) {
    if (wifi_initialized) {
        return 0;
    }

    printf("Initializing CYW43439 WiFi adapter...\n");

    // Initialize CYW43 with background LWIP
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("ERROR: Failed to initialize CYW43 WiFi chip\n");
        return -1;
    }

    wifi_initialized = true;
    printf("WiFi adapter initialized\n");
    return 0;
}

int network_adapter_start_softap(void) {
    if (!wifi_initialized) {
        printf("ERROR: WiFi not initialized\n");
        return -1;
    }

    if (current_mode == NETWORK_MODE_AP) {
        printf("SoftAP already running\n");
        return 0;
    }

    printf("Starting SoftAP mode...\n");
    printf("  SSID: %s\n", SOFTAP_SSID);
    printf("  Channel: %d\n", SOFTAP_CHANNEL);
    printf("  Security: Open (no password)\n");

    // Enable AP mode with open authentication (no password)
    // Note: Passing NULL for password is intentional and explicitly handled by the SDK.
    // The SDK will automatically set auth to CYW43_AUTH_OPEN when password is NULL.
    cyw43_arch_enable_ap_mode(SOFTAP_SSID, NULL, CYW43_AUTH_OPEN);

    // Configure AP network interface with static IP
    ip4_addr_t ap_ip, ap_netmask, ap_gw;
    IP4_ADDR(&ap_ip, 192, 168, 4, 1);       // AP IP: 192.168.4.1
    IP4_ADDR(&ap_netmask, 255, 255, 255, 0); // Netmask: 255.255.255.0
    IP4_ADDR(&ap_gw, 192, 168, 4, 1);       // Gateway: 192.168.4.1

    // Get the AP network interface
    struct netif *ap_netif = &cyw43_state.netif[CYW43_ITF_AP];
    netif_set_addr(ap_netif, &ap_ip, &ap_netmask, &ap_gw);
    netif_set_up(ap_netif);

    current_mode = NETWORK_MODE_AP;
    wifi_connected = true;  // Consider AP mode as "connected"
    softap_start_time = to_ms_since_boot(get_absolute_time());  // Record start time

    printf("SoftAP started successfully\n");
    printf("  AP IP: %s\n", ip4addr_ntoa(&ap_ip));
    printf("  Connect to '%s' to commission device\n", SOFTAP_SSID);
    printf("  Clients should use static IP in 192.168.4.x range\n");
    printf("  (DHCP server not available - use static IP configuration)\n");

    return 0;
}

int network_adapter_stop_softap(void) {
    if (current_mode != NETWORK_MODE_AP) {
        return 0;
    }

    printf("Stopping SoftAP mode...\n");

    // Disable AP mode by deinitializing and reinitializing
    cyw43_arch_deinit();
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("ERROR: Failed to reinitialize CYW43\n");
        wifi_initialized = false;
        return -1;
    }

    current_mode = NETWORK_MODE_NONE;
    wifi_connected = false;
    softap_start_time = 0;  // Clear start time
    printf("SoftAP stopped\n");

    return 0;
}

int network_adapter_connect(const char *ssid, const char *password) {
    if (!wifi_initialized) {
        printf("ERROR: WiFi not initialized. Call network_adapter_init() first\n");
        return -1;
    }

    // Stop SoftAP if running
    if (current_mode == NETWORK_MODE_AP) {
        network_adapter_stop_softap();
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
                printf("ERROR: Failed to load WiFi credentials from flash\n");
                return -1;
            }
        } else {
            printf("No WiFi credentials in flash storage\n");
            return -1;
        }
    }

    if (!ssid || strlen(ssid) == 0) {
        printf("ERROR: No SSID provided\n");
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
        printf("ERROR: Failed to connect to WiFi (error %d)\n", result);
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
    
    // Auto-disable SoftAP after successful connection to WiFi
    // This improves security by closing the open access point
    // Note: SoftAP should not be running at this point, but check anyway
    if (softap_start_time != 0) {
        printf("Auto-disabling SoftAP after successful WiFi connection\n");
        softap_start_time = 0;
    }
    
    return 0;
}

int network_adapter_save_and_connect(const char *ssid, const char *password) {
    if (!ssid || !password) {
        printf("ERROR: Invalid SSID or password\n");
        return -1;
    }

    // Save credentials to flash
    printf("Saving WiFi credentials to flash...\n");
    if (storage_adapter_save_wifi_credentials(ssid, password) != 0) {
        printf("ERROR: Failed to save WiFi credentials\n");
        return -1;
    }

    // Connect to WiFi
    return network_adapter_connect(ssid, password);
}

bool network_adapter_is_connected(void) {
    return wifi_connected && wifi_initialized;
}

bool network_adapter_is_softap_mode(void) {
    return current_mode == NETWORK_MODE_AP;
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
    
    if (current_mode == NETWORK_MODE_AP) {
        // Get AP interface IP
        netif = &cyw43_state.netif[CYW43_ITF_AP];
    } else if (current_mode == NETWORK_MODE_STA) {
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
    softap_start_time = 0;
    printf("WiFi adapter deinitialized\n");
}

bool network_adapter_softap_timeout_expired(void) {
    // Check if SoftAP is running and timeout has expired
    if (current_mode != NETWORK_MODE_AP || softap_start_time == 0) {
        return false;  // Not in SoftAP mode
    }
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_time = current_time - softap_start_time;
    
    // Handle timestamp wrap-around (occurs after ~49.7 days)
    // Unsigned arithmetic naturally handles wrap-around correctly
    // for elapsed time calculations up to 2^31 ms (~24.8 days)
    return elapsed_time >= SOFTAP_TIMEOUT_MS;
}

} // extern "C"
