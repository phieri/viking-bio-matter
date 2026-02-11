/*
 * platform_manager.cpp
 * Platform manager for Matter DeviceLayer integration
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "mbedtls/sha256.h"

// Forward declarations of adapter functions (internal C++ functions)
int network_adapter_init(void);
int network_adapter_connect(const char *ssid, const char *password);
bool network_adapter_is_connected(void);
void network_adapter_get_ip_address(char *buffer, size_t buffer_len);
void network_adapter_get_mac_address(uint8_t *mac_addr);
void network_adapter_deinit(void);

int storage_adapter_init(void);
int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len);
int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len);
int storage_adapter_delete(const char *key);
int storage_adapter_clear_all(void);

int crypto_adapter_init(void);
int crypto_adapter_random(uint8_t *buffer, size_t length);
void crypto_adapter_deinit(void);

// Product salt for PIN derivation
// This constant is combined with the device MAC address to derive a unique
// setup PIN for Matter commissioning. Change this value for production
// deployments to ensure product-specific PINs.
// Must match PRODUCT_SALT in tools/derive_pin.py for offline PIN computation.
static const char *PRODUCT_SALT = "VIKINGBIO-2026";

// Maximum salt length for PIN derivation buffer
#define MAX_SALT_LENGTH 64

static bool platform_initialized = false;

/**
 * @brief Derive an 8-digit setup PIN from device MAC address
 * 
 * This function computes a deterministic 8-digit Matter setup PIN from the
 * device's MAC address. Each device with a unique MAC gets a unique PIN.
 * 
 * Algorithm:
 * 1. Concatenate MAC address with PRODUCT_SALT constant
 * 2. Compute SHA-256 hash of the concatenated data
 * 3. Extract first 4 bytes of hash as big-endian uint32
 * 4. Compute PIN = (hash_value % 100000000) to get 8-digit number
 * 5. Format as zero-padded string "00000000" to "99999999"
 * 
 * The PRODUCT_SALT constant ensures different products using this code
 * get different PINs even with the same MAC address. For production,
 * change PRODUCT_SALT to a product-specific value.
 * 
 * Note: The Python tool tools/derive_pin.py implements the same algorithm
 * and can be used to compute PINs offline from MAC addresses.
 * 
 * @param mac 6-byte MAC address (must not be NULL)
 * @param out_pin8 Output buffer for 8-digit PIN + null terminator (9 bytes, must not be NULL)
 * @return 0 on success, -1 on failure (NULL parameters)
 */
static int derive_setup_pin_from_mac(const uint8_t mac[6], char out_pin8[9]) {
    if (!mac || !out_pin8) {
        return -1;
    }

    // Prepare input: MAC || PRODUCT_SALT
    uint8_t input[6 + MAX_SALT_LENGTH]; // 6 bytes MAC + salt
    size_t salt_len = strlen(PRODUCT_SALT);
    if (salt_len > MAX_SALT_LENGTH) {
        salt_len = MAX_SALT_LENGTH; // Limit salt length
    }
    
    memcpy(input, mac, 6);
    memcpy(input + 6, PRODUCT_SALT, salt_len);
    
    // Compute SHA-256 hash
    uint8_t hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256 (not SHA-224)
    mbedtls_sha256_update(&ctx, input, 6 + salt_len);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
    
    // Take first 4 bytes as big-endian uint32
    uint32_t hash_value = ((uint32_t)hash[0] << 24) |
                          ((uint32_t)hash[1] << 16) |
                          ((uint32_t)hash[2] << 8) |
                          ((uint32_t)hash[3]);
    
    // Compute PIN = hash_value % 100000000 (8-digit decimal)
    uint32_t pin = hash_value % 100000000;
    
    // Format as zero-padded 8-digit string
    snprintf(out_pin8, 9, "%08lu", (unsigned long)pin);
    
    return 0;
}

