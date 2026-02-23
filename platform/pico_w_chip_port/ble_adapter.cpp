/*
 * ble_adapter.cpp
 * BLE adapter — BTstack removed for CYW43 init diagnostic.
 *
 * pico_btstack_cyw43 sets CYW43_ENABLE_BLUETOOTH=1 at compile time, which
 * causes cyw43_arch_init() to load BT firmware in addition to WiFi firmware.
 * This stub eliminates all BTstack linkage so we can determine whether the
 * BT firmware load is the root cause of the CYW43 initialization hang.
 *
 * All public ble_adapter functions return success / safe defaults so that
 * platform_manager and the main loop continue to compile and run unchanged.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "ble_adapter.h"

static ble_state_t current_state   = BLE_STATE_OFF;
static bool        ble_initialized = false;

extern "C" {

int ble_adapter_init(void) {
    if (ble_initialized) {
        return 0;
    }
    printf("BLE: BTstack removed for diagnostics — ble_adapter_init() is a no-op\n");
    ble_initialized = true;
    current_state   = BLE_STATE_OFF;
    return 0;
}

int ble_adapter_start_advertising(uint16_t discriminator,
                                   uint16_t vendor_id,
                                   uint16_t product_id) {
    (void)discriminator;
    (void)vendor_id;
    (void)product_id;
    /* Return 0 (not -1) so that platform_manager_start_commissioning_mode()
     * does not abort — we want the firmware to continue past the BLE stage
     * and reach cyw43_arch_init() for the WiFi diagnostic. */
    printf("BLE: advertising skipped (BTstack removed for diagnostics)\n");
    return 0;
}

int ble_adapter_stop_advertising(void) {
    return 0;
}

int ble_adapter_send_data(const uint8_t *data, size_t length) {
    (void)data;
    (void)length;
    return -1;
}

bool ble_adapter_is_connected(void) {
    return false;
}

ble_state_t ble_adapter_get_state(void) {
    return current_state;
}

void ble_adapter_set_data_received_callback(ble_data_received_callback_t callback) {
    (void)callback;
}

void ble_adapter_set_connection_callback(ble_connection_callback_t callback) {
    (void)callback;
}

void ble_adapter_task(void) {
    /* no-op */
}

void ble_adapter_deinit(void) {
    ble_initialized = false;
    current_state   = BLE_STATE_OFF;
}

bool ble_adapter_is_initialized(void) {
    return ble_initialized;
}

} /* extern "C" */
