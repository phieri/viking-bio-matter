/*
 * test_session_mgr.c
 * Unit tests for session management
 */

#include "session_mgr.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test helper macros
#define TEST(name) printf("\nRunning test: %s\n", name)
#define PASS() printf("  ✓ PASSED\n")
#define FAIL(msg) do { printf("  ✗ FAILED: %s\n", msg); return; } while(0)

/**
 * Test: Create and destroy session
 */
void test_session_create_and_destroy(void) {
    TEST("test_session_create_and_destroy");
    
    session_mgr_init();
    
    uint16_t session_id = 1;
    uint8_t key[SESSION_KEY_LENGTH] = {
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
        0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    };
    
    // Create session
    int result = session_create(session_id, key, sizeof(key));
    assert(result == 0);
    assert(session_is_active(session_id) == true);
    assert(session_get_active_count() == 1);
    
    // Destroy session
    result = session_destroy(session_id);
    assert(result == 0);
    assert(session_is_active(session_id) == false);
    assert(session_get_active_count() == 0);
    
    PASS();
}

/**
 * Test: Encrypt and decrypt roundtrip
 */
void test_encrypt_decrypt_roundtrip(void) {
    TEST("test_encrypt_decrypt_roundtrip");
    
    session_mgr_init();
    
    uint16_t session_id = 1;
    uint8_t key[SESSION_KEY_LENGTH] = {
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
        0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00
    };
    
    // Create session
    session_create(session_id, key, sizeof(key));
    
    // Test data
    const char *plaintext = "Hello, Matter!";
    size_t plaintext_len = strlen(plaintext);
    uint8_t ciphertext[256];
    size_t ciphertext_len;
    uint8_t decrypted[256];
    size_t decrypted_len;
    
    // Encrypt
    int result = session_encrypt(session_id, (uint8_t*)plaintext, plaintext_len,
                                ciphertext, sizeof(ciphertext), &ciphertext_len);
    assert(result == 0);
    assert(ciphertext_len > plaintext_len);  // Should include nonce and tag
    
    // Decrypt
    result = session_decrypt(session_id, ciphertext, ciphertext_len,
                            decrypted, sizeof(decrypted), &decrypted_len);
    assert(result == 0);
    assert(decrypted_len == plaintext_len);
    assert(memcmp(plaintext, decrypted, plaintext_len) == 0);
    
    session_destroy(session_id);
    
    PASS();
}

/**
 * Test: Multiple sessions
 */
void test_multiple_sessions(void) {
    TEST("test_multiple_sessions");
    
    session_mgr_init();
    
    uint8_t key1[SESSION_KEY_LENGTH] = {
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
        0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
    };
    uint8_t key2[SESSION_KEY_LENGTH] = {
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02
    };
    uint8_t key3[SESSION_KEY_LENGTH] = {
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03
    };
    
    // Create 3 sessions
    assert(session_create(1, key1, sizeof(key1)) == 0);
    assert(session_create(2, key2, sizeof(key2)) == 0);
    assert(session_create(3, key3, sizeof(key3)) == 0);
    assert(session_get_active_count() == 3);
    
    // Test encryption on each session
    const char *msg1 = "Session 1 message";
    const char *msg2 = "Session 2 message";
    const char *msg3 = "Session 3 message";
    
    uint8_t cipher1[256], cipher2[256], cipher3[256];
    size_t cipher1_len, cipher2_len, cipher3_len;
    
    assert(session_encrypt(1, (uint8_t*)msg1, strlen(msg1), 
                          cipher1, sizeof(cipher1), &cipher1_len) == 0);
    assert(session_encrypt(2, (uint8_t*)msg2, strlen(msg2),
                          cipher2, sizeof(cipher2), &cipher2_len) == 0);
    assert(session_encrypt(3, (uint8_t*)msg3, strlen(msg3),
                          cipher3, sizeof(cipher3), &cipher3_len) == 0);
    
    // Decrypt and verify
    uint8_t plain1[256], plain2[256], plain3[256];
    size_t plain1_len, plain2_len, plain3_len;
    
    assert(session_decrypt(1, cipher1, cipher1_len,
                          plain1, sizeof(plain1), &plain1_len) == 0);
    assert(session_decrypt(2, cipher2, cipher2_len,
                          plain2, sizeof(plain2), &plain2_len) == 0);
    assert(session_decrypt(3, cipher3, cipher3_len,
                          plain3, sizeof(plain3), &plain3_len) == 0);
    
    assert(memcmp(msg1, plain1, strlen(msg1)) == 0);
    assert(memcmp(msg2, plain2, strlen(msg2)) == 0);
    assert(memcmp(msg3, plain3, strlen(msg3)) == 0);
    
    // Clean up
    session_destroy(1);
    session_destroy(2);
    session_destroy(3);
    assert(session_get_active_count() == 0);
    
    PASS();
}

/**
 * Test: Session limit (max 5 sessions)
 */
void test_session_limit(void) {
    TEST("test_session_limit");
    
    session_mgr_init();
    
    uint8_t key[SESSION_KEY_LENGTH] = {0};
    
    // Create 5 sessions (max)
    for (int i = 1; i <= 5; i++) {
        assert(session_create(i, key, sizeof(key)) == 0);
    }
    assert(session_get_active_count() == 5);
    
    // Try to create 6th session - should fail
    int result = session_create(6, key, sizeof(key));
    assert(result != 0);
    assert(session_get_active_count() == 5);
    
    // Destroy one and try again
    session_destroy(1);
    assert(session_create(6, key, sizeof(key)) == 0);
    assert(session_get_active_count() == 5);
    
    // Clean up
    for (int i = 2; i <= 6; i++) {
        session_destroy(i);
    }
    
    PASS();
}