// Export platform manager functions with C linkage for use from C code
extern "C" {

int platform_manager_init(void) {
    if (platform_initialized) {
        printf("Platform already initialized\n");
        return 0;
    }

    printf("===================================\n");
    printf("Matter Platform Manager for Pico W\n");
    printf("===================================\n\n");

    // Initialize crypto first (needed for other components)
    printf("Step 1/3: Initializing cryptography...\n");
    if (crypto_adapter_init() != 0) {
        printf("ERROR: Failed to initialize crypto adapter\n");
        return -1;
    }

    // Initialize storage
    printf("\nStep 2/3: Initializing storage...\n");
    if (storage_adapter_init() != 0) {
        printf("ERROR: Failed to initialize storage adapter\n");
        return -1;
    }

    // Initialize network
    printf("\nStep 3/3: Initializing network...\n");
    if (network_adapter_init() != 0) {
        printf("ERROR: Failed to initialize network adapter\n");
        return -1;
    }

    platform_initialized = true;
    printf("\n✓ Platform initialization complete\n\n");
    return 0;
}

int platform_manager_connect_wifi(const char *ssid, const char *password) {
    if (!platform_initialized) {
        printf("ERROR: Platform not initialized\n");
        return -1;
    }

    return network_adapter_connect(ssid, password);
}

bool platform_manager_is_wifi_connected(void) {
    if (!platform_initialized) {
        return false;
    }

    return network_adapter_is_connected();
}

void platform_manager_get_network_info(char *ip_buffer, size_t ip_buffer_len, 
                                       uint8_t *mac_buffer) {
    if (!platform_initialized) {
        return;
    }

    if (ip_buffer && ip_buffer_len > 0) {
        network_adapter_get_ip_address(ip_buffer, ip_buffer_len);
    }

    if (mac_buffer) {
        network_adapter_get_mac_address(mac_buffer);
    }
}

int platform_manager_generate_qr_code(char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return -1;
    }

    // Generate QR code URL for Matter commissioning
    // Format: MT:<version>.<vendor-id>.<product-id>.<discriminator>.<passcode>
    // Using test values from CHIPDevicePlatformConfig.h
    snprintf(buffer, buffer_len,
             "MT:Y.K9042C00KA0648G00");

    return 0;
}

void platform_manager_print_commissioning_info(void) {
    // Get device MAC address
    uint8_t mac[6] = {0};
    platform_manager_get_network_info(NULL, 0, mac);
    
    // Derive setup PIN from MAC
    char pin[9] = "00000000"; // Default fallback
    if (derive_setup_pin_from_mac(mac, pin) != 0) {
        printf("WARNING: Failed to derive setup PIN, using default\n");
    }
    
    printf("\n");
    printf("====================================\n");
    printf("    Matter Commissioning Info\n");
    printf("====================================\n");
    printf("Device MAC:     %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("Setup PIN Code: %s\n", pin);
    printf("Discriminator:  3840 (0x0F00)\n");
    printf("\n");
    printf("⚠️  IMPORTANT:\n");
    printf("   PIN is derived from device MAC.\n");
    printf("   Use tools/derive_pin.py to compute\n");
    printf("   the PIN from the MAC address above.\n");
    printf("\n");
    printf("⚠️  WARNING: Discriminator 3840 is for\n");
    printf("   testing only. Use unique values for\n");
    printf("   production devices.\n");
    printf("====================================\n\n");
}

void platform_manager_task(void) {
    if (!platform_initialized) {
        return;
    }

    // Periodic platform maintenance tasks
    // In a full implementation, this would:
    // - Process network events
    // - Check storage health
    // - Monitor system resources
}

void platform_manager_deinit(void) {
    if (!platform_initialized) {
        return;
    }

    printf("Shutting down platform...\n");
    
    network_adapter_deinit();
    crypto_adapter_deinit();
    
    platform_initialized = false;
    printf("Platform shutdown complete\n");
}

} // extern "C"
