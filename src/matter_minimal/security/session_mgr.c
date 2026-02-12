/*
 * session_mgr.c
 * Matter session management with AES-128-CCM encryption
 */

#include "session_mgr.h"
#include <string.h>
#include <stdio.h>

// mbedTLS headers
#include "mbedtls/ccm.h"
#include "mbedtls/platform_util.h"

// Pico SDK
#include "pico/time.h"

/**
 * Session storage (static allocation)
 */
static session_t sessions[MAX_SESSIONS];
static bool session_mgr_initialized = false;

/**
 * Helper function to get current time in seconds
 * Inline to avoid function call overhead
 */
static inline uint32_t get_current_time_sec(void) {
    return time_us_32() / 1000000;  // Convert microseconds to seconds
}

/**
 * Find session by ID
 */
static session_t* find_session(uint16_t session_id) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active && sessions[i].session_id == session_id) {
            return &sessions[i];
        }
    }
    return NULL;
}

/**
 * Find free session slot
 */
static session_t* find_free_slot(void) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (!sessions[i].active) {
            return &sessions[i];
        }
    }
    return NULL;
}

/**
 * Generate nonce from session ID and message counter
 */
static void generate_nonce(uint16_t session_id, uint32_t message_counter, 
                          uint8_t *nonce) {
    // Nonce format: [session_id (2 bytes)] [message_counter (4 bytes)] [padding (7 bytes)]
    nonce[0] = (session_id >> 8) & 0xFF;
    nonce[1] = session_id & 0xFF;
    nonce[2] = (message_counter >> 24) & 0xFF;
    nonce[3] = (message_counter >> 16) & 0xFF;
    nonce[4] = (message_counter >> 8) & 0xFF;
    nonce[5] = message_counter & 0xFF;
    
    // Padding (zero)
    memset(nonce + 6, 0, 7);
}

int session_mgr_init(void) {
    if (session_mgr_initialized) {
        printf("Session Manager: Already initialized\n");
        return 0;
    }
    
    // Clear all sessions
    memset(sessions, 0, sizeof(sessions));
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        sessions[i].active = false;
    }
    
    session_mgr_initialized = true;
    printf("Session Manager: Initialized (max sessions: %d)\n", MAX_SESSIONS);
    
    return 0;
}

int session_create(uint16_t session_id, const uint8_t *key, size_t key_len) {
    if (!session_mgr_initialized) {
        printf("Session Manager: Not initialized\n");
        return -1;
    }
    
    if (!key || key_len != SESSION_KEY_LENGTH) {
        printf("Session Manager: Invalid key (len=%zu, expected=%d)\n", 
               key_len, SESSION_KEY_LENGTH);
        return -1;
    }
    
    // Check if session already exists
    session_t *existing = find_session(session_id);
    if (existing) {
        printf("Session Manager: Session %u already exists, updating key\n", 
               session_id);
        memcpy(existing->encryption_key, key, SESSION_KEY_LENGTH);
        existing->message_counter = 0;
        existing->last_used_time = get_current_time_sec();
        return 0;
    }
    
    // Find free slot
    session_t *slot = find_free_slot();
    if (!slot) {
        printf("Session Manager: No free slots (max %d reached)\n", MAX_SESSIONS);
        return -1;
    }
    
    // Initialize session
    slot->session_id = session_id;
    memcpy(slot->encryption_key, key, SESSION_KEY_LENGTH);
    slot->message_counter = 0;
    slot->last_used_time = get_current_time_sec();
    slot->active = true;
    
    printf("Session Manager: Created session %u\n", session_id);
    
    return 0;
}

int session_encrypt(uint16_t session_id,
                   const uint8_t *plaintext, size_t plaintext_len,
                   uint8_t *ciphertext, size_t max_ciphertext_len,
                   size_t *actual_ciphertext_len) {
    if (!session_mgr_initialized || !plaintext || !ciphertext || !actual_ciphertext_len) {
        return -1;
    }
    
    // Find session
    session_t *session = find_session(session_id);
    if (!session) {
        printf("Session Manager: Session %u not found\n", session_id);
        return -1;
    }
    
    // Check buffer size (need space for nonce + ciphertext + tag)
    size_t required_len = SESSION_NONCE_LENGTH + plaintext_len + SESSION_TAG_LENGTH;
    if (max_ciphertext_len < required_len) {
        printf("Session Manager: Buffer too small (need %zu, have %zu)\n",
               required_len, max_ciphertext_len);
        return -1;
    }
    
    // Generate nonce
    uint8_t nonce[SESSION_NONCE_LENGTH];
    generate_nonce(session_id, session->message_counter, nonce);
    
    // Initialize CCM context
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    
    int ret = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                 session->encryption_key, 
                                 SESSION_KEY_LENGTH * 8);
    if (ret != 0) {
        printf("Session Manager: Failed to set CCM key: %d\n", ret);
        mbedtls_ccm_free(&ccm);
        return -1;
    }
    
    // Encrypt: [nonce || ciphertext || tag]
    // First, copy nonce
    memcpy(ciphertext, nonce, SESSION_NONCE_LENGTH);
    
    // Encrypt and authenticate
    ret = mbedtls_ccm_encrypt_and_tag(&ccm,
                                      plaintext_len,
                                      nonce, SESSION_NONCE_LENGTH,
                                      NULL, 0,  // No additional data
                                      plaintext,
                                      ciphertext + SESSION_NONCE_LENGTH,
                                      ciphertext + SESSION_NONCE_LENGTH + plaintext_len,
                                      SESSION_TAG_LENGTH);
    
    mbedtls_ccm_free(&ccm);
    
    if (ret != 0) {
        printf("Session Manager: CCM encryption failed: %d\n", ret);
        return -1;
    }
    
    *actual_ciphertext_len = required_len;
    
    // Increment message counter
    session->message_counter++;
    session->last_used_time = get_current_time_sec();
    
    return 0;
}

