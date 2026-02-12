/*
 * subscribe_handler.c
 * Matter SubscribeRequest/SubscribeResponse interaction handler implementation
 */

#include "subscribe_handler.h"
#include "report_generator.h"
#include "../codec/tlv.h"
#include "../codec/tlv_types.h"
#include <string.h>

// Static array of subscriptions
static subscription_t subscriptions[MAX_SUBSCRIPTIONS];
static uint32_t next_subscription_id = 1;
static bool initialized = false;

/**
 * Initialize subscription handler
 */
int subscribe_handler_init(void) {
    if (initialized) {
        return 0;
    }
    
    // Clear all subscriptions
    memset(subscriptions, 0, sizeof(subscriptions));
    next_subscription_id = 1;
    
    // Initialize report generator
    if (report_generator_init() < 0) {
        return -1;
    }
    
    initialized = true;
    return 0;
}

/**
 * Find an available subscription slot
 */
static subscription_t* find_free_slot(void) {
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (!subscriptions[i].active) {
            return &subscriptions[i];
        }
    }
    return NULL;
}

/**
 * Find subscription by ID
 */
static subscription_t* find_subscription_by_id(uint32_t subscription_id) {
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active && 
            subscriptions[i].subscription_id == subscription_id) {
            return &subscriptions[i];
        }
    }
    return NULL;
}

/**
 * Parse AttributePath from TLV structure
 */
static int parse_attribute_path(tlv_reader_t *reader, attribute_path_t *path) {
    tlv_element_t element;
    
    // AttributePath is a list with tags 0-4
    memset(path, 0, sizeof(attribute_path_t));
    
    // Read elements until we hit the end of the structure
    while (!tlv_reader_is_end(reader)) {
        if (tlv_reader_peek(reader, &element) < 0) {
            break;
        }
        
        // Check if we've hit a container end
        if (element.type == TLV_TYPE_END_OF_CONTAINER) {
            break;
        }
        
        if (tlv_reader_next(reader, &element) < 0) {
            return -1;
        }
        
        switch (element.tag) {
            case 0: // Endpoint
                path->endpoint = tlv_read_uint8(&element);
                break;
            case 2: // Cluster ID
                path->cluster_id = element.value.u32;
                break;
            case 3: // Attribute ID
                path->attribute_id = element.value.u32;
                break;
            default:
                // Ignore unknown tags
                break;
        }
    }
    
    return 0;
}

/**
 * Add a new subscription
 */
uint32_t subscribe_handler_add(uint16_t session_id, const attribute_path_t *path,
                               uint16_t min_interval, uint16_t max_interval) {
    if (!initialized || !path) {
        return 0;
    }
    
    // Find free slot
    subscription_t *sub = find_free_slot();
    if (!sub) {
        return 0; // No free slots
    }
    
    // Initialize subscription
    sub->session_id = session_id;
    sub->subscription_id = next_subscription_id++;
    sub->endpoint = path->endpoint;
    sub->cluster_id = path->cluster_id;
    sub->attribute_id = path->attribute_id;
    sub->min_interval = min_interval;
    sub->max_interval = max_interval;
    sub->last_report_time = 0;
    sub->active = true;
    
    return sub->subscription_id;
}

/**
 * Remove a subscription
 */
int subscribe_handler_remove(uint16_t session_id, uint32_t subscription_id) {
    subscription_t *sub = find_subscription_by_id(subscription_id);
    
    if (!sub || sub->session_id != session_id) {
        return -1;
    }
    
    sub->active = false;
    return 0;
}

/**
 * Remove all subscriptions for a session
 */
int subscribe_handler_remove_all_for_session(uint16_t session_id) {
    int count = 0;
    
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active && 
            subscriptions[i].session_id == session_id) {
            subscriptions[i].active = false;
            count++;
        }
    }
    
    return count;
}

/**
 * Process SubscribeRequest and generate SubscribeResponse
 */
