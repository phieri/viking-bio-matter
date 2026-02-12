/*
 * subscription_bridge.cpp
 * Bridge between C++ matter_attributes and C subscribe_handler
 * Allows attribute changes to trigger subscription reports
 */

#include "../../../platform/pico_w_chip_port/matter_attributes.h"
#include "subscribe_handler.h"
#include "pico/stdlib.h"

// Forward declaration - this is called when attribute changes occur
static void subscription_attribute_callback(uint8_t endpoint, uint32_t cluster_id,
                                           uint32_t attribute_id, const matter_attr_value_t *value) {
    // Get current time in milliseconds
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    // Notify subscribe_handler of the change
    // This will check active subscriptions and generate reports if needed
    subscribe_handler_notify_change(endpoint, cluster_id, attribute_id, current_time);
}

// Initialize subscription bridge
// This should be called after both matter_attributes and subscribe_handler are initialized
extern "C" int subscription_bridge_init(void) {
    // Register callback with matter_attributes system
    int subscriber_id = matter_attributes_subscribe(subscription_attribute_callback);
    
    if (subscriber_id >= 0) {
        return 0;
    } else {
        return -1;
    }
}
