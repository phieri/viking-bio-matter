/*
 * certificate_store.h
 * Simple persistent storage for Matter NOC and operational certificate chain.
 *
 * Credentials are stored in LittleFS via storage_adapter_write/read.
 * Keys:
 *   /certs/noc.der   – Node Operational Certificate (DER)
 *   /certs/icac.der  – Intermediate CA cert (optional, DER)
 *   /certs/rcac.der  – Root CA cert (optional, DER)
 */

#ifndef CERTIFICATE_STORE_H
#define CERTIFICATE_STORE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LittleFS paths */
#define CERT_STORE_NOC_PATH   "/certs/noc.der"
#define CERT_STORE_ICAC_PATH  "/certs/icac.der"
#define CERT_STORE_RCAC_PATH  "/certs/rcac.der"

/* Maximum DER certificate size */
#define CERT_STORE_MAX_CERT_SIZE  600

/*
 * Save the Node Operational Certificate (received during AddNOC).
 *
 * @param noc      DER-encoded NOC bytes.
 * @param noc_len  Length of noc.
 * @return 0 on success, -1 on error.
 */
int certificate_store_save_noc(const uint8_t *noc, size_t noc_len);

/*
 * Load the stored NOC.
 *
 * @param buf      Output buffer.
 * @param buf_size Size of buf.
 * @param out_len  Set to the actual length of the stored NOC.
 * @return 0 on success, -1 if not stored or on error.
 */
int certificate_store_load_noc(uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Check whether a NOC has been stored.
 *
 * @return 1 if a NOC is stored, 0 otherwise.
 */
int certificate_store_has_noc(void);

/*
 * Save the Intermediate CA Certificate (ICAC, optional).
 */
int certificate_store_save_icac(const uint8_t *icac, size_t icac_len);

/*
 * Load the stored ICAC.
 */
int certificate_store_load_icac(uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Save the Root CA Certificate (RCAC / Trust Anchor).
 */
int certificate_store_save_rcac(const uint8_t *rcac, size_t rcac_len);

/*
 * Load the stored RCAC.
 */
int certificate_store_load_rcac(uint8_t *buf, size_t buf_size, size_t *out_len);

/*
 * Delete all stored operational certificates (NOC, ICAC, RCAC).
 * Call during factory reset.
 *
 * @return 0 on success (even if files were absent), -1 on error.
 */
int certificate_store_clear_all(void);

#ifdef __cplusplus
}
#endif

#endif /* CERTIFICATE_STORE_H */
