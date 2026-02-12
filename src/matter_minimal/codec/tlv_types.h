#ifndef TLV_TYPES_H
#define TLV_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * TLV Element Types
 * Based on Matter Core Specification Section 4.14
 */
typedef enum {
    TLV_TYPE_SIGNED_INT = 0,     // Signed integer (1, 2, 4, or 8 bytes)
    TLV_TYPE_UNSIGNED_INT = 1,   // Unsigned integer (1, 2, 4, or 8 bytes)
    TLV_TYPE_BOOL = 2,           // Boolean (true/false)
    TLV_TYPE_FLOAT = 3,          // Floating point (not implemented in minimal version)
    TLV_TYPE_UTF8_STRING = 4,    // UTF-8 string
    TLV_TYPE_BYTE_STRING = 5,    // Byte string
    TLV_TYPE_NULL = 6,           // Null value
    TLV_TYPE_STRUCTURE = 7,      // Structure (container)
    TLV_TYPE_ARRAY = 8,          // Array (container)
    TLV_TYPE_LIST = 9,           // List (container)
    TLV_TYPE_END_OF_CONTAINER = 10  // End of container marker
} tlv_element_type_t;

/**
 * TLV Tag Types
 * Based on Matter Core Specification Section 4.14
 */
typedef enum {
    TLV_TAG_ANONYMOUS = 0,       // No tag (anonymous)
    TLV_TAG_CONTEXT_SPECIFIC = 1, // Context-specific tag (1 byte)
    TLV_TAG_COMMON_PROFILE_2 = 2, // Common profile tag (2 bytes)
    TLV_TAG_COMMON_PROFILE_4 = 3, // Common profile tag (4 bytes)
    TLV_TAG_IMPLICIT_PROFILE_2 = 4, // Implicit profile tag (2 bytes)
    TLV_TAG_IMPLICIT_PROFILE_4 = 5, // Implicit profile tag (4 bytes)
    TLV_TAG_FULLY_QUALIFIED_6 = 6,  // Fully-qualified tag (6 bytes)
    TLV_TAG_FULLY_QUALIFIED_8 = 7   // Fully-qualified tag (8 bytes)
} tlv_tag_type_t;

/**
 * TLV Element Value Union
 * Holds the actual data for different element types
 */
typedef union {
    uint8_t u8;
    uint16_t u16;
    uint32_t u32;
    uint64_t u64;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    bool boolean;
    struct {
        const uint8_t *data;
        size_t length;
    } bytes;
    struct {
        const char *data;
        size_t length;
    } string;
} tlv_value_t;

/**
 * TLV Element
 * Represents a single TLV element with type, tag, and value
 */
typedef struct {
    tlv_element_type_t type;
    tlv_tag_type_t tag_type;
    uint8_t tag;  // Only context-specific tags (1 byte) supported in minimal version
    tlv_value_t value;
} tlv_element_t;

/**
 * TLV Writer
 * State for encoding TLV data into a buffer
 */
typedef struct {
    uint8_t *buffer;      // Output buffer
    size_t buffer_size;   // Total buffer size
    size_t offset;        // Current write offset
} tlv_writer_t;

/**
 * TLV Reader
 * State for decoding TLV data from a buffer
 */
typedef struct {
    const uint8_t *buffer; // Input buffer
    size_t buffer_size;    // Total buffer size
    size_t offset;         // Current read offset
} tlv_reader_t;

#endif // TLV_TYPES_H
