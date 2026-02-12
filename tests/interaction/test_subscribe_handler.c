/*
 * test_subscribe_handler.c
 * Unit tests for Matter SubscribeRequest/SubscribeResponse handler
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/matter_minimal/interaction/subscribe_handler.h"
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
    value->uint8_val = 50;
    return 0;
}

int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type) {
    if (endpoint != 1 || attr_id != 0x0000) return -1;
    *type = ATTR_TYPE_INT16;
    value->int16_val = 2500;
    return 0;
}

// Helper to create a SubscribeRequest TLV
static size_t create_subscribe_request(uint8_t endpoint, uint32_t cluster_id,
                                       uint32_t attr_id, uint16_t min_interval,
                                       uint16_t max_interval, uint8_t *buffer,
                                       size_t max_len) {
    tlv_writer_t writer;
    tlv_writer_init(&writer, buffer, max_len);
    
    // AttributeRequests [0]: Array
    tlv_encode_array_start(&writer, 0);
    tlv_encode_structure_start(&writer, 0xFF);
    tlv_encode_uint8(&writer, 0, endpoint);
    tlv_encode_uint32(&writer, 2, cluster_id);
    tlv_encode_uint32(&writer, 3, attr_id);
    tlv_encode_container_end(&writer);
    tlv_encode_container_end(&writer);
    
    // MinIntervalFloor [2]
    tlv_encode_uint16(&writer, 2, min_interval);
    
    // MaxIntervalCeiling [3]
    tlv_encode_uint16(&writer, 3, max_interval);
    
    // KeepSubscriptions [4]: false
    tlv_encode_bool(&writer, 4, false);
    
    return tlv_writer_get_length(&writer);
}

// Test: Parse SubscribeRequest
void test_subscribe_request_parsing(void) {
    printf("Test: Parse SubscribeRequest...\n");
    
    uint8_t request[256];
    size_t request_len = create_subscribe_request(1, 0x0006, 0x0000, 1, 10,
                                                   request, sizeof(request));
    
    uint8_t response[256];
    size_t response_len;
    
    int result = subscribe_handler_process_request(request, request_len,
                                                   response, sizeof(response),
                                                   &response_len, 100);
    
    if (result == 0 && response_len > 0) {
        printf("  ✓ SubscribeRequest parsed successfully\n");
        tests_passed++;
    } else {
        printf("  ✗ SubscribeRequest parsing failed\n");
        tests_failed++;
    }
}

// Test: Encode SubscribeResponse
void test_subscribe_response_encoding(void) {
    printf("Test: Encode SubscribeResponse...\n");
    
    uint8_t request[256];
    size_t request_len = create_subscribe_request(1, 0x0006, 0x0000, 1, 10,
                                                   request, sizeof(request));
    
    uint8_t response[256];
    size_t response_len;
    
    int result = subscribe_handler_process_request(request, request_len,
                                                   response, sizeof(response),
                                                   &response_len, 101);
    
    if (result == 0 && response_len > 0) {
        // Parse response to verify structure
        tlv_reader_t reader;
        tlv_element_t element;
        tlv_reader_init(&reader, response, response_len);
        
        bool has_sub_id = false;
        bool has_max_interval = false;
        
        while (!tlv_reader_is_end(&reader)) {
            if (tlv_reader_next(&reader, &element) < 0) break;
            
            if (element.tag == 0) {
                // SubscriptionId
                has_sub_id = true;
            } else if (element.tag == 2) {
                // MaxInterval
                has_max_interval = true;
            }
        }
        
        if (has_sub_id && has_max_interval) {
            printf("  ✓ SubscribeResponse encoded correctly\n");
            tests_passed++;
        } else {
            printf("  ✗ SubscribeResponse missing required fields\n");
            tests_failed++;
        }
    } else {
        printf("  ✗ SubscribeResponse encoding failed\n");
        tests_failed++;
    }
}

// Test: Add subscription
void test_add_subscription(void) {
    printf("Test: Add subscription...\n");
    
    attribute_path_t path = {
        .endpoint = 1,
        .cluster_id = 0x0006,
        .attribute_id = 0x0000,
        .wildcard = false
    };
    
    uint32_t sub_id = subscribe_handler_add(102, &path, 1, 10);
    
    if (sub_id > 0) {
        const subscription_t *sub = subscribe_handler_get_subscription(sub_id);
        if (sub && sub->active && sub->session_id == 102) {
            printf("  ✓ Subscription added successfully\n");
            tests_passed++;
        } else {
            printf("  ✗ Subscription not found or invalid\n");
            tests_failed++;
        }
    } else {
        printf("  ✗ Failed to add subscription\n");
        tests_failed++;
    }
}

// Test: Multiple subscriptions
void test_multiple_subscriptions(void) {
    printf("Test: Multiple subscriptions (3 attributes)...\n");
    
    // Subscribe to 3 different attributes
    attribute_path_t paths[3] = {
        {1, 0x0006, 0x0000, false}, // OnOff
        {1, 0x0008, 0x0000, false}, // LevelControl
        {1, 0x0402, 0x0000, false}  // Temperature
    };
    
    uint32_t sub_ids[3];
    int success_count = 0;
    
    for (int i = 0; i < 3; i++) {
        sub_ids[i] = subscribe_handler_add(103, &paths[i], 1, 10);
        if (sub_ids[i] > 0) {
            success_count++;
        }
    }
    
    if (success_count == 3) {
        printf("  ✓ All 3 subscriptions added\n");
        tests_passed++;
    } else {
        printf("  ✗ Only %d/3 subscriptions added\n", success_count);
        tests_failed++;
    }
}

// Test: Subscription limit
void test_subscription_limit(void) {
    printf("Test: Subscription limit (11 subscriptions)...\n");
    
    // Clear ALL subscriptions first
    subscribe_handler_clear_all();
    
    attribute_path_t path = {
        .endpoint = 1,
        .cluster_id = 0x0006,
        .attribute_id = 0x0000,
        .wildcard = false
    };
    
    int success_count = 0;
    
    // Try to add 11 subscriptions (limit is 10)
    for (int i = 0; i < 11; i++) {
        uint32_t sub_id = subscribe_handler_add(104, &path, 1, 10);
        if (sub_id > 0) {
            success_count++;
        }
    }
    
    if (success_count == 10) {
        printf("  ✓ Correctly limited to 10 subscriptions\n");
        tests_passed++;
    } else {
        printf("  ✗ Expected 10 subscriptions, got %d\n", success_count);
        tests_failed++;
    }
}

// Test: Remove subscription
void test_remove_subscription(void) {
    printf("Test: Remove subscription...\n");
    
    // Clear existing and add one
    subscribe_handler_remove_all_for_session(104);
    
    attribute_path_t path = {
        .endpoint = 1,
        .cluster_id = 0x0006,
        .attribute_id = 0x0000,
        .wildcard = false
    };
    
    uint32_t sub_id = subscribe_handler_add(105, &path, 1, 10);
    
    if (sub_id == 0) {
        printf("  ✗ Failed to add subscription\n");
        tests_failed++;
        return;
    }
    
    int result = subscribe_handler_remove(105, sub_id);
    
    if (result == 0) {
        const subscription_t *sub = subscribe_handler_get_subscription(sub_id);
        if (!sub || !sub->active) {
            printf("  ✓ Subscription removed successfully\n");
            tests_passed++;
        } else {
            printf("  ✗ Subscription still active after removal\n");
            tests_failed++;
        }
    } else {
        printf("  ✗ Failed to remove subscription\n");
        tests_failed++;
    }
}

// Test: Subscription expiry (max_interval timeout)
void test_subscription_expiry(void) {
    printf("Test: Subscription expiry (max_interval)...\n");
    
    // Clear and add subscription with 5 second max interval
    subscribe_handler_remove_all_for_session(105);
    
    attribute_path_t path = {
        .endpoint = 1,
        .cluster_id = 0x0006,
        .attribute_id = 0x0000,
        .wildcard = false
    };
    
    uint32_t sub_id = subscribe_handler_add(106, &path, 1, 5);
    
    if (sub_id == 0) {
        printf("  ✗ Failed to add subscription\n");
        tests_failed++;
        return;
    }
    
    // Simulate time passing (6 seconds = 6000 ms)
    uint32_t current_time = 0;
    int reports = subscribe_handler_check_intervals(current_time);
    
    // No reports yet (last_report_time is 0)
    current_time = 6000; // 6 seconds later
    reports = subscribe_handler_check_intervals(current_time);
    
    if (reports > 0) {
        printf("  ✓ Max interval triggered report\n");
        tests_passed++;
    } else {
        printf("  ✗ Max interval did not trigger report\n");
        tests_failed++;
    }
}

int main() {
    printf("=== Matter Subscribe Handler Tests ===\n\n");
    
    // Initialize subscribe handler
    if (subscribe_handler_init() < 0) {
        printf("ERROR: Failed to initialize subscribe handler\n");
        return 1;
    }
    
    // Run tests
    test_subscribe_request_parsing();
    test_subscribe_response_encoding();
    test_add_subscription();
    test_multiple_subscriptions();
    test_subscription_limit();
    test_remove_subscription();
    test_subscription_expiry();
    
    // Summary
    printf("\n=== Test Summary ===\n");
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);
    printf("Total:  %d\n", tests_passed + tests_failed);
    
    return (tests_failed == 0) ? 0 : 1;
}
