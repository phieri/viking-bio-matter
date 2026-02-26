/*
 * certificate_store.c
 * Persistent storage for Matter operational certificates (NOC / ICAC / RCAC).
 * Uses storage_adapter_read/write (LittleFS on Pico W flash).
 */

#include "certificate_store.h"
#include <string.h>
#include <stdio.h>

/* Platform storage functions */
extern int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len);
extern int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len);
extern int storage_adapter_delete(const char *key);

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static int cert_save(const char *path, const uint8_t *der, size_t len) {
    if (!path || !der || len == 0 || len > CERT_STORE_MAX_CERT_SIZE) {
        printf("[CertStore] ERROR: invalid params for %s\n", path ? path : "?");
        return -1;
    }
    int ret = storage_adapter_write(path, der, len);
    if (ret == 0) {
        printf("[CertStore] Saved %zu bytes to %s\n", len, path);
    } else {
        printf("[CertStore] ERROR: Failed to save %s\n", path);
    }
    return ret;
}

static int cert_load(const char *path,
                     uint8_t *buf, size_t buf_size, size_t *out_len) {
    if (!path || !buf || !out_len) return -1;
    *out_len = 0;
    size_t actual = 0;
    int ret = storage_adapter_read(path, buf, buf_size, &actual);
    if (ret != 0 || actual == 0) {
        return -1;
    }
    *out_len = actual;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int certificate_store_save_noc(const uint8_t *noc, size_t noc_len) {
    return cert_save(CERT_STORE_NOC_PATH, noc, noc_len);
}

int certificate_store_load_noc(uint8_t *buf, size_t buf_size, size_t *out_len) {
    return cert_load(CERT_STORE_NOC_PATH, buf, buf_size, out_len);
}

int certificate_store_has_noc(void) {
    uint8_t tmp[CERT_STORE_MAX_CERT_SIZE];
    size_t  len = 0;
    return (certificate_store_load_noc(tmp, sizeof(tmp), &len) == 0 &&
            len > 0) ? 1 : 0;
}

int certificate_store_save_icac(const uint8_t *icac, size_t icac_len) {
    return cert_save(CERT_STORE_ICAC_PATH, icac, icac_len);
}

int certificate_store_load_icac(uint8_t *buf, size_t buf_size, size_t *out_len) {
    return cert_load(CERT_STORE_ICAC_PATH, buf, buf_size, out_len);
}

int certificate_store_save_rcac(const uint8_t *rcac, size_t rcac_len) {
    return cert_save(CERT_STORE_RCAC_PATH, rcac, rcac_len);
}

int certificate_store_load_rcac(uint8_t *buf, size_t buf_size, size_t *out_len) {
    return cert_load(CERT_STORE_RCAC_PATH, buf, buf_size, out_len);
}

int certificate_store_clear_all(void) {
    int err = 0;
    /* storage_adapter_delete returns 0 even if file is absent */
    if (storage_adapter_delete(CERT_STORE_NOC_PATH)  != 0) err = -1;
    if (storage_adapter_delete(CERT_STORE_ICAC_PATH) != 0) err = -1;
    if (storage_adapter_delete(CERT_STORE_RCAC_PATH) != 0) err = -1;
    if (err == 0) {
        printf("[CertStore] All operational certs cleared\n");
    }
    return err;
}
