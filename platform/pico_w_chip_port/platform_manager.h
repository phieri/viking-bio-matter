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
 * If ssid and password are NULL, tries to load from storage
 * If ssid and password are provided, saves them and connects
 * 
 * @param ssid WiFi SSID (NULL to load from storage or use default)
 * @param password WiFi password (NULL to load from storage or use default)
 * @return 0 on success, -1 on failure
 */
int platform_manager_connect_wifi(const char *ssid, const char *password);

/**
 * Start commissioning mode (BLE)
 * Enables Bluetooth LE advertising for Matter commissioning
 * 
 * @return 0 on success, -1 on error
 */
int platform_manager_start_commissioning_mode(void);

/**
 * Stop commissioning mode (BLE)
 * Disables Bluetooth LE advertising and services
 * Called automatically after successful commissioning
 * 
 * @return 0 on success, -1 on error
 */
int platform_manager_stop_commissioning_mode(void);

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
 * Derive setup PIN from MAC address
 * @param mac_addr MAC address (6 bytes)
 * @param out_pin8 Output buffer for 8-digit PIN (must be 9 bytes for null terminator)
 * @return 0 on success, -1 on failure
 */
int platform_manager_derive_setup_pin(const uint8_t *mac_addr, char *out_pin8);

/**
 * Get device discriminator value
 * Returns the discriminator loaded from storage or generated on first boot
 * @return 12-bit discriminator value (0-4095)
 */
uint16_t platform_manager_get_discriminator(void);

/**
 * Run periodic platform tasks
 * Call this regularly in main loop
 */
void platform_manager_task(void);

/**
 * Shutdown platform manager
 */
void platform_manager_deinit(void);

/**
 * Notify platform of Matter attribute change
 * Lightweight, non-fatal call to trigger attribute reports
 * 
 * @param cluster_id Matter cluster ID (e.g., 0x0006 for OnOff)
 * @param attribute_id Matter attribute ID
 * @param endpoint Endpoint number (typically 1)
 */
void platform_manager_report_attribute_change(uint32_t cluster_id, 
                                              uint32_t attribute_id, 
                                              uint8_t endpoint);

/**
 * Report OnOff cluster attribute change (cluster 0x0006, attribute 0x0000)
 * Convenience wrapper for flame state changes
 * 
 * @param endpoint Endpoint number (typically 1)
 */
void platform_manager_report_onoff_change(uint8_t endpoint);

/**
 * Report LevelControl cluster attribute change (cluster 0x0008, attribute 0x0000)
 * Convenience wrapper for fan speed changes
 * 
 * @param endpoint Endpoint number (typically 1)
 */
void platform_manager_report_level_change(uint8_t endpoint);

/**
 * Report TemperatureMeasurement cluster attribute change (cluster 0x0402, attribute 0x0000)
 * Convenience wrapper for temperature changes
 * 
 * @param endpoint Endpoint number (typically 1)
 */
void platform_manager_report_temperature_change(uint8_t endpoint);

/**
 * Start DNS-SD advertisement for Matter device discovery
 * Advertises the device as a commissionable Matter node on _matterc._udp
 * Should be called after network connection is established
 * 
 * @return 0 on success, -1 on failure
 */
int platform_manager_start_dns_sd_advertisement(void);

/**
 * Stop DNS-SD advertisement
 */
void platform_manager_stop_dns_sd_advertisement(void);

/**
 * Check if DNS-SD is currently advertising
 * 
 * @return true if advertising, false otherwise
 */
bool platform_manager_is_dns_sd_advertising(void);

#ifdef __cplusplus
}
#endif

#endif // PLATFORM_MANAGER_H
