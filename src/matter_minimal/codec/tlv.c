#include "tlv.h"
#include <string.h>

/**
 * TLV Control Byte Encoding (Matter Core Specification Section 4.14)
 * 
 * Bits 7-5: Element Type (3 bits)
 * Bits 4-3: Tag Control (2 bits) 
 * Bits 2-0: Length/Value (3 bits)
 */

// Control byte field masks
#define TLV_TYPE_SHIFT 5
#define TLV_TAG_SHIFT 3
#define TLV_LENGTH_MASK 0x07

// Element type encoding in control byte
#define TLV_ELEMENT_TYPE_INT (0 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_UINT (1 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_BOOL (2 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_FLOAT (3 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_UTF8_STRING (4 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_BYTE_STRING (5 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_NULL (6 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_STRUCTURE (7 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_ARRAY (8 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_LIST (9 << TLV_TYPE_SHIFT)
#define TLV_ELEMENT_TYPE_END (10 << TLV_TYPE_SHIFT)

// Tag control encoding
#define TLV_TAG_CONTROL_ANONYMOUS (0 << TLV_TAG_SHIFT)
#define TLV_TAG_CONTROL_CONTEXT (1 << TLV_TAG_SHIFT)

// Length encoding for integers
#define TLV_LENGTH_1_BYTE 0
#define TLV_LENGTH_2_BYTE 1
#define TLV_LENGTH_4_BYTE 2
#define TLV_LENGTH_8_BYTE 3

/**
 * Helper function to write a control byte and optional tag
 */
static int write_control_and_tag(tlv_writer_t *writer, uint8_t type, uint8_t length, uint8_t tag) {
    if (writer->offset >= writer->buffer_size) {
        return -1;
    }
    
    // Build control byte: type (bits 7-5) | tag control (bits 4-3) | length (bits 2-0)
    uint8_t control = type | TLV_TAG_CONTROL_CONTEXT | length;
    writer->buffer[writer->offset++] = control;
    
    // Write tag byte for context-specific tags
    if (writer->offset >= writer->buffer_size) {
        return -1;
    }
    writer->buffer[writer->offset++] = tag;
    
    return 0;
}

/**
 * Helper function to write bytes in little-endian order
 */
static int write_bytes(tlv_writer_t *writer, const void *data, size_t size) {
    if (writer->offset + size > writer->buffer_size) {
        return -1;
    }
    
    memcpy(&writer->buffer[writer->offset], data, size);
    writer->offset += size;
    return 0;
}

/**
 * Helper function to write a length prefix for strings/bytes
 */
static int write_length_prefix(tlv_writer_t *writer, size_t length) {
    if (length <= 0xFF) {
        uint8_t len = (uint8_t)length;
        return write_bytes(writer, &len, 1);
    } else if (length <= 0xFFFF) {
        uint16_t len = (uint16_t)length;
        return write_bytes(writer, &len, 2);
    } else {
        uint32_t len = (uint32_t)length;
        return write_bytes(writer, &len, 4);
    }
}

/**
 * Helper function to determine optimal integer encoding size
 */
static uint8_t get_int_encoding_length(int64_t value) {
    if (value >= -128 && value <= 127) {
        return TLV_LENGTH_1_BYTE;
    } else if (value >= -32768 && value <= 32767) {
        return TLV_LENGTH_2_BYTE;
    } else if (value >= -2147483648LL && value <= 2147483647LL) {
        return TLV_LENGTH_4_BYTE;
    } else {
        return TLV_LENGTH_8_BYTE;
    }
}

/**
 * Helper function to determine optimal unsigned integer encoding size
 */
static uint8_t get_uint_encoding_length(uint64_t value) {
    if (value <= 0xFF) {
        return TLV_LENGTH_1_BYTE;
    } else if (value <= 0xFFFF) {
        return TLV_LENGTH_2_BYTE;
    } else if (value <= 0xFFFFFFFF) {
        return TLV_LENGTH_4_BYTE;
    } else {
        return TLV_LENGTH_8_BYTE;
    }
}

// Writer Functions

void tlv_writer_init(tlv_writer_t *writer, uint8_t *buffer, size_t buffer_size) {
    if (writer == NULL) {
        return;
    }
    
    writer->buffer = buffer;
    writer->buffer_size = buffer_size;
    writer->offset = 0;
}

size_t tlv_writer_get_length(const tlv_writer_t *writer) {
    if (writer == NULL) {
        return 0;
    }
    return writer->offset;
}

// Encoder Functions

int tlv_encode_uint8(tlv_writer_t *writer, uint8_t tag, uint8_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_UINT, TLV_LENGTH_1_BYTE, tag) < 0) {
        return -1;
    }
    
    return write_bytes(writer, &value, 1);
}

