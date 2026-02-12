/*
 * read_handler.c
 * Matter ReadRequest/ReadResponse interaction handler implementation
 */

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

/**
 * Initialize read handler
 */
int read_handler_init(void) {
    // Nothing to initialize for now
    return 0;
}

/**
 * Parse AttributePath from TLV structure
 */
static int parse_attribute_path(tlv_reader_t *reader, attribute_path_t *path) {
    tlv_element_t element;
    
    // AttributePath is a list with tags 0-4
    memset(path, 0, sizeof(attribute_path_t));
    
    // Read elements until we hit the end of the structure
    while (!tlv_reader_is_end(reader)) {
        if (tlv_reader_peek(reader, &element) < 0) {
            break;
        }
        
        // Check if we've hit a container end
        if (element.type == TLV_TYPE_END_OF_CONTAINER) {
            break;
        }
        
        if (tlv_reader_next(reader, &element) < 0) {
            return -1;
        }
        
        switch (element.tag) {
            case 0: // Endpoint
                path->endpoint = tlv_read_uint8(&element);
                break;
            case 2: // Cluster ID
                path->cluster_id = element.value.u32;
                break;
            case 3: // Attribute ID
                path->attribute_id = element.value.u32;
                break;
            default:
                // Ignore unknown tags
                break;
        }
    }
    
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
 * Process ReadRequest and generate reports
 */
int read_handler_process_request(const uint8_t *request_tlv, size_t request_len,
                                 uint8_t *response_tlv, size_t max_response_len,
                                 size_t *actual_len) {
    tlv_reader_t reader;
    tlv_element_t element;
    attribute_report_t reports[MAX_READ_PATHS];
    size_t report_count = 0;
    
    if (!request_tlv || !response_tlv || !actual_len) {
        return -1;
    }
    
    // Initialize reader
    tlv_reader_init(&reader, request_tlv, request_len);
    
    // Parse ReadRequest structure
    // ReadRequest ::= {
    //   AttributeRequests [0]: List of AttributePath
    //   EventRequests [1]: (optional, skip)
    //   EventFilters [2]: (optional, skip)
    //   FabricFiltered [3]: bool (optional, default false)
    //   DataVersionFilters [4]: (optional, skip)
    // }
    
    while (!tlv_reader_is_end(&reader) && report_count < MAX_READ_PATHS) {
        if (tlv_reader_next(&reader, &element) < 0) {
            break;
        }
        
        // Look for AttributeRequests (tag 0)
        if (element.tag == 0 && (element.type == TLV_TYPE_LIST || 
                                 element.type == TLV_TYPE_ARRAY)) {
            // Parse each AttributePath in the list
            while (!tlv_reader_is_end(&reader) && report_count < MAX_READ_PATHS) {
                if (tlv_reader_peek(&reader, &element) < 0) {
                    break;
                }
                
                if (element.type == TLV_TYPE_END_OF_CONTAINER) {
                    tlv_reader_skip(&reader);
                    break;
                }
                
                if (element.type == TLV_TYPE_LIST || 
                    element.type == TLV_TYPE_STRUCTURE) {
                    tlv_reader_skip(&reader); // Skip container start
                    
                    // Parse attribute path
                    attribute_path_t path;
                    if (parse_attribute_path(&reader, &path) == 0) {
                        // Read attribute value
                        attribute_report_t *report = &reports[report_count];
                        report->path = path;
                        
                        if (route_attribute_read(&path, &report->value, 
                                                &report->type, &report->status) == 0) {
                            report_count++;
                        } else {
                            // Add error report
                            report_count++;
                        }
                    }
                    
                    // Skip to end of structure
                    while (!tlv_reader_is_end(&reader)) {
                        if (tlv_reader_peek(&reader, &element) < 0) break;
                        if (element.type == TLV_TYPE_END_OF_CONTAINER) {
                            tlv_reader_skip(&reader);
                            break;
                        }
                        tlv_reader_skip(&reader);
                    }
                } else {
                    tlv_reader_skip(&reader);
                }
            }
        }
    }
    
    // Encode response
    return read_handler_encode_response(reports, report_count, 
                                       response_tlv, max_response_len, actual_len);
}

/**
 * Encode ReadResponse with attribute reports
 */
int read_handler_encode_response(const attribute_report_t *reports, size_t count,
                                 uint8_t *response_tlv, size_t max_len,
                                 size_t *actual_len) {
    tlv_writer_t writer;
    
    if (!reports || !response_tlv || !actual_len || count == 0) {
        return -1;
    }
    
    // Initialize writer
    tlv_writer_init(&writer, response_tlv, max_len);
    
    // Start AttributeReports list (tag 0)
    if (tlv_encode_array_start(&writer, 0) < 0) {
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
