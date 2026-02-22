/*
 * storage_adapter.cpp
 * RAM-only storage stub — LittleFS removed for CYW43 init diagnostic.
 *
 * All values are stored in a simple in-RAM key-value table and are
 * lost on reset.  This stub preserves the full storage_adapter API so
 * that the rest of the firmware (platform_manager, ble_adapter,
 * btstack_tlv_littlefs) compiles and runs without any flash access,
 * letting us determine whether LittleFS flash operations are the root
 * cause of the CYW43 initialization hang.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

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

// ---- RAM key-value store ------------------------------------------------
#define MAX_KV_ENTRIES 32
#define MAX_KV_KEY_LEN 32
#define MAX_KV_VAL_LEN 256

typedef struct {
    char     key[MAX_KV_KEY_LEN];
    uint8_t  value[MAX_KV_VAL_LEN];
    size_t   value_len;
    bool     used;
} kv_entry_t;

static kv_entry_t kv_store[MAX_KV_ENTRIES];
static bool storage_initialized = false;

// Find entry by key; returns index or -1
static int kv_find(const char *key) {
    for (int i = 0; i < MAX_KV_ENTRIES; i++) {
        if (kv_store[i].used && strncmp(kv_store[i].key, key, MAX_KV_KEY_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

// Find a free slot; returns index or -1
static int kv_free_slot(void) {
    for (int i = 0; i < MAX_KV_ENTRIES; i++) {
        if (!kv_store[i].used) {
            return i;
        }
    }
    return -1;
}
// -------------------------------------------------------------------------

extern "C" {

int storage_adapter_init(void) {
    if (storage_initialized) {
        return 0;
    }
    memset(kv_store, 0, sizeof(kv_store));
    storage_initialized = true;
    printf("[Storage] RAM-only stub initialized (LittleFS removed for diagnostics)\n");
    return 0;
}

int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len) {
    if (!storage_initialized || !key || !value || value_len == 0) {
        return -1;
    }
    if (value_len > MAX_KV_VAL_LEN) {
        printf("[Storage] WARN: value truncated to %d bytes for key '%s'\n",
               MAX_KV_VAL_LEN, key);
        value_len = MAX_KV_VAL_LEN;
    }

    int idx = kv_find(key);
    if (idx < 0) {
        idx = kv_free_slot();
        if (idx < 0) {
            printf("[Storage] ERROR: store full, cannot write key '%s'\n", key);
            return -1;
        }
        strncpy(kv_store[idx].key, key, MAX_KV_KEY_LEN - 1);
        kv_store[idx].key[MAX_KV_KEY_LEN - 1] = '\0';
        kv_store[idx].used = true;
    }
    memcpy(kv_store[idx].value, value, value_len);
    kv_store[idx].value_len = value_len;
    return 0;
}

int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len) {
    if (!storage_initialized || !key || !value) {
        return -1;
    }
    int idx = kv_find(key);
    if (idx < 0) {
        return -1;
    }
    size_t copy_len = kv_store[idx].value_len;
    if (actual_len) {
        *actual_len = kv_store[idx].value_len;
    }
    if (copy_len > max_value_len) {
        copy_len = max_value_len;
    }
    memcpy(value, kv_store[idx].value, copy_len);
    return 0;
}

int storage_adapter_delete(const char *key) {
    if (!storage_initialized || !key) {
        return -1;
    }
    int idx = kv_find(key);
    if (idx >= 0) {
        memset(&kv_store[idx], 0, sizeof(kv_entry_t));
    }
    return 0;
}

int storage_adapter_clear_all(void) {
    if (!storage_initialized) {
        return -1;
    }
    memset(kv_store, 0, sizeof(kv_store));
    printf("[Storage] All RAM storage cleared\n");
    return 0;
}

int storage_adapter_save_wifi_credentials(const char *ssid, const char *password) {
    if (!storage_initialized || !ssid || !password) {
        return -1;
    }
    size_t ssid_len     = strlen(ssid);
    size_t password_len = strlen(password);
    if (ssid_len == 0 || ssid_len > MAX_SSID_LENGTH) {
        printf("[Storage] ERROR: Invalid SSID length: %zu\n", ssid_len);
        return -1;
    }
    if (password_len > MAX_PASSWORD_LENGTH) {
        printf("[Storage] ERROR: Invalid password length: %zu\n", password_len);
        return -1;
    }
    wifi_credentials_t creds = {0};
    strncpy(creds.ssid,     ssid,     MAX_SSID_LENGTH);
    strncpy(creds.password, password, MAX_PASSWORD_LENGTH);
    creds.ssid[MAX_SSID_LENGTH]         = '\0';
    creds.password[MAX_PASSWORD_LENGTH] = '\0';
    creds.ssid_len     = (uint8_t)ssid_len;
    creds.password_len = (uint8_t)password_len;
    creds.valid = true;
    int result = storage_adapter_write(WIFI_CREDENTIALS_KEY,
                                       (const uint8_t *)&creds,
                                       sizeof(wifi_credentials_t));
    if (result == 0) {
        printf("[Storage] WiFi credentials saved to RAM (SSID: %s)\n", ssid);
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
    if (storage_adapter_read(WIFI_CREDENTIALS_KEY,
                             (uint8_t *)&creds,
                             sizeof(wifi_credentials_t),
                             &actual_len) != 0 ||
        actual_len < sizeof(wifi_credentials_t)) {
        return -1;
    }
    if (!creds.valid || creds.ssid_len == 0 || creds.ssid_len > MAX_SSID_LENGTH) {
        return -1;
    }
    if (ssid_buffer_len < (size_t)(creds.ssid_len + 1) ||
        password_buffer_len < (size_t)(creds.password_len + 1)) {
        return -1;
    }
    strncpy(ssid,     creds.ssid,     ssid_buffer_len);
    strncpy(password, creds.password, password_buffer_len);
    ssid[creds.ssid_len]         = '\0';
    password[creds.password_len] = '\0';
    printf("[Storage] WiFi credentials loaded from RAM (SSID: %s)\n", ssid);
    return 0;
}

int storage_adapter_has_wifi_credentials(void) {
    if (!storage_initialized) {
        return 0;
    }
    wifi_credentials_t creds = {0};
    size_t actual_len = 0;
    if (storage_adapter_read(WIFI_CREDENTIALS_KEY,
                             (uint8_t *)&creds,
                             sizeof(wifi_credentials_t),
                             &actual_len) != 0 ||
        actual_len < sizeof(wifi_credentials_t)) {
        return 0;
    }
    return (creds.valid && creds.ssid_len > 0 && creds.ssid_len <= MAX_SSID_LENGTH) ? 1 : 0;
}

int storage_adapter_clear_wifi_credentials(void) {
    if (!storage_initialized) {
        return -1;
    }
    return storage_adapter_delete(WIFI_CREDENTIALS_KEY);
}

int storage_adapter_save_discriminator(uint16_t discriminator) {
    if (!storage_initialized) {
        return -1;
    }
    if (discriminator > 0x0FFF) {
        printf("[Storage] ERROR: Invalid discriminator: %u\n", discriminator);
        return -1;
    }
    int result = storage_adapter_write(DISCRIMINATOR_KEY,
                                       (const uint8_t *)&discriminator,
                                       sizeof(uint16_t));
    if (result == 0) {
        printf("[Storage] Discriminator saved to RAM: %u (0x%03X)\n",
               discriminator, discriminator);
    }
    return result;
}

int storage_adapter_load_discriminator(uint16_t *discriminator) {
    if (!storage_initialized || !discriminator) {
        return -1;
    }
    uint16_t stored_value = 0;
    size_t actual_len = 0;
    if (storage_adapter_read(DISCRIMINATOR_KEY,
                             (uint8_t *)&stored_value,
                             sizeof(uint16_t),
                             &actual_len) != 0 ||
        actual_len < sizeof(uint16_t)) {
        return -1;
    }
    if (stored_value > 0x0FFF) {
        return -1;
    }
    *discriminator = stored_value;
    printf("[Storage] Discriminator loaded from RAM: %u (0x%03X)\n",
           stored_value, stored_value);
    return 0;
}

int storage_adapter_has_discriminator(void) {
    if (!storage_initialized) {
        return 0;
    }
    uint16_t temp;
    size_t actual_len = 0;
    return (storage_adapter_read(DISCRIMINATOR_KEY,
                                 (uint8_t *)&temp,
                                 sizeof(uint16_t),
                                 &actual_len) == 0 &&
            actual_len == sizeof(uint16_t)) ? 1 : 0;
}

int storage_adapter_clear_discriminator(void) {
    if (!storage_initialized) {
        return -1;
    }
    return storage_adapter_delete(DISCRIMINATOR_KEY);
}

int storage_adapter_save_operational_hours(uint32_t hours) {
    if (!storage_initialized) {
        return -1;
    }
    return storage_adapter_write("/operational_hours",
                                 (const uint8_t *)&hours,
                                 sizeof(uint32_t));
}

int storage_adapter_load_operational_hours(uint32_t *hours) {
    if (!storage_initialized || !hours) {
        return -1;
    }
    uint32_t stored_value = 0;
    size_t actual_len = 0;
    if (storage_adapter_read("/operational_hours",
                             (uint8_t *)&stored_value,
                             sizeof(uint32_t),
                             &actual_len) != 0 ||
        actual_len < sizeof(uint32_t)) {
        return -1;
    }
    *hours = stored_value;
    return 0;
}

} // extern "C"
