/*
 * platform_manager.cpp
 * Platform manager for Matter DeviceLayer integration
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"

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

static bool platform_initialized = false;

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
    printf("\n");
    printf("====================================\n");
    printf("    Matter Commissioning Info\n");
    printf("====================================\n");
    printf("Setup PIN Code: 20202021\n");
    printf("Discriminator:  3840 (0x0F00)\n");
    printf("\n");
    printf("Manual Pairing Code:\n");
    printf("  34970112332\n");
    printf("\n");
    printf("QR Code URL:\n");
    printf("  MT:Y.K9042C00KA0648G00\n");
    printf("\n");
    printf("⚠️  WARNING: These are TEST credentials!\n");
    printf("   For production, use device-specific\n");
    printf("   setup codes and discriminators.\n");
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
