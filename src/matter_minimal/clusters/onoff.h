/*
 * onoff.h
 * Matter OnOff Cluster (0x0006)
 * Based on Matter Application Cluster Specification Section 1.5
 */

#ifndef CLUSTER_ONOFF_H
#define CLUSTER_ONOFF_H

#include "../interaction/interaction_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * OnOff Cluster ID and Attributes
 */
#define CLUSTER_ONOFF               0x0006
#define ATTR_ONOFF                  0x0000

/**
 * Initialize OnOff cluster
 * Registers attributes with matter_attributes system
 * 
 * @return 0 on success, -1 on failure
 */
int cluster_onoff_init(void);

/**
 * Read attribute from OnOff cluster
 * 
 * @param endpoint Endpoint number
 * @param attr_id Attribute ID
 * @param value Output value
 * @param type Output type
 * @return 0 on success, -1 if attribute not supported
 */
int cluster_onoff_read(uint8_t endpoint, uint32_t attr_id,
                      attribute_value_t *value, attribute_type_t *type);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_ONOFF_H