int tlv_encode_uint16(tlv_writer_t *writer, uint8_t tag, uint16_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    // Optimize encoding size
    uint8_t length = get_uint_encoding_length(value);
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_UINT, length, tag) < 0) {
        return -1;
    }
    
    // Write value in little-endian order
    switch (length) {
        case TLV_LENGTH_1_BYTE: {
            uint8_t val = (uint8_t)value;
            return write_bytes(writer, &val, 1);
        }
        case TLV_LENGTH_2_BYTE:
            return write_bytes(writer, &value, 2);
        default:
            return -1;
    }
}

int tlv_encode_uint32(tlv_writer_t *writer, uint8_t tag, uint32_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    // Optimize encoding size
    uint8_t length = get_uint_encoding_length(value);
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_UINT, length, tag) < 0) {
        return -1;
    }
    
    // Write value in little-endian order
    switch (length) {
        case TLV_LENGTH_1_BYTE: {
            uint8_t val = (uint8_t)value;
            return write_bytes(writer, &val, 1);
        }
        case TLV_LENGTH_2_BYTE: {
            uint16_t val = (uint16_t)value;
            return write_bytes(writer, &val, 2);
        }
        case TLV_LENGTH_4_BYTE:
            return write_bytes(writer, &value, 4);
        default:
            return -1;
    }
}

int tlv_encode_int8(tlv_writer_t *writer, uint8_t tag, int8_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_INT, TLV_LENGTH_1_BYTE, tag) < 0) {
        return -1;
    }
    
    return write_bytes(writer, &value, 1);
}

int tlv_encode_int16(tlv_writer_t *writer, uint8_t tag, int16_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    // Optimize encoding size
    uint8_t length = get_int_encoding_length(value);
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_INT, length, tag) < 0) {
        return -1;
    }
    
    // Write value in little-endian order
    switch (length) {
        case TLV_LENGTH_1_BYTE: {
            int8_t val = (int8_t)value;
            return write_bytes(writer, &val, 1);
        }
        case TLV_LENGTH_2_BYTE:
            return write_bytes(writer, &value, 2);
        default:
            return -1;
    }
}

int tlv_encode_int32(tlv_writer_t *writer, uint8_t tag, int32_t value) {
    if (writer == NULL) {
        return -1;
    }
    
    // Optimize encoding size
    uint8_t length = get_int_encoding_length(value);
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_INT, length, tag) < 0) {
        return -1;
    }
    
    // Write value in little-endian order
    switch (length) {
        case TLV_LENGTH_1_BYTE: {
            int8_t val = (int8_t)value;
            return write_bytes(writer, &val, 1);
        }
        case TLV_LENGTH_2_BYTE: {
            int16_t val = (int16_t)value;
            return write_bytes(writer, &val, 2);
        }
        case TLV_LENGTH_4_BYTE:
            return write_bytes(writer, &value, 4);
        default:
            return -1;
    }
}

int tlv_encode_bool(tlv_writer_t *writer, uint8_t tag, bool value) {
    if (writer == NULL) {
        return -1;
    }
    
    // Boolean encoding: control byte with length bit indicating true (1) or false (0)
    uint8_t length = value ? 1 : 0;
    return write_control_and_tag(writer, TLV_ELEMENT_TYPE_BOOL, length, tag);
}

int tlv_encode_null(tlv_writer_t *writer, uint8_t tag) {
    if (writer == NULL) {
        return -1;
    }
    
    return write_control_and_tag(writer, TLV_ELEMENT_TYPE_NULL, 0, tag);
}

int tlv_encode_string(tlv_writer_t *writer, uint8_t tag, const char *str) {
    if (writer == NULL || str == NULL) {
        return -1;
    }
    
    size_t length = strlen(str);
    
    // Determine length encoding
    uint8_t length_encoding;
    if (length <= 0xFF) {
        length_encoding = TLV_LENGTH_1_BYTE;
    } else if (length <= 0xFFFF) {
        length_encoding = TLV_LENGTH_2_BYTE;
    } else {
        length_encoding = TLV_LENGTH_4_BYTE;
    }
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_UTF8_STRING, length_encoding, tag) < 0) {
        return -1;
    }
    
    if (write_length_prefix(writer, length) < 0) {
        return -1;
    }
    
    return write_bytes(writer, str, length);
}

