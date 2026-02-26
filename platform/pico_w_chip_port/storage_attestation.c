/*
 * storage_attestation.c
 * Platform glue: read/write attestation credential blobs to LittleFS.
 *
 * Attestation credentials use flat LittleFS paths (no subdirectory) because
 * storage_adapter does not create parent directories automatically.  Paths:
 *   /att_dac   – Device Attestation Certificate
 *   /att_pai   – Product Attestation Intermediate cert
 *   /att_key   – DAC private key (EC, DER)
 *   /att_cd    – Certification Declaration (optional)
 *   /att_noc   – Node Operational Certificate
 *   /att_icac  – Intermediate CA cert (optional)
 *   /att_rcac  – Root CA cert (optional)
 *
 * WARNING: storing a private key in flash is TEST-MODE ONLY.
 * See PRODUCTION_README.md for production hardening guidance.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* Platform storage via existing storage_adapter API */
extern int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len);
extern int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len);
extern int storage_adapter_delete(const char *key);

/* LittleFS directory helper – create /certs directory if needed.
 * storage_adapter_write already creates intermediate path components because
 * LittleFS files are stored directly with the full path as the filename; a
 * flat file name like "/certs/dac.der" is treated as a single filename.
 * No explicit mkdir is needed. */

/*
 * storage_attestation_read – read a blob from LittleFS by path.
 *
 * @param path     LittleFS file path (e.g. "/certs/dac.der").
 * @param buf      Output buffer.
 * @param buf_size Size of buf.
 * @param out_len  Set to the number of bytes read.
 * @return 0 on success, -1 on error (file not found or too large).
 */
int storage_attestation_read(const char *path, uint8_t *buf,
                              size_t buf_size, size_t *out_len) {
    if (!path || !buf || !out_len) return -1;
    *out_len = 0;

    size_t actual = 0;
    int ret = storage_adapter_read(path, buf, buf_size, &actual);
    if (ret != 0) {
        /* File absent – not an error worth logging at WARNING level */
        return -1;
    }
    if (actual == 0) {
        return -1;
    }
    *out_len = actual;
    return 0;
}

/*
 * storage_attestation_write – write a blob to LittleFS by path.
 *
 * Used by tools/provision_attestation.py via USB serial command channel
 * (or any other provisioning mechanism).
 *
 * @param path    LittleFS file path.
 * @param buf     Data to write.
 * @param buf_len Length of data.
 * @return 0 on success, -1 on error.
 */
int storage_attestation_write(const char *path, const uint8_t *buf,
                               size_t buf_len) {
    if (!path || !buf || buf_len == 0) return -1;
    int ret = storage_adapter_write(path, buf, buf_len);
    if (ret == 0) {
        printf("[StorageAtt] Wrote %zu bytes to %s\n", buf_len, path);
    } else {
        printf("[StorageAtt] ERROR: Failed to write %s\n", path);
    }
    return ret;
}

/*
 * storage_attestation_delete – remove an attestation credential file.
 *
 * @param path LittleFS file path.
 * @return 0 on success, -1 on error.
 */
int storage_attestation_delete(const char *path) {
    if (!path) return -1;
    return storage_adapter_delete(path);
}
