/*
 * btstack_tlv_littlefs.cpp
 * BTstack TLV backend backed by LittleFS (via storage_adapter).
 *
 * Each tag is stored as the file /btk_XXXXXXXX where XXXXXXXX is the
 * 8-digit lower-case hex representation of the 32-bit tag value.
 * storage_adapter_init() must have been called before
 * btstack_tlv_littlefs_init_instance() is used.
 */

#include "btstack_tlv_littlefs.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

/* Forward declarations — implemented in storage_adapter.cpp */
extern "C" {
int storage_adapter_read(const char *key, uint8_t *value,
                         size_t max_value_len, size_t *actual_len);
int storage_adapter_write(const char *key, const uint8_t *value,
                          size_t value_len);
int storage_adapter_delete(const char *key);
}

/* Maximum key length: "/btk_" (5) + 8 hex digits + NUL = 14 bytes */
#define BTK_KEY_BUF 16

static void make_key(char *buf, size_t buf_size, uint32_t tag) {
    snprintf(buf, buf_size, "/btk_%08" PRIx32, tag);
}

/*
 * get_tag — return tag value size; copy into buffer if non-NULL.
 *
 * BTstack calls this with buffer == NULL first to determine the required
 * buffer size, then again with a suitably sized buffer.  We pass a
 * 1-byte temporary to storage_adapter_read so we obtain the file size
 * (set in *actual_len regardless of buffer_size) without reading all data.
 */
static int tlv_lfs_get_tag(void *context, uint32_t tag,
                            uint8_t *buffer, uint32_t buffer_size) {
    (void)context;

    char key[BTK_KEY_BUF];
    make_key(key, sizeof(key), tag);

    if (buffer == NULL) {
        /* Probe: just get size */
        uint8_t tmp[1];
        size_t actual_len = 0;
        if (storage_adapter_read(key, tmp, sizeof(tmp), &actual_len) != 0) {
            return -1;  /* not found */
        }
        return (int)actual_len;
    }

    size_t actual_len = 0;
    if (storage_adapter_read(key, buffer, (size_t)buffer_size,
                             &actual_len) != 0) {
        return -1;  /* not found */
    }
    return (int)actual_len;
}

/* store_tag — write tag value; returns 0 on success */
static int tlv_lfs_store_tag(void *context, uint32_t tag,
                              const uint8_t *value, uint32_t value_length) {
    (void)context;

    char key[BTK_KEY_BUF];
    make_key(key, sizeof(key), tag);
    return storage_adapter_write(key, value, (size_t)value_length);
}

/* delete_tag — remove tag if present (ignore "not found" errors) */
static void tlv_lfs_delete_tag(void *context, uint32_t tag) {
    (void)context;

    char key[BTK_KEY_BUF];
    make_key(key, sizeof(key), tag);
    storage_adapter_delete(key);
}

static const btstack_tlv_t btstack_tlv_lfs_instance = {
    /* .get_tag    = */ tlv_lfs_get_tag,
    /* .store_tag  = */ tlv_lfs_store_tag,
    /* .delete_tag = */ tlv_lfs_delete_tag,
};

extern "C" {

const btstack_tlv_t *btstack_tlv_littlefs_init_instance(void) {
    return &btstack_tlv_lfs_instance;
}

} /* extern "C" */
