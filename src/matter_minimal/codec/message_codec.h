/*
 * message_codec.h
 * Matter message encoding/decoding per Matter Core Specification Section 4.7
 * 
 * Phase 2 Implementation Note:
 * This is a simplified implementation for unsecured messages. Protocol metadata
 * (protocol_id, opcode, exchange_id) is stored in the message structure but NOT
 * encoded in the wire format. In a full Matter implementation, this data would be
 * part of the secured payload. For Phase 2, we encode only:
 *   - Message header (flags, session_id, security_flags, message_counter, node IDs)
 *   - Application payload (TLV-encoded)
 * 
 * Phase 3 will add security (PASE/CASE) and proper protocol header encoding.
 */

#ifndef MESSAGE_CODEC_H
#define MESSAGE_CODEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Matter Message Header Flags (1 byte)
 * Based on Matter Core Specification Section 4.7
 */
#define MATTER_MSG_FLAG_VERSION_MASK    0x0F  // Bits 0-3: Message version
#define MATTER_MSG_FLAG_VERSION_SHIFT   0
#define MATTER_MSG_FLAG_S               0x10  // Bit 4: Source Node ID present
#define MATTER_MSG_FLAG_DSIZ_MASK       0x60  // Bits 5-6: Destination Node ID size
#define MATTER_MSG_FLAG_DSIZ_SHIFT      5
#define MATTER_MSG_FLAG_DSIZ_NONE       0x00  // No destination ID
#define MATTER_MSG_FLAG_DSIZ_8B         0x20  // 8-byte destination ID
#define MATTER_MSG_FLAG_DSIZ_16B        0x40  // 16-byte destination ID (reserved)
#define MATTER_MSG_FLAG_DSIZ_64B        0x60  // 64-bit destination ID

/**
 * Matter Message Version
 */
#define MATTER_MSG_VERSION              0x00  // Version 0 (current)

/**
 * Matter Message Header Structure
 * Minimum 8 bytes, up to 24 bytes with optional node IDs
 */
typedef struct {
    uint8_t flags;                  // Message flags (version, node ID flags)
    uint16_t session_id;            // Session ID (0 = unsecured)
    uint8_t security_flags;         // Security flags (reserved for Phase 3)
    uint32_t message_counter;       // Message counter for replay protection
    uint64_t source_node_id;        // Optional source node ID (0 if not present)
    uint64_t dest_node_id;          // Optional destination node ID (0 if not present)
} matter_message_header_t;

/**
 * Matter Message Structure
 * Complete message with header and payload
 */
typedef struct {
    matter_message_header_t header; // Message header
    uint16_t protocol_id;           // Protocol ID (e.g., 0x0001 for InteractionModel)
    uint8_t protocol_opcode;        // Protocol-specific opcode
    uint16_t exchange_id;           // Exchange ID for request/response matching
    const uint8_t *payload;         // Pointer to payload data (TLV-encoded)
    size_t payload_length;          // Length of payload in bytes
} matter_message_t;

/**
 * Matter Protocol IDs
 */
#define MATTER_PROTOCOL_SECURE_CHANNEL      0x0000  // Secure channel protocol
#define MATTER_PROTOCOL_INTERACTION_MODEL   0x0001  // Interaction model protocol
#define MATTER_PROTOCOL_BDX                 0x0002  // Bulk data exchange
#define MATTER_PROTOCOL_USER_DIRECTED       0x0003  // User directed commissioning

/**
 * Secure Channel Protocol Opcodes (for Phase 3)
 */
#define MATTER_SC_OPCODE_MSG_COUNTER_SYNC_REQ   0x00
#define MATTER_SC_OPCODE_MSG_COUNTER_SYNC_RSP   0x01
#define MATTER_SC_OPCODE_MRP_STANDALONE_ACK     0x10
#define MATTER_SC_OPCODE_PBKDF_PARAM_REQUEST    0x20
#define MATTER_SC_OPCODE_PBKDF_PARAM_RESPONSE   0x21
#define MATTER_SC_OPCODE_PASE_PAKE1             0x22
#define MATTER_SC_OPCODE_PASE_PAKE2             0x23
#define MATTER_SC_OPCODE_PASE_PAKE3             0x24

/**
 * Interaction Model Protocol Opcodes
 */
#define MATTER_IM_OPCODE_STATUS_RESPONSE        0x01
#define MATTER_IM_OPCODE_READ_REQUEST           0x02
#define MATTER_IM_OPCODE_SUBSCRIBE_REQUEST      0x03
#define MATTER_IM_OPCODE_SUBSCRIBE_RESPONSE     0x04
#define MATTER_IM_OPCODE_REPORT_DATA            0x05
#define MATTER_IM_OPCODE_WRITE_REQUEST          0x06
#define MATTER_IM_OPCODE_WRITE_RESPONSE         0x07
#define MATTER_IM_OPCODE_INVOKE_REQUEST         0x08
#define MATTER_IM_OPCODE_INVOKE_RESPONSE        0x09
#define MATTER_IM_OPCODE_TIMED_REQUEST          0x0A

/**
 * Maximum Message Sizes
 */
#define MATTER_MAX_MESSAGE_SIZE         1280    // Maximum UDP payload (IPv6 MTU)
#define MATTER_MIN_HEADER_SIZE          8       // Minimum header without node IDs
#define MATTER_MAX_HEADER_SIZE          24      // Maximum header with both node IDs
#define MATTER_MAX_PAYLOAD_SIZE         (MATTER_MAX_MESSAGE_SIZE - MATTER_MAX_HEADER_SIZE)

/**
 * Error Codes
 */
#define MATTER_MSG_SUCCESS              0
#define MATTER_MSG_ERROR_BUFFER_TOO_SMALL   -1
#define MATTER_MSG_ERROR_INVALID_INPUT      -2
#define MATTER_MSG_ERROR_INVALID_VERSION    -3
#define MATTER_MSG_ERROR_INVALID_FLAGS      -4
#define MATTER_MSG_ERROR_BUFFER_UNDERFLOW   -5

/**
 * Initialize message codec
 * Sets up message counter and exchange ID tracking
 */
void matter_message_codec_init(void);

/**
 * Encode a Matter message into a buffer
 * 
 * @param msg Message structure to encode
 * @param buffer Output buffer for encoded message
 * @param buffer_size Size of output buffer
 * @param encoded_length Pointer to store actual encoded length
 * @return MATTER_MSG_SUCCESS on success, error code on failure
 */
int matter_message_encode(const matter_message_t *msg, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_length);

/**
 * Decode a Matter message from a buffer
 * 
 * @param buffer Input buffer containing encoded message
 * @param buffer_size Size of input buffer
 * @param msg Message structure to populate (payload will point into buffer)
 * @return MATTER_MSG_SUCCESS on success, error code on failure
 */
int matter_message_decode(const uint8_t *buffer, size_t buffer_size, 
                          matter_message_t *msg);

/**
 * Get next message counter value
 * Message counters are used for replay protection
 * 
 * @return Next message counter value
 */
uint32_t matter_message_get_next_counter(void);

/**
 * Get next exchange ID
 * Exchange IDs are used to match requests and responses
 * 
 * @return Next exchange ID value
 */
uint16_t matter_message_get_next_exchange_id(void);

/**
 * Validate message counter for replay protection
 * For unsecured messages, this is a simple incrementing check
 * 
 * @param session_id Session ID (0 for unsecured)
 * @param counter Message counter value
 * @return true if counter is valid, false if replayed
 */
bool matter_message_validate_counter(uint16_t session_id, uint32_t counter);

#ifdef __cplusplus
}
#endif

#endif // MESSAGE_CODEC_H
