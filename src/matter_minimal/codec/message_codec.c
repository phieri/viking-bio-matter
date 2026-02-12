/*
 * message_codec.c
 * Matter message encoding/decoding implementation
 * Based on Matter Core Specification Section 4.7
 */

#include "message_codec.h"
#include <string.h>

// Message counter and exchange ID tracking
static uint32_t current_message_counter = 0;
static uint16_t current_exchange_id = 0;

// Replay protection tracking (simple implementation for unsecured messages)
#define MAX_SESSIONS 8
static struct {
    uint16_t session_id;
    uint32_t last_counter;
    bool active;
} session_counters[MAX_SESSIONS];

void matter_message_codec_init(void) {
    current_message_counter = 0;
    current_exchange_id = 0;
    
    // Initialize session counter tracking
    for (int i = 0; i < MAX_SESSIONS; i++) {
        session_counters[i].session_id = 0;
        session_counters[i].last_counter = 0;
        session_counters[i].active = false;
    }
}

uint32_t matter_message_get_next_counter(void) {
    return current_message_counter++;
}

uint16_t matter_message_get_next_exchange_id(void) {
    return current_exchange_id++;
}

bool matter_message_validate_counter(uint16_t session_id, uint32_t counter) {
    // For unsecured messages (session_id = 0), accept any counter
    // This is simplified - production would need proper window checking
    if (session_id == 0) {
        return true;
    }
    
    // Find session
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (session_counters[i].active && session_counters[i].session_id == session_id) {
            // Check if counter is greater than last seen
            if (counter > session_counters[i].last_counter) {
                session_counters[i].last_counter = counter;
                return true;
            }
            return false; // Replay detected
        }
    }
    
    // New session - find free slot
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!session_counters[i].active) {
            session_counters[i].session_id = session_id;
            session_counters[i].last_counter = counter;
            session_counters[i].active = true;
            return true;
        }
    }
    
    // No free slots - accept anyway (simplified)
    return true;
}

/**
 * Write 16-bit value in little-endian format
 */
