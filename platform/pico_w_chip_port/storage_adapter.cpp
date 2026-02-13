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

// WiFi credential storage keys
#define WIFI_CREDENTIALS_KEY "wifi_credentials"
#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64

// WiFi credentials structure
typedef struct {
    char ssid[MAX_SSID_LENGTH + 1];
    char password[MAX_PASSWORD_LENGTH + 1];
    uint8_t ssid_len;
    uint8_t password_len;
    bool valid;
} wifi_credentials_t;

static const uint8_t *storage_flash_base = (const uint8_t *)(XIP_BASE + STORAGE_FLASH_OFFSET);
static bool storage_initialized = false;

extern "C" {

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

int storage_adapter_save_wifi_credentials(const char *ssid, const char *password) {
    if (!storage_initialized || !ssid || !password) {
        printf("ERROR: Invalid parameters for WiFi credential storage\n");
        return -1;
    }
    
    size_t ssid_len = strlen(ssid);
    size_t password_len = strlen(password);
    
    if (ssid_len == 0 || ssid_len > MAX_SSID_LENGTH) {
        printf("ERROR: Invalid SSID length: %zu (max %d)\n", ssid_len, MAX_SSID_LENGTH);
        return -1;
    }
    
    if (password_len > MAX_PASSWORD_LENGTH) {
        printf("ERROR: Invalid password length: %zu (max %d)\n", password_len, MAX_PASSWORD_LENGTH);
        return -1;
    }
    
    // Prepare WiFi credentials structure
    wifi_credentials_t creds = {0};
    strncpy(creds.ssid, ssid, MAX_SSID_LENGTH);
    strncpy(creds.password, password, MAX_PASSWORD_LENGTH);
    creds.ssid[MAX_SSID_LENGTH] = '\0';
    creds.password[MAX_PASSWORD_LENGTH] = '\0';
    creds.ssid_len = ssid_len;
    creds.password_len = password_len;
    creds.valid = true;
    
    // Write to storage
    int result = storage_adapter_write(WIFI_CREDENTIALS_KEY, 
                                      (const uint8_t *)&creds, 
                                      sizeof(wifi_credentials_t));
    
    if (result == 0) {
        printf("WiFi credentials saved to flash (SSID: %s)\n", ssid);
    } else {
        printf("ERROR: Failed to save WiFi credentials\n");
    }
    
    return result;
}

int storage_adapter_load_wifi_credentials(char *ssid, size_t ssid_buffer_len,
                                         char *password, size_t password_buffer_len) {
    if (!storage_initialized || !ssid || !password) {
        return -1;
    }
    
    wifi_credentials_t creds = {0};
    size_t actual_len = 0;
    
    // Read from storage
    int result = storage_adapter_read(WIFI_CREDENTIALS_KEY,
                                     (uint8_t *)&creds,
                                     sizeof(wifi_credentials_t),
                                     &actual_len);
    
    if (result != 0 || actual_len < sizeof(wifi_credentials_t)) {
        return -1;  // No credentials stored or read failed
    }
    
    // Validate credentials
    if (!creds.valid || creds.ssid_len == 0 || creds.ssid_len > MAX_SSID_LENGTH) {
        printf("ERROR: Invalid WiFi credentials in storage\n");
        return -1;
    }
    
    // Copy to output buffers
    if (ssid_buffer_len < creds.ssid_len + 1) {
        printf("ERROR: SSID buffer too small\n");
        return -1;
    }
    
    if (password_buffer_len < creds.password_len + 1) {
        printf("ERROR: Password buffer too small\n");
        return -1;
    }
    
    strncpy(ssid, creds.ssid, ssid_buffer_len);
    strncpy(password, creds.password, password_buffer_len);
    ssid[creds.ssid_len] = '\0';
    password[creds.password_len] = '\0';
    
    printf("WiFi credentials loaded from flash (SSID: %s)\n", ssid);
    return 0;
}

int storage_adapter_has_wifi_credentials(void) {
    if (!storage_initialized) {
        return 0;
    }
    
    wifi_credentials_t creds = {0};
    size_t actual_len = 0;
    
    int result = storage_adapter_read(WIFI_CREDENTIALS_KEY,
                                     (uint8_t *)&creds,
                                     sizeof(wifi_credentials_t),
                                     &actual_len);
    
    if (result != 0 || actual_len < sizeof(wifi_credentials_t)) {
        return 0;  // No credentials stored
    }
    
    return (creds.valid && creds.ssid_len > 0 && creds.ssid_len <= MAX_SSID_LENGTH) ? 1 : 0;
}

int storage_adapter_clear_wifi_credentials(void) {
    if (!storage_initialized) {
        return -1;
    }
    
    printf("Clearing WiFi credentials from storage...\n");
    return storage_adapter_delete(WIFI_CREDENTIALS_KEY);
}

} // extern "C"
