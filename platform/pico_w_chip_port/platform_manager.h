/*
 * platform_manager.h
 * Platform manager interface for Matter DeviceLayer
 */

#ifndef PLATFORM_MANAGER_H
#define PLATFORM_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize the Matter platform manager
 * This initializes crypto, storage, and network adapters
 * @return 0 on success, -1 on failure
 */
int platform_manager_init(void);

/**
 * Connect to WiFi network
 * @param ssid WiFi SSID (NULL to use default from network_adapter.cpp)
 * @param password WiFi password (NULL to use default from network_adapter.cpp)
 * @return 0 on success, -1 on failure
 */
int platform_manager_connect_wifi(const char *ssid, const char *password);

/**
 * Check if WiFi is connected
 * @return true if connected, false otherwise
 */
bool platform_manager_is_wifi_connected(void);

/**
 * Get network information
 * @param ip_buffer Buffer to store IP address string (can be NULL)
 * @param ip_buffer_len Size of IP buffer
 * @param mac_buffer Buffer to store MAC address (6 bytes, can be NULL)
 */
void platform_manager_get_network_info(char *ip_buffer, size_t ip_buffer_len, 
                                       uint8_t *mac_buffer);

/**
 * Generate Matter commissioning QR code
 * @param buffer Buffer to store QR code string
 * @param buffer_len Size of buffer
 * @return 0 on success, -1 on failure
 */
int platform_manager_generate_qr_code(char *buffer, size_t buffer_len);

/**
 * Print commissioning information to console
 */
void platform_manager_print_commissioning_info(void);

/**
 * Run periodic platform tasks
 * Call this regularly in main loop
 */
void platform_manager_task(void);

/**
 * Shutdown platform manager
 */
void platform_manager_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_MANAGER_H
