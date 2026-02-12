/*
 * read_handler.h
 * Matter ReadRequest/ReadResponse interaction handler
 * Based on Matter Core Specification Section 8.2
 */

#ifndef READ_HANDLER_H
#define READ_HANDLER_H

#include "interaction_model.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Maximum number of attribute paths in a single read request
 */
#define MAX_READ_PATHS 16

/**
 * Attribute Report Structure
 * Contains attribute path, data, and status for response
 */
typedef struct {
    attribute_path_t path;
    attribute_value_t value;
    attribute_type_t type;
    im_status_code_t status;
} attribute_report_t;

/**
 * Initialize read handler
 * Sets up internal state for processing read requests
 * 
 * @return 0 on success, -1 on failure
 */
int read_handler_init(void);

/**
 * Process a ReadRequest message
 * Parses the request TLV, reads requested attributes, and encodes response
 * 
 * @param request_tlv Input TLV-encoded ReadRequest
 * @param request_len Length of request
 * @param response_tlv Output buffer for TLV-encoded ReadResponse
 * @param max_response_len Maximum size of response buffer
 * @param actual_len Pointer to store actual response length
 * @return 0 on success, negative on error
 */
int read_handler_process_request(const uint8_t *request_tlv, size_t request_len,
                                 uint8_t *response_tlv, size_t max_response_len,
                                 size_t *actual_len);

/**
 * Encode a ReadResponse message
 * Encodes attribute reports into TLV format
 * 
 * @param reports Array of attribute reports
 * @param count Number of reports
 * @param response_tlv Output buffer for TLV-encoded response
 * @param max_len Maximum size of response buffer
 * @param actual_len Pointer to store actual response length
 * @return 0 on success, negative on error
 */
int read_handler_encode_response(const attribute_report_t *reports, size_t count,
                                 uint8_t *response_tlv, size_t max_len,
                                 size_t *actual_len);

#ifdef __cplusplus
}
#endif

#endif // READ_HANDLER_H
