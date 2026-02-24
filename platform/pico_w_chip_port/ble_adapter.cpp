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
 *                                sets up CHIPoBLE GATT service,
 *                                registers HCI + ATT packet handlers
 *   4. ble_adapter_start_advertising() → sets advert data, calls
 *                                        hci_power_control(HCI_POWER_ON)
 *
 * BLE events are driven by cyw43_arch_poll() in the main loop.
 *
 * CHIPoBLE GATT service (Matter Core Spec §3.6):
 *   Service UUID128: 0000FFF6-0000-1000-8000-00805F9B34FB
 *   Characteristic 0xFFF7: Write Without Response (controller → device)
 *   Characteristic 0xFFF8: Notify (device → controller)
 *     + Client Characteristic Configuration Descriptor (0x2902)
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

/* ATT server for CHIPoBLE GATT service */
#include "att_server.h"
#include "att_db_util.h"

/* ------------------------------------------------------------------ */
/* CHIPoBLE constants                                                   */
/* ------------------------------------------------------------------ */

/*
 * COBLe (CHIP over BLE) segment header flags per Matter Core Spec §3.6.1.2.1
 *   bit 0 (0x01) - CONN_START (SYN): first segment of a new message
 *   bit 1 (0x02) - CONN_END   (FIN): last segment of a message
 *   bit 2 (0x04) - ACK_MSG        : ack counter present in header
 */
#define COBLE_FLAG_SYN          0x01u
#define COBLE_FLAG_FIN          0x02u
#define COBLE_FLAG_ACK          0x04u

/* Maximum reassembled Matter-over-BLE message size */
#define COBLE_MAX_MSG_SIZE      1024u

/*
 * CHIPoBLE service UUID128: 0000FFF6-0000-1000-8000-00805F9B34FB
 * Stored little-endian (ATT byte order).
 */
static const uint8_t MATTER_BLE_SVC_UUID128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF6, 0xFF, 0x00, 0x00
};

/* ------------------------------------------------------------------ */
/* Internal state                                                       */
/* ------------------------------------------------------------------ */

static ble_state_t current_state   = BLE_STATE_OFF;
static bool        ble_initialized = false;
static ble_data_received_callback_t data_callback = NULL;
static ble_connection_callback_t    conn_callback  = NULL;

static btstack_packet_callback_registration_t hci_event_cb_reg;

/* Advertisement payload (max 31 bytes for legacy BLE adv) */
static uint8_t adv_data[31];
static uint8_t adv_data_len = 0;

/* GATT attribute handles for CHIPoBLE characteristics */
static uint16_t char_tx_handle = 0;   /* 0xFFF7 value handle (Write WR) */
static uint16_t char_rx_handle = 0;   /* 0xFFF8 value handle (Notify)   */

/* Active BLE connection handle (HCI_CON_HANDLE_INVALID when disconnected) */
static hci_con_handle_t active_con_handle = HCI_CON_HANDLE_INVALID;

/* ------------------------------------------------------------------ */
/* COBLe reassembly state                                               */
/* ------------------------------------------------------------------ */

static uint8_t  coble_rx_buf[COBLE_MAX_MSG_SIZE];
static size_t   coble_rx_offset      = 0;
static size_t   coble_rx_total_len   = 0;
static bool     coble_rx_in_progress = false;
static bool     coble_rx_ready       = false;

/* Outgoing message counter (incremented per transmitted message) */
static uint8_t  coble_tx_counter     = 0;

/* ------------------------------------------------------------------ */
/* ATT callbacks                                                        */
/* ------------------------------------------------------------------ */

/*
 * att_read_callback – no readable dynamic characteristics, return 0.
 */
static uint16_t att_read_callback(hci_con_handle_t connection_handle,
                                   uint16_t att_handle,
                                   uint16_t offset,
                                   uint8_t *buffer,
                                   uint16_t buffer_size) {
    (void)connection_handle;
    (void)att_handle;
    (void)offset;
    (void)buffer;
    (void)buffer_size;
    return 0;
}

/*
 * att_write_callback – receives COBLe segments written to characteristic 0xFFF7.
 *
 * Packet format per Matter Core Spec §3.6.1.2.1:
 *   byte 0      : flags (SYN=0x01, FIN=0x02, ACK=0x04)
 *   if SYN:
 *     byte 1    : message counter
 *     bytes 2-3 : total message length (little-endian uint16)
 *   if ACK:
 *     next byte : ack counter
 *   remaining   : payload bytes
 */
