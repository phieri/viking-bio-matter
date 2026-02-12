/*
 * test_read_handler.c
 * Unit tests for Matter ReadRequest/ReadResponse handler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/matter_minimal/interaction/read_handler.h"
#include "../../src/matter_minimal/interaction/interaction_model.h"
#include "../../src/matter_minimal/codec/tlv.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

// Mock cluster read functions for testing
int cluster_descriptor_read(uint8_t endpoint, uint32_t attr_id,
                           attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 0) return -1;
    
    switch (attr_id) {
        case 0x0000: // DeviceTypeList
            *type = ATTR_TYPE_ARRAY;
            value->uint8_val = 1;  // Count
            return 0;
        case 0x0001: // ServerList
            *type = ATTR_TYPE_ARRAY;
            value->uint8_val = 1;
            return 0;
        default:
            return -1;
    }
}

int cluster_onoff_read(uint8_t endpoint, uint32_t attr_id,
                      attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 1 || attr_id != 0x0000) return -1;
    *type = ATTR_TYPE_BOOL;
    value->bool_val = true;
    return 0;
}

int cluster_level_control_read(uint8_t endpoint, uint32_t attr_id,
                               attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 1 || attr_id != 0x0000) return -1;
    *type = ATTR_TYPE_UINT8;
    value->uint8_val = 50;
    return 0;
}

int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 1 || attr_id != 0x0000) return -1;
    *type = ATTR_TYPE_INT16;
    value->int16_val = 2500;  // 25.00°C
    return 0;
}

// Helper to create a simple ReadRequest TLV
static size_t create_read_request(uint8_t endpoint, uint32_t cluster_id,
                                  uint32_t attr_id, uint8_t *buffer, size_t max_len) {
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, max_len);
    
    // ReadRequest structure
    // AttributeRequests [0]: Array
    tlv_encode_array_start(&writer, 0);
    
    // AttributePath structure
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, endpoint);      // Endpoint [0]
    tlv_encode_uint32(&writer, 2, cluster_id);   // Cluster [2]
    tlv_encode_uint32(&writer, 3, attr_id);      // Attribute [3]
    tlv_encode_container_end(&writer);
    
    tlv_encode_container_end(&writer);  // End array
    
    return tlv_writer_get_length(&writer);
}

// Test: Parse single attribute path
void test_parse_read_request_single_attribute(void) {
    printf("Test: Parse ReadRequest with single attribute...\n");
    
    uint8_t request[256];
    size_t request_len = create_read_request(1, 0x0006, 0x0000, request, sizeof(request));
    
    uint8_t response[512];
    size_t response_len;
    
    int result = read_handler_process_request(request, request_len,
                                              response, sizeof(response),
                                              &response_len);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ Single attribute read succeeded\n");
        tests_passed++;
    } else {
        printf("  ✗ Single attribute read failed\n");
        tests_failed++;
    }
}

// Test: Parse multiple attribute paths
void test_parse_read_request_multiple_attributes(void) {
    printf("Test: Parse ReadRequest with multiple attributes...\n");
    
    uint8_t request[512];
    tlv_writer_t writer;
    tlv_writer_init(&writer, request, sizeof(request));
    
    // ReadRequest with multiple paths
    tlv_encode_array_start(&writer, 0);
    
    // Path 1: OnOff
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 1);
    tlv_encode_uint32(&writer, 2, 0x0006);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    // Path 2: LevelControl
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 1);
    tlv_encode_uint32(&writer, 2, 0x0008);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    tlv_encode_container_end(&writer);
    
    size_t request_len = tlv_writer_get_length(&writer);
    
    uint8_t response[512];
    size_t response_len;
    
    int result = read_handler_process_request(request, request_len,
                                              response, sizeof(response),
                                              &response_len);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ Multiple attribute read succeeded\n");
        tests_passed++;
    } else {
        printf("  ✗ Multiple attribute read failed\n");
        tests_failed++;
    }
}

// Test: Encode ReadResponse with success
void test_encode_read_response_success(void) {
    printf("Test: Encode ReadResponse with successful read...\n");
    
    attribute_report_t report;
    report.path.endpoint = 1;
    report.path.cluster_id = 0x0006;
    report.path.attribute_id = 0x0000;
    report.value.bool_val = true;
    report.type = ATTR_TYPE_BOOL;
    report.status = IM_STATUS_SUCCESS;
    
    uint8_t response[512];
    size_t response_len;
    
    int result = read_handler_encode_response(&report, 1, response,
                                             sizeof(response), &response_len);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ Response encoding succeeded (len=%zu)\n", response_len);
        tests_passed++;
    } else {
        printf("  ✗ Response encoding failed\n");
        tests_failed++;
    }
}

// Test: Encode ReadResponse with error
void test_encode_read_response_unsupported_cluster(void) {
    printf("Test: Encode ReadResponse with unsupported cluster error...\n");
    
    attribute_report_t report;
    report.path.endpoint = 1;
    report.path.cluster_id = 0x9999;  // Invalid cluster
    report.path.attribute_id = 0x0000;
    report.status = IM_STATUS_UNSUPPORTED_CLUSTER;
    
    uint8_t response[512];
    size_t response_len;
    
    int result = read_handler_encode_response(&report, 1, response,
                                             sizeof(response), &response_len);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ Error response encoding succeeded\n");
        tests_passed++;
    } else {
        printf("  ✗ Error response encoding failed\n");
        tests_failed++;
    }
}

// Test: Read from all clusters
void test_read_all_clusters(void) {
    printf("Test: Read from all clusters (Descriptor, OnOff, LevelControl, Temperature)...\n");
    
    uint8_t request[512];
    tlv_writer_t writer;
    tlv_writer_init(&writer, request, sizeof(request));
    
    tlv_encode_array_start(&writer, 0);
    
    // Descriptor cluster on endpoint 0
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 0);
    tlv_encode_uint32(&writer, 2, 0x001D);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    // OnOff cluster on endpoint 1
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 1);
    tlv_encode_uint32(&writer, 2, 0x0006);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    // LevelControl cluster on endpoint 1
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 1);
    tlv_encode_uint32(&writer, 2, 0x0008);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    // Temperature cluster on endpoint 1
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, 1);
    tlv_encode_uint32(&writer, 2, 0x0402);
    tlv_encode_uint32(&writer, 3, 0x0000);
    tlv_encode_container_end(&writer);
    
    tlv_encode_container_end(&writer);
    
    size_t request_len = tlv_writer_get_length(&writer);
    
    uint8_t response[1024];
    size_t response_len;
    
    int result = read_handler_process_request(request, request_len,
                                              response, sizeof(response),
                                              &response_len);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ All clusters read succeeded (response len=%zu)\n", response_len);
        tests_passed++;
    } else {
        printf("  ✗ All clusters read failed\n");
        tests_failed++;
    }
}

// Test: Full read request/response roundtrip
void test_read_request_response_roundtrip(void) {
    printf("Test: Full ReadRequest/Response roundtrip...\n");
    
    uint8_t request[256];
    size_t request_len = create_read_request(1, 0x0402, 0x0000, request, sizeof(request));
    
    uint8_t response[512];
    size_t response_len;
    
    int result = read_handler_process_request(request, request_len,
                                              response, sizeof(response),
                                              &response_len);
    
    if (result == 0 && response_len > 0) {
        // Verify response can be parsed
        tlv_reader_t reader;
        tlv_reader_init(&reader, response, response_len);
        
        tlv_element_t element;
        if (tlv_reader_next(&reader, &element) == 0) {
            printf("  ✓ Roundtrip succeeded, response is valid TLV\n");
            tests_passed++;
        } else {
            printf("  ✗ Response TLV parsing failed\n");
            tests_failed++;
        }
    } else {
        printf("  ✗ Roundtrip failed\n");
        tests_failed++;
    }
}

int main(void) {
    printf("\n========================================\n");
    printf("  Matter ReadHandler Tests\n");
    printf("========================================\n\n");
    
    // Initialize read handler
    if (read_handler_init() < 0) {
        printf("ERROR: Failed to initialize read handler\n");
        return 1;
    }
    
    // Run tests
    test_parse_read_request_single_attribute();
    test_parse_read_request_multiple_attributes();
    test_encode_read_response_success();
    test_encode_read_response_unsupported_cluster();
    test_read_all_clusters();
    test_read_request_response_roundtrip();
    
    // Print results
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
