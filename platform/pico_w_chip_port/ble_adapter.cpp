/*
 * ble_adapter.cpp
 * BLE adapter for Matter commissioning on Pico W using BTstack.
 *
 * Uses a LittleFS-backed TLV backend (btstack_tlv_littlefs) so that
 * BTstack's persistent state is stored alongside Matter data in LittleFS,
 * eliminating the dedicated BTstack flash-bank region and the infinite-scan
 * bug that previously prevented BTstack from being linked.
 *
 * Initialisation order that must be respected:
 *   1. network_adapter_init()  → cyw43_arch_init() → btstack_cyw43_init()
 *                                (runs setup_tlv() with default flash-bank TLV)
 *   2. storage_adapter_init()  → LittleFS mounted and ready
 *   3. ble_adapter_init()      → overrides TLV with LittleFS backend,
 *                                registers HCI packet handler
 *   4. ble_adapter_start_advertising() → sets advert data, calls
 *                                        hci_power_control(HCI_POWER_ON)
 *
 * BLE events are driven by cyw43_arch_poll() in the main loop.
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "ble_adapter.h"
#include "btstack_tlv_littlefs.h"

/* BTstack headers (available when pico_btstack_ble + pico_btstack_cyw43 are linked) */
#include "btstack.h"
#include "pico/btstack_cyw43.h"

/* ------------------------------------------------------------------ */
/* Internal state                                                       */
/* ------------------------------------------------------------------ */

static ble_state_t current_state = BLE_STATE_OFF;
static bool ble_initialized = false;
static ble_data_received_callback_t data_callback = NULL;
static ble_connection_callback_t    conn_callback  = NULL;

static btstack_packet_callback_registration_t hci_event_cb_reg;

/* Advertisement payload (max 31 bytes for legacy BLE adv) */
static uint8_t adv_data[31];
static uint8_t adv_data_len = 0;

/* Device name shown in BLE scans */
static const char DEVICE_NAME[] = "VikingBio";

/* ------------------------------------------------------------------ */
/* HCI packet handler                                                   */
/* ------------------------------------------------------------------ */

