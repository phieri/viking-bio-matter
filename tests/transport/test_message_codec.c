/*
 * test_message_codec.c
 * Unit tests for Matter message encoding/decoding
 */

#include "message_codec.h"
#include "tlv.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("FAIL: %s - %s\n", __func__, msg); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS() do { \
    printf("PASS: %s\n", __func__); \
    tests_passed++; \
} while(0)

/**
 * Test basic message encoding and decoding
 */
void test_encode_decode_basic_message(void) {
    uint8_t buffer[256];
    size_t encoded_length;
    
    // Create a simple unsecured message
    matter_message_t msg = {0};
    msg.header.flags = 0;
    msg.header.session_id = 0;  // Unsecured
    msg.header.security_flags = 0;
    msg.header.message_counter = 123;
    msg.header.source_node_id = 0;  // No source node ID
    msg.header.dest_node_id = 0;     // No dest node ID
    msg.protocol_id = MATTER_PROTOCOL_INTERACTION_MODEL;
    msg.protocol_opcode = MATTER_IM_OPCODE_READ_REQUEST;
    msg.exchange_id = 456;
    
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0x04};
    msg.payload = payload;
    msg.payload_length = sizeof(payload);
    
    // Encode message
    int result = matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_length);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Encoding failed");
    TEST_ASSERT(encoded_length > 0, "Encoded length is zero");
    TEST_ASSERT(encoded_length == (MATTER_MIN_HEADER_SIZE + 4 + sizeof(payload)), 
                "Encoded length mismatch");
    
    // Decode message
    matter_message_t decoded_msg = {0};
    result = matter_message_decode(buffer, encoded_length, &decoded_msg);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Decoding failed");
    
    // Verify header fields
    TEST_ASSERT(decoded_msg.header.session_id == msg.header.session_id, 
                "Session ID mismatch");
    TEST_ASSERT(decoded_msg.header.message_counter == msg.header.message_counter, 
                "Message counter mismatch");
    TEST_ASSERT(decoded_msg.header.source_node_id == 0, "Source node ID should be 0");
    TEST_ASSERT(decoded_msg.header.dest_node_id == 0, "Dest node ID should be 0");
    
    // Verify protocol fields
    TEST_ASSERT(decoded_msg.protocol_id == msg.protocol_id, "Protocol ID mismatch");
    TEST_ASSERT(decoded_msg.protocol_opcode == msg.protocol_opcode, "Opcode mismatch");
    TEST_ASSERT(decoded_msg.exchange_id == msg.exchange_id, "Exchange ID mismatch");
    
    // Verify payload
    TEST_ASSERT(decoded_msg.payload_length == msg.payload_length, "Payload length mismatch");
    TEST_ASSERT(memcmp(decoded_msg.payload, msg.payload, msg.payload_length) == 0,
                "Payload content mismatch");
    
    TEST_PASS();
}

/**
 * Test message header fields with node IDs
 */
