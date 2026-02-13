/*
 * network_commissioning.c
 * Network Commissioning implementation
 */

#include "network_commissioning.h"
#include "../security/pase.h"
#include "../security/session_mgr.h"
#include <string.h>
#include <stdio.h>

// Storage adapter for fabric persistence
extern int storage_adapter_write(const char *key, const uint8_t *data, size_t len);
extern int storage_adapter_read(const char *key, uint8_t *data, size_t max_len, size_t *actual_len);

// Global commissioning context
static commissioning_context_t g_commissioning_ctx;
static pase_context_t g_pase_ctx;
static bool g_commissioning_initialized = false;

// Storage key for fabric data
#define FABRIC_STORAGE_KEY "matter_fabrics"

/**
 * Initialize commissioning system
 */
int commissioning_init(void) {
    if (g_commissioning_initialized) {
        return 0;
    }
    
    // Clear context
    memset(&g_commissioning_ctx, 0, sizeof(g_commissioning_ctx));
    memset(&g_pase_ctx, 0, sizeof(g_pase_ctx));
    
    g_commissioning_ctx.state = COMMISSIONING_STATE_IDLE;
    g_commissioning_ctx.discriminator = 3840; // Default test discriminator
    
    // Try to load fabrics from storage
    if (commissioning_load_fabrics() < 0) {
        printf("Commissioning: No fabrics loaded from storage\n");
    }
    
    g_commissioning_initialized = true;
    printf("Commissioning: Initialized\n");
    return 0;
}

/**
 * Start commissioning mode
 */
int commissioning_start(const char *setup_pin, uint16_t discriminator) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    if (!setup_pin || strlen(setup_pin) != 8) {
        printf("Commissioning: Invalid setup PIN\n");
        return -1;
    }
    
    // Store commissioning parameters
    memcpy(g_commissioning_ctx.setup_pin, setup_pin, 8);
    g_commissioning_ctx.setup_pin[8] = '\0';
    g_commissioning_ctx.discriminator = discriminator;
    
    // Initialize PASE with PIN
    if (pase_init(&g_pase_ctx, setup_pin) < 0) {
        printf("Commissioning: Failed to initialize PASE\n");
        return -1;
    }
    
    g_commissioning_ctx.state = COMMISSIONING_STATE_PASE_STARTED;
    printf("Commissioning: Started with discriminator %u\n", discriminator);
    
    return 0;
}

/**
 * Handle PASE message during commissioning
 */
int commissioning_handle_pase_message(uint8_t opcode,
                                     const uint8_t *request, size_t request_len,
                                     uint8_t *response, size_t max_response_len,
                                     size_t *actual_response_len,
                                     uint8_t *session_id_out) {
    if (!g_commissioning_initialized || !request || !response || !actual_response_len) {
        return -1;
    }
    
    int result = 0;
    
    // Route based on PASE opcode
    // Matter Secure Channel Protocol opcodes:
    // 0x20 = PBKDFParamRequest
    // 0x21 = PBKDFParamResponse
    // 0x22 = PAKE1 (SPAKE2+ pA)
    // 0x23 = PAKE2 (SPAKE2+ pB)
    // 0x24 = PAKE3 (Confirmation)
    
    switch (opcode) {
        case 0x20: // PBKDFParamRequest
            printf("Commissioning: Handling PBKDFParamRequest\n");
            result = pase_handle_pbkdf_request(&g_pase_ctx,
                                              request, request_len,
                                              response, max_response_len,
                                              actual_response_len);
            break;
        
        case 0x22: // PAKE1
            printf("Commissioning: Handling PAKE1\n");
            result = pase_handle_pake1(&g_pase_ctx,
                                      request, request_len,
                                      response, max_response_len,
                                      actual_response_len);
            break;
        
        case 0x24: // PAKE3
            printf("Commissioning: Handling PAKE3\n");
            result = pase_handle_pake3(&g_pase_ctx,
                                      request, request_len,
                                      response, max_response_len,
                                      actual_response_len);
            
            // Check if PASE completed successfully
            if (result == 0 && pase_get_state(&g_pase_ctx) == PASE_STATE_COMPLETED) {
                // Derive session key
                uint8_t session_key[16];
                uint8_t session_id = 1; // Use session ID 1 for PASE
                
                if (pase_derive_session_key(&g_pase_ctx, session_id,
                                           session_key, sizeof(session_key)) == 0) {
                    // Add session to session manager
                    if (session_add(session_id, session_key, sizeof(session_key)) == 0) {
                        printf("Commissioning: PASE completed, session ID %u established\n", session_id);
                        g_commissioning_ctx.state = COMMISSIONING_STATE_COMMISSIONED;
                        
                        if (session_id_out) {
                            *session_id_out = session_id;
                        }
                        
                        // Return 1 to indicate PASE completion
                        return 1;
                    }
                }
            }
            break;
        
        default:
            printf("Commissioning: Unknown PASE opcode 0x%02X\n", opcode);
            return -1;
    }
    
    return result;
}