int tlv_encode_bytes(tlv_writer_t *writer, uint8_t tag, const uint8_t *data, size_t length) {
    if (writer == NULL || (data == NULL && length > 0)) {
        return -1;
    }
    
    // Determine length encoding
    uint8_t length_encoding;
    if (length <= 0xFF) {
        length_encoding = TLV_LENGTH_1_BYTE;
    } else if (length <= 0xFFFF) {
        length_encoding = TLV_LENGTH_2_BYTE;
    } else {
        length_encoding = TLV_LENGTH_4_BYTE;
    }
    
    if (write_control_and_tag(writer, TLV_ELEMENT_TYPE_BYTE_STRING, length_encoding, tag) < 0) {
        return -1;
    }
    
    if (write_length_prefix(writer, length) < 0) {
        return -1;
    }
    
    return write_bytes(writer, data, length);
}

// Container Functions

int tlv_encode_structure_start(tlv_writer_t *writer, uint8_t tag) {
    if (writer == NULL) {
        return -1;
    }
    
    return write_control_and_tag(writer, TLV_ELEMENT_TYPE_STRUCTURE, 0, tag);
}

int tlv_encode_array_start(tlv_writer_t *writer, uint8_t tag) {
    if (writer == NULL) {
        return -1;
    }
    
    return write_control_and_tag(writer, TLV_ELEMENT_TYPE_ARRAY, 0, tag);
}

int tlv_encode_list_start(tlv_writer_t *writer, uint8_t tag) {
    if (writer == NULL) {
        return -1;
    }
    
    return write_control_and_tag(writer, TLV_ELEMENT_TYPE_LIST, 0, tag);
}

int tlv_encode_container_end(tlv_writer_t *writer) {
    if (writer == NULL) {
        return -1;
    }
    
    if (writer->offset >= writer->buffer_size) {
        return -1;
    }
    
    // End-of-container has no tag (anonymous)
    uint8_t control = TLV_ELEMENT_TYPE_END | TLV_TAG_CONTROL_ANONYMOUS;
    writer->buffer[writer->offset++] = control;
    
    return 0;
}

// Reader Functions

void tlv_reader_init(tlv_reader_t *reader, const uint8_t *buffer, size_t buffer_size) {
    if (reader == NULL) {
        return;
    }
    
    reader->buffer = buffer;
    reader->buffer_size = buffer_size;
    reader->offset = 0;
}

/**
 * Helper function to read bytes from buffer
 */
static int read_bytes(tlv_reader_t *reader, void *dest, size_t size) {
    if (reader->offset + size > reader->buffer_size) {
        return -1;
    }
    
    memcpy(dest, &reader->buffer[reader->offset], size);
    reader->offset += size;
    return 0;
}

/**
 * Helper function to read length prefix
 */
static int read_length_prefix(tlv_reader_t *reader, uint8_t length_encoding, size_t *length) {
    switch (length_encoding) {
        case TLV_LENGTH_1_BYTE: {
            uint8_t len;
            if (read_bytes(reader, &len, 1) < 0) {
                return -1;
            }
            *length = len;
            return 0;
        }
        case TLV_LENGTH_2_BYTE: {
            uint16_t len;
            if (read_bytes(reader, &len, 2) < 0) {
                return -1;
            }
            *length = len;
            return 0;
        }
        case TLV_LENGTH_4_BYTE: {
            uint32_t len;
            if (read_bytes(reader, &len, 4) < 0) {
                return -1;
            }
            *length = len;
            return 0;
        }
        default:
            return -1;
    }
}

