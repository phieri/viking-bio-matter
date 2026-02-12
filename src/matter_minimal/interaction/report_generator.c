/*
 * report_generator.c
 * Matter ReportData message generation implementation
 */

#include "report_generator.h"
#include "read_handler.h"
#include "../codec/tlv.h"
#include "../codec/tlv_types.h"
#include <string.h>

// Forward declarations of cluster read functions
extern int cluster_descriptor_read(uint8_t endpoint, uint32_t attr_id, 
                                   attribute_value_t *value, attribute_type_t *type);
extern int cluster_onoff_read(uint8_t endpoint, uint32_t attr_id,
                             attribute_value_t *value, attribute_type_t *type);
extern int cluster_level_control_read(uint8_t endpoint, uint32_t attr_id,
                                      attribute_value_t *value, attribute_type_t *type);
extern int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id,
                                   attribute_value_t *value, attribute_type_t *type);

static bool initialized = false;

/**
 * Initialize report generator
 */
int report_generator_init(void) {
    if (initialized) {
        return 0;
    }
    
    initialized = true;
    return 0;
}

/**
 * Route attribute read to appropriate cluster handler
 */
static int route_attribute_read(const attribute_path_t *path,
                               attribute_value_t *value,
                               attribute_type_t *type,
                               im_status_code_t *status) {
    int result;
    
    // Route to appropriate cluster
    switch (path->cluster_id) {
        case 0x001D: // Descriptor
            result = cluster_descriptor_read(path->endpoint, path->attribute_id, value, type);
            break;
            
        case 0x0006: // OnOff
            result = cluster_onoff_read(path->endpoint, path->attribute_id, value, type);
            break;
            
        case 0x0008: // LevelControl
            result = cluster_level_control_read(path->endpoint, path->attribute_id, value, type);
            break;
            
        case 0x0402: // TemperatureMeasurement
            result = cluster_temperature_read(path->endpoint, path->attribute_id, value, type);
            break;
            
        default:
            *status = IM_STATUS_UNSUPPORTED_CLUSTER;
            return -1;
    }
    
    if (result < 0) {
        *status = IM_STATUS_UNSUPPORTED_ATTRIBUTE;
        return -1;
    }
    
    *status = IM_STATUS_SUCCESS;
    return 0;
}

/**
 * Encode attribute reports into AttributeReports list
 * Same format as ReadResponse
 */
