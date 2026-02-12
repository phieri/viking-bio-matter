/*
 * interaction_model.h
 * Matter Interaction Model common types and constants
 * Based on Matter Core Specification Section 8
 */

#ifndef INTERACTION_MODEL_H
#define INTERACTION_MODEL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Matter Protocol IDs
 */
#define PROTOCOL_SECURE_CHANNEL      0x0000  // Secure channel protocol (PASE/CASE)
#define PROTOCOL_INTERACTION_MODEL   0x0001  // Interaction model protocol

/**
 * Interaction Model Opcodes (Protocol 0x0001)
 */
#define OP_STATUS_RESPONSE      0x01
#define OP_READ_REQUEST         0x02
#define OP_SUBSCRIBE_REQUEST    0x03
#define OP_SUBSCRIBE_RESPONSE   0x04
#define OP_REPORT_DATA          0x05
#define OP_WRITE_REQUEST        0x06
#define OP_WRITE_RESPONSE       0x07
#define OP_INVOKE_REQUEST       0x08
#define OP_INVOKE_RESPONSE      0x09
#define OP_TIMED_REQUEST        0x0A

/**
 * Interaction Model Status Codes
 * Based on Matter Core Specification Section 8.10
 */
typedef enum {
    IM_STATUS_SUCCESS                 = 0x00,
    IM_STATUS_FAILURE                 = 0x01,
    IM_STATUS_INVALID_SUBSCRIPTION    = 0x7D,
    IM_STATUS_UNSUPPORTED_ACCESS      = 0x7E,
    IM_STATUS_UNSUPPORTED_ENDPOINT    = 0x7F,
    IM_STATUS_INVALID_ACTION          = 0x80,
    IM_STATUS_UNSUPPORTED_COMMAND     = 0x81,
    IM_STATUS_INVALID_COMMAND         = 0x85,
    IM_STATUS_UNSUPPORTED_ATTRIBUTE   = 0x86,
    IM_STATUS_CONSTRAINT_ERROR        = 0x87,
    IM_STATUS_UNSUPPORTED_WRITE       = 0x88,
    IM_STATUS_RESOURCE_EXHAUSTED      = 0x89,
    IM_STATUS_NOT_FOUND               = 0x8B,
    IM_STATUS_UNREPORTABLE_ATTRIBUTE  = 0x8C,
    IM_STATUS_INVALID_DATA_TYPE       = 0x8D,
    IM_STATUS_UNSUPPORTED_READ        = 0x8F,
    IM_STATUS_DATA_VERSION_MISMATCH   = 0x92,
    IM_STATUS_TIMEOUT                 = 0x94,
    IM_STATUS_BUSY                    = 0x9C,
    IM_STATUS_UNSUPPORTED_CLUSTER     = 0xC3,
    IM_STATUS_NO_UPSTREAM_SUBSCRIPTION = 0xC5,
    IM_STATUS_NEEDS_TIMED_INTERACTION = 0xC6,
    IM_STATUS_UNSUPPORTED_EVENT       = 0xC7,
    IM_STATUS_PATHS_EXHAUSTED         = 0xC8,
    IM_STATUS_TIMED_REQUEST_MISMATCH  = 0xC9,
    IM_STATUS_FAILSAFE_REQUIRED       = 0xCA
} im_status_code_t;

/**
 * Attribute Path Structure
 * Identifies a specific attribute in the data model
 */
typedef struct {
    uint8_t endpoint;           // Endpoint number
    uint32_t cluster_id;        // Cluster ID
    uint32_t attribute_id;      // Attribute ID
    bool wildcard;              // True if this is a wildcard path
} attribute_path_t;

/**
 * Attribute Data Value
 * Generic container for attribute values
 */
typedef union {
    bool bool_val;
    uint8_t uint8_val;
    int16_t int16_val;
    uint16_t uint16_val;
    uint32_t uint32_val;
} attribute_value_t;

/**
 * Attribute Data Type
 */
typedef enum {
    ATTR_TYPE_BOOL,
    ATTR_TYPE_UINT8,
    ATTR_TYPE_INT16,
    ATTR_TYPE_UINT16,
    ATTR_TYPE_UINT32,
    ATTR_TYPE_ARRAY
} attribute_type_t;

/**
 * Interaction Status
 * Result of an interaction operation
 */
typedef enum {
    INTERACTION_STATUS_SUCCESS = 0,
    INTERACTION_STATUS_UNSUPPORTED_CLUSTER = -1,
    INTERACTION_STATUS_UNSUPPORTED_ATTRIBUTE = -2,
    INTERACTION_STATUS_UNSUPPORTED_ENDPOINT = -3,
    INTERACTION_STATUS_BUFFER_TOO_SMALL = -4,
    INTERACTION_STATUS_INVALID_REQUEST = -5
} interaction_status_t;

#ifdef __cplusplus
}
#endif

#endif // INTERACTION_MODEL_H