int subscribe_handler_process_request(const uint8_t *request_tlv, size_t request_len,
                                      uint8_t *response_tlv, size_t max_response_len,
                                      size_t *actual_len, uint16_t session_id) {
    tlv_reader_t reader;
    tlv_element_t element;
    attribute_path_t paths[MAX_READ_PATHS];
    size_t path_count = 0;
    uint16_t min_interval = 1;  // Default 1 second
    uint16_t max_interval = 10; // Default 10 seconds
    bool keep_subscriptions = false;
    
    if (!request_tlv || !response_tlv || !actual_len || !initialized) {
        return -1;
    }
    
    // Initialize reader
    tlv_reader_init(&reader, request_tlv, request_len);
    
    // Parse SubscribeRequest structure
    // SubscribeRequest ::= {
    //   AttributeRequests [0]: List of AttributePath (optional)
    //   EventRequests [1]: (optional, skip)
    //   MinIntervalFloor [2]: uint16
    //   MaxIntervalCeiling [3]: uint16
    //   KeepSubscriptions [4]: bool (optional, default false)
    // }
    
    while (!tlv_reader_is_end(&reader)) {
        if (tlv_reader_next(&reader, &element) < 0) {
            break;
        }
        
        switch (element.tag) {
            case 0: // AttributeRequests (tag 0)
                if (element.type == TLV_TYPE_LIST || element.type == TLV_TYPE_ARRAY) {
                    // Parse each AttributePath in the list
                    while (!tlv_reader_is_end(&reader) && path_count < MAX_READ_PATHS) {
                        if (tlv_reader_peek(&reader, &element) < 0) {
                            break;
                        }
                        
                        if (element.type == TLV_TYPE_END_OF_CONTAINER) {
                            tlv_reader_skip(&reader);
                            break;
                        }
                        
                        if (element.type == TLV_TYPE_LIST || 
                            element.type == TLV_TYPE_STRUCTURE) {
                            tlv_reader_skip(&reader); // Skip container start
                            
                            // Parse attribute path
                            if (parse_attribute_path(&reader, &paths[path_count]) == 0) {
                                path_count++;
                            }
                            
                            // Skip to end of structure
                            while (!tlv_reader_is_end(&reader)) {
                                if (tlv_reader_peek(&reader, &element) < 0) break;
                                if (element.type == TLV_TYPE_END_OF_CONTAINER) {
                                    tlv_reader_skip(&reader);
                                    break;
                                }
                                tlv_reader_skip(&reader);
                            }
                        } else {
                            tlv_reader_skip(&reader);
                        }
                    }
                }
                break;
                
            case 2: // MinIntervalFloor (tag 2)
                min_interval = element.value.u16;
                break;
                
            case 3: // MaxIntervalCeiling (tag 3)
                max_interval = element.value.u16;
                break;
                
            case 4: // KeepSubscriptions (tag 4)
                keep_subscriptions = element.value.boolean;
                break;
                
            default:
                // Skip unknown tags
                break;
        }
    }
    
    // Clear existing subscriptions if KeepSubscriptions is false
    if (!keep_subscriptions) {
        subscribe_handler_remove_all_for_session(session_id);
    }
    
    // Create subscriptions for each path
    uint32_t first_subscription_id = 0;
    for (size_t i = 0; i < path_count; i++) {
        uint32_t sub_id = subscribe_handler_add(session_id, &paths[i], 
                                                min_interval, max_interval);
        if (sub_id > 0 && first_subscription_id == 0) {
            first_subscription_id = sub_id;
        }
    }
    
    if (first_subscription_id == 0) {
        return -1; // Failed to create any subscriptions
    }
    
    // Encode SubscribeResponse
    // SubscribeResponse ::= {
    //   SubscriptionId [0]: uint32
    //   MaxInterval [2]: uint16
    // }
    
    tlv_writer_t writer;
    tlv_writer_init(&writer, response_tlv, max_response_len);
    
    if (tlv_encode_uint32(&writer, 0, first_subscription_id) < 0) {
        return -1;
    }
    
    if (tlv_encode_uint16(&writer, 2, max_interval) < 0) {
        return -1;
    }
    
    *actual_len = tlv_writer_get_length(&writer);
    return 0;
}

/**
 * Check if any subscriptions need interval-based reports
 */
int subscribe_handler_check_intervals(uint32_t current_time) {
    if (!initialized) {
        return -1;
    }
    
    int reports_generated = 0;
    
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        subscription_t *sub = &subscriptions[i];
        
        if (!sub->active) {
            continue;
        }
        
        // Check if max_interval has elapsed
        uint32_t elapsed_sec = (current_time - sub->last_report_time) / 1000;
        
        if (elapsed_sec >= sub->max_interval) {
            // Generate report for this subscription
            attribute_path_t path = {
                .endpoint = sub->endpoint,
                .cluster_id = sub->cluster_id,
                .attribute_id = sub->attribute_id,
                .wildcard = false
            };
            
            // Note: report_generator_send_report will read current value
            // and send the report. For now, we just count it.
            // Full implementation would call report_generator here.
            
            sub->last_report_time = current_time;
            reports_generated++;
        }
    }
    
    return reports_generated;
}

/**
 * Notify subscription handler of attribute change
 */
int subscribe_handler_notify_change(uint8_t endpoint, uint32_t cluster_id,
                                    uint32_t attribute_id, uint32_t current_time) {
    if (!initialized) {
        return -1;
    }
    
    int reports_generated = 0;
    
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        subscription_t *sub = &subscriptions[i];
        
        if (!sub->active) {
            continue;
        }
        
        // Check if subscription matches this attribute
        if (sub->endpoint != endpoint ||
            sub->cluster_id != cluster_id ||
            sub->attribute_id != attribute_id) {
            continue;
        }
        
        // Check if min_interval has elapsed
        uint32_t elapsed_sec = (current_time - sub->last_report_time) / 1000;
        
        if (elapsed_sec >= sub->min_interval) {
            // Generate report for this subscription
            // Note: Full implementation would call report_generator here
            
            sub->last_report_time = current_time;
            reports_generated++;
        }
    }
    
    return reports_generated;
}

/**
 * Get subscription by ID (for testing)
 */
const subscription_t* subscribe_handler_get_subscription(uint32_t subscription_id) {
    return find_subscription_by_id(subscription_id);
}

/**
 * Get count of active subscriptions
 */
size_t subscribe_handler_get_count(void) {
    size_t count = 0;
    
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active) {
            count++;
        }
    }
    
    return count;
}

/**
 * Clear all subscriptions (for testing/debugging)
 */
int subscribe_handler_clear_all(void) {
    int count = 0;
    
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
        if (subscriptions[i].active) {
            subscriptions[i].active = false;
            count++;
        }
    }
    
    return count;
}
