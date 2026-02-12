/*
 * matter_network_subscriber.cpp
 * Network subscriber that sends attribute reports to Matter controllers
 */

#include <stdio.h>
#include "matter_attributes.h"
#include "matter_network_transport.h"

// Callback function for attribute changes (sends to network)
static void network_subscriber_callback(uint8_t endpoint, uint32_t cluster_id, 
                                       uint32_t attribute_id, const matter_attr_value_t *value) {
    // Send attribute report to all Matter controllers over WiFi
    int sent = matter_network_transport_send_report(endpoint, cluster_id, attribute_id, value);
    
    if (sent > 0) {
        // Success - report already logged by transport layer
    } else if (sent == 0) {
        // No controllers to send to (this is normal if no controllers registered)
    } else {
        printf("Matter Network: WARNING - Failed to send attribute report\n");
    }
}

extern "C" int matter_network_subscriber_init(void) {
    // Initialize network transport
    if (matter_network_transport_init() != 0) {
        printf("Matter Network: ERROR - Failed to initialize network transport\n");
        return -1;
    }
    
    // Subscribe to attribute changes
    int subscriber_id = matter_attributes_subscribe(network_subscriber_callback);
    
    if (subscriber_id >= 0) {
        printf("Matter Network: Network subscriber registered (ID: %d)\n", subscriber_id);
        printf("Matter Network: Ready to send attribute reports over WiFi\n");
        printf("Matter Network: Use matter_network_transport_add_controller() to register controllers\n\n");
        return 0;
    } else {
        printf("Matter Network: ERROR - Failed to register network subscriber\n");
        return -1;
    }
}
