/*
 * network_commissioning.h
 * Network Commissioning implementation for Matter
 * Manages commissioning flow and fabric storage
 */

#ifndef NETWORK_COMMISSIONING_H
#define NETWORK_COMMISSIONING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Commissioning State
 */
typedef enum {
    COMMISSIONING_STATE_IDLE,           // Not commissioned
    COMMISSIONING_STATE_PASE_STARTED,   // PASE handshake in progress
    COMMISSIONING_STATE_COMMISSIONED,   // Successfully commissioned
    COMMISSIONING_STATE_ERROR           // Error state
} commissioning_state_t;

/**
 * Fabric Information
 * Stores information about a commissioned controller/fabric
 */
typedef struct {
    bool active;                        // Is this fabric slot in use?
    uint64_t fabric_id;                 // Fabric identifier
    uint16_t vendor_id;                 // Vendor ID
    uint8_t root_public_key[65];        // Root public key (P-256 point)
    uint32_t last_seen;                 // Timestamp of last communication
} fabric_info_t;

#define MAX_FABRICS 5                   // Maximum number of fabrics (limited by RAM)

/**
 * Commissioning Context
 */
typedef struct {
    commissioning_state_t state;        // Current commissioning state
    uint8_t setup_pin[9];               // Setup PIN (8 digits + null)
    uint16_t discriminator;             // Discriminator value
    fabric_info_t fabrics[MAX_FABRICS]; // Fabric storage
    uint8_t active_fabric_count;        // Number of active fabrics
} commissioning_context_t;

/**
 * Initialize commissioning system
 * Loads fabrics from persistent storage if available
 * 
 * @return 0 on success, -1 on error
 */
int commissioning_init(void);

/**
 * Start commissioning mode
 * Enables PASE protocol and makes device discoverable
 * 
 * @param setup_pin 8-digit setup PIN (null-terminated)
 * @param discriminator Discriminator value (12 bits)
 * @return 0 on success, -1 on error
 */
int commissioning_start(const char *setup_pin, uint16_t discriminator);

/**
 * Handle PASE message during commissioning
 * Routes PASE protocol messages to appropriate handlers
 * 
 * @param opcode PASE protocol opcode
 * @param request Input request buffer
 * @param request_len Length of request
 * @param response Output response buffer
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @param session_id_out Output session ID if PASE completes
 * @return 0 on success, -1 on error, 1 if PASE completed
 */
int commissioning_handle_pase_message(uint8_t opcode,
                                     const uint8_t *request, size_t request_len,
                                     uint8_t *response, size_t max_response_len,
                                     size_t *actual_response_len,
                                     uint8_t *session_id_out);

/**
 * Complete commissioning
 * Called after successful PASE to store fabric information
 * 
 * @param fabric_id Fabric identifier
 * @param vendor_id Vendor ID
 * @param root_public_key Root public key (65 bytes)
 * @return 0 on success, -1 on error
 */
int commissioning_complete(uint64_t fabric_id, uint16_t vendor_id,
                          const uint8_t *root_public_key);

/**
 * Add or update a fabric
 * 
 * @param fabric_id Fabric identifier
 * @param vendor_id Vendor ID
 * @param root_public_key Root public key (65 bytes)
 * @return 0 on success, -1 on error (no space)
 */
int commissioning_add_fabric(uint64_t fabric_id, uint16_t vendor_id,
                            const uint8_t *root_public_key);

/**
 * Remove a fabric
 * 
 * @param fabric_id Fabric identifier to remove
 * @return 0 on success, -1 if fabric not found
 */
int commissioning_remove_fabric(uint64_t fabric_id);

/**
 * Get fabric information
 * 
 * @param fabric_id Fabric identifier
 * @param info_out Output buffer for fabric info
 * @return 0 on success, -1 if fabric not found
 */
int commissioning_get_fabric(uint64_t fabric_id, fabric_info_t *info_out);

/**
 * Check if device is commissioned
 * 
 * @return true if at least one fabric is active
 */
bool commissioning_is_commissioned(void);

/**
 * Get current commissioning state
 * 
 * @return Current commissioning state
 */
commissioning_state_t commissioning_get_state(void);

/**
 * Get setup PIN
 * 
 * @param pin_out Output buffer (must be 9 bytes)
 * @return 0 on success, -1 on error
 */
int commissioning_get_setup_pin(char *pin_out);

/**
 * Get discriminator
 * 
 * @return Current discriminator value
 */
uint16_t commissioning_get_discriminator(void);

/**
 * Save fabrics to persistent storage
 * 
 * @return 0 on success, -1 on error
 */
int commissioning_save_fabrics(void);

/**
 * Load fabrics from persistent storage
 * 
 * @return 0 on success, -1 on error
 */
int commissioning_load_fabrics(void);

/**
 * Reset commissioning state
 * Clears all fabrics and returns to idle state
 */
void commissioning_reset(void);

/**
 * Deinitialize commissioning system
 */
void commissioning_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_COMMISSIONING_H
