/*
 * pase.h
 * Password Authenticated Session Establishment (PASE) protocol
 * Implements SPAKE2+ per Matter Core Specification Section 4.12.1
 */

#ifndef PASE_H
#define PASE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * PASE Protocol Constants
 */
#define PASE_PIN_LENGTH             8       // 8-digit PIN
#define PASE_SALT_LENGTH            32      // Salt length for PBKDF2
#define PASE_SESSION_KEY_LENGTH     16      // AES-128 key length
#define PASE_SPAKE2_POINT_LENGTH    65      // Uncompressed P-256 point (0x04 + X + Y)
#define PASE_PBKDF2_ITERATIONS      2000    // PBKDF2 iterations (Matter default)
#define PASE_VERIFIER_LENGTH        97      // w0 (32) + L (65) = 97 bytes

/**
 * PASE State Machine
 */
typedef enum {
    PASE_STATE_IDLE,                // Not initialized
    PASE_STATE_INITIALIZED,         // Initialized with PIN
    PASE_STATE_PBKDF_REQ_RECEIVED,  // Received PBKDFParamRequest
    PASE_STATE_PBKDF_RESP_SENT,     // Sent PBKDFParamResponse
    PASE_STATE_PAKE1_RECEIVED,      // Received PAKE1 (prover pA)
    PASE_STATE_PAKE2_SENT,          // Sent PAKE2 (verifier pB)
    PASE_STATE_PAKE3_RECEIVED,      // Received PAKE3 (confirmation)
    PASE_STATE_COMPLETED,           // Session established
    PASE_STATE_ERROR                // Error state
} pase_state_t;

/**
 * PASE Context Structure
 * Stores state for PASE protocol execution
 */
typedef struct {
    pase_state_t state;             // Current state
    uint8_t setup_pin[PASE_PIN_LENGTH + 1];  // Setup PIN (8 digits + null terminator)
    uint8_t salt[PASE_SALT_LENGTH]; // Random salt for PBKDF2
    uint8_t w0[32];                 // PBKDF2 derived key (w0)
    uint8_t w1[32];                 // PBKDF2 derived key (w1)
    uint8_t L[PASE_SPAKE2_POINT_LENGTH];  // Verifier point L
    uint8_t pA[PASE_SPAKE2_POINT_LENGTH]; // Prover's public point
    uint8_t pB[PASE_SPAKE2_POINT_LENGTH]; // Verifier's public point
    uint8_t Z[PASE_SPAKE2_POINT_LENGTH];  // Shared secret point
    uint8_t V[PASE_SPAKE2_POINT_LENGTH];  // Shared secret point
    uint8_t Ka[32];                 // Confirmation key
    uint8_t Ke[32];                 // Encryption key
    uint8_t session_id;             // Session ID for this PASE exchange
    uint32_t pbkdf2_iterations;     // Negotiated PBKDF2 iterations
} pase_context_t;

/**
 * Initialize PASE protocol with setup PIN
 * 
 * @param ctx PASE context to initialize
 * @param setup_pin 8-digit setup PIN (null-terminated string)
 * @return 0 on success, -1 on error
 */
int pase_init(pase_context_t *ctx, const char *setup_pin);

/**
 * Handle PBKDFParamRequest message
 * Generates salt and sends PBKDFParamResponse
 * 
 * @param ctx PASE context
 * @param request Input request buffer
 * @param request_len Length of request
 * @param response Output response buffer
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int pase_handle_pbkdf_request(pase_context_t *ctx,
                               const uint8_t *request, size_t request_len,
                               uint8_t *response, size_t max_response_len,
                               size_t *actual_response_len);

/**
 * Handle PAKE1 message (prover sends pA)
 * Verifies prover's public point and generates verifier's point pB
 * 
 * @param ctx PASE context
 * @param request Input request buffer (contains pA)
 * @param request_len Length of request
 * @param response Output response buffer (will contain pB)
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int pase_handle_pake1(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len);

/**
 * Handle PAKE2 message (verifier sends pB)
 * This is called after sending our pB to process any responses
 * 
 * @param ctx PASE context
 * @param request Input request buffer
 * @param request_len Length of request
 * @param response Output response buffer
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int pase_handle_pake2(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len);

/**
 * Handle PAKE3 message (confirmation)
 * Validates confirmation tag and completes PASE
 * 
 * @param ctx PASE context
 * @param request Input request buffer (contains confirmation tag)
 * @param request_len Length of request
 * @param response Output response buffer (optional)
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int pase_handle_pake3(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len);

/**
 * Derive session key after successful PASE
 * Uses HKDF to derive session encryption key from shared secret
 * 
 * @param ctx PASE context (must be in COMPLETED state)
 * @param session_id Session ID for this session
 * @param key_out Output buffer for session key (16 bytes)
 * @param key_len Length of key buffer (must be 16)
 * @return 0 on success, -1 on error
 */
int pase_derive_session_key(pase_context_t *ctx, uint8_t session_id,
                            uint8_t *key_out, size_t key_len);

/**
 * Clean up PASE context
 * Zeroizes sensitive data
 * 
 * @param ctx PASE context to clean up
 */
void pase_deinit(pase_context_t *ctx);

/**
 * Get current PASE state
 * 
 * @param ctx PASE context
 * @return Current state
 */
pase_state_t pase_get_state(const pase_context_t *ctx);

#ifdef __cplusplus
}
#endif

#endif // PASE_H
