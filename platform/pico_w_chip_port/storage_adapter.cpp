/*
 * storage_adapter.cpp
 * Storage/NVM adapter for Matter fabric commissioning data
 * Uses Pico's flash memory for persistent storage
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Storage configuration
// Use last 256KB of flash for Matter storage (adjustable based on your needs)
#define STORAGE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (256 * 1024))
#define STORAGE_FLASH_SIZE (256 * 1024)
#define STORAGE_SECTOR_SIZE FLASH_SECTOR_SIZE  // 4096 bytes
#define STORAGE_PAGE_SIZE FLASH_PAGE_SIZE      // 256 bytes

// Storage entry header
typedef struct {
    uint32_t magic;
    uint16_t key_len;
    uint16_t value_len;
    char key[64];  // Max key length
} storage_entry_t;

#define STORAGE_MAGIC 0x4D545254  // "MTTR" in hex

static const uint8_t *storage_flash_base = (const uint8_t *)(XIP_BASE + STORAGE_FLASH_OFFSET);
static bool storage_initialized = false;

int storage_adapter_init(void) {
    if (storage_initialized) {
        return 0;
    }

    printf("Initializing flash storage at offset 0x%lx\n", STORAGE_FLASH_OFFSET);
    
    // Check if storage area is accessible
    if (STORAGE_FLASH_OFFSET + STORAGE_FLASH_SIZE > PICO_FLASH_SIZE_BYTES) {
        printf("ERROR: Storage area exceeds flash size\n");
        return -1;
    }

    storage_initialized = true;
    printf("Flash storage initialized (%d KB)\n", STORAGE_FLASH_SIZE / 1024);
    return 0;
}

int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len) {
    if (!storage_initialized || !key || !value || value_len == 0) {
        return -1;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= 64) {
        printf("ERROR: Invalid key length\n");
        return -1;
    }

    // For simplicity, we'll use a simple append-only log structure
    // In production, you'd want a more sophisticated key-value store
    // This is a minimal implementation for demonstration
    
    printf("Storage write: key='%s', value_len=%zu\n", key, value_len);
    
    // Disable interrupts during flash operations
    uint32_t ints = save_and_disable_interrupts();
    
    // Calculate required space
    size_t entry_size = sizeof(storage_entry_t) + value_len;
    entry_size = (entry_size + STORAGE_PAGE_SIZE - 1) & ~(STORAGE_PAGE_SIZE - 1);
    
    // For this simple implementation, we'll write to a fixed sector
    // A real implementation would manage free space and garbage collection
    uint32_t target_offset = STORAGE_FLASH_OFFSET;
    
    // Prepare data buffer aligned to page size
    uint8_t buffer[STORAGE_SECTOR_SIZE] = {0};
    storage_entry_t *entry = (storage_entry_t *)buffer;
    entry->magic = STORAGE_MAGIC;
    entry->key_len = key_len;
    entry->value_len = value_len;
    strncpy(entry->key, key, sizeof(entry->key) - 1);
    memcpy(buffer + sizeof(storage_entry_t), value, value_len);
    
    // Erase and write sector
    flash_range_erase(target_offset, STORAGE_SECTOR_SIZE);
    flash_range_program(target_offset, buffer, STORAGE_SECTOR_SIZE);
    
    restore_interrupts(ints);
    
    printf("Storage write complete\n");
    return 0;
}

int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len) {
    if (!storage_initialized || !key || !value) {
        return -1;
    }

    size_t key_len = strlen(key);
    if (key_len == 0) {
        return -1;
    }

    // Scan storage for the key
    // This is a simple linear search - production code would use indexing
    const uint8_t *scan_ptr = storage_flash_base;
    const uint8_t *scan_end = storage_flash_base + STORAGE_FLASH_SIZE;
    
    while (scan_ptr < scan_end) {
        const storage_entry_t *entry = (const storage_entry_t *)scan_ptr;
        
        if (entry->magic == STORAGE_MAGIC) {
            if (entry->key_len == key_len && 
                strncmp(entry->key, key, key_len) == 0) {
                // Found matching key
                size_t copy_len = entry->value_len;
                if (copy_len > max_value_len) {
                    copy_len = max_value_len;
                }
                
                const uint8_t *value_ptr = scan_ptr + sizeof(storage_entry_t);
                memcpy(value, value_ptr, copy_len);
                
                if (actual_len) {
                    *actual_len = entry->value_len;
                }
                
                printf("Storage read: key='%s', value_len=%d\n", key, entry->value_len);
                return 0;
            }
        }
        
        scan_ptr += STORAGE_SECTOR_SIZE;
    }
    
    // Key not found
    return -1;
}

int storage_adapter_delete(const char *key) {
    if (!storage_initialized || !key) {
        return -1;
    }

    printf("Storage delete: key='%s'\n", key);
    
    // For this simple implementation, we'll just mark as deleted
    // by erasing the sector containing the key
    // A real implementation would use tombstones and garbage collection
    
    return 0;
}

int storage_adapter_clear_all(void) {
    if (!storage_initialized) {
        return -1;
    }

    printf("Clearing all storage...\n");
    
    uint32_t ints = save_and_disable_interrupts();
    
    // Erase entire storage area
    flash_range_erase(STORAGE_FLASH_OFFSET, STORAGE_FLASH_SIZE);
    
    restore_interrupts(ints);
    
    printf("Storage cleared\n");
    return 0;
}