/**
 * Complete commissioning
 */
int commissioning_complete(uint64_t fabric_id, uint16_t vendor_id,
                          const uint8_t *root_public_key) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    // Add fabric
    if (commissioning_add_fabric(fabric_id, vendor_id, root_public_key) < 0) {
        return -1;
    }
    
    // Save to persistent storage
    commissioning_save_fabrics();
    
    g_commissioning_ctx.state = COMMISSIONING_STATE_COMMISSIONED;
    printf("Commissioning: Completed for fabric 0x%016llX\n", 
           (unsigned long long)fabric_id);
    
    return 0;
}

/**
 * Add or update a fabric
 */
int commissioning_add_fabric(uint64_t fabric_id, uint16_t vendor_id,
                            const uint8_t *root_public_key) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    // Check if fabric already exists (update case)
    for (int i = 0; i < MAX_FABRICS; i++) {
        if (g_commissioning_ctx.fabrics[i].active &&
            g_commissioning_ctx.fabrics[i].fabric_id == fabric_id) {
            // Update existing fabric
            g_commissioning_ctx.fabrics[i].vendor_id = vendor_id;
            if (root_public_key) {
                memcpy(g_commissioning_ctx.fabrics[i].root_public_key,
                       root_public_key, 65);
            }
            printf("Commissioning: Updated fabric 0x%016llX\n",
                   (unsigned long long)fabric_id);
            return 0;
        }
    }
    
    // Find empty slot for new fabric
    for (int i = 0; i < MAX_FABRICS; i++) {
        if (!g_commissioning_ctx.fabrics[i].active) {
            g_commissioning_ctx.fabrics[i].active = true;
            g_commissioning_ctx.fabrics[i].fabric_id = fabric_id;
            g_commissioning_ctx.fabrics[i].vendor_id = vendor_id;
            if (root_public_key) {
                memcpy(g_commissioning_ctx.fabrics[i].root_public_key,
                       root_public_key, 65);
            }
            g_commissioning_ctx.fabrics[i].last_seen = 0; // Will be updated by system
            g_commissioning_ctx.active_fabric_count++;
            
            printf("Commissioning: Added fabric 0x%016llX (slot %d)\n",
                   (unsigned long long)fabric_id, i);
            return 0;
        }
    }
    
    printf("Commissioning: No space for new fabric (max %d)\n", MAX_FABRICS);
    return -1; // No space
}

/**
 * Remove a fabric
 */
int commissioning_remove_fabric(uint64_t fabric_id) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    for (int i = 0; i < MAX_FABRICS; i++) {
        if (g_commissioning_ctx.fabrics[i].active &&
            g_commissioning_ctx.fabrics[i].fabric_id == fabric_id) {
            // Clear fabric slot
            memset(&g_commissioning_ctx.fabrics[i], 0, sizeof(fabric_info_t));
            g_commissioning_ctx.active_fabric_count--;
            
            // Save changes to storage
            commissioning_save_fabrics();
            
            printf("Commissioning: Removed fabric 0x%016llX\n",
                   (unsigned long long)fabric_id);
            return 0;
        }
    }
    
    return -1; // Not found
}

/**
 * Get fabric information
 */
int commissioning_get_fabric(uint64_t fabric_id, fabric_info_t *info_out) {
    if (!g_commissioning_initialized || !info_out) {
        return -1;
    }
    
    for (int i = 0; i < MAX_FABRICS; i++) {
        if (g_commissioning_ctx.fabrics[i].active &&
            g_commissioning_ctx.fabrics[i].fabric_id == fabric_id) {
            memcpy(info_out, &g_commissioning_ctx.fabrics[i], sizeof(fabric_info_t));
            return 0;
        }
    }
    
    return -1; // Not found
}

/**
 * Check if device is commissioned
 */
bool commissioning_is_commissioned(void) {
    return g_commissioning_initialized && 
           g_commissioning_ctx.active_fabric_count > 0;
}

/**
 * Get current commissioning state
 */
