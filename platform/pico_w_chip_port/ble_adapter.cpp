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
 *                                sets up GAP + GATT + CHIPoBLE GATT services,
 *                                registers HCI + ATT packet handlers
 *   4. ble_adapter_start_advertising() → sets advert data/params, calls
 *                                        hci_power_control(HCI_POWER_ON);
 *                                        gap_advertisements_enable(1) is
 *                                        called from the HCI_STATE_WORKING
 *                                        event handler.
 *
 * BLE events are driven by cyw43_arch_poll() in the main loop.
 *
 * ATT database layout (built with att_db_util):
 *   1. GAP service (0x1800): Device Name "Matter" + Appearance
 *   2. GATT service (0x1801): no mandatory characteristics for peripherals
 *   3. CHIPoBLE service (0xFFF6):
 *      C1 (128-bit UUID) - Write Without Response (controller → device)
 *      C2 (128-bit UUID) - Indicate + Notify (device → controller) + auto-CCCD
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

/* btstack.h (with ENABLE_BLE) already includes:
 *   ble/att_db_util.h  → att_db_util_init(), att_db_util_add_*
 *   ble/att_server.h   → att_server_init(), att_server_notify(), etc.
 * No separate includes needed.
 */

/* ------------------------------------------------------------------ */
/* CHIPoBLE constants                                                   */
/* ------------------------------------------------------------------ */

/*
 * COBLe (CHIP over BLE) / BTP segment header flags per Matter Core Spec §3.6.1
 *   bit 0 (0x01) - CONN_START (SYN): first segment of a new message
 *   bit 1 (0x02) - CONN_END   (FIN): last segment of a message
 *   bit 2 (0x04) - ACK_MSG        : ack counter present in header
 *   bit 3 (0x08) - MGMT           : BTP management frame (e.g. handshake)
 *
 * A BTP Session Init Request / Response uses SYN|FIN|MGMT (0x0B).  These
 * management frames must NOT be parsed as ordinary COBLe data frames because
 * their payload layout is completely different (no COBLe sequence-counter /
 * total-length header).
 */
#define COBLE_FLAG_SYN          0x01u
#define COBLE_FLAG_FIN          0x02u
#define COBLE_FLAG_ACK          0x04u
#define COBLE_FLAG_MGMT         0x08u   /* BTP management frame */

/*
 * BTP Hello (Session Init) selected protocol version.
 * BTP version 4 is the latest version defined in Matter Core Spec §3.6.3
 * and is the version used by iOS (connectedhomeip) for commissioning.
 * If the central does not include bit 2 (v4) in its supported-versions
 * bitmap, we still respond with BTP_VERSION=4 as a "best-offer"; in
 * practice all current Matter controllers support v4.
 */
#define BTP_VERSION             0x04u   /* BTP version 4, per Matter Core Spec §3.6.3 */
/*
 * Peripheral receive window size advertised in the BTP Hello response.
 * Window size 6 matches the default used by the connectedhomeip reference
 * stack and provides adequate throughput for PASE commissioning messages
 * without overwhelming the Pico W's cooperative polling loop.
 */
#define BTP_WINDOW_SIZE         0x06u

/* Maximum reassembled Matter-over-BLE message size */
#define COBLE_MAX_MSG_SIZE      1024u

/* Maximum single COBLe fragment size (must be >= max ATT MTU) */
#define COBLE_MAX_FRAGMENT_SIZE 256u

/*
 * Backoff (ms) applied after a real BLE disconnect before advertising
 * restarts.  Prevents rapid connect/disconnect loops (e.g. iOS 26 issue).
 * Override at compile time: -DBLE_RECONNECT_BACKOFF_MS=0 to disable.
 */
#ifndef BLE_RECONNECT_BACKOFF_MS
#define BLE_RECONNECT_BACKOFF_MS  100
#endif

/* ------------------------------------------------------------------ */
/* Internal state                                                       */
/* ------------------------------------------------------------------ */

static ble_state_t current_state   = BLE_STATE_OFF;
static bool        ble_initialized = false;
static ble_data_received_callback_t data_callback = NULL;
static ble_connection_callback_t    conn_callback  = NULL;

/* Advertisement payload (max 31 bytes for legacy BLE adv) */
static uint8_t adv_data[31];
static uint8_t adv_data_len = 0;