/**
 * Test: Message counter increment
 */
void test_message_counter_increment(void) {
    TEST("test_message_counter_increment");
    
    session_mgr_init();
    
    uint16_t session_id = 1;
    uint8_t key[SESSION_KEY_LENGTH] = {0};
    
    session_create(session_id, key, sizeof(key));
    
    // Initial counter should be 0
    assert(session_get_message_counter(session_id) == 0);
    
    // Encrypt a message - counter should increment
    const char *msg = "Test";
    uint8_t cipher[256];
    size_t cipher_len;
    
    session_encrypt(session_id, (uint8_t*)msg, strlen(msg),
                   cipher, sizeof(cipher), &cipher_len);
    assert(session_get_message_counter(session_id) == 1);
    
    // Encrypt another message
    session_encrypt(session_id, (uint8_t*)msg, strlen(msg),
                   cipher, sizeof(cipher), &cipher_len);
    assert(session_get_message_counter(session_id) == 2);
    
    session_destroy(session_id);
    
    PASS();
}

/**
 * Test: Decrypt with wrong session ID
 */
void test_decrypt_with_wrong_session_id(void) {
    TEST("test_decrypt_with_wrong_session_id");
    
    session_mgr_init();
    
    uint8_t key1[SESSION_KEY_LENGTH] = {0x11};
    uint8_t key2[SESSION_KEY_LENGTH] = {0x22};
    
    // Create two sessions
    session_create(1, key1, sizeof(key1));
    session_create(2, key2, sizeof(key2));
    
    // Encrypt with session 1
    const char *msg = "Secret message";
    uint8_t cipher[256];
    size_t cipher_len;
    
    session_encrypt(1, (uint8_t*)msg, strlen(msg),
                   cipher, sizeof(cipher), &cipher_len);
    
    // Try to decrypt with session 2 - should fail
    uint8_t plain[256];
    size_t plain_len;
    
    int result = session_decrypt(2, cipher, cipher_len,
                                plain, sizeof(plain), &plain_len);
    assert(result != 0);  // Authentication should fail
    
    // Decrypt with correct session - should succeed
    result = session_decrypt(1, cipher, cipher_len,
                            plain, sizeof(plain), &plain_len);
    assert(result == 0);
    assert(memcmp(msg, plain, strlen(msg)) == 0);
    
    session_destroy(1);
    session_destroy(2);
    
    PASS();
}

/**
 * Test: Replay protection (basic)
 * Tests that nonce includes message counter
 */
void test_replay_protection(void) {
    TEST("test_replay_protection");
    
    session_mgr_init();
    
    uint16_t session_id = 1;
    uint8_t key[SESSION_KEY_LENGTH] = {0};
    
    session_create(session_id, key, sizeof(key));
    
    const char *msg = "Test message";
    uint8_t cipher1[256], cipher2[256];
    size_t cipher1_len, cipher2_len;
    
    // Encrypt same message twice
    session_encrypt(session_id, (uint8_t*)msg, strlen(msg),
                   cipher1, sizeof(cipher1), &cipher1_len);
    session_encrypt(session_id, (uint8_t*)msg, strlen(msg),
                   cipher2, sizeof(cipher2), &cipher2_len);
    
    // Ciphertexts should be different (different nonces)
    assert(cipher1_len == cipher2_len);
    assert(memcmp(cipher1, cipher2, cipher1_len) != 0);
    
    // Both should decrypt correctly
    uint8_t plain1[256], plain2[256];
    size_t plain1_len, plain2_len;
    
    assert(session_decrypt(session_id, cipher1, cipher1_len,
                          plain1, sizeof(plain1), &plain1_len) == 0);
    assert(session_decrypt(session_id, cipher2, cipher2_len,
                          plain2, sizeof(plain2), &plain2_len) == 0);
    
    assert(memcmp(msg, plain1, strlen(msg)) == 0);
    assert(memcmp(msg, plain2, strlen(msg)) == 0);
    
    session_destroy(session_id);
    
    PASS();
}

/**
 * Test: Create session with invalid key length
 */
void test_invalid_key_length(void) {
    TEST("test_invalid_key_length");
    
    session_mgr_init();
    
    uint8_t key_short[8] = {0};
    uint8_t key_long[32] = {0};
    
    // Too short
    assert(session_create(1, key_short, sizeof(key_short)) != 0);
    
    // Too long
    assert(session_create(2, key_long, sizeof(key_long)) != 0);
    
    // NULL key
    assert(session_create(3, NULL, SESSION_KEY_LENGTH) != 0);
    
    PASS();
}

int main(void) {
    printf("=====================================\n");
    printf("Session Manager Unit Tests\n");
    printf("=====================================\n");
    
    test_session_create_and_destroy();
    test_encrypt_decrypt_roundtrip();
    test_multiple_sessions();
    test_session_limit();
    test_message_counter_increment();
    test_decrypt_with_wrong_session_id();
    test_replay_protection();
    test_invalid_key_length();
    
    printf("\n=====================================\n");
    printf("All Session Manager tests passed!\n");
    printf("=====================================\n");
    
    return 0;
}
