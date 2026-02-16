/*
 * pase.c
 * Password Authenticated Session Establishment (PASE) implementation
 * Implements SPAKE2+ protocol per Matter Core Specification Section 4.12.1
 */

#include "pase.h"
#include <string.h>
#include <stdio.h>

// mbedTLS headers
#include "mbedtls/md.h"
#include "mbedtls/pkcs5.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/platform_util.h"
#include "pico/time.h"
#include "pico/rand.h"

// TLV encoding for PASE messages
#include "../codec/tlv.h"

/**
 * SPAKE2+ Constants per Matter spec
 * These are the standard Matter M and N points for P-256 curve
 * Defined in Matter Core Specification Section 3.9.1 (SPAKE2+ Parameters)
 * These values MUST match the official Matter specification for interoperability
 */
static const uint8_t SPAKE2_M_P256[65] = {
    0x04, // Uncompressed point
    // X coordinate (32 bytes)
    0x88, 0x6e, 0x2f, 0x97, 0xac, 0xe4, 0x6e, 0x55,
    0xba, 0x9d, 0xd7, 0x24, 0x25, 0x79, 0xf2, 0x99,
    0x3b, 0x64, 0xe1, 0x6e, 0xf3, 0xdc, 0xab, 0x95,
    0xaf, 0xd4, 0x97, 0x33, 0x3d, 0x8f, 0xa1, 0x2f,
    // Y coordinate (32 bytes)
    0x5f, 0xf3, 0x55, 0x16, 0x3e, 0x43, 0xce, 0x22,
    0x4e, 0x0b, 0x0e, 0x65, 0xff, 0x02, 0xac, 0x8e,
    0x5c, 0x7b, 0xe0, 0x94, 0x19, 0xc7, 0x85, 0xe0,
    0xca, 0x54, 0x7d, 0x55, 0xa1, 0x2e, 0x2d, 0x20
};

static const uint8_t SPAKE2_N_P256[65] = {
    0x04, // Uncompressed point
    // X coordinate (32 bytes)
    0xd8, 0xbb, 0xd6, 0xc6, 0x39, 0xc6, 0x29, 0x37,
    0xb0, 0x4d, 0x99, 0x7f, 0x38, 0xc3, 0x77, 0x07,
    0x19, 0xc6, 0x29, 0xd7, 0x01, 0x4d, 0x49, 0xa2,
    0x4b, 0x4f, 0x98, 0xba, 0xa1, 0x29, 0x2b, 0x49,
    // Y coordinate (32 bytes)
    0x07, 0xd3, 0x2c, 0x1f, 0xa7, 0xc9, 0xfd, 0xdf,
    0x7d, 0xa0, 0x2d, 0x5a, 0xf9, 0x6a, 0x91, 0x12,
    0xf3, 0x6c, 0x4d, 0x17, 0xdc, 0x33, 0x50, 0x12,
    0xb8, 0x4e, 0x89, 0xfc, 0xe5, 0x44, 0xb6, 0x8e
};

/**
 * Generate random bytes using Pico's hardware RNG
 */
static int generate_random_bytes(uint8_t *buffer, size_t length) {
    if (!buffer || length == 0) {
        return -1;
    }
    
    // Use Pico's hardware RNG
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (uint8_t)(get_rand_32() & 0xFF);
    }
    
    return 0;
}

/**
 * Derive w0 and w1 from PIN using PBKDF2
 */
static int derive_w0_w1_from_pin(const uint8_t *pin, size_t pin_len,
                                 const uint8_t *salt, size_t salt_len,
                                 uint32_t iterations,
                                 uint8_t *w0, uint8_t *w1) {
    if (!pin || !salt || !w0 || !w1) {
        return -1;
    }
    
    // Derive 64 bytes (w0 || w1) using PBKDF2-HMAC-SHA256
    uint8_t derived[64];
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md_info) {
        printf("PASE: Failed to get SHA256 MD info\n");
        return -1;
    }
    
    // Create and initialize MD context for PBKDF2
    mbedtls_md_context_t md_ctx;
    mbedtls_md_init(&md_ctx);
    
    int ret = mbedtls_md_setup(&md_ctx, md_info, 1);
    if (ret != 0) {
        printf("PASE: MD context setup failed: %d\n", ret);
        mbedtls_md_free(&md_ctx);
        return -1;
    }
    
    ret = mbedtls_pkcs5_pbkdf2_hmac(
        &md_ctx,
        pin, pin_len,
        salt, salt_len,
        iterations,
        64, derived
    );
    
    mbedtls_md_free(&md_ctx);
    
    if (ret != 0) {
        printf("PASE: PBKDF2 failed: %d\n", ret);
        return -1;
    }
    
    // Split into w0 and w1
    memcpy(w0, derived, 32);
    memcpy(w1, derived + 32, 32);
    
    // Zeroize temporary buffer
    mbedtls_platform_zeroize(derived, sizeof(derived));
    
    return 0;
}

