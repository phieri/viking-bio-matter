/*
 * ble_adapter.cpp
 * Bluetooth LE adapter implementation for Pico W
 * Implements Matter commissioning over BLE using BTstack
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/btstack_run_loop_async_context.h"

// BTstack headers
#include "btstack.h"
#include "ble/gatt-service/battery_service_server.h"

#include "ble_adapter.h"

// Matter BLE Service UUIDs (from Matter specification)
// Matter uses a custom GATT service for commissioning
#define MATTER_BLE_SERVICE_UUID             "0000FFF6-0000-1000-8000-00805F9B34FB"
#define MATTER_BLE_TX_CHARACTERISTIC_UUID   "18EE2EF5-263D-4559-959F-4F9C429F9D11"
#define MATTER_BLE_RX_CHARACTERISTIC_UUID   "18EE2EF5-263D-4559-959F-4F9C429F9D12"

// BLE state
static bool ble_initialized = false;
static bool ble_advertising = false;
static bool ble_connected = false;
static hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
static ble_state_t current_state = BLE_STATE_OFF;

// Callbacks
static ble_data_received_callback_t data_received_callback = NULL;
static ble_connection_callback_t connection_callback = NULL;

// BLE buffers
#define BLE_RX_BUFFER_SIZE 512
#define BLE_TX_BUFFER_SIZE 512
static uint8_t ble_rx_buffer[BLE_RX_BUFFER_SIZE];
static size_t ble_rx_buffer_len = 0;

// Matter device info for advertising
static uint16_t device_discriminator = 0;
static uint16_t device_vendor_id = 0;
static uint16_t device_product_id = 0;

// ATT database (simplified for Matter commissioning)
// This would normally be generated from a .gatt file using pico_btstack_make_gatt_header
// For now, we define it manually

static uint8_t adv_data[31];
static uint8_t adv_data_len = 0;

/**
 * @brief HCI packet handler for BLE events
 */