/* Scan response payload (max 31 bytes) */
static uint8_t scan_rsp_data[31];
static uint8_t scan_rsp_data_len = 0;

/* Saved advertising parameters — applied from HCI_STATE_WORKING (canonical BTstack pattern) */
static uint16_t adv_discriminator = 0;
static uint16_t adv_vendor_id     = 0;
static uint16_t adv_product_id    = 0;
static bool     adv_configured    = false;

/*
 * CHIPoBLE characteristic 128-bit UUIDs per Matter Core Spec §4.12.
 * UUID string form is represented big-endian (MSB first), which is how
 * BTstack's att_db_util_add_characteristic_uuid128() expects them.
 *
 * C1 (controller → device, Write Without Response):
 *   18EE2EF5-263D-4559-959F-4F9C429F9D11
 * C2 (device → controller, Notify + CCCD):
 *   18EE2EF5-263D-4559-959F-4F9C429F9D12
 */
static const uint8_t chip_c1_uuid128[16] = {
    0x18, 0xEE, 0x2E, 0xF5, 0x26, 0x3D, 0x45, 0x59,
    0x95, 0x9F, 0x4F, 0x9C, 0x42, 0x9F, 0x9D, 0x11
};
static const uint8_t chip_c2_uuid128[16] = {
    0x18, 0xEE, 0x2E, 0xF5, 0x26, 0x3D, 0x45, 0x59,
    0x95, 0x9F, 0x4F, 0x9C, 0x42, 0x9F, 0x9D, 0x12
};

/* GATT attribute handles for CHIPoBLE characteristics */
static uint16_t char_tx_handle      = 0;   /* C1 value handle (Write WR, controller→device) */
static uint16_t char_rx_handle      = 0;   /* C2 value handle (Notify, device→controller)   */
static uint16_t char_rx_cccd_handle = 0;   /* C2 CCCD handle  (= char_rx_handle + 1)        */

/*
 * C2 Client Characteristic Configuration Descriptor (CCCD) value.
 *
 * BTstack marks the auto-generated CCCD as ATT_PROPERTY_DYNAMIC, meaning
 * all reads/writes go through att_read_callback / att_write_callback.  We
 * must therefore maintain the CCCD state ourselves so that:
 *   (a) att_read_callback returns the correct 2-byte value when iOS reads it
 *   (b) att_server_indicate() / att_server_notify() can verify iOS subscribed
 *       (BTstack reads the CCCD via att_read_callback before sending)
 * Value 0x0000 = disabled, 0x0001 = notifications, 0x0002 = indications.
 */
static uint16_t char_rx_cccd_value  = 0;

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
/* Indication-based TX state (COBLe fragmented sending via indicate)   */
/* ------------------------------------------------------------------ */

static uint8_t  coble_tx_msg[COBLE_MAX_MSG_SIZE];
static size_t   coble_tx_msg_len         = 0;
static size_t   coble_tx_msg_sent        = 0;
static bool     coble_tx_first_frag      = false;
static bool     coble_tx_active          = false;

/* Forward declaration for indication-driven fragment sender */
static void coble_tx_send_next(void);

/* ------------------------------------------------------------------ */
/* ATT callbacks                                                        */
/* ------------------------------------------------------------------ */

/*
 * att_read_callback – return CCCD value for C2; return 0 bytes for everything
 * else (no other readable dynamic characteristics).
 *
 * BTstack calls this for DYNAMIC attributes when:
 *   • A central reads an attribute directly (ATT Read Request)
 *   • att_server_indicate() / att_server_notify() checks the CCCD to decide
 *     whether the client has subscribed before sending
 */
static uint16_t att_read_callback(hci_con_handle_t connection_handle,
                                   uint16_t att_handle,
                                   uint16_t offset,
                                   uint8_t *buffer,
                                   uint16_t buffer_size) {
    (void)connection_handle;
    (void)offset;

    if (att_handle == char_rx_cccd_handle) {
        /* Return the 2-byte CCCD value (little-endian). */
        if (buffer && buffer_size >= 2) {
            buffer[0] = (uint8_t)(char_rx_cccd_value & 0xFF);
            buffer[1] = (uint8_t)(char_rx_cccd_value >> 8);
        }
        return 2;
    }

    (void)buffer;
    (void)buffer_size;
    return 0;
}

