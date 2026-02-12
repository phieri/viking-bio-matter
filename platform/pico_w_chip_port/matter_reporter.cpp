/*
 * matter_reporter.cpp
 * Example Matter attribute report subscriber
 * Demonstrates functional attribute reporting system
 */

#include <stdio.h>
#include <inttypes.h>
#include "matter_attributes.h"

// Callback function for attribute changes
static void attribute_report_callback(uint8_t endpoint, uint32_t cluster_id, 
                                     uint32_t attribute_id, const matter_attr_value_t *value) {
    printf("Matter Report Sent:\n");
    printf("  Endpoint: %u\n", endpoint);
    printf("  Cluster:  0x%04" PRIx32 "\n", cluster_id);
    printf("  Attribute: 0x%04" PRIx32 "\n", attribute_id);
    
    // Decode based on known cluster types
    if (cluster_id == MATTER_CLUSTER_ON_OFF) {
        printf("  Value: %s (OnOff)\n", value->bool_val ? "ON" : "OFF");
    } else if (cluster_id == MATTER_CLUSTER_LEVEL_CONTROL) {
        printf("  Value: %u%% (Level)\n", value->uint8_val);
    } else if (cluster_id == MATTER_CLUSTER_TEMPERATURE_MEASUREMENT) {
        printf("  Value: %.2f°C (Temperature)\n", value->int16_val / 100.0);
    }
    
    printf("\n");
}

// Initialize the report subscriber
extern "C" int matter_reporter_init(void) {
    int subscriber_id = matter_attributes_subscribe(attribute_report_callback);
    
    if (subscriber_id >= 0) {
        printf("Matter Reporter: Subscriber registered (ID: %d)\n", subscriber_id);
        printf("Matter Reporter: Listening for attribute changes...\n\n");
        return 0;
    } else {
        printf("Matter Reporter: ERROR - Failed to register subscriber\n");
        return -1;
    }
}