static int att_write_callback(hci_con_handle_t connection_handle,
                               uint16_t att_handle,
                               uint16_t transaction_mode,
                               uint16_t offset,
                               uint8_t *buffer,
                               uint16_t buffer_size) {
    (void)connection_handle;
    (void)transaction_mode;
    (void)offset;

    if (att_handle != char_tx_handle || buffer_size < 1) {
        return 0;
    }

    uint8_t  flags         = buffer[0];
    uint16_t payload_start = 1;

    if (flags & COBLE_FLAG_SYN) {
        /* First segment – header contains counter + total length */
        if (buffer_size < 4) {
            printf("BLE COBLe: SYN segment too short (%u bytes)\n",
                   (unsigned)buffer_size);
            return 0;
        }

        /* byte[1] = message counter (consume but don't act on for now) */
        payload_start++;        /* skip counter                  */

        uint16_t total_len = (uint16_t)(buffer[payload_start] |
                             ((uint16_t)buffer[payload_start + 1] << 8));
        payload_start += 2;

        if (total_len > COBLE_MAX_MSG_SIZE) {
            printf("BLE COBLe: Message too large (%u bytes), dropping\n",
                   (unsigned)total_len);
            coble_rx_in_progress = false;
            return 0;
        }

        coble_rx_offset      = 0;
        coble_rx_total_len   = total_len;
        coble_rx_in_progress = true;
        coble_rx_ready       = false;
    }

    /* Skip optional ACK byte when present */
    if ((flags & COBLE_FLAG_ACK) && payload_start < buffer_size) {
        payload_start++;
    }

    if (!coble_rx_in_progress) {
        return 0;   /* FIN without SYN – stray segment, ignore */
    }

    /* Append payload bytes to reassembly buffer */
    if (payload_start < buffer_size) {
        uint16_t data_len = buffer_size - payload_start;
        if (coble_rx_offset + data_len > COBLE_MAX_MSG_SIZE) {
            data_len = (uint16_t)(COBLE_MAX_MSG_SIZE - coble_rx_offset);
        }
        memcpy(coble_rx_buf + coble_rx_offset,
               buffer + payload_start, data_len);
        coble_rx_offset += data_len;
    }

    if (flags & COBLE_FLAG_FIN) {
        coble_rx_ready       = true;
        coble_rx_in_progress = false;
        printf("BLE COBLe: Complete message received (%zu/%zu bytes)\n",
               coble_rx_offset, coble_rx_total_len);
        /* Notify data callback if registered */
        if (data_callback) {
            data_callback(coble_rx_buf, coble_rx_offset);
        }
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* HCI + ATT packet handler                                             */
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
                current_state = BLE_STATE_ADVERTISING;
            } else if (state == HCI_STATE_OFF) {
                current_state       = BLE_STATE_OFF;
                active_con_handle   = HCI_CON_HANDLE_INVALID;
            }
            break;
        }

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("BLE: Client disconnected\n");
            active_con_handle = HCI_CON_HANDLE_INVALID;
            current_state     = BLE_STATE_ADVERTISING;
            /* Reset COBLe state */
            coble_rx_in_progress = false;
            coble_rx_ready       = false;
            coble_rx_offset      = 0;
            if (conn_callback) {
                conn_callback(false);
            }
            break;

        case HCI_EVENT_LE_META: {
            if (hci_event_le_meta_get_subevent_code(packet) ==
                    HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                active_con_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                printf("BLE: Client connected (handle=0x%04X)\n",
                       (unsigned)active_con_handle);
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
 * Format per Matter Core Spec 1.5 §5.4.2.5.2:
 *   AD 0x01 Flags           (3 bytes)
 *   AD 0x03 Complete 16-bit UUID list: 0xFFF6   (4 bytes)
 *   AD 0x16 Service Data UUID 0xFFF6 (12 bytes)
 *
 * Service-data payload (7 bytes, Table 5.14):
 *   byte 0: bits[1:0]=CM, bits[3:2]=OpCode
 *             CM=0x02 (standard/open commissioning mode per §5.4.2.5.3)
 *             OpCode=0x00 (commissionable)
 *   byte 1: Discriminator[7:0]
 *   byte 2: Discriminator[11:8]  (upper nibble, bits[3:0])
 *   bytes 3-4: Vendor ID  (little-endian)
 *   bytes 5-6: Product ID (little-endian)
 */
static void build_matter_adv_data(uint16_t discriminator,
                                   uint16_t vendor_id,
                                   uint16_t product_id) {
    uint8_t *p = adv_data;

    /* --- Flags --- */
    *p++ = 0x02;        /* length */
    *p++ = 0x01;        /* AD type: Flags */
    *p++ = 0x06;        /* LE General Discoverable | BR/EDR Not Supported */

    /* --- Complete list of 16-bit UUIDs: 0xFFF6 (Matter) --- */
    *p++ = 0x03;        /* length */
    *p++ = 0x03;        /* AD type: Complete List of 16-bit Service UUIDs */
    *p++ = 0xF6;        /* UUID low byte  */
    *p++ = 0xFF;        /* UUID high byte */

    /* --- Service Data for UUID 0xFFF6 --- */
    *p++ = 0x0A;        /* length: 10 bytes (type + UUID + 7 payload bytes) */
    *p++ = 0x16;        /* AD type: Service Data – 16-bit UUID */
    *p++ = 0xF6;        /* UUID low byte  */
    *p++ = 0xFF;        /* UUID high byte */
    /* Service-data payload (7 bytes, Matter Core Spec 1.5 Table 5.14):
     *   byte 0: bits[1:0]=CM, bits[3:2]=OpCode
     *     CM=0x02 (standard open commissioning window) per §5.4.2.5.3
     *     OpCode=0x00 (commissionable)  →  byte value = 0x02
     *   byte 1: Discriminator[7:0]
     *   byte 2: Discriminator[11:8] (lower nibble)
     *   bytes 3-6: Vendor ID and Product ID (little-endian)
     */
    *p++ = 0x02;                                         /* OpCode=0x00, CM=0x02        */
    *p++ = (uint8_t)(discriminator & 0xFF);              /* Discriminator[7:0]          */
    *p++ = (uint8_t)((discriminator >> 8) & 0x0F);      /* Discriminator[11:8], 4 bits */
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

    /* Register HCI event handler (multiple handlers are supported) */
    hci_event_cb_reg.callback = &packet_handler;
    hci_add_event_handler(&hci_event_cb_reg);

    /* -------------------------------------------------------------- */
    /* CHIPoBLE GATT service setup                                      */
    /* -------------------------------------------------------------- */
    att_db_util_init();

    /* Primary service: 0000FFF6-0000-1000-8000-00805F9B34FB */
    att_db_util_add_service_uuid128(MATTER_BLE_SVC_UUID128);

    /*
     * Characteristic 0xFFF7: TX channel – controller writes Matter
     * messages to this characteristic (Write Without Response).
     * Parameters: uuid16, properties, read_permission, write_permission, data, data_len
     */
    char_tx_handle = att_db_util_add_characteristic_uuid16(
        0xFFF7,
        ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        NULL, 0);

    /*
     * Characteristic 0xFFF8: RX channel – device sends Matter
     * responses to the controller via notifications.
     * A Client Characteristic Configuration Descriptor (CCCD) at
     * char_rx_handle+1 is automatically created by BTstack when
     * ATT_PROPERTY_NOTIFY is specified.
     */
    char_rx_handle = att_db_util_add_characteristic_uuid16(
        0xFFF8,
        ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        NULL, 0);

    /* Start the ATT server with the database we just built */
    att_server_init(att_db_util_get_address(),
                    att_read_callback,
                    att_write_callback);

    /* Register the same packet handler for ATT events (MTU updates, etc.) */
    att_server_register_packet_handler(packet_handler);

    printf("BLE: CHIPoBLE GATT service registered "
           "(TX handle=0x%04X, RX handle=0x%04X)\n",
           (unsigned)char_tx_handle, (unsigned)char_rx_handle);

    ble_initialized   = true;
    current_state     = BLE_STATE_OFF;
    active_con_handle = HCI_CON_HANDLE_INVALID;
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
     *
     * Per Matter spec §5.4.2.3: advertising interval must be 20–240 ms.
     */
    bd_addr_t unused_peer_addr = {0, 0, 0, 0, 0, 0};
    gap_advertisements_set_params(
        0x00A0,           /* adv_int_min: ~100 ms */
        0x0140,           /* adv_int_max: ~200 ms */
        0,                /* ADV_IND */
        0,                /* own address type: public */
        unused_peer_addr,
        0x07,             /* all three advertising channels */
        0                 /* no filter policy */
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

/*
 * ble_adapter_send_data – send a raw Matter message over the BLE RX
 * characteristic using COBLe framing (Matter Core Spec §3.6.1).
 *
 * Large messages are fragmented into ATT-sized segments.
 * Uses the default conservative fragment size; the controller will
 * negotiate a larger MTU if desired.
 */
int ble_adapter_send_data(const uint8_t *data, size_t length) {
    if (!ble_initialized || active_con_handle == HCI_CON_HANDLE_INVALID ||
        char_rx_handle == 0) {
        return -1;
    }

    /*
     * Conservative fragment payload size: allow 4 bytes for the
     * SYN header (flags + counter + length-LE16) leaving room
     * within the default 20-byte BLE payload.  Controllers that
     * negotiate a larger MTU will still receive valid fragments.
     */
    const size_t MAX_PAYLOAD_PER_FRAG = 244u; /* Negotiated ATT MTU (247) minus
                                                * 3-byte ATT PDU header         */
    const size_t MAX_DATA_IN_FIRST    = MAX_PAYLOAD_PER_FRAG - 4u; /* SYN hdr */
    const size_t MAX_DATA_IN_REST     = MAX_PAYLOAD_PER_FRAG - 1u; /* flags only */

    uint8_t fragment[247];
    size_t  sent    = 0;
    bool    is_first = true;

    while (sent < length) {
        size_t frag_hdr = 0;
        size_t data_capacity;

        if (is_first) {
            uint8_t flags = COBLE_FLAG_SYN;
            if (sent + MAX_DATA_IN_FIRST >= length) {
                flags |= COBLE_FLAG_FIN;
            }
            fragment[frag_hdr++] = flags;
            fragment[frag_hdr++] = coble_tx_counter++;
            fragment[frag_hdr++] = (uint8_t)(length & 0xFF);
            fragment[frag_hdr++] = (uint8_t)((length >> 8) & 0xFF);
            data_capacity = MAX_DATA_IN_FIRST;
            is_first = false;
        } else {
            bool is_last = (sent + MAX_DATA_IN_REST >= length);
            fragment[frag_hdr++] = is_last ? COBLE_FLAG_FIN : 0x00u;
            data_capacity = MAX_DATA_IN_REST;
        }

        size_t chunk = length - sent;
        if (chunk > data_capacity) {
            chunk = data_capacity;
        }
        memcpy(fragment + frag_hdr, data + sent, chunk);
        sent += chunk;

        uint8_t err = att_server_notify(active_con_handle, char_rx_handle,
                                    fragment, (uint16_t)(frag_hdr + chunk));
        if (err != ERROR_CODE_SUCCESS) {
            printf("BLE: Notification failed (err=0x%02X, sent=%zu/%zu)\n",
                   (unsigned)err, sent - chunk, length);
            return -1;
        }
    }

    printf("BLE: Sent %zu-byte message via COBLe (%zu fragments)\n",
           length, (length + MAX_DATA_IN_FIRST - 1) / MAX_DATA_IN_FIRST);
    return 0;
}

/*
 * ble_adapter_receive_message – dequeue the most recently reassembled
 * COBLe message.  Returns 0 and fills *buffer if a complete message is
 * available; returns -1 if no message is ready.
 */
int ble_adapter_receive_message(uint8_t *buffer, size_t max_len,
                                 size_t *actual_len) {
    if (!coble_rx_ready) {
        return -1;
    }

    size_t copy_len = coble_rx_offset;
    if (copy_len > max_len) {
        copy_len = max_len;
    }
    memcpy(buffer, coble_rx_buf, copy_len);
    if (actual_len) {
        *actual_len = copy_len;
    }

    coble_rx_ready  = false;
    coble_rx_offset = 0;
    return 0;
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
    ble_initialized   = false;
    current_state     = BLE_STATE_OFF;
    active_con_handle = HCI_CON_HANDLE_INVALID;
}

bool ble_adapter_is_initialized(void) {
    return ble_initialized;
}

} /* extern "C" */
