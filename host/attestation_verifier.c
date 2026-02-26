/*
 * attestation_verifier.c
 * Host-side Matter Device Attestation Verifier (test harness)
 *
 * Verifies the DAC → PAI → PAA certificate chain and validates the
 * attestation signature produced by the device.
 *
 * Build (requires mbedTLS or OpenSSL headers and library):
 *   Using mbedTLS:
 *     gcc -o attestation_verifier attestation_verifier.c \
 *         $(pkg-config --cflags --libs mbedtls mbedcrypto mbedx509) \
 *         -DUSE_MBEDTLS
 *
 *   Using OpenSSL:
 *     gcc -o attestation_verifier attestation_verifier.c \
 *         $(pkg-config --cflags --libs openssl)
 *
 * See docs/tests/ATT_TEST.md for example usage.
 *
 * WARNING: TEST-MODE ONLY. Do not use in production.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/* Crypto back-end selection                                            */
/* ------------------------------------------------------------------ */

#ifdef USE_MBEDTLS
  #include "mbedtls/x509_crt.h"
  #include "mbedtls/pk.h"
  #include "mbedtls/sha256.h"
  #include "mbedtls/error.h"
  #include "mbedtls/entropy.h"
  #include "mbedtls/ctr_drbg.h"
  #define HAVE_CRYPTO 1
#else
  /* Default to OpenSSL */
  #include <openssl/x509.h>
  #include <openssl/pem.h>
  #include <openssl/evp.h>
  #include <openssl/sha.h>
  #include <openssl/err.h>
  #define HAVE_CRYPTO 1
#endif

/* ------------------------------------------------------------------ */
/* Utility: read file into heap-allocated buffer                       */
/* ------------------------------------------------------------------ */

static uint8_t *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ERROR: Cannot open %s\n", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return NULL;
    }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}

/* ------------------------------------------------------------------ */
/* mbedTLS verifier                                                     */
/* ------------------------------------------------------------------ */

#ifdef USE_MBEDTLS

static int verify_chain_mbedtls(const char *dac_path, const char *pai_path,
                                 const char *paa_path) {
    mbedtls_x509_crt dac, pai, paa_store;
    uint32_t flags = 0;
    int ret;

    mbedtls_x509_crt_init(&dac);
    mbedtls_x509_crt_init(&pai);
    mbedtls_x509_crt_init(&paa_store);

    /* Load certs */
    ret = mbedtls_x509_crt_parse_file(&dac, dac_path);
    if (ret != 0) {
        char buf[128]; mbedtls_strerror(ret, buf, sizeof(buf));
        fprintf(stderr, "ERROR: Failed to parse DAC: %s\n", buf);
        goto cleanup;
    }
    ret = mbedtls_x509_crt_parse_file(&pai, pai_path);
    if (ret != 0) {
        char buf[128]; mbedtls_strerror(ret, buf, sizeof(buf));
        fprintf(stderr, "ERROR: Failed to parse PAI: %s\n", buf);
        goto cleanup;
    }
    if (paa_path) {
        ret = mbedtls_x509_crt_parse_file(&paa_store, paa_path);
        if (ret != 0) {
            char buf[128]; mbedtls_strerror(ret, buf, sizeof(buf));
            fprintf(stderr, "ERROR: Failed to parse PAA: %s\n", buf);
            goto cleanup;
        }
    }

    /* Build chain: DAC → PAI → PAA */
    dac.next = &pai;
    if (paa_path) pai.next = &paa_store;

    ret = mbedtls_x509_crt_verify(&dac,
                                   paa_path ? &paa_store : &pai,
                                   NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        char vbuf[256]; mbedtls_x509_crt_verify_info(vbuf, sizeof(vbuf), "  ", flags);
        fprintf(stderr, "ERROR: Certificate chain verification failed:\n%s\n", vbuf);
    } else {
        printf("[OK] Certificate chain verified: DAC → PAI%s\n",
               paa_path ? " → PAA" : " (self-signed or no PAA provided)");
    }

cleanup:
    dac.next = NULL;
    pai.next = NULL;
    mbedtls_x509_crt_free(&dac);
    mbedtls_x509_crt_free(&pai);
    mbedtls_x509_crt_free(&paa_store);
    return ret;
}

