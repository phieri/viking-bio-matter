/*
 * case.c
 * Certificate-Authenticated Session Establishment (CASE / Sigma) – responder.
 *
 * Implements Matter CASE §4.13:
 *   Sigma1 → derive shared secret → build Sigma2
 *   Sigma3 → verify credentials → create CASE session
 *
 * Cryptographic primitives (all in the project's mbedTLS config):
 *   mbedtls_ecp_gen_keypair – ephemeral P-256 keypair generation
 *   mbedtls_ecp_mul         – ECDH shared-secret computation
 *   mbedtls_hkdf            – HKDF-SHA256 key derivation
 *   mbedtls_pk_sign         – ECDSA-P256 attestation signing
 *   mbedtls_ccm             – AES-128-CCM TBE encryption/decryption
 *   mbedtls_sha256          – transcript hashing
 *
 * WARNING – TEST-MODE ONLY.  See PRODUCTION_README.md.
 */

#include "case.h"
#include "attestation.h"
#include "certificate_store.h"
#include "session_mgr.h"
#include "../codec/tlv.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* mbedTLS */
#include "mbedtls/hkdf.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ccm.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"
#include "mbedtls/platform_util.h"
#include "mbedtls/error.h"

/* Pico hardware RNG */
#include "pico/rand.h"

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define P256_PUBKEY_SIZE   65   /* 0x04 || X(32) || Y(32) */
#define P256_PRIVKEY_SIZE  32
#define SHA256_SIZE        32

/* HKDF info labels (Matter spec §4.13) */
static const uint8_t HKDF_INFO_S2K[] = {
    'S','i','g','m','a','S','e','s','s','i','o','n','K','e','y','s'
}; /* "SigmaSessionKeys" */

static const uint8_t HKDF_INFO_TBE2[] = {
    'S','i','g','m','a','2','R','e','s','u','m','e',
    'S','e','s','s','i','o','n','K','e','y'
}; /* "Sigma2ResumeSessionKey" */

static const uint8_t HKDF_INFO_TBE3[] = {
    'S','i','g','m','a','3','T','B','E','K','e','y'
}; /* "Sigma3TBEKey" */

#define CCM_NONCE_SIZE   13
#define CCM_TAG_SIZE     16

/* ------------------------------------------------------------------ */
/* State machine                                                        */
/* ------------------------------------------------------------------ */

typedef enum {
    CASE_STATE_IDLE = 0,
    CASE_STATE_SIGMA2_SENT,
    CASE_STATE_ESTABLISHED,
} case_state_t;

typedef struct {
    case_state_t state;

    uint8_t  initiator_random[32];
    uint16_t initiator_session_id;

    uint8_t  responder_random[32];
    uint16_t responder_session_id;

    uint8_t  reph_pub[P256_PUBKEY_SIZE];  /* Responder ephemeral public key */
    uint8_t  ieph_pub[P256_PUBKEY_SIZE];  /* Initiator ephemeral public key */
    uint8_t  reph_priv[P256_PRIVKEY_SIZE]; /* Responder ephemeral private key */

    uint8_t  shared_secret[P256_PRIVKEY_SIZE]; /* ECDH Z (X-coord) */
    uint8_t  i2r_key[CASE_SESSION_KEY_LEN];
    uint8_t  r2i_key[CASE_SESSION_KEY_LEN];

    uint8_t  t1_hash[SHA256_SIZE]; /* Hash(Sigma1) */
    uint8_t  t2_hash[SHA256_SIZE]; /* Hash(Sigma1 || Sigma2) */

    uint16_t established_session_id;
    bool     session_established;
} case_ctx_t;

