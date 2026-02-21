/*
 * storage_adapter.cpp
 * Storage/NVM adapter for Matter fabric commissioning data
 * Uses LittleFS on Pico's flash memory for persistent storage with wear leveling
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Include mutex header for LittleFS thread safety
#include "pico/mutex.h"

#include "pico_lfs.h"

// Storage configuration
// Use last 256KB of flash for Matter storage (adjustable based on your needs)
#define STORAGE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (256 * 1024))
#define STORAGE_FLASH_SIZE (256 * 1024)

// Storage keys
#define WIFI_CREDENTIALS_KEY "/wifi_credentials"
#define DISCRIMINATOR_KEY "/discriminator"
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

static struct lfs_config *lfs_cfg = NULL;
static lfs_t lfs;
static bool storage_initialized = false;

extern "C" {

// Helper function to construct filesystem path from key
static void construct_filename(const char *key, char *filename, size_t filename_size) {
    if (key[0] != '/') {
        snprintf(filename, filename_size, "/%s", key);
    } else {
        strncpy(filename, key, filename_size - 1);
        filename[filename_size - 1] = '\0';
    }
}

int storage_adapter_init(void) {
    if (storage_initialized) {
        return 0;
    }

    printf("Initializing LittleFS storage at offset 0x%lx\n", STORAGE_FLASH_OFFSET);
    
    // Check if storage area is accessible
    if (STORAGE_FLASH_OFFSET + STORAGE_FLASH_SIZE > PICO_FLASH_SIZE_BYTES) {
        printf("[Storage] ERROR: Storage area exceeds flash size\n");
        return -1;
    }

    // Initialize LittleFS configuration
    lfs_cfg = pico_lfs_init(STORAGE_FLASH_OFFSET, STORAGE_FLASH_SIZE);
    if (!lfs_cfg) {
        printf("[Storage] ERROR: Failed to initialize LittleFS configuration\n");
        return -1;
    }

    // Try to mount the filesystem
    int err = lfs_mount(&lfs, lfs_cfg);
    if (err != LFS_ERR_OK) {
        printf("LittleFS not formatted, formatting...\n");
        
        // Format the filesystem
        err = lfs_format(&lfs, lfs_cfg);
        if (err != LFS_ERR_OK) {
            printf("[Storage] ERROR: Failed to format filesystem (error %d)\n", err);
            pico_lfs_destroy(lfs_cfg);
            lfs_cfg = NULL;
            return -1;
        }
        
        // Mount the newly formatted filesystem
        err = lfs_mount(&lfs, lfs_cfg);
        if (err != LFS_ERR_OK) {
            printf("[Storage] ERROR: Failed to mount new filesystem (error %d)\n", err);
            pico_lfs_destroy(lfs_cfg);
            lfs_cfg = NULL;
            return -1;
        }
        
        printf("LittleFS formatted and mounted successfully\n");
    } else {
        printf("LittleFS mounted successfully\n");
    }

    storage_initialized = true;
    printf("Flash storage initialized (%d KB) with wear leveling\n", STORAGE_FLASH_SIZE / 1024);
    return 0;
}

void storage_adapter_enable_multicore_lockout(void) {
    // Re-enable multicore lockout after Core 1 has called multicore_lockout_victim_init().
    // This must be called only after Core 1 is running and registered as a lockout victim.
    if (!lfs_cfg) {
        // Safe no-op if storage not initialized yet. Multicore now starts earlier,
        // so this helper may be invoked before storage_adapter_init completes.
        return;
    }
#ifdef LIB_PICO_MULTICORE
    struct pico_lfs_context *pico_ctx = (struct pico_lfs_context *)lfs_cfg->context;
    pico_ctx->multicore_lockout_enabled = true;
#endif
}

int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len) {
    if (!storage_initialized || !key || !value || value_len == 0) {
        return -1;
    }

    size_t key_len = strlen(key);
    if (key_len == 0 || key_len >= LFS_NAME_MAX) {
        printf("[Storage] ERROR: Invalid key length\n");
        return -1;
    }

    printf("Storage write: key='%s', value_len=%zu\n", key, value_len);
    
    // Construct filename (prefix with / for LittleFS)
    char filename[LFS_NAME_MAX + 1];
    construct_filename(key, filename, sizeof(filename));

    // Open file for writing (create if doesn't exist, truncate if exists)
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, filename, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (err < 0) {
        printf("[Storage] ERROR: Failed to open file for writing (error %d)\n", err);
        return -1;
    }

    // Write data
    lfs_ssize_t written = lfs_file_write(&lfs, &file, value, value_len);
    if (written < 0) {
        printf("[Storage] ERROR: Failed to write data (error %d)\n", (int)written);
        lfs_file_close(&lfs, &file);
        return -1;
    } else if ((size_t)written != value_len) {
        printf("[Storage] ERROR: Incomplete write (wrote %d of %zu bytes)\n", (int)written, value_len);
        lfs_file_close(&lfs, &file);
        return -1;
    }

    // Close file
    err = lfs_file_close(&lfs, &file);
    if (err < 0) {
        printf("[Storage] ERROR: Failed to close file (error %d)\n", err);
        return -1;
    }

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

    // Construct filename (prefix with / for LittleFS)
    char filename[LFS_NAME_MAX + 1];
    construct_filename(key, filename, sizeof(filename));

    // Open file for reading
    lfs_file_t file;
    int err = lfs_file_open(&lfs, &file, filename, LFS_O_RDONLY);
    if (err < 0) {
        // File doesn't exist
        return -1;
    }

    // Get file size
    lfs_soff_t file_size = lfs_file_size(&lfs, &file);
    if (file_size < 0) {
        printf("[Storage] ERROR: Failed to get file size (error %d)\n", (int)file_size);
        lfs_file_close(&lfs, &file);
        return -1;
    }

    // Read data
    size_t copy_len = (size_t)file_size;
    if (copy_len > max_value_len) {
        copy_len = max_value_len;
    }

    lfs_ssize_t bytes_read = lfs_file_read(&lfs, &file, value, copy_len);
    if (bytes_read < 0) {
        printf("[Storage] ERROR: Failed to read data (error %d)\n", (int)bytes_read);
        lfs_file_close(&lfs, &file);
        return -1;
    }

    // Close file
    lfs_file_close(&lfs, &file);

    if (actual_len) {
        *actual_len = (size_t)file_size;
    }

    printf("Storage read: key='%s', value_len=%zu\n", key, (size_t)file_size);
    return 0;
}

int storage_adapter_delete(const char *key) {
    if (!storage_initialized || !key) {
        return -1;
    }

    printf("Storage delete: key='%s'\n", key);
    
    // Construct filename (prefix with / for LittleFS)
    char filename[LFS_NAME_MAX + 1];
    construct_filename(key, filename, sizeof(filename));

    // Remove file
    int err = lfs_remove(&lfs, filename);
    if (err < 0 && err != LFS_ERR_NOENT) {
        printf("[Storage] ERROR: Failed to delete file (error %d)\n", err);
        return -1;
    }

    return 0;
}

int storage_adapter_clear_all(void) {
    if (!storage_initialized) {
        return -1;
    }

    printf("Clearing all storage...\n");
    
    // Unmount filesystem
    lfs_unmount(&lfs);
    
    // Format (this erases everything)
    int err = lfs_format(&lfs, lfs_cfg);
    if (err != LFS_ERR_OK) {
        printf("[Storage] ERROR: Failed to format filesystem (error %d)\n", err);
        // Try to remount the old filesystem
        lfs_mount(&lfs, lfs_cfg);
        return -1;
    }
    
    // Remount
    err = lfs_mount(&lfs, lfs_cfg);
    if (err != LFS_ERR_OK) {
        printf("[Storage] ERROR: Failed to remount filesystem (error %d)\n", err);
        return -1;
    }
    
    printf("Storage cleared\n");
    return 0;
}

int storage_adapter_save_wifi_credentials(const char *ssid, const char *password) {
    if (!storage_initialized || !ssid || !password) {
        printf("[Storage] ERROR: Invalid parameters for WiFi credential storage\n");
        return -1;
    }
    
    size_t ssid_len = strlen(ssid);
    size_t password_len = strlen(password);
    
    if (ssid_len == 0 || ssid_len > MAX_SSID_LENGTH) {
        printf("[Storage] ERROR: Invalid SSID length: %zu (max %d)\n", ssid_len, MAX_SSID_LENGTH);
        return -1;
    }
    
    if (password_len > MAX_PASSWORD_LENGTH) {
        printf("[Storage] ERROR: Invalid password length: %zu (max %d)\n", password_len, MAX_PASSWORD_LENGTH);
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
        printf("[Storage] ERROR: Failed to save WiFi credentials\n");
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
        printf("[Storage] ERROR: Invalid WiFi credentials in storage\n");
        return -1;
    }
    
    // Copy to output buffers
    if (ssid_buffer_len < creds.ssid_len + 1) {
        printf("[Storage] ERROR: SSID buffer too small\n");
        return -1;
    }
    
    if (password_buffer_len < creds.password_len + 1) {
        printf("[Storage] ERROR: Password buffer too small\n");
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

int storage_adapter_save_discriminator(uint16_t discriminator) {
    if (!storage_initialized) {
        printf("[Storage] ERROR: Storage not initialized\n");
        return -1;
    }
    
    // Validate discriminator is within 12-bit range (0-4095)
    if (discriminator > 0x0FFF) {
        printf("[Storage] ERROR: Invalid discriminator value: %u (max 4095)\n", discriminator);
        return -1;
    }
    
    printf("Saving discriminator %u (0x%03X) to flash\n", discriminator, discriminator);
    
    // Write discriminator to storage as 16-bit value
    int result = storage_adapter_write(DISCRIMINATOR_KEY, 
                                      (const uint8_t *)&discriminator, 
                                      sizeof(uint16_t));
    
    if (result == 0) {
        printf("Discriminator saved successfully\n");
    } else {
        printf("[Storage] ERROR: Failed to save discriminator\n");
    }
    
    return result;
}

int storage_adapter_load_discriminator(uint16_t *discriminator) {
    if (!storage_initialized || !discriminator) {
        return -1;
    }
    
    uint16_t stored_value = 0;
    size_t actual_len = 0;
    
    // Read from storage
    int result = storage_adapter_read(DISCRIMINATOR_KEY,
                                     (uint8_t *)&stored_value,
                                     sizeof(uint16_t),
                                     &actual_len);
    
    if (result != 0 || actual_len < sizeof(uint16_t)) {
        return -1;  // No discriminator stored or read failed
    }
    
    // Validate discriminator is within 12-bit range
    if (stored_value > 0x0FFF) {
        printf("[Storage] ERROR: Invalid discriminator in storage: %u\n", stored_value);
        return -1;
    }
    
    *discriminator = stored_value;
    printf("Discriminator loaded from flash: %u (0x%03X)\n", stored_value, stored_value);
    return 0;
}

int storage_adapter_has_discriminator(void) {
    if (!storage_initialized) {
        return 0;
    }
    
    // Try to read without validation or logging - just check if file exists
    uint16_t temp;
    size_t actual_len = 0;
    
    int result = storage_adapter_read(DISCRIMINATOR_KEY,
                                     (uint8_t *)&temp,
                                     sizeof(uint16_t),
                                     &actual_len);
    
    // Return 1 if file exists and has correct size, 0 otherwise
    return (result == 0 && actual_len == sizeof(uint16_t)) ? 1 : 0;
}

int storage_adapter_clear_discriminator(void) {
    if (!storage_initialized) {
        return -1;
    }
    
    printf("Clearing discriminator from storage...\n");
    return storage_adapter_delete(DISCRIMINATOR_KEY);
}

int storage_adapter_save_operational_hours(uint32_t hours) {
    if (!storage_initialized) {
        return -1;
    }
    
    // Write operational hours to storage as 32-bit value
    int result = storage_adapter_write("/operational_hours", 
                                      (const uint8_t *)&hours, 
                                      sizeof(uint32_t));
    
    if (result != 0) {
        printf("[Storage] ERROR: Failed to save operational hours\n");
    }
    
    return result;
}

int storage_adapter_load_operational_hours(uint32_t *hours) {
    if (!storage_initialized || !hours) {
        return -1;
    }
    
    uint32_t stored_value = 0;
    size_t actual_len = 0;
    
    // Read from storage
    int result = storage_adapter_read("/operational_hours",
                                     (uint8_t *)&stored_value,
                                     sizeof(uint32_t),
                                     &actual_len);
    
    if (result != 0 || actual_len < sizeof(uint32_t)) {
        return -1;  // No hours stored or read failed
    }
    
    *hours = stored_value;
    return 0;
}

} // extern "C"
