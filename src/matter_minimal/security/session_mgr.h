/*
 * session_mgr.h
 * Matter session management with AES-128-CCM encryption
 */

#ifndef SESSION_MGR_H
#define SESSION_MGR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Session Manager Constants
 */
#define MAX_SESSIONS                5       // Maximum concurrent sessions
#define SESSION_KEY_LENGTH          16      // AES-128 key length
#define SESSION_TIMEOUT_SECONDS     3600    // 1 hour timeout
#define SESSION_NONCE_LENGTH        13      // CCM nonce length
#define SESSION_TAG_LENGTH          16      // CCM authentication tag length

/**
 * Session Structure
 */
typedef struct {
    uint16_t session_id;                    // Session identifier
    uint8_t encryption_key[SESSION_KEY_LENGTH];  // AES-128 key
    uint32_t message_counter;               // Message counter for nonce generation
    uint32_t last_used_time;                // Last message time (for timeout)
    bool active;                            // Session is active
} session_t;

/**
 * Initialize session manager
 * Clears all sessions and prepares for use
 * 
 * @return 0 on success, -1 on error
 */
int session_mgr_init(void);

/**
 * Create a new session with the given key
 * 
 * @param session_id Unique session identifier
 * @param key Encryption key (16 bytes)
 * @param key_len Length of key (must be 16)
 * @return 0 on success, -1 on error (session limit reached or invalid params)
 */
int session_create(uint16_t session_id, const uint8_t *key, size_t key_len);

/**
 * Encrypt data using session key
 * Uses AES-128-CCM with automatically generated nonce
 * 
 * @param session_id Session to use for encryption
 * @param plaintext Input plaintext data
 * @param plaintext_len Length of plaintext
 * @param ciphertext Output buffer for encrypted data
 * @param max_ciphertext_len Maximum size of output buffer
 * @param actual_ciphertext_len Actual length of encrypted data (includes tag)
 * @return 0 on success, -1 on error
 */
int session_encrypt(uint16_t session_id,
                   const uint8_t *plaintext, size_t plaintext_len,
                   uint8_t *ciphertext, size_t max_ciphertext_len,
                   size_t *actual_ciphertext_len);

/**
 * Decrypt data using session key
 * Uses AES-128-CCM with nonce extracted from ciphertext
 * 
 * @param session_id Session to use for decryption
 * @param ciphertext Input encrypted data (includes nonce and tag)
 * @param ciphertext_len Length of ciphertext
 * @param plaintext Output buffer for decrypted data
 * @param max_plaintext_len Maximum size of output buffer
 * @param actual_plaintext_len Actual length of decrypted data
 * @return 0 on success, -1 on error (including authentication failure)
 */
int session_decrypt(uint16_t session_id,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   uint8_t *plaintext, size_t max_plaintext_len,
                   size_t *actual_plaintext_len);

/**
 * Check if a session exists and is active
 * 
 * @param session_id Session to check
 * @return true if session exists and is active, false otherwise
 */
bool session_is_active(uint16_t session_id);

/**
 * Destroy a session
 * Zeroizes key material and marks session as inactive
 * 
 * @param session_id Session to destroy
 * @return 0 on success, -1 if session not found
 */
int session_destroy(uint16_t session_id);

/**
 * Clean up expired sessions
 * Removes sessions that have been inactive for more than SESSION_TIMEOUT_SECONDS
 * 
 * @param current_time Current time in seconds (e.g., from time_us_32())
 * @return Number of sessions cleaned up
 */
int session_cleanup_expired(uint32_t current_time);

/**
 * Get current message counter for a session
 * Used for replay protection
 * 
 * @param session_id Session to query
 * @return Current message counter, or 0 if session not found
 */
uint32_t session_get_message_counter(uint16_t session_id);

/**
 * Get number of active sessions
 * 
 * @return Number of active sessions
 */
int session_get_active_count(void);

#ifdef __cplusplus
}
#endif

#endif // SESSION_MGR_H
