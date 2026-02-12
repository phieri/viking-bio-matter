/*
 * onoff.c
 * Matter OnOff Cluster implementation
 * Links to existing matter_attributes system
 */

#include "onoff.h"

// Forward declaration of matter_attributes functions
// These are implemented in platform/pico_w_chip_port/matter_attributes.cpp
extern int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                                 uint32_t attribute_id, void *value);

/**
 * Initialize OnOff cluster
 */
int cluster_onoff_init(void) {
    // Attributes are already registered by platform_manager
    // Nothing to do here
    return 0;
}

/**
 * Read attribute from OnOff cluster
 */
int cluster_onoff_read(uint8_t endpoint, uint32_t attr_id,
                      attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }
    
    // OnOff cluster only exists on endpoint 1
    if (endpoint != 1) {
        return -1;
    }
    
    switch (attr_id) {
        case ATTR_ONOFF: {
            // Read OnOff attribute from matter_attributes
            bool onoff_state = false;
            if (matter_attributes_get(endpoint, CLUSTER_ONOFF, ATTR_ONOFF, &onoff_state) == 0) {
                value->bool_val = onoff_state;
                *type = ATTR_TYPE_BOOL;
                return 0;
            }
            return -1;
        }
        
        default:
            return -1;
    }
}