int tlv_reader_next(tlv_reader_t *reader, tlv_element_t *element) {
    if (reader == NULL || element == NULL) {
        return -1;
    }
    
    if (reader->offset >= reader->buffer_size) {
        return -1;
    }
    
    // Read control byte
    uint8_t control = reader->buffer[reader->offset++];
    
    // Extract fields from control byte
    uint8_t type_field = (control >> TLV_TYPE_SHIFT) & 0x1F;
    uint8_t tag_control = (control >> TLV_TAG_SHIFT) & 0x03;
    uint8_t length_field = control & TLV_LENGTH_MASK;
    
    // Parse element type
    switch (type_field) {
        case 0: element->type = TLV_TYPE_SIGNED_INT; break;
        case 1: element->type = TLV_TYPE_UNSIGNED_INT; break;
        case 2: element->type = TLV_TYPE_BOOL; break;
        case 3: element->type = TLV_TYPE_FLOAT; break;
        case 4: element->type = TLV_TYPE_UTF8_STRING; break;
        case 5: element->type = TLV_TYPE_BYTE_STRING; break;
        case 6: element->type = TLV_TYPE_NULL; break;
        case 7: element->type = TLV_TYPE_STRUCTURE; break;
        case 8: element->type = TLV_TYPE_ARRAY; break;
        case 9: element->type = TLV_TYPE_LIST; break;
        case 10: element->type = TLV_TYPE_END_OF_CONTAINER; break;
        default: return -1;
    }
    
    // Parse tag
    if (tag_control == 0) {
        // Anonymous tag
        element->tag_type = TLV_TAG_ANONYMOUS;
        element->tag = 0;
    } else if (tag_control == 1) {
        // Context-specific tag
        element->tag_type = TLV_TAG_CONTEXT_SPECIFIC;
        if (read_bytes(reader, &element->tag, 1) < 0) {
            return -1;
        }
    } else {
        // Other tag types not supported in minimal version
        return -1;
    }
    
    // Parse value based on type
    switch (element->type) {
        case TLV_TYPE_SIGNED_INT: {
            switch (length_field) {
                case TLV_LENGTH_1_BYTE:
                    if (read_bytes(reader, &element->value.i8, 1) < 0) return -1;
                    break;
                case TLV_LENGTH_2_BYTE:
                    if (read_bytes(reader, &element->value.i16, 2) < 0) return -1;
                    break;
                case TLV_LENGTH_4_BYTE:
                    if (read_bytes(reader, &element->value.i32, 4) < 0) return -1;
                    break;
                case TLV_LENGTH_8_BYTE:
                    if (read_bytes(reader, &element->value.i64, 8) < 0) return -1;
                    break;
                default:
                    return -1;
            }
            break;
        }
        
        case TLV_TYPE_UNSIGNED_INT: {
            switch (length_field) {
                case TLV_LENGTH_1_BYTE:
                    if (read_bytes(reader, &element->value.u8, 1) < 0) return -1;
                    break;
                case TLV_LENGTH_2_BYTE:
                    if (read_bytes(reader, &element->value.u16, 2) < 0) return -1;
                    break;
                case TLV_LENGTH_4_BYTE:
                    if (read_bytes(reader, &element->value.u32, 4) < 0) return -1;
                    break;
                case TLV_LENGTH_8_BYTE:
                    if (read_bytes(reader, &element->value.u64, 8) < 0) return -1;
                    break;
                default:
                    return -1;
            }
            break;
        }
        
        case TLV_TYPE_BOOL:
            element->value.boolean = (length_field != 0);
            break;
        
        case TLV_TYPE_UTF8_STRING: {
            size_t length;
            if (read_length_prefix(reader, length_field, &length) < 0) {
                return -1;
            }
            element->value.string.data = (const char *)&reader->buffer[reader->offset];
            element->value.string.length = length;
            reader->offset += length;
            break;
        }
        
        case TLV_TYPE_BYTE_STRING: {
            size_t length;
            if (read_length_prefix(reader, length_field, &length) < 0) {
                return -1;
            }
            element->value.bytes.data = &reader->buffer[reader->offset];
            element->value.bytes.length = length;
            reader->offset += length;
            break;
        }
        
        case TLV_TYPE_NULL:
        case TLV_TYPE_STRUCTURE:
        case TLV_TYPE_ARRAY:
        case TLV_TYPE_LIST:
        case TLV_TYPE_END_OF_CONTAINER:
            // No value data for these types
            break;
        
        default:
            return -1;
    }
    
    return 0;
}

int tlv_reader_peek(tlv_reader_t *reader, tlv_element_t *element) {
    if (reader == NULL || element == NULL) {
        return -1;
    }
    
    // Save current offset
    size_t saved_offset = reader->offset;
    
    // Try to read next element
    int result = tlv_reader_next(reader, element);
    
    // Restore offset
    reader->offset = saved_offset;
    
    return result;
}

int tlv_reader_skip(tlv_reader_t *reader) {
    tlv_element_t element;
    return tlv_reader_next(reader, &element);
}

bool tlv_reader_is_end(const tlv_reader_t *reader) {
    if (reader == NULL) {
        return true;
    }
    return reader->offset >= reader->buffer_size;
}

// Convenience Functions

uint8_t tlv_read_uint8(const tlv_element_t *element) {
    if (element == NULL || element->type != TLV_TYPE_UNSIGNED_INT) {
        return 0;
    }
    return element->value.u8;
}

uint16_t tlv_read_uint16(const tlv_element_t *element) {
    if (element == NULL || element->type != TLV_TYPE_UNSIGNED_INT) {
        return 0;
    }
    return element->value.u16;
}

int16_t tlv_read_int16(const tlv_element_t *element) {
    if (element == NULL || element->type != TLV_TYPE_SIGNED_INT) {
        return 0;
    }
    return element->value.i16;
}

bool tlv_read_bool(const tlv_element_t *element) {
    if (element == NULL || element->type != TLV_TYPE_BOOL) {
        return false;
    }
    return element->value.boolean;
}
