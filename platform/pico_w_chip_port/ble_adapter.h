/*
 * ble_adapter.h
 * Bluetooth LE adapter for Matter commissioning on Pico W
 * Uses BTstack for BLE GATT services
 */

#ifndef BLE_ADAPTER_H
#define BLE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// BLE connection states
typedef enum {
    BLE_STATE_OFF = 0,
    BLE_STATE_ADVERTISING,
    BLE_STATE_CONNECTED,
    BLE_STATE_ERROR
} ble_state_t;

// BLE adapter callbacks
typedef void (*ble_data_received_callback_t)(const uint8_t *data, size_t length);
typedef void (*ble_connection_callback_t)(bool connected);

/**
 * @brief Initialize BLE adapter
 * @return 0 on success, -1 on failure
 */
int ble_adapter_init(void);

/**
 * @brief Start BLE advertising for Matter commissioning
 * @param device_discriminator Matter discriminator for advertising
 * @param vendor_id Matter vendor ID
 * @param product_id Matter product ID
 * @return 0 on success, -1 on failure
 */
int ble_adapter_start_advertising(uint16_t device_discriminator, 
                                   uint16_t vendor_id, 
                                   uint16_t product_id);

/**
 * @brief Stop BLE advertising
 * @return 0 on success, -1 on failure
 */
int ble_adapter_stop_advertising(void);

/**
 * @brief Send data over BLE connection
 * @param data Pointer to data buffer
 * @param length Length of data
 * @return Number of bytes sent, or -1 on failure
 */
int ble_adapter_send_data(const uint8_t *data, size_t length);

/**
 * @brief Check if BLE is connected
 * @return true if connected, false otherwise
 */
bool ble_adapter_is_connected(void);

/**
 * @brief Get current BLE state
 * @return Current BLE state
 */
ble_state_t ble_adapter_get_state(void);

/**
 * @brief Set callback for received data
 * @param callback Callback function
 */
void ble_adapter_set_data_received_callback(ble_data_received_callback_t callback);

/**
 * @brief Set callback for connection state changes
 * @param callback Callback function
 */
void ble_adapter_set_connection_callback(ble_connection_callback_t callback);

/**
 * @brief Process BLE events (call from main loop)
 */
void ble_adapter_task(void);

/**
 * @brief Shutdown BLE adapter
 */
void ble_adapter_deinit(void);

/**
 * @brief Check if BLE is initialized
 * @return true if initialized, false otherwise
 */
bool ble_adapter_is_initialized(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_ADAPTER_H