static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    UNUSED(channel);
    UNUSED(size);
    
    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    
    uint8_t event_type = hci_event_packet_get_type(packet);
    
    switch (event_type) {
        case HCI_EVENT_LE_META:
            {
                uint8_t subevent = hci_event_le_meta_get_subevent_code(packet);
                
                switch (subevent) {
                    case HCI_SUBEVENT_LE_CONNECTION_COMPLETE:
                        connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                        ble_connected = true;
                        ble_advertising = false;
                        current_state = BLE_STATE_CONNECTED;
                        
                        printf("BLE: Device connected (handle 0x%04x)\n", connection_handle);
                        
                        if (connection_callback) {
                            connection_callback(true);
                        }
                        break;
                        
                    default:
                        break;
                }
            }
            break;
            
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;
            ble_connected = false;
            current_state = BLE_STATE_OFF;
            
            printf("BLE: Device disconnected\n");
            
            if (connection_callback) {
                connection_callback(false);
            }
            break;
            
        case BTSTACK_EVENT_STATE:
            {
                uint8_t bt_state = btstack_event_state_get_state(packet);
                
                if (bt_state == HCI_STATE_WORKING) {
                    printf("BLE: BTstack ready\n");
                } else if (bt_state == HCI_STATE_OFF) {
                    printf("BLE: BTstack off\n");
                }
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief ATT read callback
 */
static uint16_t att_read_callback(hci_con_handle_t con_handle, uint16_t att_handle, 
                                   uint16_t offset, uint8_t * buffer, uint16_t buffer_size) {
    UNUSED(con_handle);
    UNUSED(att_handle);
    UNUSED(offset);
    UNUSED(buffer);
    UNUSED(buffer_size);
    
    // TODO: Implement Matter characteristic reads
    return 0;
}

/**
 * @brief ATT write callback
 */
static int att_write_callback(hci_con_handle_t con_handle, uint16_t att_handle, 
                              uint16_t transaction_mode, uint16_t offset, 
                              uint8_t *buffer, uint16_t buffer_size) {
    UNUSED(con_handle);
    UNUSED(transaction_mode);
    UNUSED(offset);
    
    // Check if this is Matter RX characteristic write
    // For now, store data and call callback
    if (buffer_size > 0 && buffer_size <= BLE_RX_BUFFER_SIZE) {
        memcpy(ble_rx_buffer, buffer, buffer_size);
        ble_rx_buffer_len = buffer_size;
        
        if (data_received_callback) {
            data_received_callback(ble_rx_buffer, ble_rx_buffer_len);
        }
        
        return 0;
    }
    
    return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
}

extern "C" {

int ble_adapter_init(void) {
    if (ble_initialized) {
        printf("BLE: Already initialized\n");
        return 0;
    }
    
    printf("BLE: Initializing BTstack for Matter commissioning...\n");
    
    // Initialize BTstack with async context
    // Note: CYW43 arch should already be initialized by network_adapter_init()
    
    // Get the async context from CYW43
    async_context_t *async_context = cyw43_arch_async_context();
    
    // Initialize BTstack run loop
    btstack_run_loop_init(btstack_run_loop_async_context_get_instance(async_context));
    
    // Initialize L2CAP
    l2cap_init();
    
    // Initialize LE Security Manager (required for BLE)
    sm_init();
    
    // Setup ATT server
    att_server_init(NULL, att_read_callback, att_write_callback);
    
    // Register packet handler
    btstack_packet_callback_registration_t hci_event_callback;
    hci_event_callback.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback);
    
    // Power on
    hci_power_control(HCI_POWER_ON);
    
    ble_initialized = true;
    current_state = BLE_STATE_OFF;
    
    printf("BLE: Initialization complete\n");
    return 0;
}

int ble_adapter_start_advertising(uint16_t discriminator, uint16_t vendor_id, uint16_t product_id) {
    if (!ble_initialized) {
        printf("BLE: ERROR - Not initialized\n");
        return -1;
    }
    
    if (ble_advertising || ble_connected) {
        printf("BLE: Already advertising or connected\n");
        return 0;
    }
    
    device_discriminator = discriminator;
    device_vendor_id = vendor_id;
    device_product_id = product_id;
    
    printf("BLE: Starting Matter commissioning advertisement\n");
    printf("  Discriminator: 0x%04X\n", discriminator);
    printf("  Vendor ID: 0x%04X\n", vendor_id);
    printf("  Product ID: 0x%04X\n", product_id);
    
    // Setup advertising data
    // Format: Flags + Complete Local Name + Service UUID + Matter-specific data
    
    uint8_t adv_index = 0;
    
    // Flags: General Discoverable, BR/EDR not supported
    adv_data[adv_index++] = 2;  // Length
    adv_data[adv_index++] = BLUETOOTH_DATA_TYPE_FLAGS;
    adv_data[adv_index++] = 0x06;  // LE General Discoverable + BR/EDR not supported
    
    // Complete Local Name
    const char *device_name = "Viking Bio Matter";
    size_t name_len = strlen(device_name);
    adv_data[adv_index++] = (uint8_t)(name_len + 1);
    adv_data[adv_index++] = BLUETOOTH_DATA_TYPE_COMPLETE_LOCAL_NAME;
    memcpy(&adv_data[adv_index], device_name, name_len);
    adv_index += name_len;
    
    // TODO: Add Matter service UUID and discriminator in manufacturer data
    // For now, keep it simple
    
    adv_data_len = adv_index;
    
    // Setup advertising parameters
    uint16_t adv_int_min = 0x0030;  // 30ms
    uint16_t adv_int_max = 0x0030;  // 30ms
    uint8_t adv_type = 0;  // ADV_IND - connectable undirected advertising
    bd_addr_t null_addr = {0};
    
    gap_advertisements_set_params(adv_int_min, adv_int_max, adv_type, 0, null_addr, 0x07, 0x00);
    gap_advertisements_set_data(adv_data_len, adv_data);
    gap_advertisements_enable(1);
    
    ble_advertising = true;
    current_state = BLE_STATE_ADVERTISING;
    
    printf("BLE: Advertising started\n");
    return 0;
}

int ble_adapter_stop_advertising(void) {
    if (!ble_initialized) {
        return -1;
    }
    
    if (!ble_advertising) {
        return 0;
    }
    
    printf("BLE: Stopping advertisement\n");
    gap_advertisements_enable(0);
    
    ble_advertising = false;
    current_state = BLE_STATE_OFF;
    
    return 0;
}

int ble_adapter_send_data(const uint8_t *data, size_t length) {
    if (!ble_initialized || !ble_connected) {
        return -1;
    }
    
    if (!data || length == 0 || length > BLE_TX_BUFFER_SIZE) {
        return -1;
    }
    
    // TODO: Implement Matter TX characteristic notification
    // For now, this is a placeholder
    printf("BLE: Sending %zu bytes (placeholder)\n", length);
    
    return (int)length;
}

bool ble_adapter_is_connected(void) {
    return ble_connected;
}

ble_state_t ble_adapter_get_state(void) {
    return current_state;
}

void ble_adapter_set_data_received_callback(ble_data_received_callback_t callback) {
    data_received_callback = callback;
}

void ble_adapter_set_connection_callback(ble_connection_callback_t callback) {
    connection_callback = callback;
}

void ble_adapter_task(void) {
    // BTstack async context handles event processing automatically
    // This function is kept for API compatibility
}

void ble_adapter_deinit(void) {
    if (!ble_initialized) {
        return;
    }
    
    printf("BLE: Shutting down\n");
    
    // Stop advertising if active
    if (ble_advertising) {
        ble_adapter_stop_advertising();
    }
    
    // Disconnect if connected
    if (ble_connected && connection_handle != HCI_CON_HANDLE_INVALID) {
        gap_disconnect(connection_handle);
    }
    
    // Power off
    hci_power_control(HCI_POWER_OFF);
    
    ble_initialized = false;
    ble_connected = false;
    ble_advertising = false;
    current_state = BLE_STATE_OFF;
    
    printf("BLE: Shutdown complete\n");
}

bool ble_adapter_is_initialized(void) {
    return ble_initialized;
}

} // extern "C"
