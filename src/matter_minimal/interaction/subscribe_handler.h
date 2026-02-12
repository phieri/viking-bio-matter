/*
 * subscribe_handler.h
 * Matter SubscribeRequest/SubscribeResponse interaction handler
 * Based on Matter Core Specification Section 8.5
 */

#ifndef SUBSCRIBE_HANDLER_H
#define SUBSCRIBE_HANDLER_H

#include "interaction_model.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum number of concurrent subscriptions
 */
#define MAX_SUBSCRIPTIONS 10

/**
 * Subscription Structure
 * Stores state for a single attribute subscription
 */
typedef struct {
    uint16_t session_id;         // Session ID for this subscription
    uint32_t subscription_id;    // Unique subscription ID
    uint8_t endpoint;            // Endpoint number
    uint32_t cluster_id;         // Cluster ID
    uint32_t attribute_id;       // Attribute ID
    uint16_t min_interval;       // Minimum interval in seconds
    uint16_t max_interval;       // Maximum interval in seconds
    uint32_t last_report_time;   // Last report time (milliseconds)
    bool active;                 // Subscription is active
} subscription_t;

/**
 * Initialize subscription handler
 * Sets up internal state for managing subscriptions
 * 
 * @return 0 on success, -1 on failure
 */
int subscribe_handler_init(void);

/**
 * Process a SubscribeRequest message
 * Parses the request TLV, creates subscriptions, and encodes response
 * 
 * @param request_tlv Input TLV-encoded SubscribeRequest
 * @param request_len Length of request
 * @param response_tlv Output buffer for TLV-encoded SubscribeResponse
 * @param max_response_len Maximum size of response buffer
 * @param actual_len Pointer to store actual response length
 * @param session_id Session ID for this subscription
 * @return 0 on success, negative on error
 */
int subscribe_handler_process_request(const uint8_t *request_tlv, size_t request_len,
                                      uint8_t *response_tlv, size_t max_response_len,
                                      size_t *actual_len, uint16_t session_id);

/**
 * Add a new subscription
 * Creates a subscription for the specified attribute path
 * 
 * @param session_id Session ID
 * @param path Attribute path to subscribe to
 * @param min_interval Minimum reporting interval in seconds
 * @param max_interval Maximum reporting interval in seconds
 * @return Subscription ID on success, 0 on failure
 */
uint32_t subscribe_handler_add(uint16_t session_id, const attribute_path_t *path,
                               uint16_t min_interval, uint16_t max_interval);

/**
 * Remove a subscription
 * 
 * @param session_id Session ID
 * @param subscription_id Subscription ID to remove
 * @return 0 on success, -1 if not found
 */
int subscribe_handler_remove(uint16_t session_id, uint32_t subscription_id);

/**
 * Remove all subscriptions for a session
 * Called when session closes to clean up
 * 
 * @param session_id Session ID
 * @return Number of subscriptions removed
 */
int subscribe_handler_remove_all_for_session(uint16_t session_id);

/**
 * Check if any subscriptions need interval-based reports
 * Should be called periodically from main protocol task
 * 
 * @param current_time Current time in milliseconds
 * @return Number of reports generated, -1 on error
 */
int subscribe_handler_check_intervals(uint32_t current_time);

/**
 * Notify subscription handler of attribute change
 * Checks if any subscriptions match and generates reports if needed
 * 
 * @param endpoint Endpoint that changed
 * @param cluster_id Cluster ID that changed
 * @param attribute_id Attribute ID that changed
 * @param current_time Current time in milliseconds
 * @return Number of reports generated, -1 on error
 */
int subscribe_handler_notify_change(uint8_t endpoint, uint32_t cluster_id,
                                    uint32_t attribute_id, uint32_t current_time);

/**
 * Get subscription by ID
 * Internal function for testing/debugging
 * 
 * @param subscription_id Subscription ID
 * @return Pointer to subscription or NULL if not found
 */
const subscription_t* subscribe_handler_get_subscription(uint32_t subscription_id);

/**
 * Get count of active subscriptions
 * 
 * @return Number of active subscriptions
 */
size_t subscribe_handler_get_count(void);

#ifdef __cplusplus
}
#endif

#endif // SUBSCRIBE_HANDLER_H
