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
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;
static bool crypto_initialized = false;

int crypto_adapter_init(void) {
    if (crypto_initialized) {
        return 0;
    }

    printf("Initializing crypto adapter (mbedTLS)...\n");

    // Initialize entropy and random number generator
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    // Get unique ID from Pico as entropy seed
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    const char *pers = "viking_bio_matter";
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        (const unsigned char *)pers,
        strlen(pers)
    );

    if (ret != 0) {
        printf("ERROR: mbedTLS RNG initialization failed: -0x%04x\n", -ret);
        return -1;
    }

    // Mix in board ID for additional entropy
    mbedtls_ctr_drbg_update(&ctr_drbg, board_id.id, PICO_UNIQUE_BOARD_ID_SIZE_BYTES);

    crypto_initialized = true;
    printf("Crypto adapter initialized\n");
    return 0;
}

int crypto_adapter_random(uint8_t *buffer, size_t length) {
    if (!crypto_initialized || !buffer || length == 0) {
        return -1;
    }

    return mbedtls_ctr_drbg_random(&ctr_drbg, buffer, length);
}

int crypto_adapter_sha256(const uint8_t *input, size_t input_len, uint8_t *output) {
    if (!input || !output) {
        return -1;
    }

    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    int ret = mbedtls_sha256_starts(&ctx, 0);  // 0 = SHA-256 (not SHA-224)
    if (ret == 0) {
        ret = mbedtls_sha256_update(&ctx, input, input_len);
    }
    if (ret == 0) {
        ret = mbedtls_sha256_finish(&ctx, output);
    }

    mbedtls_sha256_free(&ctx);
    return ret;
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

    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
    crypto_initialized = false;
    printf("Crypto adapter deinitialized\n");
}