int report_generator_encode_attribute_reports(const attribute_report_t *reports, size_t count,
                                              uint8_t *tlv_out, size_t max_len,
                                              size_t *actual_len) {
    tlv_writer_t writer;
    
    if (!reports || !tlv_out || !actual_len || count == 0) {
        return -1;
    }
    
    // Initialize writer
    tlv_writer_init(&writer, tlv_out, max_len);
    
    // Start AttributeReports list (tag 1 for ReportData)
    if (tlv_encode_array_start(&writer, 1) < 0) {
        return -1;
    }
    
    // Encode each attribute report
    for (size_t i = 0; i < count; i++) {
        const attribute_report_t *report = &reports[i];
        
        // Start AttributeReport structure (anonymous tag in array)
        if (tlv_encode_structure_start(&writer, 0xFF) < 0) {
            return -1;
        }
        
        // Start AttributeStatus (tag 0) or AttributeData (tag 1)
        if (report->status != IM_STATUS_SUCCESS) {
            // Encode AttributeStatus for errors
            if (tlv_encode_structure_start(&writer, 0) < 0) {
                return -1;
            }
            
            // AttributePath (tag 0)
            if (tlv_encode_structure_start(&writer, 0) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, report->path.endpoint) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 2, report->path.cluster_id) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 3, report->path.attribute_id) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            // Status (tag 1)
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, (uint8_t)report->status) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
        } else {
            // Encode AttributeData for success
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            
            // DataVersion (tag 0) - optional, use 0
            if (tlv_encode_uint32(&writer, 0, 0) < 0) {
                return -1;
            }
            
            // AttributePath (tag 1)
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, report->path.endpoint) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 2, report->path.cluster_id) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 3, report->path.attribute_id) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            // Data (tag 2) - encode attribute value
            switch (report->type) {
                case ATTR_TYPE_BOOL:
                    if (tlv_encode_bool(&writer, 2, report->value.bool_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT8:
                    if (tlv_encode_uint8(&writer, 2, report->value.uint8_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_INT16:
                    if (tlv_encode_int16(&writer, 2, report->value.int16_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT16:
                    if (tlv_encode_uint16(&writer, 2, report->value.uint16_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT32:
                    if (tlv_encode_uint32(&writer, 2, report->value.uint32_val) < 0) {
                        return -1;
                    }
                    break;
                default:
                    // Unsupported type
                    return -1;
            }
            
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
        }
        
        // End AttributeReport structure
        if (tlv_encode_container_end(&writer) < 0) {
            return -1;
        }
    }
    
    // End AttributeReports list
    if (tlv_encode_container_end(&writer) < 0) {
        return -1;
    }
    
    *actual_len = tlv_writer_get_length(&writer);
    return 0;
}

/**
 * Encode ReportData message
 */
int report_generator_encode_report(uint32_t subscription_id,
                                   const attribute_report_t *reports, size_t count,
                                   uint8_t *tlv_out, size_t max_len,
                                   size_t *actual_len) {
    tlv_writer_t writer;
    
    if (!reports || !tlv_out || !actual_len || count == 0 || !initialized) {
        return -1;
    }
    
    // Initialize writer
    tlv_writer_init(&writer, tlv_out, max_len);
    
    // ReportData structure:
    // {
    //   SubscriptionId [0]: uint32
    //   AttributeReports [1]: Array of AttributeReport
    //   EventReports [2]: (optional, skip)
    //   MoreChunkedMessages [3]: bool (optional)
    //   SuppressResponse [4]: bool (optional)
    // }
    
    // Encode SubscriptionId (tag 0)
    if (tlv_encode_uint32(&writer, 0, subscription_id) < 0) {
        return -1;
    }
    
    // Encode AttributeReports inline (same format as report_generator_encode_attribute_reports)
    // Start AttributeReports list (tag 1)
    if (tlv_encode_array_start(&writer, 1) < 0) {
        return -1;
    }
    
    // Encode each attribute report
    for (size_t i = 0; i < count; i++) {
        const attribute_report_t *report = &reports[i];
        
        // Start AttributeReport structure (anonymous tag in array)
        if (tlv_encode_structure_start(&writer, 0xFF) < 0) {
            return -1;
        }
        
        // Check if this is an error report or success report
        if (report->status != IM_STATUS_SUCCESS) {
            // Encode AttributeStatus for errors (tag 0)
            if (tlv_encode_structure_start(&writer, 0) < 0) {
                return -1;
            }
            
            // AttributePath (tag 0)
            if (tlv_encode_structure_start(&writer, 0) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, report->path.endpoint) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 2, report->path.cluster_id) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 3, report->path.attribute_id) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            // Status (tag 1)
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, (uint8_t)report->status) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
        } else {
            // Encode AttributeData for success (tag 1)
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            
            // DataVersion (tag 0) - optional, use 0
            if (tlv_encode_uint32(&writer, 0, 0) < 0) {
                return -1;
            }
            
            // AttributePath (tag 1)
            if (tlv_encode_structure_start(&writer, 1) < 0) {
                return -1;
            }
            if (tlv_encode_uint8(&writer, 0, report->path.endpoint) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 2, report->path.cluster_id) < 0) {
                return -1;
            }
            if (tlv_encode_uint32(&writer, 3, report->path.attribute_id) < 0) {
                return -1;
            }
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
            
            // Data (tag 2) - encode attribute value
            switch (report->type) {
                case ATTR_TYPE_BOOL:
                    if (tlv_encode_bool(&writer, 2, report->value.bool_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT8:
                    if (tlv_encode_uint8(&writer, 2, report->value.uint8_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_INT16:
                    if (tlv_encode_int16(&writer, 2, report->value.int16_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT16:
                    if (tlv_encode_uint16(&writer, 2, report->value.uint16_val) < 0) {
                        return -1;
                    }
                    break;
                case ATTR_TYPE_UINT32:
                    if (tlv_encode_uint32(&writer, 2, report->value.uint32_val) < 0) {
                        return -1;
                    }
                    break;
                default:
                    // Unsupported type
                    return -1;
            }
            
            if (tlv_encode_container_end(&writer) < 0) {
                return -1;
            }
        }
        
        // End AttributeReport structure
        if (tlv_encode_container_end(&writer) < 0) {
            return -1;
        }
    }
    
    // End AttributeReports list
    if (tlv_encode_container_end(&writer) < 0) {
        return -1;
    }
    
    *actual_len = tlv_writer_get_length(&writer);
    return 0;
}

/**
 * Send a ReportData message
 */
int report_generator_send_report(uint16_t session_id, uint32_t subscription_id,
                                 const attribute_path_t *paths, size_t count) {
    if (!initialized || !paths || count == 0) {
        return -1;
    }
    
    // Build attribute reports by reading current values
    attribute_report_t reports[MAX_READ_PATHS];
    size_t report_count = 0;
    
    for (size_t i = 0; i < count && report_count < MAX_READ_PATHS; i++) {
        attribute_report_t *report = &reports[report_count];
        report->path = paths[i];
        
        if (route_attribute_read(&paths[i], &report->value, 
                               &report->type, &report->status) == 0) {
            report_count++;
        } else {
            // Add error report
            report_count++;
        }
    }
    
    // Encode ReportData
    uint8_t report_tlv[1024];
    size_t report_len;
    
    if (report_generator_encode_report(subscription_id, reports, report_count,
                                       report_tlv, sizeof(report_tlv),
                                       &report_len) < 0) {
        return -1;
    }
    
    // Note: In a full implementation, this would:
    // 1. Encrypt the payload using session_encrypt(session_id, ...)
    // 2. Send via matter_protocol_send() or matter_transport_send()
    // For now, we just return success
    
    (void)session_id; // Suppress unused warning
    
    return 0;
}