static int verify_signature_mbedtls(const char *dac_path,
                                     const char *att_elements_path,
                                     const char *sig_path) {
    size_t dac_len = 0, att_len = 0, sig_len = 0;
    uint8_t *dac_der = read_file(dac_path, &dac_len);
    uint8_t *att_tlv = read_file(att_elements_path, &att_len);
    uint8_t *sig_der  = read_file(sig_path, &sig_len);

    if (!dac_der || !att_tlv || !sig_der) {
        free(dac_der); free(att_tlv); free(sig_der);
        return -1;
    }

    mbedtls_x509_crt dac;
    mbedtls_x509_crt_init(&dac);
    int ret = mbedtls_x509_crt_parse_der(&dac, dac_der, dac_len);
    free(dac_der);
    if (ret != 0) {
        char buf[128]; mbedtls_strerror(ret, buf, sizeof(buf));
        fprintf(stderr, "ERROR: Parse DAC: %s\n", buf);
        free(att_tlv); free(sig_der);
        return -1;
    }

    /* SHA-256 over attestation elements */
    uint8_t hash[32];
    mbedtls_sha256((const unsigned char *)att_tlv, att_len, hash, 0);
    free(att_tlv);

    /* Verify signature */
    ret = mbedtls_pk_verify(&dac.pk,
                             MBEDTLS_MD_SHA256,
                             hash, sizeof(hash),
                             sig_der, sig_len);
    free(sig_der);
    mbedtls_x509_crt_free(&dac);

    if (ret != 0) {
        char buf[128]; mbedtls_strerror(ret, buf, sizeof(buf));
        fprintf(stderr, "ERROR: Signature verification failed: %s\n", buf);
    } else {
        printf("[OK] Attestation signature verified with DAC public key\n");
    }
    return ret;
}

#else /* OpenSSL */

/* ------------------------------------------------------------------ */
/* OpenSSL verifier                                                     */
/* ------------------------------------------------------------------ */

