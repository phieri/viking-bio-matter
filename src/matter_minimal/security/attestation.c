/*
 * attestation.c
 * Device Attestation for Matter commissioning (test-mode implementation).
 *
 * WARNING – TEST-MODE ONLY.  Private key is stored in LittleFS (flash).
 * See PRODUCTION_README.md for production hardening requirements.
 *
 * Credentials are loaded from LittleFS via storage_attestation_read():
 *   /certs/dac.der     – Device Attestation Certificate (DER)
 *   /certs/pai.der     – Product Attestation Intermediate cert (DER)
 *   /certs/dac_key.der – EC private key (SEC.1 or PKCS#8 DER)
 *   /certs/cd.der      – Certification Declaration blob (optional)
 *
 * Uses mbedTLS for ECDSA-P256 signing and TLV encoding.
 */

#include "attestation.h"
#include "../codec/tlv.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* mbedTLS */
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/sha256.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

/* Platform RNG (Pico hardware RNG) */
#include "pico/rand.h"

/* Platform storage */
extern int storage_attestation_read(const char *path, uint8_t *buf,
                                     size_t buf_size, size_t *out_len);

/* ------------------------------------------------------------------ */
/* Module state                                                         */
/* ------------------------------------------------------------------ */

static bool          g_att_initialized = false;

/* Loaded credential blobs (DER bytes) */
static uint8_t  g_dac_der[ATT_MAX_CERT_SIZE];
static size_t   g_dac_der_len;
static uint8_t  g_pai_der[ATT_MAX_CERT_SIZE];
static size_t   g_pai_der_len;
static uint8_t  g_cd_der[ATT_MAX_CD_SIZE];
static size_t   g_cd_der_len;

