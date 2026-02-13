/*
 * network_adapter.h
 * Network adapter API for Pico W (CYW43439 WiFi chip)
 * Supports both Station (STA) and Access Point (AP) modes
 */

#ifndef NETWORK_ADAPTER_H
#define NETWORK_ADAPTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Network mode
 */
typedef enum {
    NETWORK_MODE_NONE = 0,  // Not connected
    NETWORK_MODE_STA,       // Station mode (WiFi client)
    NETWORK_MODE_AP         // Access Point mode (SoftAP)
} network_mode_t;

/**
 * Initialize network adapter
 * 
 * @return 0 on success, -1 on error
 */
int network_adapter_init(void);

/**
 * Start SoftAP mode for commissioning
 * Creates a WiFi access point that clients can connect to
 * 
 * @return 0 on success, -1 on error
 */
int network_adapter_start_softap(void);

/**
 * Stop SoftAP mode
 * 
 * @return 0 on success, -1 on error
 */
int network_adapter_stop_softap(void);

/**
 * Connect to WiFi network in station mode
 * If ssid/password are NULL, tries to load from flash storage
 * 
 * @param ssid WiFi SSID (NULL to load from storage)
 * @param password WiFi password (NULL to load from storage)
 * @return 0 on success, non-zero on error
 */
int network_adapter_connect(const char *ssid, const char *password);

/**
 * Save WiFi credentials to flash and connect
 * 
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return 0 on success, -1 on error
 */
int network_adapter_save_and_connect(const char *ssid, const char *password);

/**
 * Check if WiFi is connected
 * 
 * @return true if connected (either STA or AP mode)
 */
bool network_adapter_is_connected(void);

/**
 * Check if in SoftAP mode
 * 
 * @return true if in AP mode
 */
bool network_adapter_is_softap_mode(void);

/**
 * Get current network mode
 * 
 * @return Current network_mode_t
 */
network_mode_t network_adapter_get_mode(void);

/**
 * Get IP address
 * 
 * @param buffer Output buffer for IP address string
 * @param buffer_len Buffer size
 */
void network_adapter_get_ip_address(char *buffer, size_t buffer_len);

/**
 * Get MAC address
 * 
 * @param mac_addr Output buffer for 6-byte MAC address
 */
void network_adapter_get_mac_address(uint8_t *mac_addr);

/**
 * Deinitialize network adapter
 */
void network_adapter_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_ADAPTER_H