int session_decrypt(uint16_t session_id,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   uint8_t *plaintext, size_t max_plaintext_len,
                   size_t *actual_plaintext_len) {
    if (!session_mgr_initialized || !ciphertext || !plaintext || !actual_plaintext_len) {
        return -1;
    }
    
    // Find session
    session_t *session = find_session(session_id);
    if (!session) {
        printf("Session Manager: Session %u not found\n", session_id);
        return -1;
    }
    
    // Check minimum size (nonce + tag at least)
    if (ciphertext_len < SESSION_NONCE_LENGTH + SESSION_TAG_LENGTH) {
        printf("Session Manager: Ciphertext too short\n");
        return -1;
    }
    
    // Extract nonce
    uint8_t nonce[SESSION_NONCE_LENGTH];
    memcpy(nonce, ciphertext, SESSION_NONCE_LENGTH);
    
    // Calculate plaintext length
    size_t encrypted_len = ciphertext_len - SESSION_NONCE_LENGTH - SESSION_TAG_LENGTH;
    
    if (max_plaintext_len < encrypted_len) {
        printf("Session Manager: Plaintext buffer too small\n");
        return -1;
    }
    
    // Initialize CCM context
    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    
    int ret = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                 session->encryption_key,
                                 SESSION_KEY_LENGTH * 8);
    if (ret != 0) {
        printf("Session Manager: Failed to set CCM key: %d\n", ret);
        mbedtls_ccm_free(&ccm);
        return -1;
    }
    
    // Decrypt and verify
    const uint8_t *encrypted_data = ciphertext + SESSION_NONCE_LENGTH;
    const uint8_t *tag = ciphertext + SESSION_NONCE_LENGTH + encrypted_len;
    
    ret = mbedtls_ccm_auth_decrypt(&ccm,
                                   encrypted_len,
                                   nonce, SESSION_NONCE_LENGTH,
                                   NULL, 0,  // No additional data
                                   encrypted_data,
                                   plaintext,
                                   tag, SESSION_TAG_LENGTH);
    
    mbedtls_ccm_free(&ccm);
    
    if (ret != 0) {
        printf("Session Manager: CCM decryption/auth failed: %d\n", ret);
        return -1;
    }
    
    *actual_plaintext_len = encrypted_len;
    
    // Update last used time
    session->last_used_time = get_current_time_sec();
    
    return 0;
}

bool session_is_active(uint16_t session_id) {
    if (!session_mgr_initialized) {
        return false;
    }
    
    return find_session(session_id) != NULL;
}

int session_destroy(uint16_t session_id) {
    if (!session_mgr_initialized) {
        return -1;
    }
    
    session_t *session = find_session(session_id);
    if (!session) {
        printf("Session Manager: Session %u not found\n", session_id);
        return -1;
    }
    
    // Zeroize sensitive data
    mbedtls_platform_zeroize(session->encryption_key, SESSION_KEY_LENGTH);
    
    // Mark as inactive
    session->active = false;
    
    printf("Session Manager: Destroyed session %u\n", session_id);
    
    return 0;
}

int session_cleanup_expired(uint32_t current_time) {
    if (!session_mgr_initialized) {
        return 0;
    }
    
    int cleaned = 0;
    
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            uint32_t age = current_time - sessions[i].last_used_time;
            if (age > SESSION_TIMEOUT_SECONDS) {
                printf("Session Manager: Cleaning up expired session %u (age=%u sec)\n",
                       sessions[i].session_id, age);
                session_destroy(sessions[i].session_id);
                cleaned++;
            }
        }
    }
    
    return cleaned;
}

uint32_t session_get_message_counter(uint16_t session_id) {
    if (!session_mgr_initialized) {
        return 0;
    }
    
    session_t *session = find_session(session_id);
    if (!session) {
        return 0;
    }
    
    return session->message_counter;
}

int session_get_active_count(void) {
    if (!session_mgr_initialized) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].active) {
            count++;
        }
    }
    
    return count;
}
