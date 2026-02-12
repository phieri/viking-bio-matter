/*
 * descriptor.c
 * Matter Descriptor Cluster implementation
 */

#include "descriptor.h"
#include <string.h>

// Static device configuration
// Endpoint 0: Root Node (has descriptor for device structure)
// Endpoint 1: Temperature Sensor with OnOff and LevelControl

static const device_type_entry_t ep0_device_types[] = {
    { 0x0016, 1 }  // Root Node device type, revision 1
};

static const device_type_entry_t ep1_device_types[] = {
    { 0x0302, 1 }  // Temperature Sensor device type, revision 1
};

static const uint32_t ep0_server_clusters[] = {
    0x001D  // Descriptor cluster (only on endpoint 0)
};

static const uint32_t ep1_server_clusters[] = {
    0x0006,  // OnOff
    0x0008,  // LevelControl
    0x0402   // TemperatureMeasurement
};

static const uint8_t ep0_parts_list[] = {
    1  // Endpoint 1 is the only part
};

/**
 * Initialize descriptor cluster
 */
int cluster_descriptor_init(void) {
    // Nothing to initialize - using static data
    return 0;
}

/**
 * Get device type list for an endpoint
 */
int cluster_descriptor_get_device_types(uint8_t endpoint, 
                                       device_type_entry_t *types,
                                       size_t max_count, size_t *actual_count) {
    if (!types || !actual_count) {
        return -1;
    }
    
    const device_type_entry_t *src = NULL;
    size_t count = 0;
    
    switch (endpoint) {
        case 0:
            src = ep0_device_types;
            count = sizeof(ep0_device_types) / sizeof(ep0_device_types[0]);
            break;
        case 1:
            src = ep1_device_types;
            count = sizeof(ep1_device_types) / sizeof(ep1_device_types[0]);
            break;
        default:
            return -1;
    }
    
    if (count > max_count) {
        count = max_count;
    }
    
    memcpy(types, src, count * sizeof(device_type_entry_t));
    *actual_count = count;
    return 0;
}

/**
 * Get server cluster list for an endpoint
 */
int cluster_descriptor_get_server_list(uint8_t endpoint,
                                      uint32_t *clusters,
                                      size_t max_count, size_t *actual_count) {
    if (!clusters || !actual_count) {
        return -1;
    }
    
    const uint32_t *src = NULL;
    size_t count = 0;
    
    switch (endpoint) {
        case 0:
            src = ep0_server_clusters;
            count = sizeof(ep0_server_clusters) / sizeof(ep0_server_clusters[0]);
            break;
        case 1:
            src = ep1_server_clusters;
            count = sizeof(ep1_server_clusters) / sizeof(ep1_server_clusters[0]);
            break;
        default:
            return -1;
    }
    
    if (count > max_count) {
        count = max_count;
    }
    
    memcpy(clusters, src, count * sizeof(uint32_t));
    *actual_count = count;
    return 0;
}

/**
 * Read attribute from descriptor cluster
 * Note: DeviceTypeList and ServerList return just the count here,
 * actual array encoding will be handled by read_handler
 */
int cluster_descriptor_read(uint8_t endpoint, uint32_t attr_id,
                           attribute_value_t *value, attribute_type_t *type) {
    if (!value || !type) {
        return -1;
    }
    
    // Descriptor cluster only exists on endpoint 0
    if (endpoint != 0) {
        return -1;
    }
    
    switch (attr_id) {
        case ATTR_DEVICE_TYPE_LIST: {
            // Return array type - caller will need to handle array encoding
            *type = ATTR_TYPE_ARRAY;
            // Store count in value for now (simplified)
            device_type_entry_t types[4];
            size_t count;
            if (cluster_descriptor_get_device_types(endpoint, types, 4, &count) == 0) {
                value->uint8_val = (uint8_t)count;
                return 0;
            }
            return -1;
        }
        
        case ATTR_SERVER_LIST: {
            // Return array type
            *type = ATTR_TYPE_ARRAY;
            uint32_t clusters[8];
            size_t count;
            if (cluster_descriptor_get_server_list(endpoint, clusters, 8, &count) == 0) {
                value->uint8_val = (uint8_t)count;
                return 0;
            }
            return -1;
        }
        
        case ATTR_CLIENT_LIST:
            // Empty list for client clusters
            *type = ATTR_TYPE_ARRAY;
            value->uint8_val = 0;  // Count is 0
            return 0;
        
        case ATTR_PARTS_LIST: {
            // Return parts list (endpoints)
            *type = ATTR_TYPE_ARRAY;
            value->uint8_val = sizeof(ep0_parts_list);
            return 0;
        }
        
        default:
            return -1;
    }
}
