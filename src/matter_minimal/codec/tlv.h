#ifndef TLV_H
#define TLV_H

#include "tlv_types.h"

/**
 * TLV Writer Functions
 * Initialize and manage TLV writer state
 */

/**
 * Initialize a TLV writer with the provided buffer
 * @param writer Pointer to writer structure
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 */
void tlv_writer_init(tlv_writer_t *writer, uint8_t *buffer, size_t buffer_size);

/**
 * Get the current length of encoded data
 * @param writer Pointer to writer structure
 * @return Number of bytes written
 */
size_t tlv_writer_get_length(const tlv_writer_t *writer);

/**
 * TLV Encoder Functions
 * Encode primitive values into TLV format
 */

/**
 * Encode an unsigned 8-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_uint8(tlv_writer_t *writer, uint8_t tag, uint8_t value);

/**
 * Encode an unsigned 16-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_uint16(tlv_writer_t *writer, uint8_t tag, uint16_t value);

/**
 * Encode an unsigned 32-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_uint32(tlv_writer_t *writer, uint8_t tag, uint32_t value);

/**
 * Encode a signed 8-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_int8(tlv_writer_t *writer, uint8_t tag, int8_t value);

/**
 * Encode a signed 16-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_int16(tlv_writer_t *writer, uint8_t tag, int16_t value);

/**
 * Encode a signed 32-bit integer
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Value to encode
 * @return 0 on success, -1 on error
 */
int tlv_encode_int32(tlv_writer_t *writer, uint8_t tag, int32_t value);

/**
 * Encode a boolean value
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param value Boolean value (true/false)
 * @return 0 on success, -1 on error
 */
int tlv_encode_bool(tlv_writer_t *writer, uint8_t tag, bool value);

/**
 * Encode a null value
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @return 0 on success, -1 on error
 */
int tlv_encode_null(tlv_writer_t *writer, uint8_t tag);

/**
 * Encode a UTF-8 string
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param str String to encode (null-terminated)
 * @return 0 on success, -1 on error
 */
int tlv_encode_string(tlv_writer_t *writer, uint8_t tag, const char *str);

/**
 * Encode a byte string
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @param data Byte data to encode
 * @param length Length of byte data
 * @return 0 on success, -1 on error
 */
int tlv_encode_bytes(tlv_writer_t *writer, uint8_t tag, const uint8_t *data, size_t length);

/**
 * TLV Container Functions
 * Encode structures, arrays, and lists
 */

/**
 * Start encoding a structure
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @return 0 on success, -1 on error
 */
int tlv_encode_structure_start(tlv_writer_t *writer, uint8_t tag);

/**
 * Start encoding an array
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @return 0 on success, -1 on error
 */
int tlv_encode_array_start(tlv_writer_t *writer, uint8_t tag);

/**
 * Start encoding a list
 * @param writer Pointer to writer structure
 * @param tag Context-specific tag (0-255)
 * @return 0 on success, -1 on error
 */
int tlv_encode_list_start(tlv_writer_t *writer, uint8_t tag);

/**
 * End the current container (structure, array, or list)
 * @param writer Pointer to writer structure
 * @return 0 on success, -1 on error
 */
int tlv_encode_container_end(tlv_writer_t *writer);

/**
 * TLV Reader Functions
 * Read and navigate TLV encoded data
 */

/**
 * Initialize a TLV reader with the provided buffer
 * @param reader Pointer to reader structure
 * @param buffer Input buffer
 * @param buffer_size Size of input buffer
 */
void tlv_reader_init(tlv_reader_t *reader, const uint8_t *buffer, size_t buffer_size);

/**
 * Read the next TLV element
 * @param reader Pointer to reader structure
 * @param element Pointer to element structure to fill
 * @return 0 on success, -1 on error or end of buffer
 */
int tlv_reader_next(tlv_reader_t *reader, tlv_element_t *element);

/**
 * Peek at the next TLV element without advancing the reader
 * @param reader Pointer to reader structure
 * @param element Pointer to element structure to fill
 * @return 0 on success, -1 on error or end of buffer
 */
int tlv_reader_peek(tlv_reader_t *reader, tlv_element_t *element);

/**
 * Skip the next TLV element
 * @param reader Pointer to reader structure
 * @return 0 on success, -1 on error
 */
int tlv_reader_skip(tlv_reader_t *reader);

/**
 * Check if reader is at the end of the buffer
 * @param reader Pointer to reader structure
 * @return true if at end, false otherwise
 */
bool tlv_reader_is_end(const tlv_reader_t *reader);

/**
 * TLV Convenience Functions
 * Helper functions for common read operations
 */

/**
 * Read an unsigned 8-bit integer value
 * @param element Pointer to element
 * @return Value, or 0 if type mismatch
 */
uint8_t tlv_read_uint8(const tlv_element_t *element);

/**
 * Read an unsigned 16-bit integer value
 * @param element Pointer to element
 * @return Value, or 0 if type mismatch
 */
uint16_t tlv_read_uint16(const tlv_element_t *element);

/**
 * Read a signed 16-bit integer value
 * @param element Pointer to element
 * @return Value, or 0 if type mismatch
 */
int16_t tlv_read_int16(const tlv_element_t *element);

/**
 * Read a boolean value
 * @param element Pointer to element
 * @return Value, or false if type mismatch
 */
bool tlv_read_bool(const tlv_element_t *element);

#endif // TLV_H
