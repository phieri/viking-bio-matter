/*
 * attestation.h
 * Device Attestation support for Matter commissioning
 *
 * TEST-MODE ONLY – private key stored in LittleFS. See PRODUCTION_README.md
 * for production hardening requirements (secure element, real DAC/PAA, etc.)
 */

#ifndef ATTESTATION_H
#define ATTESTATION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LittleFS paths for attestation credentials */
#define ATT_CERTS_DIR       "/certs"
#define ATT_DAC_PATH        "/certs/dac.der"
#define ATT_PAI_PATH        "/certs/pai.der"
#define ATT_DAC_KEY_PATH    "/certs/dac_key.der"
#define ATT_CD_PATH         "/certs/cd.der"   /* Certification Declaration (optional) */

/* Maximum sizes for attestation blobs */
#define ATT_MAX_CERT_SIZE   600   /* DER-encoded cert (DAC or PAI) */
#define ATT_MAX_KEY_SIZE    128   /* DER-encoded EC private key (SEC.1/PKCS#8) */
#define ATT_MAX_CD_SIZE     512   /* Certification Declaration */
#define ATT_NONCE_SIZE       32   /* Attestation nonce */
#define ATT_SIG_SIZE         72   /* Max DER ECDSA-P256 signature */

/*
 * Initialize attestation subsystem.
 * Loads DAC, PAI and private key from LittleFS.  Must be called after
 * storage_adapter_init().
 *
 * @return 0 on success, -1 if credentials are missing or malformed.
 */
int attestation_init(void);

/*
 * Return the device attestation cert chain (DAC || PAI, DER-encoded bytes).
 * The controller uses this to verify the device against the PAA.
 *
 * @param buf       Output buffer.
 * @param buf_size  Size of buf.
 * @param out_len   Set to the number of bytes written.
 * @return 0 on success, -1 on error.
 */
int attestation_get_cert_chain(uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Sign a challenge with the device attestation private key (ECDSA-P256).
 * The signature is returned in DER format.
 *
 * @param challenge      Bytes to sign.
 * @param challenge_len  Length of challenge.
 * @param sig            Output buffer for DER signature.
 * @param sig_len        In: size of sig buffer.  Out: actual signature length.
 * @return 0 on success, -1 on error.
 */
int attestation_sign_challenge(const uint8_t *challenge, size_t challenge_len,
                               uint8_t *sig, size_t *sig_len);

/*
 * Build the AttestationElements TLV blob expected by the commissioning
 * controller (Matter Core Spec §11.22.5.4).
 *
 * Structure (anonymous struct, anonymous tags):
 *   Tag 1 (CertificationDeclaration): ByteString  – CD blob or empty stub
 *   Tag 2 (AttestationNonce):         ByteString  – nonce echoed from request
 *   Tag 3 (Timestamp):                UInt32      – Unix timestamp (0 in test-mode)
 *
 * @param nonce      Nonce received in AttestationRequest (must be 32 bytes).
 * @param nonce_len  Must be ATT_NONCE_SIZE.
 * @param out        Output buffer.
 * @param out_size   Size of out buffer.
 * @param out_len    Set to number of bytes written.
 * @return 0 on success, -1 on error.
 */
int attestation_generate_attestation_tlv(const uint8_t *nonce, size_t nonce_len,
                                         uint8_t *out, size_t out_size,
                                         size_t *out_len);

/*
 * Check whether attestation credentials are loaded and ready.
 *
 * @return 1 if ready, 0 otherwise.
 */
int attestation_is_ready(void);

/*
 * Release attestation resources and zeroize sensitive key material.
 */
void attestation_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* ATTESTATION_H */