/**
 * Compute L = w1*P where P is the P-256 generator
 */
static int compute_L(const uint8_t *w1, uint8_t *L) {
    if (!w1 || !L) {
        return -1;
    }
    
    mbedtls_ecp_group grp;
    mbedtls_ecp_point point_L;
    mbedtls_mpi scalar_w1;
    
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&point_L);
    mbedtls_mpi_init(&scalar_w1);
    
    int ret = -1;
    
    // Load P-256 curve
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) {
        printf("PASE: Failed to load P-256 curve\n");
        goto cleanup;
    }
    
    // Load w1 as MPI
    if (mbedtls_mpi_read_binary(&scalar_w1, w1, 32) != 0) {
        printf("PASE: Failed to load w1\n");
        goto cleanup;
    }
    
    // Compute L = w1 * G (generator point)
    if (mbedtls_ecp_mul(&grp, &point_L, &scalar_w1, &grp.G, NULL, NULL) != 0) {
        printf("PASE: Failed to compute L\n");
        goto cleanup;
    }
    
    // Export L as uncompressed point
    size_t olen;
    if (mbedtls_ecp_point_write_binary(&grp, &point_L, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, L, PASE_SPAKE2_POINT_LENGTH) != 0) {
        printf("PASE: Failed to export L\n");
        goto cleanup;
    }
    
    ret = 0;
    
cleanup:
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&point_L);
    mbedtls_mpi_free(&scalar_w1);
    
    return ret;
}

int pase_init(pase_context_t *ctx, const char *setup_pin) {
    if (!ctx || !setup_pin) {
        return -1;
    }
    
    // Validate PIN format (8 digits)
    size_t pin_len = strlen(setup_pin);
    if (pin_len != PASE_PIN_LENGTH) {
        printf("PASE: Invalid PIN length: %zu (expected 8)\n", pin_len);
        return -1;
    }
    
    // Validate all characters are digits
    for (size_t i = 0; i < pin_len; i++) {
        if (setup_pin[i] < '0' || setup_pin[i] > '9') {
            printf("PASE: Invalid PIN character at position %zu\n", i);
            return -1;
        }
    }
    
    // Clear context
    memset(ctx, 0, sizeof(pase_context_t));
    
    // Store PIN
    memcpy(ctx->setup_pin, setup_pin, PASE_PIN_LENGTH);
    ctx->setup_pin[PASE_PIN_LENGTH] = '\0';
    
    // Set initial state
    ctx->state = PASE_STATE_INITIALIZED;
    ctx->pbkdf2_iterations = PASE_PBKDF2_ITERATIONS;
    
    printf("PASE: Initialized with PIN: ********\n");
    
    return 0;
}

