/*
 * level_control.h
 * Matter LevelControl Cluster (0x0008)
 * Based on Matter Application Cluster Specification Section 1.6
 */

#ifndef CLUSTER_LEVEL_CONTROL_H
#define CLUSTER_LEVEL_CONTROL_H

#include "../interaction/interaction_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * LevelControl Cluster ID and Attributes
 */
#define CLUSTER_LEVEL_CONTROL       0x0008
#define ATTR_CURRENT_LEVEL          0x0000
#define ATTR_MIN_LEVEL              0x0002
#define ATTR_MAX_LEVEL              0x0003

/**
 * Initialize LevelControl cluster
 * Registers attributes with matter_attributes system
 * 
 * @return 0 on success, -1 on failure
 */
int cluster_level_control_init(void);

/**
 * Read attribute from LevelControl cluster
 * 
 * @param endpoint Endpoint number
 * @param attr_id Attribute ID
 * @param value Output value
 * @param type Output type
 * @return 0 on success, -1 if attribute not supported
 */
int cluster_level_control_read(uint8_t endpoint, uint32_t attr_id,
                               attribute_value_t *value, attribute_type_t *type);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_LEVEL_CONTROL_H
