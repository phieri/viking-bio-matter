/*
 * test_report_generator.c
 * Unit tests for Matter ReportData generation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/matter_minimal/interaction/report_generator.h"
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
    *type = ATTR_TYPE_UINT8;
    value->uint8_val = 1;
    return 0;
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
    value->uint8_val = 75;
    return 0;
}

int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 1 || attr_id != 0x0000) return -1;
    *type = ATTR_TYPE_INT16;
    value->int16_val = 2350; // 23.50°C
    return 0;
}

// Test: Encode report with single attribute
void test_encode_report_single_attribute(void) {
    printf("Test: Encode ReportData with single attribute...\n");
    
    attribute_report_t reports[1] = {
        {
            .path = {1, 0x0006, 0x0000, false},
            .value = {.bool_val = true},
            .type = ATTR_TYPE_BOOL,
            .status = IM_STATUS_SUCCESS
        }
    };
    
    uint8_t output[512];
    size_t output_len;
    
    int result = report_generator_encode_report(123, reports, 1,
                                               output, sizeof(output),
                                               &output_len);
    
    if (result == 0 && output_len > 0) {
        printf("  ✓ Single attribute report encoded\n");
        tests_passed++;
    } else {
        printf("  ✗ Failed to encode single attribute report\n");
        tests_failed++;
    }
}

// Test: Encode report with multiple attributes
void test_encode_report_multiple_attributes(void) {
    printf("Test: Encode ReportData with multiple attributes...\n");
    
    attribute_report_t reports[3] = {
        {
            .path = {1, 0x0006, 0x0000, false},
            .value = {.bool_val = true},
            .type = ATTR_TYPE_BOOL,
            .status = IM_STATUS_SUCCESS
        },
        {
            .path = {1, 0x0008, 0x0000, false},
            .value = {.uint8_val = 75},
            .type = ATTR_TYPE_UINT8,
            .status = IM_STATUS_SUCCESS
        },
        {
            .path = {1, 0x0402, 0x0000, false},
            .value = {.int16_val = 2350},
            .type = ATTR_TYPE_INT16,
            .status = IM_STATUS_SUCCESS
        }
    };
    
    uint8_t output[1024];
    size_t output_len;
    
    int result = report_generator_encode_report(456, reports, 3,
                                               output, sizeof(output),
                                               &output_len);
    
    if (result == 0 && output_len > 0) {
        printf("  ✓ Multiple attribute report encoded\n");
        tests_passed++;
    } else {
        printf("  ✗ Failed to encode multiple attribute report\n");
        tests_failed++;
    }
}

// Test: Report includes subscription ID
void test_report_with_subscription_id(void) {
    printf("Test: ReportData includes SubscriptionId...\n");
    
    attribute_report_t reports[1] = {
        {
            .path = {1, 0x0006, 0x0000, false},
            .value = {.bool_val = false},
            .type = ATTR_TYPE_BOOL,
            .status = IM_STATUS_SUCCESS
        }
    };
    
    uint32_t expected_sub_id = 789;
    uint8_t output[512];
    size_t output_len;
    
    int result = report_generator_encode_report(expected_sub_id, reports, 1,
                                               output, sizeof(output),
                                               &output_len);
    
    if (result == 0 && output_len > 0) {
        // Parse output to verify subscription ID
        tlv_reader_t reader;
        tlv_element_t element;
        tlv_reader_init(&reader, output, output_len);
        
        bool found_sub_id = false;
        uint32_t actual_sub_id = 0;
        
        while (!tlv_reader_is_end(&reader)) {
            if (tlv_reader_next(&reader, &element) < 0) break;
            
            if (element.tag == 0) {
                // SubscriptionId (tag 0)
                actual_sub_id = element.value.u32;
                found_sub_id = true;
                break;
            }
        }
        
        if (found_sub_id && actual_sub_id == expected_sub_id) {
            printf("  ✓ SubscriptionId included in report\n");
            tests_passed++;
        } else {
            printf("  ✗ SubscriptionId missing or incorrect (expected %u, got %u)\n",
                   expected_sub_id, actual_sub_id);
            tests_failed++;
        }
    } else {
        printf("  ✗ Failed to encode report\n");
        tests_failed++;
    }
}

// Test: Report format matches ReadResponse
void test_report_format_matches_read_response(void) {
    printf("Test: ReportData AttributeReports match ReadResponse format...\n");
    
    // Create a report using report_generator
    attribute_report_t reports[1] = {
        {
            .path = {1, 0x0008, 0x0000, false},
            .value = {.uint8_val = 50},
            .type = ATTR_TYPE_UINT8,
            .status = IM_STATUS_SUCCESS
        }
    };
    
    uint8_t report_output[512];
    size_t report_len;
    
    int result = report_generator_encode_report(999, reports, 1,
                                               report_output, sizeof(report_output),
                                               &report_len);
    
    if (result != 0) {
        printf("  ✗ Failed to encode report\n");
        tests_failed++;
        return;
    }
    
    // Parse ReportData to find AttributeReports
    tlv_reader_t reader;
    tlv_element_t element;
    tlv_reader_init(&reader, report_output, report_len);
    
    bool found_attribute_reports = false;
    
    while (!tlv_reader_is_end(&reader)) {
        if (tlv_reader_next(&reader, &element) < 0) break;
        
        if (element.tag == 1 && (element.type == TLV_TYPE_ARRAY || 
                                 element.type == TLV_TYPE_LIST)) {
            // Found AttributeReports list
            found_attribute_reports = true;
            
            // Verify it contains AttributeReport structures
            while (!tlv_reader_is_end(&reader)) {
                if (tlv_reader_peek(&reader, &element) < 0) break;
                
                if (element.type == TLV_TYPE_END_OF_CONTAINER) {
                    tlv_reader_skip(&reader);
                    break;
                }
                
                if (element.type == TLV_TYPE_STRUCTURE) {
                    // Found AttributeReport structure
                    printf("  ✓ AttributeReports format matches ReadResponse\n");
                    tests_passed++;
                    return;
                }
                
                tlv_reader_skip(&reader);
            }
        }
    }
    
    if (found_attribute_reports) {
        printf("  ✗ AttributeReports found but format doesn't match\n");
    } else {
        printf("  ✗ AttributeReports not found in ReportData\n");
    }
    tests_failed++;
}

// Test: Encode AttributeReports directly
void test_encode_attribute_reports_directly(void) {
    printf("Test: Encode AttributeReports directly...\n");
    
    attribute_report_t reports[2] = {
        {
            .path = {1, 0x0006, 0x0000, false},
            .value = {.bool_val = true},
            .type = ATTR_TYPE_BOOL,
            .status = IM_STATUS_SUCCESS
        },
        {
            .path = {1, 0x0008, 0x0000, false},
            .value = {.uint8_val = 100},
            .type = ATTR_TYPE_UINT8,
            .status = IM_STATUS_SUCCESS
        }
    };
    
    uint8_t output[512];
    size_t output_len;
    
    int result = report_generator_encode_attribute_reports(reports, 2,
                                                          output, sizeof(output),
                                                          &output_len);
    
    if (result == 0 && output_len > 0) {
        printf("  ✓ AttributeReports encoded directly\n");
        tests_passed++;
    } else {
        printf("  ✗ Failed to encode AttributeReports\n");
        tests_failed++;
    }
}

// Test: Send report with paths
void test_send_report_with_paths(void) {
    printf("Test: Send report with attribute paths...\n");
    
    attribute_path_t paths[2] = {
        {1, 0x0006, 0x0000, false},
        {1, 0x0008, 0x0000, false}
    };
    
    int result = report_generator_send_report(200, 111, paths, 2);
    
    if (result == 0) {
        printf("  ✓ Report sent successfully\n");
        tests_passed++;
    } else {
        printf("  ✗ Failed to send report\n");
        tests_failed++;
    }
}

int main() {
    printf("=== Matter Report Generator Tests ===\n\n");
    
    // Initialize report generator
    if (report_generator_init() < 0) {
        printf("ERROR: Failed to initialize report generator\n");
        return 1;
    }
    
    // Run tests
    test_encode_report_single_attribute();
    test_encode_report_multiple_attributes();
    test_report_with_subscription_id();
    test_report_format_matches_read_response();
    test_encode_attribute_reports_directly();
    test_send_report_with_paths();
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return (tests_failed == 0) ? 0 : 1;
}
