/*
 * network_adapter.cpp
 * Network adapter implementation for Pico W (CYW43439 WiFi chip)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

// Configuration - Update these for your WiFi network
#ifndef WIFI_SSID
#define WIFI_SSID "YourSSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "YourPassword"
#endif

static bool wifi_connected = false;
static bool wifi_initialized = false;

int network_adapter_init(void) {
    if (wifi_initialized) {
        return 0;
    }

    printf("Initializing CYW43439 WiFi adapter...\n");

    // Initialize CYW43 in station mode with background LWIP
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_USA)) {
        printf("ERROR: Failed to initialize CYW43 WiFi chip\n");
        return -1;
    }

    cyw43_arch_enable_sta_mode();
    wifi_initialized = true;

    printf("WiFi adapter initialized\n");
    return 0;
}

int network_adapter_connect(const char *ssid, const char *password) {
    if (!wifi_initialized) {
        printf("ERROR: WiFi not initialized. Call network_adapter_init() first\n");
        return -1;
    }

    const char *connect_ssid = ssid ? ssid : WIFI_SSID;
    const char *connect_pass = password ? password : WIFI_PASSWORD;

    printf("Connecting to WiFi SSID: %s\n", connect_ssid);

    int result = cyw43_arch_wifi_connect_timeout_ms(
        connect_ssid,
        connect_pass,
        CYW43_AUTH_WPA2_AES_PSK,
        30000  // 30 second timeout
    );

    if (result != 0) {
        printf("ERROR: Failed to connect to WiFi (error %d)\n", result);
        wifi_connected = false;
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

    wifi_connected = true;
    return 0;
}

bool network_adapter_is_connected(void) {
    return wifi_connected && wifi_initialized;
}

void network_adapter_get_ip_address(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return;
    }

    if (!wifi_connected || !wifi_initialized) {
        snprintf(buffer, buffer_len, "0.0.0.0");
        return;
    }

    struct netif *netif = netif_default;
    if (netif) {
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

    // Get MAC address from CYW43
    cyw43_hal_get_mac(0, mac_addr);
}

void network_adapter_deinit(void) {
    if (!wifi_initialized) {
        return;
    }

    cyw43_arch_deinit();
    wifi_initialized = false;
    wifi_connected = false;
    printf("WiFi adapter deinitialized\n");
}