void test_message_header_fields(void) {
    uint8_t buffer[256];
    size_t encoded_length;
    
    // Create message with source and dest node IDs
    matter_message_t msg = {0};
    msg.header.session_id = 0x1234;
    msg.header.security_flags = 0;
    msg.header.message_counter = 0xABCDEF01;
    msg.header.source_node_id = 0x1122334455667788ULL;
    msg.header.dest_node_id = 0x8877665544332211ULL;
    msg.protocol_id = MATTER_PROTOCOL_SECURE_CHANNEL;
    msg.protocol_opcode = MATTER_SC_OPCODE_PBKDF_PARAM_REQUEST;
    msg.exchange_id = 999;
    msg.payload = NULL;
    msg.payload_length = 0;
    
    // Encode message
    int result = matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_length);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Encoding with node IDs failed");
    
    // Should be: 8 (min header) + 16 (2 node IDs) + 4 (protocol header) = 28 bytes
    TEST_ASSERT(encoded_length == 28, "Encoded length incorrect for message with node IDs");
    
    // Decode message
    matter_message_t decoded_msg = {0};
    result = matter_message_decode(buffer, encoded_length, &decoded_msg);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Decoding with node IDs failed");
    
    // Verify all fields
    TEST_ASSERT(decoded_msg.header.session_id == msg.header.session_id, 
                "Session ID mismatch with node IDs");
    TEST_ASSERT(decoded_msg.header.message_counter == msg.header.message_counter,
                "Message counter mismatch with node IDs");
    TEST_ASSERT(decoded_msg.header.source_node_id == msg.header.source_node_id,
                "Source node ID mismatch");
    TEST_ASSERT(decoded_msg.header.dest_node_id == msg.header.dest_node_id,
                "Dest node ID mismatch");
    TEST_ASSERT(decoded_msg.protocol_id == msg.protocol_id,
                "Protocol ID mismatch with node IDs");
    TEST_ASSERT(decoded_msg.protocol_opcode == msg.protocol_opcode,
                "Opcode mismatch with node IDs");
    TEST_ASSERT(decoded_msg.exchange_id == msg.exchange_id,
                "Exchange ID mismatch with node IDs");
    
    TEST_PASS();
}

/**
 * Test message with TLV-encoded payload
 */
void test_payload_with_tlv_data(void) {
    uint8_t buffer[256];
    uint8_t tlv_buffer[128];
    size_t encoded_length;
    
    // Create TLV payload
    tlv_writer_t writer;
    tlv_writer_init(&writer, tlv_buffer, sizeof(tlv_buffer));
    
    // Encode some TLV values
    TEST_ASSERT(tlv_encode_uint8(&writer, 1, 42) == 0, "TLV encode uint8 failed");
    TEST_ASSERT(tlv_encode_int16(&writer, 2, -1234) == 0, "TLV encode int16 failed");
    TEST_ASSERT(tlv_encode_bool(&writer, 3, true) == 0, "TLV encode bool failed");
    
    size_t tlv_length = tlv_writer_get_length(&writer);
    TEST_ASSERT(tlv_length > 0, "TLV length is zero");
    
    // Create message with TLV payload
    matter_message_t msg = {0};
    msg.header.session_id = 0;
    msg.header.message_counter = matter_message_get_next_counter();
    msg.protocol_id = MATTER_PROTOCOL_INTERACTION_MODEL;
    msg.protocol_opcode = MATTER_IM_OPCODE_REPORT_DATA;
    msg.exchange_id = matter_message_get_next_exchange_id();
    msg.payload = tlv_buffer;
    msg.payload_length = tlv_length;
    
    // Encode message
    int result = matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_length);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Encoding with TLV payload failed");
    
    // Decode message
    matter_message_t decoded_msg = {0};
    result = matter_message_decode(buffer, encoded_length, &decoded_msg);
    TEST_ASSERT(result == MATTER_MSG_SUCCESS, "Decoding with TLV payload failed");
    
    // Verify TLV payload
    TEST_ASSERT(decoded_msg.payload_length == tlv_length, "TLV payload length mismatch");
    TEST_ASSERT(memcmp(decoded_msg.payload, tlv_buffer, tlv_length) == 0,
                "TLV payload content mismatch");
    
    // Decode TLV values to verify integrity
    tlv_reader_t reader;
    tlv_reader_init(&reader, decoded_msg.payload, decoded_msg.payload_length);
    
    tlv_element_t elem;
    TEST_ASSERT(tlv_decode(&reader, &elem) == 0, "TLV decode element 1 failed");
    TEST_ASSERT(elem.type == TLV_TYPE_UNSIGNED_INT, "TLV element 1 type mismatch");
    TEST_ASSERT(elem.tag == 1, "TLV element 1 tag mismatch");
    TEST_ASSERT(elem.value.u8 == 42, "TLV element 1 value mismatch");
    
    TEST_ASSERT(tlv_decode(&reader, &elem) == 0, "TLV decode element 2 failed");
    TEST_ASSERT(elem.type == TLV_TYPE_SIGNED_INT, "TLV element 2 type mismatch");
    TEST_ASSERT(elem.tag == 2, "TLV element 2 tag mismatch");
    TEST_ASSERT(elem.value.i16 == -1234, "TLV element 2 value mismatch");
    
    TEST_ASSERT(tlv_decode(&reader, &elem) == 0, "TLV decode element 3 failed");
    TEST_ASSERT(elem.type == TLV_TYPE_BOOL, "TLV element 3 type mismatch");
    TEST_ASSERT(elem.tag == 3, "TLV element 3 tag mismatch");
    TEST_ASSERT(elem.value.boolean == true, "TLV element 3 value mismatch");
    
    TEST_PASS();
}

