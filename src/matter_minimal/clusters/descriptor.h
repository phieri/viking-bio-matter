/*
 * descriptor.h
 * Matter Descriptor Cluster (0x001D)
 * Based on Matter Device Library Specification Section 9.5
 * REQUIRED on endpoint 0 for all Matter devices
 */

#ifndef CLUSTER_DESCRIPTOR_H
#define CLUSTER_DESCRIPTOR_H

#include "../interaction/interaction_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Descriptor Cluster ID and Attributes
 */
#define CLUSTER_DESCRIPTOR          0x001D

#define ATTR_DEVICE_TYPE_LIST       0x0000
#define ATTR_SERVER_LIST            0x0001
#define ATTR_CLIENT_LIST            0x0002
#define ATTR_PARTS_LIST             0x0003

/**
 * Device Type Structure
 */
typedef struct {
    uint16_t device_type;
    uint8_t revision;
} device_type_entry_t;

/**
 * Initialize descriptor cluster
 * Registers cluster with the system
 * 
 * @return 0 on success, -1 on failure
 */
int cluster_descriptor_init(void);

/**
 * Read attribute from descriptor cluster
 * 
 * @param endpoint Endpoint number (should be 0)
 * @param attr_id Attribute ID
 * @param value Output value
 * @param type Output type
 * @return 0 on success, -1 if attribute not supported
 */
int cluster_descriptor_read(uint8_t endpoint, uint32_t attr_id,
                           attribute_value_t *value, attribute_type_t *type);

/**
 * Get device type list for an endpoint
 * 
 * @param endpoint Endpoint number
 * @param types Output array for device types
 * @param max_count Maximum number of entries in array
 * @param actual_count Actual number of entries returned
 * @return 0 on success, -1 on failure
 */
int cluster_descriptor_get_device_types(uint8_t endpoint, 
                                       device_type_entry_t *types,
                                       size_t max_count, size_t *actual_count);

/**
 * Get server cluster list for an endpoint
 * 
 * @param endpoint Endpoint number
 * @param clusters Output array for cluster IDs
 * @param max_count Maximum number of entries in array
 * @param actual_count Actual number of entries returned
 * @return 0 on success, -1 on failure
 */
int cluster_descriptor_get_server_list(uint8_t endpoint,
                                      uint32_t *clusters,
                                      size_t max_count, size_t *actual_count);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_DESCRIPTOR_H