/*
 * att_write_callback – receives COBLe/BTP frames written to C1 and CCCD writes
 * to C2.
 *
 * Normal COBLe data frame format per Matter Core Spec §3.6.1:
 *   byte 0      : flags (SYN=0x01, FIN=0x02, ACK=0x04)
 *   if SYN:
 *     byte 1    : message counter
 *     bytes 2-3 : total message length (little-endian uint16)
 *   if ACK:
 *     next byte : ack counter
 *   remaining   : payload bytes
 *
 * BTP management frames (handshake) have SYN|FIN|MGMT (0x0B) and a
 * completely different payload layout – they must NOT be parsed as data.
 */

/* Forward declaration for BTP handshake handler (defined below) */
static void handle_btp_hello_request(const uint8_t *buf, uint16_t len);

static int att_write_callback(hci_con_handle_t connection_handle,
                               uint16_t att_handle,
                               uint16_t transaction_mode,
                               uint16_t offset,
                               uint8_t *buffer,
                               uint16_t buffer_size) {
    (void)connection_handle;
    (void)transaction_mode;
    (void)offset;

    /* ---- C2 CCCD: store the subscription value ---- */
    if (att_handle == char_rx_cccd_handle) {
        if (buffer_size >= 2) {
            char_rx_cccd_value = (uint16_t)(buffer[0] |
                                  ((uint16_t)buffer[1] << 8));
            printf("BLE: C2 CCCD = 0x%04X%s\n",
                   (unsigned)char_rx_cccd_value,
                   char_rx_cccd_value == 0x0002 ? " (indications enabled)" :
                   char_rx_cccd_value == 0x0001 ? " (notifications enabled)" :
                                                  " (disabled)");
        }
        return 0;
    }

    /* All other writes must target C1 */
    if (att_handle != char_tx_handle || buffer_size < 1) {
        return 0;
    }

    uint8_t  flags         = buffer[0];

    /* ---- BTP management frame (handshake) ---- */
    if ((flags & COBLE_FLAG_MGMT) &&
        (flags & COBLE_FLAG_SYN) &&
        (flags & COBLE_FLAG_FIN)) {
        /* BTP Session Init Request (Hello) – respond with Hello Response */
        handle_btp_hello_request(buffer, buffer_size);
        return 0;
    }

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
/* Forward declarations                                                 */
/* ------------------------------------------------------------------ */
static void build_matter_adv_data(uint16_t discriminator,
                                   uint16_t vendor_id,
                                   uint16_t product_id);
static void build_scan_response(void);

/* ------------------------------------------------------------------ */
/* BTP Hello (Session Init) handshake handler                          */
/* ------------------------------------------------------------------ */

/*
 * handle_btp_hello_request – process a BTP Session Init Request written to
 * C1 by the central (iOS) and immediately send back a Session Init Response
 * on C2 via ATT indication.
 *
 * The BTP Hello frame format (both request and response):
 *   byte 0   : Header Flags = SYN|FIN|MGMT (0x0B)
 *   byte 1   : Supported BTP versions bitmap  (request) /
 *              Selected BTP version           (response)
 *   byte 2   : Client receive window size     (request) /
 *              Peripheral receive window size (response)
 *   bytes 3-4: Client ATT MTU (LE)            (request) /
 *              Server ATT MTU (LE)            (response)
 *
 * Reference: Matter Core Spec §3.6.3 (BTP Session Establishment).
 */
static void handle_btp_hello_request(const uint8_t *buf, uint16_t len) {
    if (len < 5) {
        printf("BLE BTP: Hello too short (%u bytes)\n", (unsigned)len);
        return;
    }

    uint8_t  supported_versions = buf[1];
    uint8_t  client_window      = buf[2];
    uint16_t client_mtu         = (uint16_t)(buf[3] | ((uint16_t)buf[4] << 8));

    printf("BLE BTP: Hello REQ "
           "(versions=0x%02X window=%u mtu=%u)\n",
           (unsigned)supported_versions, (unsigned)client_window,
           (unsigned)client_mtu);

    /*
     * We always respond with BTP_VERSION (4) and BTP_WINDOW_SIZE (6) regardless
     * of the values the central advertised.  In a full BTP implementation the
     * peripheral would pick the highest mutually supported version; for our
     * single-fabric, single-connection commissioning flow the fixed values work
     * with every current Matter controller.
     * The client_window is not directly used here — the peripheral's own window
     * (BTP_WINDOW_SIZE) governs how many unacknowledged segments we may receive
     * before the central must wait for an ACK.
     */

    /* Negotiate ATT MTU: use the value already negotiated at the HCI level */
    uint16_t server_mtu = (active_con_handle != HCI_CON_HANDLE_INVALID)
                          ? att_server_get_mtu(active_con_handle)
                          : 23u;

    /* Build BTP Hello Response */
    uint8_t resp[5];
    resp[0] = COBLE_FLAG_SYN | COBLE_FLAG_FIN | COBLE_FLAG_MGMT;  /* 0x0B */
    resp[1] = BTP_VERSION;       /* selected BTP version                  */
    resp[2] = BTP_WINDOW_SIZE;   /* peripheral receive window             */
    resp[3] = (uint8_t)(server_mtu & 0xFF);
    resp[4] = (uint8_t)(server_mtu >> 8);

    printf("BLE BTP: Sending Hello RESP "
           "(version=%u window=%u mtu=%u)\n",
           (unsigned)BTP_VERSION, (unsigned)BTP_WINDOW_SIZE,
           (unsigned)server_mtu);

    /* Send via C2 indication; fall back to notification if needed.
     * Matter Core Spec §4.12.3.3 requires C2 to support Indicate; the
     * BTP Hello response is a small, single-packet message so both
     * delivery methods are functionally equivalent here – the important
     * thing is that the central receives the response. */
    uint8_t err = att_server_indicate(active_con_handle, char_rx_handle,
                                      resp, sizeof(resp));
    if (err != ERROR_CODE_SUCCESS) {
        err = att_server_notify(active_con_handle, char_rx_handle,
                                resp, sizeof(resp));
    }
    if (err != ERROR_CODE_SUCCESS) {
        printf("BLE BTP: Failed to send Hello RESP (err=0x%02X)\n",
               (unsigned)err);
    }
}

/* ------------------------------------------------------------------ */
/* HCI state handler (BTSTACK_EVENT_STATE only)                        */
/* ------------------------------------------------------------------ */

/*
 * hci_state_handler – registered via hci_add_event_handler.
 *
 * Handles ONLY BTSTACK_EVENT_STATE so that the advertisement setup runs
 * at the right moment (HCI_STATE_WORKING).  All other BLE events
 * (connection, disconnection, ATT) are handled by packet_handler which
 * is registered via att_server_register_packet_handler; registering the
 * same function with both would cause every HCI event to be processed
 * twice, producing the "Duplicate" event noise seen in the original log.
 */
static btstack_packet_callback_registration_t hci_state_cb_reg;

static void hci_state_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }
    if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) {
        return;
    }

    uint8_t state = btstack_event_state_get_state(packet);
    if (state == HCI_STATE_WORKING) {
        printf("BLE: HCI controller ready\n");
        /*
         * Canonical BTstack pattern: set advertisement data, scan
         * response, and parameters HERE (after HCI is working), then
         * enable.  Calling these before hci_power_control() risks the
         * controller discarding the data during its reset sequence.
         * See pico-examples/pico_w/bt/standalone/server/server.c.
         */
        if (adv_configured) {
            build_matter_adv_data(adv_discriminator, adv_vendor_id,
                                  adv_product_id);
            gap_advertisements_set_data(adv_data_len, adv_data);

            build_scan_response();
            gap_scan_response_set_data(scan_rsp_data_len, scan_rsp_data);

            bd_addr_t unused_peer_addr = {0, 0, 0, 0, 0, 0};
            gap_advertisements_set_params(
                0x0020,           /* adv_int_min: 32 × 0.625ms = 20ms */
                0x0060,           /* adv_int_max: 96 × 0.625ms = 60ms */
                0,                /* ADV_IND: connectable undirected */
                0,                /* own address type: public */
                unused_peer_addr,
                0x07,             /* all three advertising channels */
                0                 /* no filter policy */
            );

            gap_advertisements_enable(1);
            current_state = BLE_STATE_ADVERTISING;
            printf("BLE: Matter advertisements enabled "
                   "(discriminator=0x%03X)\n",
                   (unsigned)adv_discriminator);
        }
    } else if (state == HCI_STATE_OFF) {
        current_state     = BLE_STATE_OFF;
        active_con_handle = HCI_CON_HANDLE_INVALID;
    }
}