/**
 * Test message counter increment
 */
void test_message_counter_increment(void) {
    matter_message_codec_init();
    
    uint32_t counter1 = matter_message_get_next_counter();
    uint32_t counter2 = matter_message_get_next_counter();
    uint32_t counter3 = matter_message_get_next_counter();
    
    TEST_ASSERT(counter2 == counter1 + 1, "Counter not incrementing");
    TEST_ASSERT(counter3 == counter2 + 1, "Counter not incrementing");
    
    uint16_t exchange1 = matter_message_get_next_exchange_id();
    uint16_t exchange2 = matter_message_get_next_exchange_id();
    
    TEST_ASSERT(exchange2 == exchange1 + 1, "Exchange ID not incrementing");
    
    TEST_PASS();
}

/**
 * Test invalid message handling
 */
void test_invalid_message_handling(void) {
    uint8_t buffer[256];
    size_t encoded_length;
    matter_message_t msg = {0};
    
    // Test NULL pointer checks
    TEST_ASSERT(matter_message_encode(NULL, buffer, sizeof(buffer), &encoded_length) 
                == MATTER_MSG_ERROR_INVALID_INPUT, 
                "Should reject NULL message");
    TEST_ASSERT(matter_message_encode(&msg, NULL, sizeof(buffer), &encoded_length)
                == MATTER_MSG_ERROR_INVALID_INPUT,
                "Should reject NULL buffer");
    TEST_ASSERT(matter_message_encode(&msg, buffer, sizeof(buffer), NULL)
                == MATTER_MSG_ERROR_INVALID_INPUT,
                "Should reject NULL length pointer");
    
    // Test buffer too small
    msg.payload_length = 2000;  // Larger than max
    TEST_ASSERT(matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_length)
                == MATTER_MSG_ERROR_BUFFER_TOO_SMALL,
                "Should reject oversized payload");
    
    // Test decode with invalid input
    TEST_ASSERT(matter_message_decode(NULL, 100, &msg)
                == MATTER_MSG_ERROR_INVALID_INPUT,
                "Should reject NULL buffer for decode");
    TEST_ASSERT(matter_message_decode(buffer, 0, &msg)
                == MATTER_MSG_ERROR_INVALID_INPUT,
                "Should reject zero-length buffer");
    TEST_ASSERT(matter_message_decode(buffer, 4, &msg)
                == MATTER_MSG_ERROR_INVALID_INPUT,
                "Should reject buffer smaller than min header");
    
    // Test invalid version
    buffer[0] = 0x0F;  // Invalid version (max version bits set)
    TEST_ASSERT(matter_message_decode(buffer, 20, &msg)
                == MATTER_MSG_ERROR_INVALID_VERSION,
                "Should reject invalid version");
    
    TEST_PASS();
}

/**
 * Main test runner
 */
int main(void) {
    printf("=== Matter Message Codec Tests ===\n\n");
    
    matter_message_codec_init();
    
    test_encode_decode_basic_message();
    test_message_header_fields();
    test_payload_with_tlv_data();
    test_message_counter_increment();
    test_invalid_message_handling();
    
    printf("\n=== Test Results ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    
    return (tests_failed == 0) ? 0 : 1;
}