/* Loaded private-key context */
static mbedtls_pk_context g_pk_ctx;
static bool               g_pk_loaded = false;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/** Hardware RNG callback for mbedTLS */
static int pico_rng_callback(void *ctx, unsigned char *buf, size_t len) {
    (void)ctx;
    size_t off = 0;
    while (off < len) {
        uint32_t w = get_rand_32();
        size_t chunk = len - off;
        if (chunk > sizeof(w)) chunk = sizeof(w);
        memcpy(buf + off, &w, chunk);
        off += chunk;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int attestation_init(void) {
    if (g_att_initialized) {
        return 0;
    }

    printf("[ATT] Initializing attestation subsystem\n");

    g_dac_der_len = 0;
    g_pai_der_len = 0;
    g_cd_der_len  = 0;
    g_pk_loaded   = false;

    /* Load DAC */
    if (storage_attestation_read(ATT_DAC_PATH,
                                 g_dac_der, sizeof(g_dac_der),
                                 &g_dac_der_len) != 0 || g_dac_der_len == 0) {
        printf("[ATT] WARNING: DAC not found at %s (provision with "
               "tools/provision_attestation.py)\n", ATT_DAC_PATH);
    } else {
        printf("[ATT] DAC loaded (%zu bytes)\n", g_dac_der_len);
    }

    /* Load PAI */
    if (storage_attestation_read(ATT_PAI_PATH,
                                 g_pai_der, sizeof(g_pai_der),
                                 &g_pai_der_len) != 0 || g_pai_der_len == 0) {
        printf("[ATT] WARNING: PAI not found at %s\n", ATT_PAI_PATH);
    } else {
        printf("[ATT] PAI loaded (%zu bytes)\n", g_pai_der_len);
    }

    /* Load CD (optional, may be absent) */
    if (storage_attestation_read(ATT_CD_PATH,
                                 g_cd_der, sizeof(g_cd_der),
                                 &g_cd_der_len) != 0) {
        g_cd_der_len = 0; /* Not present – use empty stub */
        printf("[ATT] CD not found (using empty stub)\n");
    } else {
        printf("[ATT] CD loaded (%zu bytes)\n", g_cd_der_len);
    }

    /* Load private key */
    uint8_t key_der[ATT_MAX_KEY_SIZE];
    size_t  key_der_len = 0;

    mbedtls_pk_init(&g_pk_ctx);

    if (storage_attestation_read(ATT_DAC_KEY_PATH,
                                 key_der, sizeof(key_der),
                                 &key_der_len) != 0 || key_der_len == 0) {
        printf("[ATT] WARNING: Private key not found at %s\n", ATT_DAC_KEY_PATH);
    } else {
        /* mbedTLS 3.x: parse_key requires f_rng for RSA; NULL is OK for ECC */
        int ret = mbedtls_pk_parse_key(&g_pk_ctx,
                                       key_der, key_der_len,
                                       NULL, 0,
                                       pico_rng_callback, NULL);
        mbedtls_platform_zeroize(key_der, key_der_len);

        if (ret != 0) {
            char errbuf[64];
            mbedtls_strerror(ret, errbuf, sizeof(errbuf));
            printf("[ATT] ERROR: Failed to parse private key: %s (%d)\n",
                   errbuf, ret);
            mbedtls_pk_free(&g_pk_ctx);
        } else {
            g_pk_loaded = true;
            printf("[ATT] Private key loaded (type %d)\n",
                   (int)mbedtls_pk_get_type(&g_pk_ctx));
        }
    }

    g_att_initialized = true;

    if (g_dac_der_len > 0 && g_pai_der_len > 0 && g_pk_loaded) {
        printf("[ATT] Attestation credentials ready\n");
    } else {
        printf("[ATT] Attestation credentials incomplete – "
               "attestation will fail until certs are provisioned\n");
    }

    return 0;
}

int attestation_get_cert_chain(uint8_t *buf, size_t buf_size, size_t *out_len) {
    if (!buf || !out_len) return -1;
    if (!g_att_initialized) return -1;

    /* Return DAC || PAI concatenated (DER blobs) */
    size_t total = g_dac_der_len + g_pai_der_len;
    if (total == 0) {
        *out_len = 0;
        return -1;
    }
    if (total > buf_size) {
        printf("[ATT] ERROR: cert chain buffer too small (%zu < %zu)\n",
               buf_size, total);
        return -1;
    }

    memcpy(buf, g_dac_der, g_dac_der_len);
    memcpy(buf + g_dac_der_len, g_pai_der, g_pai_der_len);
    *out_len = total;
    return 0;
}

int attestation_sign_challenge(const uint8_t *challenge, size_t challenge_len,
                               uint8_t *sig, size_t *sig_len) {
    if (!challenge || challenge_len == 0 || !sig || !sig_len) return -1;
    if (!g_att_initialized || !g_pk_loaded) {
        printf("[ATT] ERROR: private key not loaded\n");
        return -1;
    }
    if (*sig_len < ATT_SIG_SIZE) {
        printf("[ATT] ERROR: signature buffer too small\n");
        return -1;
    }

    /* Hash challenge with SHA-256 then sign */
    uint8_t hash[32];
    mbedtls_sha256_context sha_ctx;
    mbedtls_sha256_init(&sha_ctx);
    mbedtls_sha256_starts(&sha_ctx, 0);
    mbedtls_sha256_update(&sha_ctx, challenge, challenge_len);
    mbedtls_sha256_finish(&sha_ctx, hash);
    mbedtls_sha256_free(&sha_ctx);

    size_t out_len = *sig_len;
    int ret = mbedtls_pk_sign(&g_pk_ctx,
                               MBEDTLS_MD_SHA256,
                               hash, sizeof(hash),
                               sig, out_len, &out_len,
                               pico_rng_callback, NULL);
    mbedtls_platform_zeroize(hash, sizeof(hash));

    if (ret != 0) {
        char errbuf[64];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        printf("[ATT] ERROR: pk_sign failed: %s (%d)\n", errbuf, ret);
        return -1;
    }

    *sig_len = out_len;
    printf("[ATT] Signed challenge (%zu-byte sig)\n", out_len);
    return 0;
}

int attestation_generate_attestation_tlv(const uint8_t *nonce, size_t nonce_len,
                                         uint8_t *out, size_t out_size,
                                         size_t *out_len) {
    if (!nonce || nonce_len != ATT_NONCE_SIZE || !out || !out_len) return -1;
    if (!g_att_initialized) return -1;

    /*
     * AttestationElements TLV structure (Matter Core Spec §11.22.5.4):
     *
     *   AnonymousTag, Structure {
     *     ContextTag(1): ByteString  -- CertificationDeclaration
     *     ContextTag(2): ByteString  -- AttestationNonce (32 bytes)
     *     ContextTag(3): UInt32      -- Timestamp (0 in test-mode)
     *   }
     */
    tlv_writer_t w;
    tlv_writer_init(&w, out, out_size);

    if (tlv_encode_structure_start(&w, 0) != 0) return -1;

    /* Tag 1: CertificationDeclaration (CD blob or empty) */
    const uint8_t *cd_data = (g_cd_der_len > 0) ? g_cd_der : (const uint8_t *)"";
    size_t         cd_len  = g_cd_der_len;
    if (tlv_encode_bytes(&w, 1, cd_data, cd_len) != 0) return -1;

    /* Tag 2: AttestationNonce */
    if (tlv_encode_bytes(&w, 2, nonce, nonce_len) != 0) return -1;

    /* Tag 3: Timestamp (0 in test-mode; production should use real time) */
    if (tlv_encode_uint32(&w, 3, 0) != 0) return -1;

    if (tlv_encode_container_end(&w) != 0) return -1;

    *out_len = tlv_writer_get_length(&w);
    printf("[ATT] AttestationElements TLV: %zu bytes\n", *out_len);
    return 0;
}

int attestation_is_ready(void) {
    return (g_att_initialized &&
            g_dac_der_len > 0 &&
            g_pai_der_len > 0 &&
            g_pk_loaded) ? 1 : 0;
}

void attestation_deinit(void) {
    if (!g_att_initialized) return;

    if (g_pk_loaded) {
        mbedtls_pk_free(&g_pk_ctx);
        g_pk_loaded = false;
    }

    mbedtls_platform_zeroize(g_dac_der, sizeof(g_dac_der));
    mbedtls_platform_zeroize(g_pai_der, sizeof(g_pai_der));
    mbedtls_platform_zeroize(g_cd_der,  sizeof(g_cd_der));

    g_dac_der_len     = 0;
    g_pai_der_len     = 0;
    g_cd_der_len      = 0;
    g_att_initialized = false;

    printf("[ATT] Attestation deinitialized\n");
}