static void packet_handler(uint8_t packet_type, uint16_t channel,
                            uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {

        case BTSTACK_EVENT_STATE: {
            uint8_t state = btstack_event_state_get_state(packet);
            if (state == HCI_STATE_WORKING) {
                printf("BLE: HCI controller ready\n");
                /* Advertising was enabled before power-on; it becomes
                 * active now that HCI is up. */
                current_state = BLE_STATE_ADVERTISING;
            } else if (state == HCI_STATE_OFF) {
                current_state = BLE_STATE_OFF;
            }
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("BLE: Client disconnected\n");
            current_state = BLE_STATE_ADVERTISING;
            if (conn_callback) {
                conn_callback(false);
            }
            break;

        case HCI_EVENT_LE_META: {
            if (hci_event_le_meta_get_subevent_code(packet) ==
                    HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                printf("BLE: Client connected\n");
                current_state = BLE_STATE_CONNECTED;
                if (conn_callback) {
                    conn_callback(true);
                }
            }
            break;
        }

        default:
            break;
    }
}

/* ------------------------------------------------------------------ */
/* Advertisement payload builder                                        */
/* ------------------------------------------------------------------ */

/*
 * Build a Matter-compliant BLE advertisement payload.
 *
 * Format (Matter Core Spec v1.2 §5.4.2.5):
 *   Flags (3B) | 16-bit UUID list (4B) | Service Data UUID 0xFFF6 (11B)
 *
 * Service-data payload (7 bytes after UUID):
 *   [OpCode+CM] [disc_low8] [disc_high4] [VID_L] [VID_H] [PID_L] [PID_H]
 *
 *   OpCode  = 0x0 (commissionable advertisement)
 *   CM_bits = 0x3 (standard commissioning mode open)
 */
static void build_matter_adv_data(uint16_t discriminator,
                                   uint16_t vendor_id,
                                   uint16_t product_id) {
    uint8_t *p = adv_data;

    /* --- Flags --- */
    *p++ = 0x02;        /* length */
    *p++ = 0x01;        /* AD type: Flags */
    *p++ = 0x06;        /* LE General Discoverable | BR/EDR Not Supported */

    /* --- Incomplete list of 16-bit UUIDs: 0xFFF6 (Matter) --- */
    *p++ = 0x03;        /* length */
    *p++ = 0x02;        /* AD type: Incomplete List of 16-bit Service UUIDs */
    *p++ = 0xF6;        /* UUID low byte */
    *p++ = 0xFF;        /* UUID high byte */

    /* --- Service Data for UUID 0xFFF6 --- */
    *p++ = 0x0A;        /* length: 10 bytes (type + UUID + 7 payload bytes) */
    *p++ = 0x16;        /* AD type: Service Data – 16-bit UUID */
    *p++ = 0xF6;        /* UUID low byte */
    *p++ = 0xFF;        /* UUID high byte */
    /* Payload */
    *p++ = 0x03;                                         /* OpCode=0, CM=3 */
    *p++ = (uint8_t)(discriminator & 0xFF);              /* disc[7:0]  */
    *p++ = (uint8_t)((discriminator >> 8) & 0x0F);      /* disc[11:8] */
    *p++ = (uint8_t)(vendor_id  & 0xFF);
    *p++ = (uint8_t)(vendor_id  >> 8);
    *p++ = (uint8_t)(product_id & 0xFF);
    *p++ = (uint8_t)(product_id >> 8);

    adv_data_len = (uint8_t)(p - adv_data);
}

/* ------------------------------------------------------------------ */
/* Public API (C linkage)                                               */
/* ------------------------------------------------------------------ */

extern "C" {

int ble_adapter_init(void) {
    if (ble_initialized) {
        return 0;
    }

    printf("BLE: Initializing BTstack BLE adapter\n");

    /*
     * Override the flash-bank TLV that btstack_cyw43_init() registered
     * during cyw43_arch_init() with our LittleFS-backed backend.
     * storage_adapter_init() must have run before this call.
     */
    const btstack_tlv_t *lfs_tlv = btstack_tlv_littlefs_init_instance();
    btstack_tlv_set_instance(lfs_tlv, NULL);
    printf("BLE: LittleFS TLV backend registered\n");

    /* Register our HCI event handler (multiple handlers are supported) */
    hci_event_cb_reg.callback = &packet_handler;
    hci_add_event_handler(&hci_event_cb_reg);

    ble_initialized = true;
    current_state   = BLE_STATE_OFF;
    printf("BLE: BTstack adapter ready\n");
    return 0;
}

int ble_adapter_start_advertising(uint16_t discriminator,
                                   uint16_t vendor_id,
                                   uint16_t product_id) {
    if (!ble_initialized) {
        printf("BLE: ERROR: Not initialized\n");
        return -1;
    }

    printf("BLE: Starting Matter advertisement "
           "(discriminator=0x%03X VID=0x%04X PID=0x%04X)\n",
           discriminator, vendor_id, product_id);

    build_matter_adv_data(discriminator, vendor_id, product_id);

    /* Set advertisement payload */
    gap_advertisements_set_data(adv_data_len, adv_data);

    /*
     * Advertising parameters:
     *   interval 100–200 ms (units of 0.625 ms → 0x00A0–0x0140)
     *   ADV_IND: connectable undirected, own public address, all channels
     */
    bd_addr_t no_direct_addr = {0, 0, 0, 0, 0, 0};
    gap_advertisements_set_params(
        0x00A0,         /* adv_int_min: ~100 ms */
        0x0140,         /* adv_int_max: ~200 ms */
        0,              /* ADV_IND */
        0,              /* own address type: public */
        no_direct_addr,
        0x07,           /* all three advertising channels */
        0               /* no filter policy */
    );

    gap_advertisements_enable(1);

    /* Power on the HCI controller; advertising starts once
     * BTSTACK_EVENT_STATE / HCI_STATE_WORKING is received. */
    hci_power_control(HCI_POWER_ON);

    current_state = BLE_STATE_ADVERTISING;
    return 0;
}

int ble_adapter_stop_advertising(void) {
    if (!ble_initialized) {
        return -1;
    }
    printf("BLE: Stopping advertising\n");
    gap_advertisements_enable(0);
    current_state = BLE_STATE_OFF;
    return 0;
}

int ble_adapter_send_data(const uint8_t *data, size_t length) {
    (void)data;
    (void)length;
    /* Full COBLe bidirectional data exchange is not implemented here */
    return -1;
}

bool ble_adapter_is_connected(void) {
    return current_state == BLE_STATE_CONNECTED;
}

ble_state_t ble_adapter_get_state(void) {
    return current_state;
}

void ble_adapter_set_data_received_callback(ble_data_received_callback_t callback) {
    data_callback = callback;
}

void ble_adapter_set_connection_callback(ble_connection_callback_t callback) {
    conn_callback = callback;
}

void ble_adapter_task(void) {
    /* BLE events are driven by cyw43_arch_poll() in the main loop */
}

void ble_adapter_deinit(void) {
    if (!ble_initialized) {
        return;
    }
    printf("BLE: Shutting down\n");
    gap_advertisements_enable(0);
    hci_power_control(HCI_POWER_OFF);
    ble_initialized = false;
    current_state   = BLE_STATE_OFF;
}

bool ble_adapter_is_initialized(void) {
    return ble_initialized;
}

} /* extern "C" */