static case_ctx_t g_case_ctx;
static bool       g_case_initialized = false;

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int pico_rng_cb(void *ctx, unsigned char *buf, size_t len) {
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

static uint16_t random_session_id(void) {
    uint16_t id;
    do { pico_rng_cb(NULL, (uint8_t *)&id, sizeof(id)); } while (id == 0);
    return id;
}

static void log_mbedtls_err(const char *fn, int ret) {
    char buf[64]; mbedtls_strerror(ret, buf, sizeof(buf));
    printf("[CASE] %s: %s (%d)\n", fn, buf, ret);
}

static int hkdf_derive(const uint8_t *salt, size_t salt_len,
                       const uint8_t *ikm,  size_t ikm_len,
                       const uint8_t *info, size_t info_len,
                       uint8_t *out, size_t out_len) {
    const mbedtls_md_info_t *md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if (!md) return -1;
    return mbedtls_hkdf(md, salt, salt_len, ikm, ikm_len,
                        info, info_len, out, out_len);
}

/* TLV: find first element with given context tag, return byte string */
static int tlv_find_bytes(const uint8_t *buf, size_t len, uint8_t tag,
                          uint8_t *out, size_t max_out, size_t *out_len) {
    tlv_reader_t r;
    tlv_element_t el;
    tlv_reader_init(&r, buf, len);
    if (tlv_reader_peek(&r, &el) == 0 && el.type == TLV_TYPE_STRUCTURE)
        tlv_reader_next(&r, &el);
    while (tlv_reader_next(&r, &el) == 0) {
        if (el.type == TLV_TYPE_END_OF_CONTAINER) break;
        if (el.tag_type == TLV_TAG_CONTEXT_SPECIFIC && el.tag == tag
                && el.type == TLV_TYPE_BYTE_STRING) {
            size_t n = el.value.bytes.length;
            if (n > max_out) n = max_out;
            memcpy(out, el.value.bytes.data, n);
            *out_len = n;
            return 0;
        }
    }
    return -1;
}

/* TLV: find uint16 element with given context tag */
static int tlv_find_uint16(const uint8_t *buf, size_t len, uint8_t tag,
                           uint16_t *out) {
    tlv_reader_t r;
    tlv_element_t el;
    tlv_reader_init(&r, buf, len);
    if (tlv_reader_peek(&r, &el) == 0 && el.type == TLV_TYPE_STRUCTURE)
        tlv_reader_next(&r, &el);
    while (tlv_reader_next(&r, &el) == 0) {
        if (el.type == TLV_TYPE_END_OF_CONTAINER) break;
        if (el.tag_type == TLV_TAG_CONTEXT_SPECIFIC && el.tag == tag
                && el.type == TLV_TYPE_UNSIGNED_INT) {
            *out = (uint16_t)el.value.u16;
            return 0;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* AES-128-CCM helpers                                                  */
/* ------------------------------------------------------------------ */

static int ccm_encrypt(const uint8_t *key,
                       const uint8_t *plain, size_t plain_len,
                       uint8_t *enc_out) {
    /* enc_out must be plain_len + CCM_TAG_SIZE bytes */
    uint8_t nonce[CCM_NONCE_SIZE];
    memset(nonce, 0, sizeof(nonce));

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    int ret = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                  key, CASE_SESSION_KEY_LEN * 8);
    if (ret == 0) {
        ret = mbedtls_ccm_encrypt_and_tag(&ccm, plain_len,
                                           nonce, CCM_NONCE_SIZE,
                                           NULL, 0,
                                           plain, enc_out,
                                           enc_out + plain_len, CCM_TAG_SIZE);
    }
    mbedtls_ccm_free(&ccm);
    return ret;
}

static int ccm_decrypt(const uint8_t *key,
                       const uint8_t *enc, size_t enc_len,
                       uint8_t *plain_out, size_t *plain_len_out) {
    if (enc_len <= CCM_TAG_SIZE) return -1;
    size_t plen = enc_len - CCM_TAG_SIZE;

    uint8_t nonce[CCM_NONCE_SIZE];
    memset(nonce, 0, sizeof(nonce));

    mbedtls_ccm_context ccm;
    mbedtls_ccm_init(&ccm);
    int ret = mbedtls_ccm_setkey(&ccm, MBEDTLS_CIPHER_ID_AES,
                                  key, CASE_SESSION_KEY_LEN * 8);
    if (ret == 0) {
        ret = mbedtls_ccm_auth_decrypt(&ccm, plen,
                                        nonce, CCM_NONCE_SIZE,
                                        NULL, 0,
                                        enc, plain_out,
                                        enc + plen, CCM_TAG_SIZE);
    }
    mbedtls_ccm_free(&ccm);
    if (ret == 0 && plain_len_out) *plain_len_out = plen;
    return ret;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int case_init(void) {
    if (g_case_initialized) return 0;
    printf("[CASE] Initializing CASE subsystem\n");
    memset(&g_case_ctx, 0, sizeof(g_case_ctx));
    g_case_ctx.state = CASE_STATE_IDLE;
    g_case_initialized = true;
    return 0;
}

int case_handle_sigma1(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len) {
    if (!g_case_initialized || !in || !out || !out_len) return -1;

    printf("[CASE] Handling Sigma1 (%zu bytes)\n", in_len);

    /* Reset state */
    mbedtls_platform_zeroize(&g_case_ctx, sizeof(g_case_ctx));
    g_case_ctx.state = CASE_STATE_IDLE;

    /* ----------------------------------------------------------
     * Parse Sigma1 TLV (Matter §4.13.2.1)
     *   Tag 1: InitiatorRandom   ByteString(32)
     *   Tag 2: InitiatorSessionId UInt16
     *   Tag 3: DestinationId     ByteString(32)
     *   Tag 4: InitiatorEphPubKey ByteString(65)
     * ---------------------------------------------------------- */
    size_t tmp_len = 0;

    if (tlv_find_bytes(in, in_len, 1,
                       g_case_ctx.initiator_random, 32, &tmp_len) != 0
            || tmp_len != 32) {
        printf("[CASE] Sigma1: missing InitiatorRandom\n");
        return -1;
    }
    if (tlv_find_uint16(in, in_len, 2, &g_case_ctx.initiator_session_id) != 0) {
        printf("[CASE] Sigma1: missing InitiatorSessionId\n");
        return -1;
    }
    if (tlv_find_bytes(in, in_len, 4,
                       g_case_ctx.ieph_pub, P256_PUBKEY_SIZE, &tmp_len) != 0
            || tmp_len != P256_PUBKEY_SIZE) {
        printf("[CASE] Sigma1: missing InitiatorEphPubKey\n");
        return -1;
    }

    /* Transcript T1 = SHA-256(Sigma1) */
    mbedtls_sha256((const unsigned char *)in, in_len, g_case_ctx.t1_hash, 0);

    /* ----------------------------------------------------------
     * Generate responder ephemeral P-256 keypair
     * ---------------------------------------------------------- */
    mbedtls_ecp_group grp;
    mbedtls_ecp_group_init(&grp);
    int ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    if (ret != 0) { log_mbedtls_err("ecp_group_load", ret); goto err_grp; }

    {
        mbedtls_mpi    d;       /* private scalar */
        mbedtls_ecp_point Q;   /* public point */
        mbedtls_mpi_init(&d);
        mbedtls_ecp_point_init(&Q);

        ret = mbedtls_ecp_gen_keypair(&grp, &d, &Q, pico_rng_cb, NULL);
        if (ret != 0) {
            log_mbedtls_err("ecp_gen_keypair", ret);
            mbedtls_mpi_free(&d);
            mbedtls_ecp_point_free(&Q);
            goto err_grp;
        }

        /* Export Reph public key (uncompressed) */
        size_t pub_len = P256_PUBKEY_SIZE;
        ret = mbedtls_ecp_point_write_binary(&grp, &Q,
                                              MBEDTLS_ECP_PF_UNCOMPRESSED,
                                              &pub_len,
                                              g_case_ctx.reph_pub,
                                              P256_PUBKEY_SIZE);
        if (ret != 0 || pub_len != P256_PUBKEY_SIZE) {
            log_mbedtls_err("ecp_point_write_binary(Reph)", ret);
            mbedtls_mpi_free(&d);
            mbedtls_ecp_point_free(&Q);
            goto err_grp;
        }

        /* Export Reph private key (raw scalar, 32 bytes) */
        ret = mbedtls_mpi_write_binary(&d,
                                        g_case_ctx.reph_priv, P256_PRIVKEY_SIZE);
        if (ret != 0) {
            log_mbedtls_err("mpi_write_binary(Reph_priv)", ret);
            mbedtls_mpi_free(&d);
            mbedtls_ecp_point_free(&Q);
            goto err_grp;
        }

        /* ECDH: Z = d_R * Q_I  (import Ieph_pub → ecp_point) */
        mbedtls_ecp_point I_pub;
        mbedtls_ecp_point_init(&I_pub);
        ret = mbedtls_ecp_point_read_binary(&grp, &I_pub,
                                             g_case_ctx.ieph_pub,
                                             P256_PUBKEY_SIZE);
        if (ret != 0) {
            log_mbedtls_err("ecp_point_read_binary(Ieph)", ret);
            mbedtls_mpi_free(&d);
            mbedtls_ecp_point_free(&Q);
            mbedtls_ecp_point_free(&I_pub);
            goto err_grp;
        }

        mbedtls_ecp_point Z;
        mbedtls_ecp_point_init(&Z);
        ret = mbedtls_ecp_mul(&grp, &Z, &d, &I_pub, pico_rng_cb, NULL);
        if (ret == 0) {
            ret = mbedtls_mpi_write_binary(
                      &Z.MBEDTLS_PRIVATE(X),
                      g_case_ctx.shared_secret, P256_PRIVKEY_SIZE);
        }

        mbedtls_ecp_point_free(&Z);
        mbedtls_ecp_point_free(&I_pub);
        mbedtls_mpi_free(&d);
        mbedtls_ecp_point_free(&Q);

        if (ret != 0) { log_mbedtls_err("ecdh_shared_secret", ret); goto err_grp; }
    }

    mbedtls_ecp_group_free(&grp);

    /* ----------------------------------------------------------
     * Derive I2R + R2I session keys via HKDF
     *   HKDF(salt=shared_secret, IKM=T1, info="SigmaSessionKeys")
     *   → 48 bytes: I2R(16) || R2I(16) || unused(16)
     * ---------------------------------------------------------- */
    {
        uint8_t keys[48];
        ret = hkdf_derive(g_case_ctx.shared_secret, SHA256_SIZE,
                          g_case_ctx.t1_hash, SHA256_SIZE,
                          HKDF_INFO_S2K, sizeof(HKDF_INFO_S2K),
                          keys, sizeof(keys));
        if (ret != 0) {
            log_mbedtls_err("hkdf S2K", ret);
            mbedtls_platform_zeroize(keys, sizeof(keys));
            return -1;
        }
        memcpy(g_case_ctx.i2r_key, keys,      CASE_SESSION_KEY_LEN);
        memcpy(g_case_ctx.r2i_key, keys + 16, CASE_SESSION_KEY_LEN);
        mbedtls_platform_zeroize(keys, sizeof(keys));
    }

    /* Generate responder random and session ID */
    pico_rng_cb(NULL, g_case_ctx.responder_random, 32);
    g_case_ctx.responder_session_id = random_session_id();

    /* ----------------------------------------------------------
     * Build Sigma2-TBE plaintext
     *   { Tag1: ResponderNOC, Tag3: Signature(TBS2) }
     *   TBS2 = T1 || Reph_pub || Ieph_pub
     *   Signature = ECDSA-P256-SHA256(DAC_key, TBS2)
     * ---------------------------------------------------------- */
    static uint8_t tbe2_plain[768];
    size_t         tbe2_plain_len = 0;
    {
        uint8_t noc[CERT_STORE_MAX_CERT_SIZE];
        size_t  noc_len = 0;
        certificate_store_load_noc(noc, sizeof(noc), &noc_len);

        /* Compute TBS2 = T1 || Reph_pub || Ieph_pub */
        uint8_t tbs2[SHA256_SIZE + P256_PUBKEY_SIZE * 2];
        memcpy(tbs2,                                  g_case_ctx.t1_hash,  SHA256_SIZE);
        memcpy(tbs2 + SHA256_SIZE,                    g_case_ctx.reph_pub, P256_PUBKEY_SIZE);
        memcpy(tbs2 + SHA256_SIZE + P256_PUBKEY_SIZE, g_case_ctx.ieph_pub, P256_PUBKEY_SIZE);

        uint8_t sig[ATT_SIG_SIZE];
        size_t  sig_len = sizeof(sig);
        int sign_ok = attestation_sign_challenge(tbs2, sizeof(tbs2), sig, &sig_len);

        tlv_writer_t w;
        tlv_writer_init(&w, tbe2_plain, sizeof(tbe2_plain));
        tlv_encode_structure_start(&w, 0);
        if (noc_len > 0)  tlv_encode_bytes(&w, 1, noc, noc_len);
        if (sign_ok == 0) tlv_encode_bytes(&w, 3, sig, sig_len);
        tlv_encode_container_end(&w);
        tbe2_plain_len = tlv_writer_get_length(&w);
    }

    /* ----------------------------------------------------------
     * Derive TBE2 key via HKDF, then AES-128-CCM encrypt
     * ---------------------------------------------------------- */
    static uint8_t tbe2_enc[768 + CCM_TAG_SIZE];
    size_t         tbe2_enc_len = tbe2_plain_len + CCM_TAG_SIZE;
    {
        uint8_t tbe2_key[CASE_SESSION_KEY_LEN];
        ret = hkdf_derive(g_case_ctx.shared_secret, SHA256_SIZE,
                          g_case_ctx.t1_hash, SHA256_SIZE,
                          HKDF_INFO_TBE2, sizeof(HKDF_INFO_TBE2),
                          tbe2_key, sizeof(tbe2_key));
        if (ret != 0) {
            log_mbedtls_err("hkdf TBE2", ret);
            mbedtls_platform_zeroize(tbe2_key, sizeof(tbe2_key));
            return -1;
        }
        ret = ccm_encrypt(tbe2_key, tbe2_plain, tbe2_plain_len, tbe2_enc);
        mbedtls_platform_zeroize(tbe2_key, sizeof(tbe2_key));
        if (ret != 0) { log_mbedtls_err("ccm_encrypt TBE2", ret); return -1; }
    }

    /* ----------------------------------------------------------
     * Build Sigma2 TLV
     *   Tag 1: ResponderRandom     ByteString(32)
     *   Tag 2: ResponderSessionId  UInt16
     *   Tag 3: ResponderEphPubKey  ByteString(65)
     *   Tag 4: Encrypted2          ByteString(ciphertext+tag)
     * ---------------------------------------------------------- */
    {
        tlv_writer_t w;
        tlv_writer_init(&w, out, out_size);
        tlv_encode_structure_start(&w, 0);
        tlv_encode_bytes(&w, 1, g_case_ctx.responder_random, 32);
        tlv_encode_uint16(&w, 2, g_case_ctx.responder_session_id);
        tlv_encode_bytes(&w, 3, g_case_ctx.reph_pub, P256_PUBKEY_SIZE);
        tlv_encode_bytes(&w, 4, tbe2_enc, tbe2_enc_len);
        tlv_encode_container_end(&w);
        *out_len = tlv_writer_get_length(&w);
    }

    /* Transcript T2 = SHA-256(Sigma1 || Sigma2) */
    {
        mbedtls_sha256_context sh;
        mbedtls_sha256_init(&sh);
        mbedtls_sha256_starts(&sh, 0);
        mbedtls_sha256_update(&sh, in, in_len);
        mbedtls_sha256_update(&sh, out, *out_len);
        mbedtls_sha256_finish(&sh, g_case_ctx.t2_hash);
        mbedtls_sha256_free(&sh);
    }

    g_case_ctx.state = CASE_STATE_SIGMA2_SENT;
    printf("[CASE] Sigma2 built (%zu bytes)\n", *out_len);
    return 0;

err_grp:
    mbedtls_ecp_group_free(&grp);
    return -1;
}

int case_handle_sigma2(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len) {
    /* Sigma2 is sent by the responder; on device side this is a no-op. */
    (void)in; (void)in_len; (void)out; (void)out_size;
    if (out_len) *out_len = 0;
    return 0;
}

int case_handle_sigma3(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len) {
    if (!g_case_initialized || !in || !out_len) return -1;
    if (g_case_ctx.state != CASE_STATE_SIGMA2_SENT) {
        printf("[CASE] Sigma3: unexpected state %d\n", g_case_ctx.state);
        return -1;
    }
    printf("[CASE] Handling Sigma3 (%zu bytes)\n", in_len);

    /* Extract Encrypted3 blob (Tag 1) */
    static uint8_t enc3[CASE_SIGMA3_MAX_SIZE];
    size_t         enc3_len = 0;
    if (tlv_find_bytes(in, in_len, 1, enc3, sizeof(enc3), &enc3_len) != 0
            || enc3_len <= CCM_TAG_SIZE) {
        printf("[CASE] Sigma3: missing or short Encrypted3\n");
        return -1;
    }

    /* Derive TBE3 key via HKDF(Z, T2, "Sigma3TBEKey") */
    uint8_t tbe3_key[CASE_SESSION_KEY_LEN];
    int ret = hkdf_derive(g_case_ctx.shared_secret, SHA256_SIZE,
                          g_case_ctx.t2_hash, SHA256_SIZE,
                          HKDF_INFO_TBE3, sizeof(HKDF_INFO_TBE3),
                          tbe3_key, sizeof(tbe3_key));
    if (ret != 0) {
        log_mbedtls_err("hkdf TBE3", ret);
        mbedtls_platform_zeroize(tbe3_key, sizeof(tbe3_key));
        return -1;
    }

    /* Decrypt TBE3 */
    static uint8_t tbe3_plain[CASE_SIGMA3_MAX_SIZE];
    size_t         tbe3_plain_len = 0;
    ret = ccm_decrypt(tbe3_key, enc3, enc3_len, tbe3_plain, &tbe3_plain_len);
    mbedtls_platform_zeroize(tbe3_key, sizeof(tbe3_key));
    if (ret != 0) {
        log_mbedtls_err("ccm_auth_decrypt TBE3", ret);
        return -1;
    }

    /*
     * TEST-MODE: Accept any TBE3 that decrypts + authenticates correctly.
     * Production TODO: parse initiator NOC (TBE3 Tag 1), verify against RCAC,
     * extract public key, verify Sigma3 signature (TBE3 Tag 3) over T2.
     */
    printf("[CASE] TBE3 decrypted (%zu bytes) "
           "[test-mode: NOC chain verify skipped]\n", tbe3_plain_len);

    /* Store initiator NOC if present (TBE3 Tag 1) */
    {
        uint8_t noc[CERT_STORE_MAX_CERT_SIZE];
        size_t  noc_len = 0;
        if (tlv_find_bytes(tbe3_plain, tbe3_plain_len, 1,
                           noc, sizeof(noc), &noc_len) == 0 && noc_len > 0) {
            certificate_store_save_noc(noc, noc_len);
        }
    }

    /* Register CASE session in session manager (use I2R key) */
    g_case_ctx.established_session_id = g_case_ctx.responder_session_id;
    if (session_create(g_case_ctx.established_session_id,
                       g_case_ctx.i2r_key, CASE_SESSION_KEY_LEN) != 0) {
        printf("[CASE] WARNING: session_create failed\n");
    } else {
        printf("[CASE] Session %u established\n",
               g_case_ctx.established_session_id);
    }

    g_case_ctx.state             = CASE_STATE_ESTABLISHED;
    g_case_ctx.session_established = true;

    /* Zeroize ephemeral key material */
    mbedtls_platform_zeroize(g_case_ctx.shared_secret, SHA256_SIZE);
    mbedtls_platform_zeroize(g_case_ctx.i2r_key, CASE_SESSION_KEY_LEN);
    mbedtls_platform_zeroize(g_case_ctx.r2i_key, CASE_SESSION_KEY_LEN);
    mbedtls_platform_zeroize(g_case_ctx.reph_priv, P256_PRIVKEY_SIZE);

    if (out_len) *out_len = 0;
    (void)out; (void)out_size;
    return 0;
}

int case_session_in_progress(void) {
    return (g_case_initialized &&
            g_case_ctx.state == CASE_STATE_SIGMA2_SENT) ? 1 : 0;
}

int case_get_established_session_id(uint16_t *session_id_out) {
    if (!session_id_out) return -1;
    if (!g_case_initialized || !g_case_ctx.session_established) return -1;
    *session_id_out = g_case_ctx.established_session_id;
    return 0;
}

void case_deinit(void) {
    if (!g_case_initialized) return;
    mbedtls_platform_zeroize(&g_case_ctx, sizeof(g_case_ctx));
    g_case_initialized = false;
    printf("[CASE] CASE deinitialized\n");
}