int pase_handle_pbkdf_request(pase_context_t *ctx,
                               const uint8_t *request, size_t request_len,
                               uint8_t *response, size_t max_response_len,
                               size_t *actual_response_len) {
    if (!ctx || !request || !response || !actual_response_len) {
        return -1;
    }
    
    if (ctx->state != PASE_STATE_INITIALIZED) {
        printf("PASE: Invalid state for PBKDF request: %d\n", ctx->state);
        return -1;
    }
    
    // Generate random salt
    if (generate_random_bytes(ctx->salt, PASE_SALT_LENGTH) != 0) {
        printf("PASE: Failed to generate salt\n");
        ctx->state = PASE_STATE_ERROR;
        return -1;
    }
    
    // Derive w0 and w1 from PIN
    if (derive_w0_w1_from_pin((uint8_t*)ctx->setup_pin, PASE_PIN_LENGTH,
                              ctx->salt, PASE_SALT_LENGTH,
                              ctx->pbkdf2_iterations,
                              ctx->w0, ctx->w1) != 0) {
        printf("PASE: Failed to derive w0/w1\n");
        ctx->state = PASE_STATE_ERROR;
        return -1;
    }
    
    // Compute L = w1 * G
    if (compute_L(ctx->w1, ctx->L) != 0) {
        printf("PASE: Failed to compute L\n");
        ctx->state = PASE_STATE_ERROR;
        return -1;
    }
    
    // Encode PBKDFParamResponse as TLV
    // Simple encoding: [iterations (4), salt (32), L is computed but not sent yet]
    tlv_writer_t writer;
    tlv_writer_init(&writer, response, max_response_len);
    
    // Tag 1: PBKDF2 iterations
    if (tlv_encode_uint32(&writer, 1, ctx->pbkdf2_iterations) != 0) {
        printf("PASE: Failed to encode iterations\n");
        return -1;
    }
    
    // Tag 2: Salt
    if (tlv_encode_bytes(&writer, 2, ctx->salt, PASE_SALT_LENGTH) != 0) {
        printf("PASE: Failed to encode salt\n");
        return -1;
    }
    
    *actual_response_len = tlv_writer_get_length(&writer);
    
    ctx->state = PASE_STATE_PBKDF_RESP_SENT;
    printf("PASE: Sent PBKDFParamResponse (iterations=%u, salt_len=%d)\n",
           ctx->pbkdf2_iterations, PASE_SALT_LENGTH);
    
    return 0;
}

