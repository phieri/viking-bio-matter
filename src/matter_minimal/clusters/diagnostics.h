/*
 * diagnostics.h
 * Matter General Diagnostics Cluster (0x0033)
 * Based on Matter Core Specification Section 11.11
 */

#ifndef CLUSTER_DIAGNOSTICS_H
#define CLUSTER_DIAGNOSTICS_H

#include "../interaction/interaction_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * General Diagnostics Cluster ID and Attributes
 */
#define CLUSTER_DIAGNOSTICS                 0x0033

#define ATTR_TOTAL_OPERATIONAL_HOURS        0x0003
#define ATTR_DEVICE_ENABLED_STATE           0x0005
#define ATTR_NUMBER_OF_ACTIVE_FAULTS        0x0001

/**
 * Initialize diagnostics cluster
 * Registers attributes with matter_attributes system
 * 
 * @return 0 on success, -1 on failure
 */
int cluster_diagnostics_init(void);

/**
 * Read attribute from diagnostics cluster
 * 
 * @param endpoint Endpoint number
 * @param attr_id Attribute ID
 * @param value Output value
 * @param type Output type
 * @return 0 on success, -1 if attribute not supported
 */
int cluster_diagnostics_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_DIAGNOSTICS_H
