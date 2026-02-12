#include "tlv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test helper to print test names
#define TEST(name) printf("Running test: %s\n", name)
#define PASS() printf("  ✓ PASSED\n")

// Test: Encode uint8
void test_encode_uint8(void) {
    TEST("test_encode_uint8");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_uint8(&writer, 1, 42);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode uint16
void test_encode_uint16(void) {
    TEST("test_encode_uint16");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_uint16(&writer, 2, 1000);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode uint32
void test_encode_uint32(void) {
    TEST("test_encode_uint32");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_uint32(&writer, 3, 100000);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode int8
void test_encode_int8(void) {
    TEST("test_encode_int8");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_int8(&writer, 4, -50);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode int16
void test_encode_int16(void) {
    TEST("test_encode_int16");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_int16(&writer, 5, -1000);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode int32
void test_encode_int32(void) {
    TEST("test_encode_int32");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    int result = tlv_encode_int32(&writer, 6, -100000);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode bool
void test_encode_bool(void) {
    TEST("test_encode_bool");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    
    // Test true
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    int result = tlv_encode_bool(&writer, 7, true);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    // Test false
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    result = tlv_encode_bool(&writer, 8, false);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode string
void test_encode_string(void) {
    TEST("test_encode_string");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    const char *test_str = "Hello";
    int result = tlv_encode_string(&writer, 9, test_str);
    assert(result == 0);
    assert(tlv_writer_get_length(&writer) > strlen(test_str));
    
    PASS();
}

// Test: Encode structure with nested fields
void test_encode_structure_with_nested_fields(void) {
    TEST("test_encode_structure_with_nested_fields");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    // Encode a structure with tag 10
    int result = tlv_encode_structure_start(&writer, 10);
    assert(result == 0);
    
    // Add nested fields
    result = tlv_encode_uint8(&writer, 1, 42);
    assert(result == 0);
    
    result = tlv_encode_bool(&writer, 2, true);
    assert(result == 0);
    
    result = tlv_encode_string(&writer, 3, "test");
    assert(result == 0);
    
    // End structure
    result = tlv_encode_container_end(&writer);
    assert(result == 0);
    
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Encode array
void test_encode_array(void) {
    TEST("test_encode_array");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    // Encode array with tag 11
    int result = tlv_encode_array_start(&writer, 11);
    assert(result == 0);
    
    // Add array elements
    result = tlv_encode_uint8(&writer, 0, 1);
    assert(result == 0);
    
    result = tlv_encode_uint8(&writer, 0, 2);
    assert(result == 0);
    
    result = tlv_encode_uint8(&writer, 0, 3);
    assert(result == 0);
    
    // End array
    result = tlv_encode_container_end(&writer);
    assert(result == 0);
    
    assert(tlv_writer_get_length(&writer) > 0);
    
    PASS();
}

// Test: Decode all types
void test_decode_all_types(void) {
    TEST("test_decode_all_types");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    // Encode several values
    tlv_encode_uint8(&writer, 1, 42);
    tlv_encode_int8(&writer, 2, -10);
    tlv_encode_bool(&writer, 3, true);
    tlv_encode_string(&writer, 4, "hi");
    
    // Now decode
    tlv_reader_t reader;
    tlv_reader_init(&reader, buffer, tlv_writer_get_length(&writer));
    
    tlv_element_t element;
    
    // Read uint8
    int result = tlv_reader_next(&reader, &element);
    assert(result == 0);
    assert(element.type == TLV_TYPE_UNSIGNED_INT);
    assert(element.tag == 1);
    assert(tlv_read_uint8(&element) == 42);
    
    // Read int8
    result = tlv_reader_next(&reader, &element);
    assert(result == 0);
    assert(element.type == TLV_TYPE_SIGNED_INT);
    assert(element.tag == 2);
    assert(element.value.i8 == -10);
    
    // Read bool
    result = tlv_reader_next(&reader, &element);
    assert(result == 0);
    assert(element.type == TLV_TYPE_BOOL);
    assert(element.tag == 3);
    assert(tlv_read_bool(&element) == true);
    
    // Read string
    result = tlv_reader_next(&reader, &element);
    assert(result == 0);
    assert(element.type == TLV_TYPE_UTF8_STRING);
    assert(element.tag == 4);
    assert(element.value.string.length == 2);
    assert(memcmp(element.value.string.data, "hi", 2) == 0);
    
    PASS();
}

// Test: Reader find tag
void test_reader_find_tag(void) {
    TEST("test_reader_find_tag");
    
    uint8_t buffer[128];
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    // Encode multiple values with different tags
    tlv_encode_uint8(&writer, 1, 10);
    tlv_encode_uint8(&writer, 2, 20);
    tlv_encode_uint8(&writer, 3, 30);
    tlv_encode_uint8(&writer, 4, 40);
    
    // Read and find specific tag
    tlv_reader_t reader;
    tlv_reader_init(&reader, buffer, tlv_writer_get_length(&writer));
    
    tlv_element_t element;
    bool found = false;
    
    // Find tag 3
    while (tlv_reader_next(&reader, &element) == 0) {
        if (element.tag == 3) {
            found = true;
            assert(tlv_read_uint8(&element) == 30);
            break;
        }
    }
    
    assert(found == true);
    
    PASS();
}

// Test: Buffer overflow handling
void test_buffer_overflow_handling(void) {
    TEST("test_buffer_overflow_handling");
    
    uint8_t buffer[10]; // Very small buffer
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, sizeof(buffer));
    
    // Try to encode more data than buffer can hold
    int result = tlv_encode_uint8(&writer, 1, 10);
    assert(result == 0); // Should succeed (4 bytes used)
    
    // After first encode, we have 6 bytes left, which is enough for another uint8
    result = tlv_encode_uint8(&writer, 2, 20);
    assert(result == 0); // Should succeed (8 bytes used total)
    
    // Try to encode a large string that won't fit
    result = tlv_encode_string(&writer, 3, "This is a very long string");
    assert(result == -1); // Should fail - string requires 31+ bytes (control + tag + length_enc + length + data)
    
    PASS();
}

int main(void) {
    printf("=== TLV Codec Test Suite ===\n\n");
    
    test_encode_uint8();
    test_encode_uint16();
    test_encode_uint32();
    test_encode_int8();
    test_encode_int16();
    test_encode_int32();
    test_encode_bool();
    test_encode_string();
    test_encode_structure_with_nested_fields();
    test_encode_array();
    test_decode_all_types();
    test_reader_find_tag();
    test_buffer_overflow_handling();
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
