/*
 * level_control.c
 * Matter LevelControl Cluster implementation
 * Links to existing matter_attributes system
 */

#include "level_control.h"

// Forward declaration of matter_attributes functions
extern int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                                 uint32_t attribute_id, void *value);

/**
 * Initialize LevelControl cluster
 */
int cluster_level_control_init(void) {
    // Attributes are already registered by platform_manager
    // Nothing to do here
    return 0;
}

/**
 * Read attribute from LevelControl cluster
 */
int cluster_level_control_read(uint8_t endpoint, uint32_t attr_id,
                               attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }
    
    // LevelControl cluster only exists on endpoint 1
    if (endpoint != 1) {
        return -1;
    }
    
    switch (attr_id) {
        case ATTR_CURRENT_LEVEL: {
            // Read CurrentLevel attribute from matter_attributes
            uint8_t level = 0;
            if (matter_attributes_get(endpoint, CLUSTER_LEVEL_CONTROL, 
                                     ATTR_CURRENT_LEVEL, &level) == 0) {
                value->uint8_val = level;
                *type = ATTR_TYPE_UINT8;
                return 0;
            }
            return -1;
        }
        
        case ATTR_MIN_LEVEL:
            // Return minimum level (0%)
            value->uint8_val = 0;
            *type = ATTR_TYPE_UINT8;
            return 0;
        
        case ATTR_MAX_LEVEL:
            // Return maximum level (100%)
            value->uint8_val = 100;
            *type = ATTR_TYPE_UINT8;
            return 0;
        
        default:
            return -1;
    }
}