/* ------------------------------------------------------------------ */
/* BLE connection + ATT packet handler                                  */
/* ------------------------------------------------------------------ */

/*
 * packet_handler – registered via att_server_register_packet_handler.
 *
 * The att_server forwards HCI_EVENT_LE_META (connection) and
 * HCI_EVENT_DISCONNECTION_COMPLETE to this handler in addition to
 * ATT_EVENT_* events.  Because we no longer also register with
 * hci_add_event_handler for these events, each event is processed
 * exactly once – eliminating the duplicate-event dedup code that
 * PR #90 had to add.
 */
static void packet_handler(uint8_t packet_type, uint16_t channel,
                            uint8_t *packet, uint16_t size) {
    (void)channel;
    (void)size;

    if (packet_type != HCI_EVENT_PACKET) {
        return;
    }

    switch (hci_event_packet_get_type(packet)) {

        case HCI_EVENT_DISCONNECTION_COMPLETE: {
            uint8_t reason = hci_event_disconnection_complete_get_reason(packet);
            printf("BLE: Client disconnected (reason=0x%02X)\n",
                   (unsigned)reason);
            active_con_handle    = HCI_CON_HANDLE_INVALID;
            current_state        = BLE_STATE_ADVERTISING;
            /* Reset COBLe/BTP state for next connection */
            coble_rx_in_progress = false;
            coble_rx_ready       = false;
            coble_rx_offset      = 0;
            coble_tx_active      = false;
            char_rx_cccd_value   = 0;   /* reset subscription for next session */
            /* Notify higher layers */
            if (conn_callback) {
                conn_callback(false);
            }
            /* Small backoff before restarting advertising */
#if BLE_RECONNECT_BACKOFF_MS > 0
            sleep_ms(BLE_RECONNECT_BACKOFF_MS);
#endif
            /* Explicitly re-enable advertising so iOS can reconnect and
             * retry commissioning. */
            if (adv_configured) {
                gap_advertisements_enable(1);
                printf("BLE: Advertising restarted after disconnect\n");
            }
            break;
        }

        case HCI_EVENT_LE_META: {
            if (hci_event_le_meta_get_subevent_code(packet) ==
                    HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                active_con_handle =
                    hci_subevent_le_connection_complete_get_connection_handle(
                        packet);
                printf("BLE: Client connected (handle=0x%04X)\n",
                       (unsigned)active_con_handle);
                current_state = BLE_STATE_CONNECTED;
                if (conn_callback) {
                    conn_callback(true);
                }
            }
            break;
        }

        case ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE:
            /* Previous indication was acknowledged — send next fragment */
            if (coble_tx_active && coble_tx_msg_sent < coble_tx_msg_len) {
                coble_tx_send_next();
            } else {
                coble_tx_active = false;
            }
            break;

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
 * Format per Matter Core Spec §5.4.2.5.2 (stable across v1.x):
 *   AD 0x01 Flags                               (3 bytes)
 *   AD 0x03 Complete 16-bit UUID list: 0xFFF6   (4 bytes)
 *   AD 0x16 Service Data UUID 0xFFF6           (13 bytes)
 *
 * The local name is placed in the scan response (built separately) so that
 * the advertisement stays minimal and iOS active-scan receives the device
 * name in the scan response PDU.
 *
 * Service-data payload (8 bytes, ChipBLEDeviceIdentificationInfo):
 *   byte 0: OpCode  (0x01 = Device Identification Info)
 *   byte 1: Discriminator[7:0]
 *   byte 2: Discriminator[11:8] (low nibble) | AdvVersion (high nibble)
 *   bytes 3-4: Vendor ID  (little-endian)
 *   bytes 5-6: Product ID (little-endian)
 *   byte 7: AdditionalDataFlag (0x00 = no additional data)
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
    *p++ = 0x0B;        /* length: 11 bytes (type + UUID + 8 payload bytes) */
    *p++ = 0x16;        /* AD type: Service Data – 16-bit UUID */
    *p++ = 0xF6;        /* UUID low byte  */
    *p++ = 0xFF;        /* UUID high byte */
    /* Service-data payload (8 bytes, Matter Core Spec §5.4.2.5.2,
     * matching ChipBLEDeviceIdentificationInfo in connectedhomeip):
     *   byte 0: OpCode (0x01 = Device Identification Info)
     *   byte 1: Discriminator[7:0]
     *   byte 2: Discriminator[11:8] (low nibble) | AdvVersion (high nibble)
     *   bytes 3-4: Vendor ID (little-endian)
     *   bytes 5-6: Product ID (little-endian)
     *   byte 7: AdditionalDataFlag (0x00 = no additional data)
     */
    *p++ = 0x01;                                         /* OpCode: DeviceIdentificationInfo */
    *p++ = (uint8_t)(discriminator & 0xFF);              /* Discriminator[7:0]               */
    *p++ = (uint8_t)((discriminator >> 8) & 0x0F);      /* Discriminator[11:8], 4 bits      */
    *p++ = (uint8_t)(vendor_id  & 0xFF);
    *p++ = (uint8_t)(vendor_id  >> 8);
    *p++ = (uint8_t)(product_id & 0xFF);
    *p++ = (uint8_t)(product_id >> 8);
    *p++ = 0x00;                                         /* AdditionalDataFlag: none         */

    adv_data_len = (uint8_t)(p - adv_data);
}

/*
 * Build the scan response payload.
 *
 * iOS (and other active-scanning BLE hosts) sends a Scan Request after
 * seeing the advertisement.  Including the Complete Local Name here lets
 * iOS display a friendly device name and — critically — ensures the device
 * is surfaced in the Matter commissioning UI even on stricter iOS builds.
 */
static void build_scan_response(void) {
    uint8_t *p = scan_rsp_data;

    /* --- Complete Local Name: "Matter" --- */
    const char *name     = "Matter";
    uint8_t     name_len = (uint8_t)(sizeof("Matter") - 1);  /* 6 */
    *p++ = name_len + 1;             /* length: 1 (type) + 6 (chars) */
    *p++ = 0x09;                     /* AD type: Complete Local Name  */
    for (uint8_t i = 0; i < name_len; i++) {
        *p++ = (uint8_t)name[i];
    }

    scan_rsp_data_len = (uint8_t)(p - scan_rsp_data);
}

/* ------------------------------------------------------------------ */
/* Indication-based fragment sender (C++ linkage, used by packet_handler) */
/* ------------------------------------------------------------------ */

/*
 * coble_tx_send_next – build and send the next COBLe fragment via
 * ATT indication.  Called from ble_adapter_send_data() for the first
 * fragment, and from the ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE
 * handler for subsequent fragments.
 */
static void coble_tx_send_next(void) {
    if (!coble_tx_active || active_con_handle == HCI_CON_HANDLE_INVALID) {
        coble_tx_active = false;
        return;
    }

    uint16_t att_mtu = att_server_get_mtu(active_con_handle);
    if (att_mtu < 23u) {
        att_mtu = 23u;
    }
    const size_t MAX_ATT_DATA    = (size_t)(att_mtu - 3u);
    const size_t MAX_DATA_FIRST  = MAX_ATT_DATA - 4u; /* SYN hdr: flags+ctr+len16 */
    const size_t MAX_DATA_REST   = MAX_ATT_DATA - 1u; /* flags only              */

    uint8_t fragment[COBLE_MAX_FRAGMENT_SIZE];
    size_t  frag_hdr = 0;
    size_t  data_capacity;

    if (coble_tx_first_frag) {
        uint8_t flags = COBLE_FLAG_SYN;
        if (coble_tx_msg_sent + MAX_DATA_FIRST >= coble_tx_msg_len) {
            flags |= COBLE_FLAG_FIN;
        }
        fragment[frag_hdr++] = flags;
        fragment[frag_hdr++] = coble_tx_counter++;
        fragment[frag_hdr++] = (uint8_t)(coble_tx_msg_len & 0xFF);
        fragment[frag_hdr++] = (uint8_t)((coble_tx_msg_len >> 8) & 0xFF);
        data_capacity = MAX_DATA_FIRST;
        coble_tx_first_frag = false;
    } else {
        bool is_last = (coble_tx_msg_sent + MAX_DATA_REST >= coble_tx_msg_len);
        fragment[frag_hdr++] = is_last ? COBLE_FLAG_FIN : 0x00u;
        data_capacity = MAX_DATA_REST;
    }

    size_t chunk = coble_tx_msg_len - coble_tx_msg_sent;
    if (chunk > data_capacity) {
        chunk = data_capacity;
    }
    memcpy(fragment + frag_hdr, coble_tx_msg + coble_tx_msg_sent, chunk);
    coble_tx_msg_sent += chunk;

    /* Try indication first (required by Matter spec §4.12.3.3),
     * fall back to notification if the controller has not subscribed
     * for indications. */
    uint8_t err = att_server_indicate(active_con_handle, char_rx_handle,
                                       fragment, (uint16_t)(frag_hdr + chunk));
    if (err != ERROR_CODE_SUCCESS) {
        /* Fallback: try notification */
        err = att_server_notify(active_con_handle, char_rx_handle,
                                fragment, (uint16_t)(frag_hdr + chunk));
        if (err != ERROR_CODE_SUCCESS) {
            printf("BLE: Send failed (err=0x%02X, sent=%zu/%zu)\n",
                   (unsigned)err, coble_tx_msg_sent - chunk, coble_tx_msg_len);
            coble_tx_active = false;
            return;
        }
        /* Notifications don't wait for confirmation — continue immediately */
        if (coble_tx_msg_sent < coble_tx_msg_len) {
            coble_tx_send_next();
        } else {
            coble_tx_active = false;
        }
        return;
    }

    /* Indication queued; next fragment will be sent from the
     * ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE handler.
     * That handler also clears coble_tx_active after the last fragment. */
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

    /*
     * Mandatory BTstack BLE peripheral initialisation steps.
     * These MUST be called before att_server_init() (and before
     * hci_power_control) – see pico-examples/pico_w/bt/standalone/server.c
     *
     *  l2cap_init() – initialises the L2CAP layer that carries all BLE data
     *  sm_init()    – initialises the Security Manager (required for
     *                 connectable advertisements even when no pairing is used)
     *  sm_set_io_capabilities() – tells SM that the device has no display /
     *                 keyboard; prevents SM from blocking connections by
     *                 demanding a pairing confirmation it can never receive
     */
    l2cap_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    /*
     * Register hci_state_handler ONLY for BTSTACK_EVENT_STATE so that the
     * advertising setup (gap_advertisements_*) runs exactly once when the HCI
     * controller is ready.  BLE connection/disconnection events are handled by
     * packet_handler which is registered via att_server_register_packet_handler
     * below.  Registering the same callback with both hci_add_event_handler AND
     * att_server_register_packet_handler would cause every HCI connection /
     * disconnection event to be delivered twice, producing spurious "Duplicate"
     * log lines and potential state-machine glitches.
     */
    hci_state_cb_reg.callback = &hci_state_handler;
    hci_add_event_handler(&hci_state_cb_reg);

    /* -------------------------------------------------------------- */
    /* GATT database setup                                             */
    /* -------------------------------------------------------------- */
    att_db_util_init();

    /* 1. GAP service (0x1800): Device Name + Appearance.
     *    Many BLE stacks and Matter controllers expect these to be present
     *    before accepting an ATT connection to a peripheral.
     */
    att_db_util_add_service_uuid16(0x1800);
    {
        static const uint8_t device_name[] = "Matter";
        att_db_util_add_characteristic_uuid16(
            0x2A00,                /* Device Name */
            ATT_PROPERTY_READ,
            ATT_SECURITY_NONE, ATT_SECURITY_NONE,
            (uint8_t *)device_name, sizeof(device_name) - 1);
    }
    {
        static const uint8_t appearance[] = {0x00, 0x00}; /* unknown */
        att_db_util_add_characteristic_uuid16(
            0x2A01,                /* Appearance */
            ATT_PROPERTY_READ,
            ATT_SECURITY_NONE, ATT_SECURITY_NONE,
            (uint8_t *)appearance, sizeof(appearance));
    }

    /* 2. GATT service (0x1801): no mandatory characteristics for a peripheral. */
    att_db_util_add_service_uuid16(0x1801);

    /* 3. CHIPoBLE service (0xFFF6 = 0000FFF6-0000-1000-8000-00805F9B34FB).
     *    Use the 16-bit UUID form — equivalent for Bluetooth SIG base UUIDs.
     */
    att_db_util_add_service_uuid16(0xFFF6);

    /*
     * C1 characteristic (18EE2EF5-263D-4559-959F-4F9C429F9D11):
     * Controller writes Matter messages to this characteristic
     * (Write Without Response).
     * Per Matter Core Spec §4.12.3.2 — MUST use the 128-bit UUID; iOS
     * and other controllers will NOT find this characteristic if a
     * 16-bit shorthand (0xFFF7) is used instead.
     */
    char_tx_handle = att_db_util_add_characteristic_uuid128(
        chip_c1_uuid128,
        ATT_PROPERTY_WRITE_WITHOUT_RESPONSE | ATT_PROPERTY_DYNAMIC,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        NULL, 0);

    /*
     * C2 characteristic (18EE2EF5-263D-4559-959F-4F9C429F9D12):
     * Device sends Matter responses to the controller via indications.
     * Per Matter Core Spec §4.12.3.3 — MUST use the 128-bit UUID and
     * MUST support the Indicate property.  Notify is also included for
     * compatibility with controllers that prefer notifications.
     * A CCCD is automatically created by BTstack when ATT_PROPERTY_NOTIFY
     * or ATT_PROPERTY_INDICATE is specified.
     */
    char_rx_handle = att_db_util_add_characteristic_uuid128(
        chip_c2_uuid128,
        ATT_PROPERTY_INDICATE | ATT_PROPERTY_NOTIFY | ATT_PROPERTY_DYNAMIC,
        ATT_SECURITY_NONE, ATT_SECURITY_NONE,
        NULL, 0);
    /*
     * The CCCD for C2 is created by att_db_util immediately after the value
     * attribute (handle N+1).  We record its handle so that att_read_callback
     * and att_write_callback can maintain the subscription state that BTstack
     * reads before allowing att_server_indicate() / att_server_notify().
     */
    char_rx_cccd_handle = char_rx_handle + 1;

    /* Start the ATT server with the database we just built */
    att_server_init(att_db_util_get_address(),
                    att_read_callback,
                    att_write_callback);

    /* Register the same packet handler for ATT events (MTU updates, etc.) */
    att_server_register_packet_handler(packet_handler);

    printf("BLE: CHIPoBLE GATT service registered "
           "(TX handle=0x%04X, RX handle=0x%04X, CCCD handle=0x%04X)\n",
           (unsigned)char_tx_handle, (unsigned)char_rx_handle,
           (unsigned)char_rx_cccd_handle);

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

    printf("BLE: Preparing Matter advertisement "
           "(discriminator=0x%03X VID=0x%04X PID=0x%04X)\n",
           discriminator, vendor_id, product_id);

    /*
     * Save parameters; the actual gap_advertisements_set_data(),
     * gap_scan_response_set_data(), gap_advertisements_set_params(), and
     * gap_advertisements_enable(1) calls are deferred to the
     * HCI_STATE_WORKING event handler.  This is the canonical BTstack
     * pattern: applying advertisement data before the controller is ready
     * can cause the data to be silently discarded during HCI reset.
     */
    adv_discriminator = discriminator;
    adv_vendor_id     = vendor_id;
    adv_product_id    = product_id;
    adv_configured    = true;

    hci_power_control(HCI_POWER_ON);
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
 * The message is buffered and sent as one or more ATT indications.
 * For multi-fragment messages, each subsequent fragment is sent from
 * the ATT_EVENT_HANDLE_VALUE_INDICATION_COMPLETE callback.  If
 * indications are not available (controller subscribed for notifications
 * only), falls back to ATT notifications.
 */
int ble_adapter_send_data(const uint8_t *data, size_t length) {
    if (!ble_initialized || active_con_handle == HCI_CON_HANDLE_INVALID ||
        char_rx_handle == 0 || length == 0 || length > COBLE_MAX_MSG_SIZE) {
        return -1;
    }

    /* Buffer the complete message */
    memcpy(coble_tx_msg, data, length);
    coble_tx_msg_len    = length;
    coble_tx_msg_sent   = 0;
    coble_tx_first_frag = true;
    coble_tx_active     = true;

    /* Send the first fragment */
    coble_tx_send_next();
    return coble_tx_active ? 0 : -1;
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
