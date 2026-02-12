/*
 * matter_network_transport.h
 * Network transport for sending Matter attribute reports over WiFi
 */

#ifndef MATTER_NETWORK_TRANSPORT_H
#define MATTER_NETWORK_TRANSPORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "matter_attributes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of Matter controllers that can subscribe
#define MAX_MATTER_CONTROLLERS 4

// Matter controller information
typedef struct {
    uint32_t ip_address;        // IP address (network byte order)
    uint16_t port;              // UDP port for reports
    bool active;                // Controller is subscribed
    uint32_t last_report_time;  // Last time we sent a report (ms)
} matter_controller_t;

/**
 * Initialize Matter network transport
 * @return 0 on success, -1 on failure
 */
int matter_network_transport_init(void);

/**
 * Add a Matter controller to receive attribute reports
 * @param ip_address Controller IP address (e.g., "192.168.1.100")
 * @param port UDP port to send reports to (default: 5540)
 * @return Controller ID on success, -1 on failure
 */
int matter_network_transport_add_controller(const char *ip_address, uint16_t port);

/**
 * Remove a Matter controller
 * @param controller_id Controller ID from add_controller
 */
void matter_network_transport_remove_controller(int controller_id);

/**
 * Send an attribute report to all subscribed controllers
 * @param endpoint Endpoint number
 * @param cluster_id Cluster ID
 * @param attribute_id Attribute ID
 * @param value Attribute value
 * @return Number of controllers notified, or -1 on error
 */
int matter_network_transport_send_report(uint8_t endpoint, uint32_t cluster_id,
                                         uint32_t attribute_id, const matter_attr_value_t *value);

/**
 * Get count of active Matter controllers
 * @return Number of active controllers
 */
int matter_network_transport_get_controller_count(void);

/**
 * Set the attribute reporting interval (throttling)
 * @param interval_ms Minimum milliseconds between reports (0 = no throttling)
 */
void matter_network_transport_set_report_interval(uint32_t interval_ms);

/**
 * Process network transport tasks (call periodically)
 */
void matter_network_transport_task(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_NETWORK_TRANSPORT_H