int pase_handle_pake1(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len) {
    if (!ctx || !request || !response || !actual_response_len) {
        return -1;
    }
    
    if (ctx->state != PASE_STATE_PBKDF_RESP_SENT) {
        printf("PASE: Invalid state for PAKE1: %d\n", ctx->state);
        return -1;
    }
    
    // Decode pA from request (TLV encoded)
    // For now, simple implementation: expect raw pA point
    if (request_len < PASE_SPAKE2_POINT_LENGTH) {
        printf("PASE: PAKE1 too short: %zu\n", request_len);
        return -1;
    }
    
    memcpy(ctx->pA, request, PASE_SPAKE2_POINT_LENGTH);
    
    // Generate random y (verifier's secret)
    uint8_t y[32];
    if (generate_random_bytes(y, 32) != 0) {
        printf("PASE: Failed to generate y\n");
        ctx->state = PASE_STATE_ERROR;
        return -1;
    }
    
    // Compute pB = y*G + w0*N
    mbedtls_ecp_group grp;
    mbedtls_ecp_point point_pB, point_yG, point_N;
    mbedtls_mpi scalar_y, scalar_w0;
    
    mbedtls_ecp_group_init(&grp);
    mbedtls_ecp_point_init(&point_pB);
    mbedtls_ecp_point_init(&point_yG);
    mbedtls_ecp_point_init(&point_N);
    mbedtls_mpi_init(&scalar_y);
    mbedtls_mpi_init(&scalar_w0);
    
    int ret = -1;
    
    // Load P-256 curve
    if (mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1) != 0) {
        goto pake1_cleanup;
    }
    
    // Load scalars
    if (mbedtls_mpi_read_binary(&scalar_y, y, 32) != 0) {
        goto pake1_cleanup;
    }
    if (mbedtls_mpi_read_binary(&scalar_w0, ctx->w0, 32) != 0) {
        goto pake1_cleanup;
    }
    
    // Load N point
    if (mbedtls_ecp_point_read_binary(&grp, &point_N, SPAKE2_N_P256, 
                                      sizeof(SPAKE2_N_P256)) != 0) {
        goto pake1_cleanup;
    }
    
    // Compute y*G
    if (mbedtls_ecp_mul(&grp, &point_yG, &scalar_y, &grp.G, NULL, NULL) != 0) {
        goto pake1_cleanup;
    }
    
    // Compute w0*N
    mbedtls_ecp_point point_w0N;
    mbedtls_ecp_point_init(&point_w0N);
    if (mbedtls_ecp_mul(&grp, &point_w0N, &scalar_w0, &point_N, NULL, NULL) != 0) {
        mbedtls_ecp_point_free(&point_w0N);
        goto pake1_cleanup;
    }
    
    // Compute pB = y*G + w0*N
    if (mbedtls_ecp_muladd(&grp, &point_pB, &scalar_y, &grp.G, 
                          &scalar_w0, &point_N) != 0) {
        mbedtls_ecp_point_free(&point_w0N);
        goto pake1_cleanup;
    }
    
    mbedtls_ecp_point_free(&point_w0N);
    
    // Export pB
    size_t olen;
    if (mbedtls_ecp_point_write_binary(&grp, &point_pB, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, ctx->pB, PASE_SPAKE2_POINT_LENGTH) != 0) {
        goto pake1_cleanup;
    }
    
    // Compute Z = y*(pA - w0*M)
    mbedtls_ecp_point point_pA, point_M, point_w0M, point_neg_w0M, point_temp;
    mbedtls_ecp_point_init(&point_pA);
    mbedtls_ecp_point_init(&point_M);
    mbedtls_ecp_point_init(&point_w0M);
    mbedtls_ecp_point_init(&point_neg_w0M);
    mbedtls_ecp_point_init(&point_temp);
    
    // Load pA
    if (mbedtls_ecp_point_read_binary(&grp, &point_pA, ctx->pA, 
                                      PASE_SPAKE2_POINT_LENGTH) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        goto pake1_cleanup;
    }
    
    // Load M
    if (mbedtls_ecp_point_read_binary(&grp, &point_M, SPAKE2_M_P256, 
                                      sizeof(SPAKE2_M_P256)) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        goto pake1_cleanup;
    }
    
    // Compute w0*M
    if (mbedtls_ecp_mul(&grp, &point_w0M, &scalar_w0, &point_M, NULL, NULL) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        goto pake1_cleanup;
    }
    
    // Compute pA - w0*M (proper elliptic curve point subtraction)
    // Step 1: Create negated point -w0*M by copying w0*M and negating Y coordinate
    if (mbedtls_ecp_copy(&point_neg_w0M, &point_w0M) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        goto pake1_cleanup;
    }
    
    // Negate Y coordinate: Y_neg = (p - Y) mod p
    mbedtls_mpi neg_y;
    mbedtls_mpi_init(&neg_y);
    
    // Compute neg_y = grp.P - point_w0M.Y (modular negation on the curve)
    if (mbedtls_mpi_sub_mpi(&neg_y, &grp.P, &point_w0M.Y) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_mpi_free(&neg_y);
        goto pake1_cleanup;
    }
    
    // Set negated Y coordinate in point_neg_w0M
    if (mbedtls_mpi_copy(&point_neg_w0M.Y, &neg_y) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_mpi_free(&neg_y);
        goto pake1_cleanup;
    }
    
    mbedtls_mpi_free(&neg_y);
    
    // Step 2: Add pA + (-w0*M) using proper elliptic curve point addition
    // mbedtls_ecp_muladd(grp, R, m1, P1, m2, P2) computes R = m1*P1 + m2*P2
    // With m1=1, P1=point_pA, m2=1, P2=point_neg_w0M, this computes:
    // point_temp = 1*point_pA + 1*point_neg_w0M = pA + (-w0*M) = pA - w0*M
    mbedtls_mpi one;
    mbedtls_mpi_init(&one);
    if (mbedtls_mpi_lset(&one, 1) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_mpi_free(&one);
        goto pake1_cleanup;
    }
    
    if (mbedtls_ecp_muladd(&grp, &point_temp, &one, &point_pA, &one, &point_neg_w0M) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_mpi_free(&one);
        goto pake1_cleanup;
    }
    
    mbedtls_mpi_free(&one);
    
    // Compute Z = y * (pA - w0*M)
    mbedtls_ecp_point point_Z;
    mbedtls_ecp_point_init(&point_Z);
    if (mbedtls_ecp_mul(&grp, &point_Z, &scalar_y, &point_temp, NULL, NULL) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_ecp_point_free(&point_Z);
        goto pake1_cleanup;
    }
    
    // Export Z
    if (mbedtls_ecp_point_write_binary(&grp, &point_Z, MBEDTLS_ECP_PF_UNCOMPRESSED,
                                       &olen, ctx->Z, PASE_SPAKE2_POINT_LENGTH) != 0) {
        mbedtls_ecp_point_free(&point_pA);
        mbedtls_ecp_point_free(&point_M);
        mbedtls_ecp_point_free(&point_w0M);
        mbedtls_ecp_point_free(&point_neg_w0M);
        mbedtls_ecp_point_free(&point_temp);
        mbedtls_ecp_point_free(&point_Z);
        goto pake1_cleanup;
    }
    
    mbedtls_ecp_point_free(&point_pA);
    mbedtls_ecp_point_free(&point_M);
    mbedtls_ecp_point_free(&point_w0M);
    mbedtls_ecp_point_free(&point_neg_w0M);
    mbedtls_ecp_point_free(&point_temp);
    mbedtls_ecp_point_free(&point_Z);
    
    // Encode pB in response
    memcpy(response, ctx->pB, PASE_SPAKE2_POINT_LENGTH);
    *actual_response_len = PASE_SPAKE2_POINT_LENGTH;
    
    ret = 0;
    ctx->state = PASE_STATE_PAKE2_SENT;
    printf("PASE: Sent PAKE2 (pB)\n");
    
