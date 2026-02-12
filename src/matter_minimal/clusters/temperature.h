/*
 * temperature.h
 * Matter TemperatureMeasurement Cluster (0x0402)
 * Based on Matter Application Cluster Specification Section 2.3
 */

#ifndef CLUSTER_TEMPERATURE_H
#define CLUSTER_TEMPERATURE_H

#include "../interaction/interaction_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TemperatureMeasurement Cluster ID and Attributes
 */
#define CLUSTER_TEMPERATURE         0x0402
#define ATTR_MEASURED_VALUE         0x0000
#define ATTR_MIN_MEASURED_VALUE     0x0001
#define ATTR_MAX_MEASURED_VALUE     0x0002
#define ATTR_TOLERANCE              0x0003

/**
 * Initialize TemperatureMeasurement cluster
 * Registers attributes with matter_attributes system
 * 
 * @return 0 on success, -1 on failure
 */
int cluster_temperature_init(void);

/**
 * Read attribute from TemperatureMeasurement cluster
 * 
 * @param endpoint Endpoint number
 * @param attr_id Attribute ID
 * @param value Output value
 * @param type Output type
 * @return 0 on success, -1 if attribute not supported
 */
int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_TEMPERATURE_H
