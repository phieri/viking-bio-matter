/*
 * temperature.c
 * Matter TemperatureMeasurement Cluster implementation
 * Links to existing matter_attributes system
 */

#include "temperature.h"

// Forward declaration of matter_attributes functions
extern int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                                 uint32_t attribute_id, void *value);

/**
 * Initialize TemperatureMeasurement cluster
 */
int cluster_temperature_init(void) {
    // Attributes are already registered by platform_manager
    // Nothing to do here
    return 0;
}

/**
 * Read attribute from TemperatureMeasurement cluster
 */
int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                            attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }
    
    // TemperatureMeasurement cluster only exists on endpoint 1
    if (endpoint != 1) {
        return -1;
    }
    
    switch (attr_id) {
        case ATTR_MEASURED_VALUE: {
            // Read MeasuredValue attribute from matter_attributes
            // Temperature is stored in centidegrees (2500 = 25.00°C)
            int16_t temp = 0;
            if (matter_attributes_get(endpoint, CLUSTER_TEMPERATURE, 
                                     ATTR_MEASURED_VALUE, &temp) == 0) {
                value->int16_val = temp;
                *type = ATTR_TYPE_INT16;
                return 0;
            }
            return -1;
        }
        
        case ATTR_MIN_MEASURED_VALUE:
            // Return minimum temperature (0°C = 0 centidegrees)
            value->int16_val = 0;
            *type = ATTR_TYPE_INT16;
            return 0;
        
        case ATTR_MAX_MEASURED_VALUE:
            // Return maximum temperature (100°C = 10000 centidegrees)
            value->int16_val = 10000;
            *type = ATTR_TYPE_INT16;
            return 0;
        
        case ATTR_TOLERANCE:
            // Return tolerance (±1°C = ±100 centidegrees)
            value->uint16_val = 100;
            *type = ATTR_TYPE_UINT16;
            return 0;
        
        default:
            return -1;
    }
}