static inline void write_le16(uint8_t *buffer, uint16_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * Write 32-bit value in little-endian format
 */
static inline void write_le32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
    buffer[2] = (uint8_t)((value >> 16) & 0xFF);
    buffer[3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * Write 64-bit value in little-endian format
 */
static inline void write_le64(uint8_t *buffer, uint64_t value) {
    buffer[0] = (uint8_t)(value & 0xFF);
    buffer[1] = (uint8_t)((value >> 8) & 0xFF);
    buffer[2] = (uint8_t)((value >> 16) & 0xFF);
    buffer[3] = (uint8_t)((value >> 24) & 0xFF);
    buffer[4] = (uint8_t)((value >> 32) & 0xFF);
    buffer[5] = (uint8_t)((value >> 40) & 0xFF);
    buffer[6] = (uint8_t)((value >> 48) & 0xFF);
    buffer[7] = (uint8_t)((value >> 56) & 0xFF);
}

/**
 * Read 16-bit value in little-endian format
 */
static inline uint16_t read_le16(const uint8_t *buffer) {
    return ((uint16_t)buffer[0]) | ((uint16_t)buffer[1] << 8);
}

/**
 * Read 32-bit value in little-endian format
 */
static inline uint32_t read_le32(const uint8_t *buffer) {
    return ((uint32_t)buffer[0]) | 
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

/**
 * Read 64-bit value in little-endian format
 */
static inline uint64_t read_le64(const uint8_t *buffer) {
    return ((uint64_t)buffer[0]) | 
           ((uint64_t)buffer[1] << 8) |
           ((uint64_t)buffer[2] << 16) |
           ((uint64_t)buffer[3] << 24) |
           ((uint64_t)buffer[4] << 32) |
           ((uint64_t)buffer[5] << 40) |
           ((uint64_t)buffer[6] << 48) |
           ((uint64_t)buffer[7] << 56);
}

int matter_message_encode(const matter_message_t *msg, uint8_t *buffer, 
                          size_t buffer_size, size_t *encoded_length) {
    if (!msg || !buffer || !encoded_length) {
        return MATTER_MSG_ERROR_INVALID_INPUT;
    }
    
    // Calculate required buffer size
    size_t header_size = MATTER_MIN_HEADER_SIZE;
    if (msg->header.source_node_id != 0) {
        header_size += 8;
    }
    if (msg->header.dest_node_id != 0) {
        header_size += 8;
    }
    
    // Add protocol header (4 bytes) and payload
    size_t total_size = header_size + 4 + msg->payload_length;
    
    if (total_size > buffer_size) {
        return MATTER_MSG_ERROR_BUFFER_TOO_SMALL;
    }
    
    if (total_size > MATTER_MAX_MESSAGE_SIZE) {
        return MATTER_MSG_ERROR_BUFFER_TOO_SMALL;
    }
    
    size_t offset = 0;
    
    // Encode message flags
    uint8_t flags = (MATTER_MSG_VERSION << MATTER_MSG_FLAG_VERSION_SHIFT) & MATTER_MSG_FLAG_VERSION_MASK;
    
    // Set source node ID flag
    if (msg->header.source_node_id != 0) {
        flags |= MATTER_MSG_FLAG_S;
    }
    
    // Set destination node ID flags
    if (msg->header.dest_node_id != 0) {
        flags |= MATTER_MSG_FLAG_DSIZ_8B;
    }
    
    buffer[offset++] = flags;
    
    // Encode session ID (2 bytes, little-endian)
    write_le16(&buffer[offset], msg->header.session_id);
    offset += 2;
    
    // Encode security flags (1 byte) - reserved for Phase 3
    buffer[offset++] = msg->header.security_flags;
    
    // Encode message counter (4 bytes, little-endian)
    write_le32(&buffer[offset], msg->header.message_counter);
    offset += 4;
    
    // Encode source node ID if present (8 bytes, little-endian)
    if (msg->header.source_node_id != 0) {
        write_le64(&buffer[offset], msg->header.source_node_id);
        offset += 8;
    }
    
    // Encode destination node ID if present (8 bytes, little-endian)
    if (msg->header.dest_node_id != 0) {
        write_le64(&buffer[offset], msg->header.dest_node_id);
        offset += 8;
    }
    
    // For Phase 2, protocol info (protocol_id, opcode, exchange_id) is metadata
    // In a full implementation, this would be encoded in the secured payload
    // For now, we just copy the payload directly
    
    // Copy payload
    if (msg->payload_length > 0 && msg->payload != NULL) {
        memcpy(&buffer[offset], msg->payload, msg->payload_length);
        offset += msg->payload_length;
    }
    
    *encoded_length = offset;
    return MATTER_MSG_SUCCESS;
}

int matter_message_decode(const uint8_t *buffer, size_t buffer_size, 
                          matter_message_t *msg) {
    if (!buffer || !msg || buffer_size < MATTER_MIN_HEADER_SIZE) {
        return MATTER_MSG_ERROR_INVALID_INPUT;
    }
    
    size_t offset = 0;
    
    // Decode message flags
    uint8_t flags = buffer[offset++];
    uint8_t version = (flags & MATTER_MSG_FLAG_VERSION_MASK) >> MATTER_MSG_FLAG_VERSION_SHIFT;
    
    if (version != MATTER_MSG_VERSION) {
        return MATTER_MSG_ERROR_INVALID_VERSION;
    }
    
    bool has_source_node_id = (flags & MATTER_MSG_FLAG_S) != 0;
    uint8_t dest_size = (flags & MATTER_MSG_FLAG_DSIZ_MASK) >> MATTER_MSG_FLAG_DSIZ_SHIFT;
    bool has_dest_node_id = (dest_size != 0);
    
    msg->header.flags = flags;
    
    // Decode session ID (2 bytes)
    if (offset + 2 > buffer_size) {
        return MATTER_MSG_ERROR_BUFFER_UNDERFLOW;
    }
    msg->header.session_id = read_le16(&buffer[offset]);
    offset += 2;
    
    // Decode security flags (1 byte)
    if (offset + 1 > buffer_size) {
        return MATTER_MSG_ERROR_BUFFER_UNDERFLOW;
    }
    msg->header.security_flags = buffer[offset++];
    
    // Decode message counter (4 bytes)
    if (offset + 4 > buffer_size) {
        return MATTER_MSG_ERROR_BUFFER_UNDERFLOW;
    }
    msg->header.message_counter = read_le32(&buffer[offset]);
    offset += 4;
    
    // Decode source node ID if present (8 bytes)
    if (has_source_node_id) {
        if (offset + 8 > buffer_size) {
            return MATTER_MSG_ERROR_BUFFER_UNDERFLOW;
        }
        msg->header.source_node_id = read_le64(&buffer[offset]);
        offset += 8;
    } else {
        msg->header.source_node_id = 0;
    }
    
    // Decode destination node ID if present (8 bytes)
    if (has_dest_node_id) {
        if (offset + 8 > buffer_size) {
            return MATTER_MSG_ERROR_BUFFER_UNDERFLOW;
        }
        msg->header.dest_node_id = read_le64(&buffer[offset]);
        offset += 8;
    } else {
        msg->header.dest_node_id = 0;
    }
    
    // For Phase 2, protocol info is metadata not encoded in message
    // Set to default values - application layer will set these
    msg->protocol_id = 0;
    msg->protocol_opcode = 0;
    msg->exchange_id = 0;
    
    // Remaining data is payload
    msg->payload = &buffer[offset];
    msg->payload_length = buffer_size - offset;
    
    return MATTER_MSG_SUCCESS;
}
