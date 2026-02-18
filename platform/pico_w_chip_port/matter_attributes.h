/*
 * matter_attributes.h
 * Matter attribute storage and reporting system
 */

#ifndef MATTER_ATTRIBUTES_H
#define MATTER_ATTRIBUTES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Maximum number of attribute subscribers
#define MATTER_MAX_SUBSCRIBERS 4

// Matter cluster IDs
#define MATTER_CLUSTER_ON_OFF 0x0006
#define MATTER_CLUSTER_LEVEL_CONTROL 0x0008
#define MATTER_CLUSTER_TEMPERATURE_MEASUREMENT 0x0402
#define MATTER_CLUSTER_DIAGNOSTICS 0x0033

// Matter attribute IDs
#define MATTER_ATTR_ON_OFF 0x0000
#define MATTER_ATTR_CURRENT_LEVEL 0x0000
#define MATTER_ATTR_MEASURED_VALUE 0x0000
#define MATTER_ATTR_TOTAL_OPERATIONAL_HOURS 0x0003
#define MATTER_ATTR_DEVICE_ENABLED_STATE 0x0005
#define MATTER_ATTR_NUMBER_OF_ACTIVE_FAULTS 0x0001

// Attribute data types (simplified Matter types)
typedef enum {
    MATTER_TYPE_BOOL,
    MATTER_TYPE_UINT8,
    MATTER_TYPE_INT16,
    MATTER_TYPE_UINT32
} matter_attr_type_t;

// Attribute value union
typedef union {
    bool bool_val;
    uint8_t uint8_val;
    int16_t int16_val;
    uint32_t uint32_val;
} matter_attr_value_t;

// Attribute descriptor
typedef struct {
    uint32_t cluster_id;
    uint32_t attribute_id;
    uint8_t endpoint;
    matter_attr_type_t type;
    matter_attr_value_t value;
    bool dirty;  // Attribute has changed since last report
} matter_attribute_t;

// Subscriber callback for attribute changes
typedef void (*matter_subscriber_callback_t)(uint8_t endpoint, uint32_t cluster_id, 
                                             uint32_t attribute_id, const matter_attr_value_t *value);

/**
 * Initialize the Matter attribute system
 * @return 0 on success, -1 on failure
 */
int matter_attributes_init(void);

/**
 * Register an attribute with the system
 * @param endpoint Endpoint number
 * @param cluster_id Cluster ID
 * @param attribute_id Attribute ID
 * @param type Attribute data type
 * @param initial_value Initial value
 * @return 0 on success, -1 on failure
 */
int matter_attributes_register(uint8_t endpoint, uint32_t cluster_id, 
                               uint32_t attribute_id, matter_attr_type_t type,
                               const matter_attr_value_t *initial_value);

/**
 * Update an attribute value
 * Marks the attribute as dirty if value changed
 * @param endpoint Endpoint number
 * @param cluster_id Cluster ID
 * @param attribute_id Attribute ID
 * @param value New value
 * @return 0 on success, -1 if attribute not found
 */
int matter_attributes_update(uint8_t endpoint, uint32_t cluster_id,
                            uint32_t attribute_id, const matter_attr_value_t *value);

/**
 * Get an attribute value
 * @param endpoint Endpoint number
 * @param cluster_id Cluster ID
 * @param attribute_id Attribute ID
 * @param value Output value
 * @return 0 on success, -1 if attribute not found
 */
int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                         uint32_t attribute_id, matter_attr_value_t *value);

/**
 * Subscribe to attribute changes
 * @param callback Callback function to call when attributes change
 * @return Subscriber ID on success, -1 on failure
 */
int matter_attributes_subscribe(matter_subscriber_callback_t callback);

/**
 * Unsubscribe from attribute changes
 * @param subscriber_id Subscriber ID from subscribe call
 */
void matter_attributes_unsubscribe(int subscriber_id);

/**
 * Process dirty attributes and notify subscribers
 * Call this periodically to send attribute reports
 */
void matter_attributes_process_reports(void);

/**
 * Get count of registered attributes
 * @return Number of registered attributes
 */
size_t matter_attributes_count(void);

/**
 * Clear all attributes (for shutdown/reset)
 */
void matter_attributes_clear(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_ATTRIBUTES_H
