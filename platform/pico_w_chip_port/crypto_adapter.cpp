/*
 * crypto_adapter.cpp
 * Cryptography adapter using mbedTLS for Matter security
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/unique_id.h"

// mbedTLS headers
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
// Note: DRBG temporarily disabled due to Pico SDK mbedTLS bugs
// #include "mbedtls/ctr_drbg.h"
// #include "mbedtls/entropy.h"

static bool crypto_initialized = false;

extern "C" {

int crypto_adapter_init(void) {
    if (crypto_initialized) {
        return 0;
    }

    printf("Initializing crypto adapter (mbedTLS stub)...\n");

    // TODO: Initialize proper DRBG when Pico SDK mbedTLS issues are resolved
    // Current issue: SDK 1.5.1 mbedTLS has missing limits.h includes in CTR_DRBG
    // Workaround: Using stub mode without DRBG for now
    // For now, mark as initialized
    crypto_initialized = true;

    printf("Crypto adapter initialized (stub mode)\n");
    return 0;
}

int crypto_adapter_random(uint8_t *buffer, size_t length) {
    if (!crypto_initialized || !buffer || length == 0) {
        return -1;
    }

    // TODO: Implement proper RNG when Pico SDK mbedTLS issues are resolved
    // Current issue: DRBG initialization requires entropy source setup
    // Workaround: RNG currently unavailable, returns error
    // For testing only - Matter commissioning does not currently use this
    return -1;
}

int crypto_adapter_sha256(const uint8_t *input, size_t input_len, uint8_t *output) {
    if (!input || !output) {
        return -1;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not SHA-224)
    mbedtls_sha256_update(&ctx, input, input_len);
    mbedtls_sha256_finish(&ctx, output);

    mbedtls_sha256_free(&ctx);
    return 0;  // Success
}

int crypto_adapter_aes_encrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *input, size_t input_len,
    uint8_t *output
) {
    if (!key || !input || !output) {
        return -1;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    int ret = mbedtls_aes_setkey_enc(&aes, key, key_len * 8);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return ret;
    }

    // Use CBC mode if IV is provided, ECB otherwise
    if (iv && iv_len >= 16) {
        unsigned char iv_copy[16];
        memcpy(iv_copy, iv, 16);
        ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, input_len, iv_copy, input, output);
    } else {
        // ECB mode for single block
        if (input_len == 16) {
            ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, input, output);
        } else {
            ret = -1;  // Invalid length for ECB
        }
    }

    mbedtls_aes_free(&aes);
    return ret;
}

int crypto_adapter_aes_decrypt(
    const uint8_t *key, size_t key_len,
    const uint8_t *iv, size_t iv_len,
    const uint8_t *input, size_t input_len,
    uint8_t *output
) {
    if (!key || !input || !output) {
        return -1;
    }

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);

    int ret = mbedtls_aes_setkey_dec(&aes, key, key_len * 8);
    if (ret != 0) {
        mbedtls_aes_free(&aes);
        return ret;
    }

    // Use CBC mode if IV is provided, ECB otherwise
    if (iv && iv_len >= 16) {
        unsigned char iv_copy[16];
        memcpy(iv_copy, iv, 16);
        ret = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, input_len, iv_copy, input, output);
    } else {
        // ECB mode for single block
        if (input_len == 16) {
            ret = mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_DECRYPT, input, output);
        } else {
            ret = -1;  // Invalid length for ECB
        }
    }

    mbedtls_aes_free(&aes);
    return ret;
}

void crypto_adapter_deinit(void) {
    if (!crypto_initialized) {
        return;
    }

    // TODO: Free DRBG/entropy resources when implemented
    // Currently no resources to free in stub mode
    crypto_initialized = false;
    printf("Crypto adapter deinitialized (stub)\n");
}

} // extern "C"