static int verify_chain_openssl(const char *dac_path, const char *pai_path,
                                 const char *paa_path) {
    X509_STORE     *store  = X509_STORE_new();
    X509_STORE_CTX *ctx    = X509_STORE_CTX_new();
    FILE           *f      = NULL;
    X509           *dac    = NULL;
    X509           *pai    = NULL;
    STACK_OF(X509) *chain  = sk_X509_new_null();
    int             ret    = -1;

    if (!store || !ctx || !chain) goto cleanup;

    /* Load PAA into trust store (optional) */
    if (paa_path) {
        if (X509_STORE_load_locations(store, paa_path, NULL) != 1) {
            fprintf(stderr, "ERROR: Failed to load PAA into trust store\n");
            goto cleanup;
        }
    }

    /* Load DAC */
    f = fopen(dac_path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", dac_path); goto cleanup; }
    dac = d2i_X509_fp(f, NULL);
    fclose(f); f = NULL;
    if (!dac) { fprintf(stderr, "ERROR: Failed to parse DAC\n"); goto cleanup; }

    /* Load PAI */
    f = fopen(pai_path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", pai_path); goto cleanup; }
    pai = d2i_X509_fp(f, NULL);
    fclose(f); f = NULL;
    if (!pai) { fprintf(stderr, "ERROR: Failed to parse PAI\n"); goto cleanup; }
    sk_X509_push(chain, pai);

    /* Verify DAC → PAI → [PAA] */
    X509_STORE_CTX_init(ctx, store, dac, chain);
    if (paa_path == NULL) {
        /* Accept partial chain when no PAA provided (for testing only) */
        X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_PARTIAL_CHAIN);
    }

    if (X509_verify_cert(ctx) == 1) {
        printf("[OK] Certificate chain verified: DAC → PAI%s\n",
               paa_path ? " → PAA" : " (no PAA provided)");
        ret = 0;
    } else {
        int err = X509_STORE_CTX_get_error(ctx);
        fprintf(stderr, "ERROR: Certificate chain verification failed: %s\n",
                X509_verify_cert_error_string(err));
    }

cleanup:
    if (ctx)   X509_STORE_CTX_free(ctx);
    if (store) X509_STORE_free(store);
    if (dac)   X509_free(dac);
    if (pai)   X509_free(pai);
    if (chain) sk_X509_free(chain); /* elements freed separately */
    ERR_print_errors_fp(stderr);
    return ret;
}

static int verify_signature_openssl(const char *dac_path,
                                     const char *att_elements_path,
                                     const char *sig_path) {
    int ret = -1;
    FILE *f = NULL;
    X509 *dac = NULL;
    EVP_PKEY *pkey = NULL;
    EVP_MD_CTX *mdctx = NULL;

    size_t att_len = 0, sig_len = 0;
    uint8_t *att_tlv = read_file(att_elements_path, &att_len);
    uint8_t *sig_der  = read_file(sig_path, &sig_len);

    if (!att_tlv || !sig_der) goto cleanup;

    f = fopen(dac_path, "rb");
    if (!f) { fprintf(stderr, "ERROR: Cannot open %s\n", dac_path); goto cleanup; }
    dac = d2i_X509_fp(f, NULL);
    fclose(f); f = NULL;
    if (!dac) { fprintf(stderr, "ERROR: Failed to parse DAC\n"); goto cleanup; }

    pkey = X509_get_pubkey(dac);
    if (!pkey) { fprintf(stderr, "ERROR: Failed to get DAC public key\n"); goto cleanup; }

    mdctx = EVP_MD_CTX_new();
    if (!mdctx) goto cleanup;

    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) != 1) goto cleanup;
    if (EVP_DigestVerifyUpdate(mdctx, att_tlv, att_len) != 1) goto cleanup;
    if (EVP_DigestVerifyFinal(mdctx, sig_der, sig_len) == 1) {
        printf("[OK] Attestation signature verified with DAC public key\n");
        ret = 0;
    } else {
        fprintf(stderr, "ERROR: Signature verification failed\n");
        ERR_print_errors_fp(stderr);
    }

cleanup:
    if (mdctx) EVP_MD_CTX_free(mdctx);
    if (pkey)  EVP_PKEY_free(pkey);
    if (dac)   X509_free(dac);
    free(att_tlv);
    free(sig_der);
    return ret;
}

#endif /* USE_MBEDTLS / OpenSSL */

/* ------------------------------------------------------------------ */
/* CLI                                                                  */
/* ------------------------------------------------------------------ */

static void usage(const char *prog) {
    fprintf(stderr,
        "Usage:\n"
        "  Verify cert chain:\n"
        "    %s chain <dac.der> <pai.der> [paa.der]\n\n"
        "  Verify attestation signature:\n"
        "    %s sig <dac.der> <att_elements.bin> <signature.der>\n\n"
        "File formats:\n"
        "  *.der       – DER-encoded X.509 certificate\n"
        "  att_elements.bin – raw AttestationElements TLV from device\n"
        "  signature.der    – DER-encoded ECDSA signature from device\n\n"
        "See docs/tests/ATT_TEST.md for end-to-end test instructions.\n",
        prog, prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char *cmd = argv[1];

    if (strcmp(cmd, "chain") == 0) {
        if (argc < 4) { usage(argv[0]); return 1; }
        const char *paa = (argc >= 5) ? argv[4] : NULL;
#ifdef USE_MBEDTLS
        return verify_chain_mbedtls(argv[2], argv[3], paa) == 0 ? 0 : 2;
#else
        return verify_chain_openssl(argv[2], argv[3], paa) == 0 ? 0 : 2;
#endif
    }

    if (strcmp(cmd, "sig") == 0) {
        if (argc < 5) { usage(argv[0]); return 1; }
#ifdef USE_MBEDTLS
        return verify_signature_mbedtls(argv[2], argv[3], argv[4]) == 0 ? 0 : 2;
#else
        return verify_signature_openssl(argv[2], argv[3], argv[4]) == 0 ? 0 : 2;
#endif
    }

    fprintf(stderr, "ERROR: Unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
}