commissioning_state_t commissioning_get_state(void) {
    return g_commissioning_ctx.state;
}

/**
 * Get setup PIN
 */
int commissioning_get_setup_pin(char *pin_out) {
    if (!g_commissioning_initialized || !pin_out) {
        return -1;
    }
    
    memcpy(pin_out, g_commissioning_ctx.setup_pin, 9);
    return 0;
}

/**
 * Get discriminator
 */
uint16_t commissioning_get_discriminator(void) {
    return g_commissioning_ctx.discriminator;
}

/**
 * Save fabrics to persistent storage
 */
int commissioning_save_fabrics(void) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    // Serialize fabric data
    // Format: [count][fabric0][fabric1]...[fabricN]
    uint8_t buffer[sizeof(uint8_t) + MAX_FABRICS * sizeof(fabric_info_t)];
    size_t offset = 0;
    
    // Write active fabric count
    buffer[offset++] = g_commissioning_ctx.active_fabric_count;
    
    // Write each fabric
    for (int i = 0; i < MAX_FABRICS; i++) {
        if (g_commissioning_ctx.fabrics[i].active) {
            memcpy(buffer + offset, &g_commissioning_ctx.fabrics[i],
                   sizeof(fabric_info_t));
            offset += sizeof(fabric_info_t);
        }
    }
    
    // Write to storage
    if (storage_adapter_write(FABRIC_STORAGE_KEY, buffer, offset) < 0) {
        printf("Commissioning: Failed to save fabrics to storage\n");
        return -1;
    }
    
    printf("Commissioning: Saved %u fabrics to storage\n",
           g_commissioning_ctx.active_fabric_count);
    return 0;
}

/**
 * Load fabrics from persistent storage
 */
int commissioning_load_fabrics(void) {
    if (!g_commissioning_initialized) {
        return -1;
    }
    
    uint8_t buffer[sizeof(uint8_t) + MAX_FABRICS * sizeof(fabric_info_t)];
    size_t actual_len;
    
    // Read from storage
    if (storage_adapter_read(FABRIC_STORAGE_KEY, buffer, sizeof(buffer),
                            &actual_len) < 0) {
        return -1;
    }
    
    if (actual_len < 1) {
        return -1;
    }
    
    // Parse fabric data
    size_t offset = 0;
    uint8_t count = buffer[offset++];
    
    if (count > MAX_FABRICS) {
        printf("Commissioning: Invalid fabric count in storage: %u\n", count);
        return -1;
    }
    
    // Clear existing fabrics
    memset(&g_commissioning_ctx.fabrics, 0, sizeof(g_commissioning_ctx.fabrics));
    g_commissioning_ctx.active_fabric_count = 0;
    
    // Load each fabric
    int loaded = 0;
    for (int i = 0; i < MAX_FABRICS && loaded < count; i++) {
        if (offset + sizeof(fabric_info_t) <= actual_len) {
            memcpy(&g_commissioning_ctx.fabrics[i], buffer + offset,
                   sizeof(fabric_info_t));
            offset += sizeof(fabric_info_t);
            
            if (g_commissioning_ctx.fabrics[i].active) {
                g_commissioning_ctx.active_fabric_count++;
                loaded++;
            }
        }
    }
    
    printf("Commissioning: Loaded %u fabrics from storage\n",
           g_commissioning_ctx.active_fabric_count);
    
    if (g_commissioning_ctx.active_fabric_count > 0) {
        g_commissioning_ctx.state = COMMISSIONING_STATE_COMMISSIONED;
    }
    
    return 0;
}

/**
 * Reset commissioning state
 */
void commissioning_reset(void) {
    if (!g_commissioning_initialized) {
        return;
    }
    
    // Clear all fabrics
    memset(&g_commissioning_ctx.fabrics, 0, sizeof(g_commissioning_ctx.fabrics));
    g_commissioning_ctx.active_fabric_count = 0;
    g_commissioning_ctx.state = COMMISSIONING_STATE_IDLE;
    
    // Clear from storage
    commissioning_save_fabrics();
    
    // Clean up PASE context
    pase_deinit(&g_pase_ctx);
    
    printf("Commissioning: Reset to factory defaults\n");
}

/**
 * Deinitialize commissioning system
 */
void commissioning_deinit(void) {
    if (!g_commissioning_initialized) {
        return;
    }
    
    // Clean up PASE context
    pase_deinit(&g_pase_ctx);
    
    g_commissioning_initialized = false;
    printf("Commissioning: Deinitialized\n");
}
