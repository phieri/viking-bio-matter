/*
 * btstack_tlv_littlefs.h
 * BTstack TLV backend backed by LittleFS via the storage adapter.
 *
 * Each BTstack tag is persisted as a file named /btk_XXXXXXXX in the
 * LittleFS filesystem that storage_adapter manages.  This eliminates the
 * need for a dedicated BTstack flash-bank region and prevents the
 * btstack_tlv_flash_bank scan from conflicting with Matter / LittleFS
 * storage.
 *
 * storage_adapter_init() MUST succeed before calling
 * btstack_tlv_littlefs_init_instance().
 */

#ifndef BTSTACK_TLV_LITTLEFS_H
#define BTSTACK_TLV_LITTLEFS_H

#include "btstack_tlv.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return the LittleFS-backed BTstack TLV instance.
 *
 * The returned pointer is valid for the lifetime of the program.
 * Call btstack_tlv_set_instance() with this pointer to make BTstack use
 * LittleFS for all its persistent storage.
 *
 * @return Pointer to btstack_tlv_t; never NULL.
 */
const btstack_tlv_t *btstack_tlv_littlefs_init_instance(void);

#ifdef __cplusplus
}
#endif

#endif /* BTSTACK_TLV_LITTLEFS_H */
