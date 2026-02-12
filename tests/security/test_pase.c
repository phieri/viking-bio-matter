/*
 * test_pase.c
 * Unit tests for PASE protocol
 */

#include "pase.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

// Test helper macros
#define TEST(name) printf("\nRunning test: %s\n", name)
#define PASS() printf("  ✓ PASSED\n")
#define FAIL(msg) do { printf("  ✗ FAILED: %s\n", msg); return; } while(0)

/**
 * Test: Initialize PASE with valid PIN
 */
void test_pase_init_with_pin(void) {
    TEST("test_pase_init_with_pin");
    
    pase_context_t ctx;
    const char *pin = "12345678";
    
    int result = pase_init(&ctx, pin);
    assert(result == 0);
    assert(pase_get_state(&ctx) == PASE_STATE_INITIALIZED);
    
    // Verify PIN is stored correctly (can't compare directly due to security)
    assert(strlen((char*)ctx.setup_pin) == PASE_PIN_LENGTH);
    
    pase_deinit(&ctx);
    
    PASS();
}

/**
 * Test: Initialize PASE with invalid PIN (wrong length)
 */
void test_pase_init_invalid_pin_length(void) {
    TEST("test_pase_init_invalid_pin_length");
    
    pase_context_t ctx;
    const char *pin = "123456";  // Too short
    
    int result = pase_init(&ctx, pin);
    assert(result != 0);
    
    PASS();
}

/**
 * Test: Initialize PASE with invalid PIN (non-digits)
 */
void test_pase_init_invalid_pin_format(void) {
    TEST("test_pase_init_invalid_pin_format");
    
    pase_context_t ctx;
    const char *pin = "1234567a";  // Contains letter
    
    int result = pase_init(&ctx, pin);
    assert(result != 0);
    
    PASS();
}

/**
 * Test: PBKDF2 derivation
 * Tests that PBKDF2 produces consistent output
 */
void test_pbkdf2_derivation(void) {
    TEST("test_pbkdf2_derivation");
    
    pase_context_t ctx;
    const char *pin = "20202021";  // Matter spec test PIN
    
    int result = pase_init(&ctx, pin);
    assert(result == 0);
    
    // Simulate PBKDF request
    uint8_t request[64] = {0};  // Minimal request
    uint8_t response[256];
    size_t response_len;
    
    result = pase_handle_pbkdf_request(&ctx, request, sizeof(request),
                                       response, sizeof(response), &response_len);
    assert(result == 0);
    assert(response_len > 0);
    assert(pase_get_state(&ctx) == PASE_STATE_PBKDF_RESP_SENT);
    
    // Verify salt was generated
    bool salt_is_zero = true;
    for (int i = 0; i < PASE_SALT_LENGTH; i++) {
        if (ctx.salt[i] != 0) {
            salt_is_zero = false;
            break;
        }
    }
    assert(!salt_is_zero);  // Salt should be random, not all zeros
    
    pase_deinit(&ctx);
    
    PASS();
}

/**
 * Test: PASE state machine
 * Tests that state transitions are enforced correctly
 */
void test_pase_state_machine(void) {
    TEST("test_pase_state_machine");
    
    pase_context_t ctx;
    const char *pin = "12345678";
    
    // Initial state
    memset(&ctx, 0, sizeof(ctx));
    assert(pase_get_state(&ctx) == PASE_STATE_IDLE);
    
    // After init
    pase_init(&ctx, pin);
    assert(pase_get_state(&ctx) == PASE_STATE_INITIALIZED);
    
    // Try to handle PAKE1 before PBKDF - should fail
    uint8_t request[128] = {0};
    uint8_t response[256];
    size_t response_len;
    
    int result = pase_handle_pake1(&ctx, request, sizeof(request),
                                   response, sizeof(response), &response_len);
    // Should fail because we're not in the right state
    assert(result != 0);
    
    // Now do PBKDF request
    result = pase_handle_pbkdf_request(&ctx, request, sizeof(request),
                                       response, sizeof(response), &response_len);
    assert(result == 0);
    assert(pase_get_state(&ctx) == PASE_STATE_PBKDF_RESP_SENT);
    
    pase_deinit(&ctx);
    
    PASS();
}

/**
 * Test: Session key derivation
 * Tests that we can derive a session key after PASE completes
 */
void test_session_key_derivation(void) {
    TEST("test_session_key_derivation");
    
    pase_context_t ctx;
    const char *pin = "12345678";
    
    pase_init(&ctx, pin);
    
    // Simulate completion (normally done by PAKE3)
    // For testing, manually set state and Z
    ctx.state = PASE_STATE_COMPLETED;
    
    // Set a dummy Z point (uncompressed P-256 point)
    ctx.Z[0] = 0x04;  // Uncompressed marker
    for (int i = 1; i < PASE_SPAKE2_POINT_LENGTH; i++) {
        ctx.Z[i] = (uint8_t)(i & 0xFF);  // Dummy data
    }
    
    // Derive session key
    uint8_t session_key[PASE_SESSION_KEY_LENGTH];
    uint8_t session_id = 1;
    
    int result = pase_derive_session_key(&ctx, session_id, 
                                         session_key, sizeof(session_key));
    assert(result == 0);
    
    // Verify key is not all zeros
    bool key_is_zero = true;
    for (int i = 0; i < PASE_SESSION_KEY_LENGTH; i++) {
        if (session_key[i] != 0) {
            key_is_zero = false;
            break;
        }
    }
    assert(!key_is_zero);
    
    pase_deinit(&ctx);
    
    PASS();
}

/**
 * Test: Attempting session key derivation before PASE completion
 */
void test_session_key_derivation_not_ready(void) {
    TEST("test_session_key_derivation_not_ready");
    
    pase_context_t ctx;
    const char *pin = "12345678";
    
    pase_init(&ctx, pin);
    
    // Try to derive key before completion
    uint8_t session_key[PASE_SESSION_KEY_LENGTH];
    uint8_t session_id = 1;
    
    int result = pase_derive_session_key(&ctx, session_id,
                                         session_key, sizeof(session_key));
    assert(result != 0);  // Should fail
    
    pase_deinit(&ctx);
    
    PASS();
}

/**
 * Test: Invalid PIN handling
 */
void test_invalid_pin_handling(void) {
    TEST("test_invalid_pin_handling");
    
    pase_context_t ctx;
    
    // NULL PIN
    assert(pase_init(&ctx, NULL) != 0);
    
    // Empty PIN
    assert(pase_init(&ctx, "") != 0);
    
    // Too short
    assert(pase_init(&ctx, "1234") != 0);
    
    // Too long
    assert(pase_init(&ctx, "123456789") != 0);
    
    // Non-numeric
    assert(pase_init(&ctx, "abcdefgh") != 0);
    assert(pase_init(&ctx, "1234567!") != 0);
    
    PASS();
}

int main(void) {
    printf("=====================================\n");
    printf("PASE Protocol Unit Tests\n");
    printf("=====================================\n");
    
    test_pase_init_with_pin();
    test_pase_init_invalid_pin_length();
    test_pase_init_invalid_pin_format();
    test_pbkdf2_derivation();
    test_pase_state_machine();
    test_session_key_derivation();
    test_session_key_derivation_not_ready();
    test_invalid_pin_handling();
    
    printf("\n=====================================\n");
    printf("All PASE tests passed!\n");
    printf("=====================================\n");
    
    return 0;
}
