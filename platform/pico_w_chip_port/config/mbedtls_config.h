/**
 * @file mbedtls_config.h
 * @brief mbedTLS configuration for Viking Bio Matter Bridge (Pico W)
 * 
 * Minimal mbedTLS configuration for Pico W.
 * Vendored to resolve configuration header access issues.
 */

#ifndef MBEDTLS_CONFIG_H
#define MBEDTLS_CONFIG_H

// System support
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_FREE_MACRO     free
#define MBEDTLS_PLATFORM_CALLOC_MACRO   calloc

// Disable features not needed for embedded
#define MBEDTLS_NO_PLATFORM_ENTROPY

// Core crypto modules - minimal set for Matter
#define MBEDTLS_AES_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_BASE64_C
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_CCM_C
#define MBEDTLS_CIPHER_C
// Skip CTR_DRBG due to SDK bug - use HMAC_DRBG instead
#define MBEDTLS_HMAC_DRBG_C
#define MBEDTLS_ECDH_C
#define MBEDTLS_ECDSA_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ENTROPY_C
#define MBEDTLS_GCM_C
#define MBEDTLS_HKDF_C
#define MBEDTLS_MD_C
#define MBEDTLS_OID_C
#define MBEDTLS_PK_C
#define MBEDTLS_PK_PARSE_C
#define MBEDTLS_PK_WRITE_C
#define MBEDTLS_PKCS5_C
#define MBEDTLS_RSA_C
#define MBEDTLS_SHA1_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_SHA512_C
// Skip X509 due to SDK bugs with missing includes
// #define MBEDTLS_X509_USE_C
// #define MBEDTLS_X509_CRT_PARSE_C

// Cipher modes
#define MBEDTLS_CIPHER_MODE_CBC
#define MBEDTLS_CIPHER_MODE_CTR

// ECP curves for Matter
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED

// Key exchange
#define MBEDTLS_KEY_EXCHANGE_ECDHE_ECDSA_ENABLED

#endif /* MBEDTLS_CONFIG_H */
