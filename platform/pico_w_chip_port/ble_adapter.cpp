/*
 * ble_adapter.cpp
 * BLE adapter stub — BTstack libraries (pico_btstack_ble, pico_btstack_cyw43)
 * are not linked because btstack_cyw43_init() -> setup_tlv() ->
 * btstack_tlv_flash_bank_init_instance() scans the BTstack flash bank and
 * hangs indefinitely when that area contains LittleFS data from prior
 * firmware runs.  BLE commissioning is therefore unavailable; WiFi
 * credentials must be set by other means (e.g. editing network_adapter.cpp).
 */

#include <stdio.h>
#include "ble_adapter.h"

static bool ble_initialized = false;

extern "C" {

int ble_adapter_init(void) {
    printf("BLE: disabled (BTstack not linked)\n");
    ble_initialized = true;
    return 0;
}

int ble_adapter_start_advertising(uint16_t /*discriminator*/, uint16_t /*vendor_id*/, uint16_t /*product_id*/) {
    return 0;
}

int ble_adapter_stop_advertising(void) {
    return 0;
}

int ble_adapter_send_data(const uint8_t * /*data*/, size_t /*length*/) {
    return -1;
}

bool ble_adapter_is_connected(void) {
    return false;
}

ble_state_t ble_adapter_get_state(void) {
    return BLE_STATE_OFF;
}

void ble_adapter_set_data_received_callback(ble_data_received_callback_t) {}

void ble_adapter_set_connection_callback(ble_connection_callback_t) {}

void ble_adapter_task(void) {}

void ble_adapter_deinit(void) {}

bool ble_adapter_is_initialized(void) {
    return ble_initialized;
}

} // extern "C"
