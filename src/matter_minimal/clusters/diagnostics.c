/*
 * diagnostics.c
 * Matter General Diagnostics Cluster implementation
 * Links to existing matter_attributes system
 */

#include "diagnostics.h"

// Forward declaration of matter_attributes functions
extern int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                                 uint32_t attribute_id, void *value);

/**
 * Initialize diagnostics cluster
 */
int cluster_diagnostics_init(void) {
    // Attributes are already registered by platform_manager
    // Nothing to do here
    return 0;
}

/**
 * Read attribute from diagnostics cluster
 */
int cluster_diagnostics_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }
    
    // Diagnostics cluster only exists on endpoint 1
    if (endpoint != 1) {
        return -1;
    }
    
    switch (attr_id) {
        case ATTR_TOTAL_OPERATIONAL_HOURS: {
            // Read TotalOperationalHours attribute from matter_attributes
            // Hours accumulated when flame is on
            uint32_t hours = 0;
            if (matter_attributes_get(endpoint, CLUSTER_DIAGNOSTICS, 
                                     ATTR_TOTAL_OPERATIONAL_HOURS, &hours) == 0) {
                value->uint32_val = hours;
                *type = ATTR_TYPE_UINT32;
                return 0;
            }
            return -1;
        }
        
        case ATTR_DEVICE_ENABLED_STATE: {
            // Read DeviceEnabledState attribute from matter_attributes
            // 0 = Disabled, 1 = Enabled
            uint8_t state = 0;
            if (matter_attributes_get(endpoint, CLUSTER_DIAGNOSTICS,
                                     ATTR_DEVICE_ENABLED_STATE, &state) == 0) {
                value->uint8_val = state;
                *type = ATTR_TYPE_UINT8;
                return 0;
            }
            return -1;
        }
        
        case ATTR_NUMBER_OF_ACTIVE_FAULTS: {
            // Read NumberOfActiveFaults attribute from matter_attributes
            uint8_t faults = 0;
            if (matter_attributes_get(endpoint, CLUSTER_DIAGNOSTICS,
                                     ATTR_NUMBER_OF_ACTIVE_FAULTS, &faults) == 0) {
                value->uint8_val = faults;
                *type = ATTR_TYPE_UINT8;
                return 0;
            }
            return -1;
        }
        
        default:
            return -1;
    }
}
