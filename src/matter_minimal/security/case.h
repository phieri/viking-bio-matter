/*
 * case.h
 * Certificate-Authenticated Session Establishment (CASE / Sigma) for Matter.
 *
 * Implements the responder side of Matter CASE (§4.13):
 *   Sigma1 → Sigma2 (Sigma2 resume not yet supported)
 *   Sigma3 → Session established
 *
 * TEST-MODE ONLY – uses test NOC stored in LittleFS.
 * See PRODUCTION_README.md for production hardening.
 */

#ifndef CASE_H
#define CASE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum encoded Sigma message sizes */
#define CASE_SIGMA1_MAX_SIZE   512
#define CASE_SIGMA2_MAX_SIZE   1024
#define CASE_SIGMA3_MAX_SIZE   1024

/* CASE session key lengths (AES-128-CCM) */
#define CASE_SESSION_KEY_LEN   16   /* bytes – I2R and R2I keys */

/* CASE resumption ID length */
#define CASE_RESUMPTION_ID_LEN 16

/*
 * Initialize the CASE subsystem.
 * Loads the device NOC from certificate_store and sets up internal state.
 * Must be called after attestation_init() and storage_adapter_init().
 *
 * @return 0 on success, -1 on error.
 */
int case_init(void);

/*
 * Process an incoming Sigma1 message from the initiator.
 * Generates and returns the Sigma2 response containing the responder's
 * ephemeral public key and encrypted TBE data.
 *
 * @param in        Raw Sigma1 TLV bytes.
 * @param in_len    Length of in.
 * @param out       Buffer to receive encoded Sigma2 bytes.
 * @param out_size  Size of out buffer.
 * @param out_len   Set to the actual length written.
 * @return 0 on success, -1 on error.
 */
int case_handle_sigma1(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Process an incoming Sigma2 message (not expected on the responder side;
 * provided for symmetry and potential host-side initiator use).
 *
 * @return 0 on success, -1 on error.
 */
int case_handle_sigma2(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Process an incoming Sigma3 message from the initiator.
 * Verifies the initiator credentials, derives the final session keys
 * and registers the CASE session in the session manager.
 *
 * @param in        Raw Sigma3 TLV bytes.
 * @param in_len    Length of in.
 * @param out       Buffer to receive encoded status/sigma3 ack bytes.
 * @param out_size  Size of out buffer.
 * @param out_len   Set to actual length (may be 0 if no response needed).
 * @return 0 on success, -1 on error.
 */
int case_handle_sigma3(const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_size, size_t *out_len);

/*
 * Check whether a CASE session is currently in progress.
 *
 * @return 1 if a handshake is in progress, 0 otherwise.
 */
int case_session_in_progress(void);

/*
 * Retrieve the session ID of the most recently established CASE session.
 * Valid only after case_handle_sigma3() returns 0.
 *
 * @param session_id_out Receives the session ID.
 * @return 0 on success, -1 if no session established.
 */
int case_get_established_session_id(uint16_t *session_id_out);

/*
 * Release CASE resources and zeroize ephemeral key material.
 */
void case_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* CASE_H */