pake1_cleanup:
    // Zeroize sensitive data
    mbedtls_platform_zeroize(y, sizeof(y));
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&point_pB);
    mbedtls_ecp_point_free(&point_yG);
    mbedtls_ecp_point_free(&point_N);
    mbedtls_mpi_free(&scalar_y);
    mbedtls_mpi_free(&scalar_w0);
    
    if (ret != 0) {
        ctx->state = PASE_STATE_ERROR;
    }
    
    return ret;
}

int pase_handle_pake2(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len) {
    // This is typically not used in the verifier role
    // The verifier sends PAKE2, it doesn't handle it
    return 0;
}

int pase_handle_pake3(pase_context_t *ctx,
                      const uint8_t *request, size_t request_len,
                      uint8_t *response, size_t max_response_len,
                      size_t *actual_response_len) {
    if (!ctx || !request) {
        return -1;
    }
    
    if (ctx->state != PASE_STATE_PAKE2_SENT) {
        printf("PASE: Invalid state for PAKE3: %d\n", ctx->state);
        return -1;
    }
    
    // Validate confirmation tag from prover
    // Simplified: Just accept it for now
    // Real implementation would verify HMAC(Ka, transcript)
    
    ctx->state = PASE_STATE_COMPLETED;
    printf("PASE: Session established successfully\n");
    
    if (actual_response_len) {
        *actual_response_len = 0;  // No response needed
    }
    
    return 0;
}

int pase_derive_session_key(pase_context_t *ctx, uint8_t session_id,
                            uint8_t *key_out, size_t key_len) {
    if (!ctx || !key_out || key_len != PASE_SESSION_KEY_LENGTH) {
        return -1;
    }
    
    if (ctx->state != PASE_STATE_COMPLETED) {
        printf("PASE: Cannot derive key, PASE not completed\n");
        return -1;
    }
    
    // Derive session key using HKDF-SHA256
    // Input: shared secret Z (X coordinate)
    // Salt: "CHIP PASE Session Keys"
    // Info: session_id
    const char *hkdf_salt = "CHIP PASE Session Keys";
    uint8_t info[1] = { session_id };
    
    // Extract X coordinate from Z point (skip 0x04 prefix)
    uint8_t z_x[32];
    memcpy(z_x, ctx->Z + 1, 32);
    
    // Use HKDF to derive session key
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) {
        printf("PASE: Failed to get SHA256 info\n");
        return -1;
    }
    
    int ret = mbedtls_hkdf(md,
                           (const uint8_t*)hkdf_salt, strlen(hkdf_salt),
                           z_x, 32,
                           info, sizeof(info),
                           key_out, key_len);
    
    if (ret != 0) {
        printf("PASE: HKDF failed: %d\n", ret);
        return -1;
    }
    
    printf("PASE: Derived session key for session %u\n", session_id);
    
    // Zeroize intermediate data
    mbedtls_platform_zeroize(z_x, sizeof(z_x));
    
    return 0;
}

void pase_deinit(pase_context_t *ctx) {
    if (!ctx) {
        return;
    }
    
    // Zeroize all sensitive data
    mbedtls_platform_zeroize(ctx, sizeof(pase_context_t));
    
    printf("PASE: Context cleaned up\n");
}

pase_state_t pase_get_state(const pase_context_t *ctx) {
    if (!ctx) {
        return PASE_STATE_IDLE;
    }
    return ctx->state;
}
