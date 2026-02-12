/*
 * report_generator.h
 * Matter ReportData message generation
 * Based on Matter Core Specification Section 8.6
 */

#ifndef REPORT_GENERATOR_H
#define REPORT_GENERATOR_H

#include "interaction_model.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize report generator
 * Sets up internal state for generating reports
 * 
 * @return 0 on success, -1 on failure
 */
int report_generator_init(void);

/**
 * Send a ReportData message for a subscription
 * Reads current attribute values and generates a report
 * 
 * @param session_id Session ID
 * @param subscription_id Subscription ID triggering this report
 * @param paths Array of attribute paths to report
 * @param count Number of paths
 * @return 0 on success, negative on error
 */
int report_generator_send_report(uint16_t session_id, uint32_t subscription_id,
                                 const attribute_path_t *paths, size_t count);

/**
 * Encode a ReportData message
 * Encodes subscription ID and attribute reports into TLV format
 * 
 * @param subscription_id Subscription ID
 * @param reports Array of attribute reports (same format as ReadResponse)
 * @param count Number of reports
 * @param tlv_out Output buffer for TLV-encoded ReportData
 * @param max_len Maximum size of output buffer
 * @param actual_len Pointer to store actual output length
 * @return 0 on success, negative on error
 */
int report_generator_encode_report(uint32_t subscription_id,
                                   const attribute_report_t *reports, size_t count,
                                   uint8_t *tlv_out, size_t max_len,
                                   size_t *actual_len);

/**
 * Encode attribute reports into ReportData AttributeReports list
 * Helper function that encodes just the AttributeReports portion
 * Uses same format as ReadResponse AttributeReports
 * 
 * @param reports Array of attribute reports
 * @param count Number of reports
 * @param tlv_out Output buffer for TLV-encoded AttributeReports
 * @param max_len Maximum size of output buffer
 * @param actual_len Pointer to store actual output length
 * @return 0 on success, negative on error
 */
int report_generator_encode_attribute_reports(const attribute_report_t *reports, size_t count,
                                              uint8_t *tlv_out, size_t max_len,
                                              size_t *actual_len);

#ifdef __cplusplus
}
#endif

#endif // REPORT_GENERATOR_H
