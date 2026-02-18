/*
 * platform_manager.cpp
 * Platform manager for Matter DeviceLayer integration
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "mbedtls/sha256.h"
#include "matter_attributes.h"
#include "network_adapter.h"
#include "ble_adapter.h"
#include "CHIPDevicePlatformConfig.h"

// Matter DNS-SD discovery
extern "C" {
#include "dns_sd.h"
}

// Forward declarations of adapter functions
extern "C" {
int network_adapter_init(void);
int network_adapter_connect(const char *ssid, const char *password);
int network_adapter_save_and_connect(const char *ssid, const char *password);
bool network_adapter_is_connected(void);
void network_adapter_get_ip_address(char *buffer, size_t buffer_len);
void network_adapter_get_mac_address(uint8_t *mac_addr);
void network_adapter_deinit(void);

int ble_adapter_init(void);
int ble_adapter_start_advertising(uint16_t device_discriminator, uint16_t vendor_id, uint16_t product_id);
int ble_adapter_stop_advertising(void);
bool ble_adapter_is_connected(void);
void ble_adapter_deinit(void);

int storage_adapter_init(void);
int storage_adapter_write(const char *key, const uint8_t *value, size_t value_len);
int storage_adapter_read(const char *key, uint8_t *value, size_t max_value_len, size_t *actual_len);
int storage_adapter_delete(const char *key);
int storage_adapter_clear_all(void);
int storage_adapter_save_discriminator(uint16_t discriminator);
int storage_adapter_load_discriminator(uint16_t *discriminator);
int storage_adapter_has_discriminator(void);
int storage_adapter_clear_discriminator(void);

int crypto_adapter_init(void);
int crypto_adapter_random(uint8_t *buffer, size_t length);
void crypto_adapter_deinit(void);
}

// Product salt for PIN derivation
// This constant is combined with the device MAC address to derive a unique
// setup PIN for Matter commissioning. Change this value for production
// deployments to ensure product-specific PINs.
// Must match PRODUCT_SALT in tools/derive_pin.py for offline PIN computation.
static const char *PRODUCT_SALT = "VIKINGBIO-2026";

// Maximum salt length for PIN derivation buffer
#define MAX_SALT_LENGTH 64

// Testing discriminator range (0x0F00-0x0FFF, 3840-4095)
// Per Matter spec, values 0xF00-0xFFF are commonly used for testing
#define DISCRIMINATOR_TEST_MIN 0x0F00
#define DISCRIMINATOR_TEST_MAX 0x0FFF

static bool platform_initialized = false;
static uint16_t device_discriminator = 0;  // Loaded from storage or generated

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
    printf("Step 1/4: Initializing cryptography...\n");
    if (crypto_adapter_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize crypto adapter\n");
        return -1;
    }

    // Initialize storage
    printf("\nStep 2/4: Initializing storage...\n");
    if (storage_adapter_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize storage adapter\n");
        return -1;
    }
    
    // Initialize or load discriminator
    printf("\nInitializing discriminator...\n");
    if (storage_adapter_has_discriminator()) {
        // Load from storage
        if (storage_adapter_load_discriminator(&device_discriminator) == 0) {
            printf("✓ Loaded discriminator from storage: %u (0x%03X)\n", 
                   device_discriminator, device_discriminator);
        } else {
            printf("[PlatformManager] ERROR: Failed to load discriminator from storage\n");
            return -1;
        }
    } else {
        // First boot - generate random discriminator in testing range
        printf("First boot detected - generating random discriminator\n");
        
        // Generate random value in testing range (0xF00-0xFFF)
        uint8_t random_byte;
        if (crypto_adapter_random(&random_byte, 1) != 0) {
            printf("[PlatformManager] ERROR: Failed to generate random discriminator\n");
            return -1;
        }
        
        // Map to testing range: 0xF00 + (random % 256)
        // This gives us exactly 256 values from 0xF00 (3840) to 0xFFF (4095)
        device_discriminator = DISCRIMINATOR_TEST_MIN + random_byte;
        
        printf("Generated discriminator: %u (0x%03X)\n", 
               device_discriminator, device_discriminator);
        
        // Save to storage
        if (storage_adapter_save_discriminator(device_discriminator) != 0) {
            printf("[PlatformManager] ERROR: Failed to save discriminator to storage\n");
            return -1;
        }
        
        printf("✓ Discriminator saved to flash\n");
    }

    // Initialize network
    printf("\nStep 3/4: Initializing network...\n");
    if (network_adapter_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize network adapter\n");
        return -1;
    }
    
    // Fast blink LED to indicate initialization in progress
    // This is the earliest point where LED is available (right after CYW43 chip init)
    for (int i = 0; i < 5; i++) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        sleep_ms(100);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        sleep_ms(100);
    }
    
    // Initialize BLE for Matter commissioning
    printf("\nInitializing BLE for commissioning...\n");
    if (ble_adapter_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize BLE adapter\n");
        return -1;
    }
    printf("✓ BLE initialized\n");
    
    // Initialize DNS-SD for Matter device discovery
    printf("\nInitializing DNS-SD...\n");
    if (dns_sd_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize DNS-SD\n");
        return -1;
    }
    printf("✓ DNS-SD initialized\n");

    // Initialize Matter attribute system
    printf("\nStep 4/4: Initializing Matter attributes...\n");
    if (matter_attributes_init() != 0) {
        printf("[PlatformManager] ERROR: Failed to initialize Matter attributes\n");
        return -1;
    }
    
    // Register Matter clusters/attributes for Viking Bio bridge
    matter_attr_value_t initial_value;
    
    // OnOff cluster (flame state)
    initial_value.bool_val = false;
    matter_attributes_register(1, MATTER_CLUSTER_ON_OFF, MATTER_ATTR_ON_OFF, 
                              MATTER_TYPE_BOOL, &initial_value);
    
    // LevelControl cluster (fan speed)
    initial_value.uint8_val = 0;
    matter_attributes_register(1, MATTER_CLUSTER_LEVEL_CONTROL, MATTER_ATTR_CURRENT_LEVEL,
                              MATTER_TYPE_UINT8, &initial_value);
    
    // TemperatureMeasurement cluster (temperature in centidegrees)
    initial_value.int16_val = 0;
    matter_attributes_register(1, MATTER_CLUSTER_TEMPERATURE_MEASUREMENT, MATTER_ATTR_MEASURED_VALUE,
                              MATTER_TYPE_INT16, &initial_value);
    
    // Diagnostics cluster - TotalOperationalHours
    initial_value.uint32_val = 0;
    matter_attributes_register(1, MATTER_CLUSTER_DIAGNOSTICS, MATTER_ATTR_TOTAL_OPERATIONAL_HOURS,
                              MATTER_TYPE_UINT32, &initial_value);
    
    // Diagnostics cluster - DeviceEnabledState (1 = enabled)
    initial_value.uint8_val = 1;
    matter_attributes_register(1, MATTER_CLUSTER_DIAGNOSTICS, MATTER_ATTR_DEVICE_ENABLED_STATE,
                              MATTER_TYPE_UINT8, &initial_value);
    
    // Diagnostics cluster - NumberOfActiveFaults
    initial_value.uint8_val = 0;
    matter_attributes_register(1, MATTER_CLUSTER_DIAGNOSTICS, MATTER_ATTR_NUMBER_OF_ACTIVE_FAULTS,
                              MATTER_TYPE_UINT8, &initial_value);

    platform_initialized = true;
    printf("\n✓ Platform initialization complete\n");
    printf("✓ %zu Matter attributes registered\n\n", matter_attributes_count());
    return 0;
}

int platform_manager_connect_wifi(const char *ssid, const char *password) {
    if (!platform_initialized) {
        printf("[PlatformManager] ERROR: Platform not initialized\n");
        return -1;
    }

    // If credentials provided, save and connect
    if (ssid && password) {
        return network_adapter_save_and_connect(ssid, password);
    }
    
    // Otherwise try to connect with stored credentials
    return network_adapter_connect(NULL, NULL);
}

int platform_manager_start_commissioning_mode(void) {
    if (!platform_initialized) {
        printf("[PlatformManager] ERROR: Platform not initialized\n");
        return -1;
    }
    
    printf("\n");
    printf("====================================\n");
    printf("  Starting Commissioning Mode\n");
    printf("====================================\n");
    
    // Get device info for BLE advertising
    uint16_t vendor_id = CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID;
    uint16_t product_id = CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID;
    
    // Start BLE advertising for Matter commissioning
    if (ble_adapter_start_advertising(device_discriminator, vendor_id, product_id) != 0) {
        printf("[PlatformManager] ERROR: Failed to start BLE advertising\n");
        return -1;
    }
    
    printf("\nDevice is now in BLE commissioning mode.\n");
    printf("Use a Matter controller to discover and\n");
    printf("commission the device over Bluetooth LE.\n");
    printf("====================================\n\n");
    
    return 0;
}

int platform_manager_stop_commissioning_mode(void) {
    if (!platform_initialized) {
        printf("[PlatformManager] ERROR: Platform not initialized\n");
        return -1;
    }
    
    printf("\n");
    printf("====================================\n");
    printf("  Stopping Commissioning Mode\n");
    printf("====================================\n");
    
    // Stop BLE advertising
    if (ble_adapter_stop_advertising() != 0) {
        printf("[PlatformManager] ERROR: Failed to stop BLE advertising\n");
        return -1;
    }
    
    printf("\nBLE commissioning mode stopped.\n");
    printf("Device will remain connected to WiFi.\n");
    printf("====================================\n\n");
    
    return 0;
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
    printf("Discriminator:  %u (0x%03X)\n", device_discriminator, device_discriminator);
    printf("\n");
    printf("⚠️  IMPORTANT:\n");
    printf("   PIN is derived from device MAC.\n");
    printf("   Use tools/derive_pin.py to compute\n");
    printf("   the PIN from the MAC address above.\n");
    printf("\n");
    printf("⚠️  NOTE: Discriminator was randomly\n");
    printf("   generated on first boot and saved\n");
    printf("   to flash. Value is in testing range.\n");
    printf("====================================\n\n");
}

int platform_manager_derive_setup_pin(const uint8_t *mac_addr, char *out_pin8) {
    if (!mac_addr || !out_pin8) {
        return -1;
    }
    
    uint8_t mac[6];
    memcpy(mac, mac_addr, 6);
    return derive_setup_pin_from_mac(mac, out_pin8);
}

uint16_t platform_manager_get_discriminator(void) {
    return device_discriminator;
}

void platform_manager_task(void) {
    if (!platform_initialized) {
        return;
    }

    // Process Matter attribute reports
    matter_attributes_process_reports();
    
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
    
    // Shutdown BLE first
    ble_adapter_deinit();
    
    // Then network and crypto
    network_adapter_deinit();
    crypto_adapter_deinit();
    
    platform_initialized = false;
    printf("Platform shutdown complete\n");
}

void platform_manager_report_attribute_change(uint32_t cluster_id, 
                                              uint32_t attribute_id, 
                                              uint8_t endpoint) {
    // This function is now a no-op since matter_attributes_update()
    // automatically handles the reporting through the subscription system.
    // Kept for API compatibility.
    
    if (!platform_initialized) {
        return;
    }
    
    // The actual reporting happens in matter_attributes_process_reports()
    // which is called from platform_manager_task()
}

void platform_manager_report_onoff_change(uint8_t endpoint) {
    // OnOff cluster (0x0006), OnOff attribute (0x0000)
    // Note: Actual update happens in matter_bridge via matter_attributes_update()
    platform_manager_report_attribute_change(MATTER_CLUSTER_ON_OFF, MATTER_ATTR_ON_OFF, endpoint);
}

void platform_manager_report_level_change(uint8_t endpoint) {
    // LevelControl cluster (0x0008), CurrentLevel attribute (0x0000)
    // Note: Actual update happens in matter_bridge via matter_attributes_update()
    platform_manager_report_attribute_change(MATTER_CLUSTER_LEVEL_CONTROL, MATTER_ATTR_CURRENT_LEVEL, endpoint);
}

void platform_manager_report_temperature_change(uint8_t endpoint) {
    // TemperatureMeasurement cluster (0x0402), MeasuredValue attribute (0x0000)
    // Note: Actual update happens in matter_bridge via matter_attributes_update()
    platform_manager_report_attribute_change(MATTER_CLUSTER_TEMPERATURE_MEASUREMENT, MATTER_ATTR_MEASURED_VALUE, endpoint);
}

int platform_manager_start_dns_sd_advertisement(void) {
    if (!platform_initialized) {
        printf("[PlatformManager] ERROR: Platform not initialized\n");
        return -1;
    }
    
    // Only advertise if connected to network
    if (!network_adapter_is_connected()) {
        printf("WARNING: Not connected to network, cannot advertise DNS-SD\n");
        return -1;
    }
    
    printf("\n");
    printf("====================================\n");
    printf("  Starting DNS-SD Advertisement\n");
    printf("====================================\n");
    
    // Get device info from config
    uint16_t vendor_id = CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID;
    uint16_t product_id = CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID;
    uint16_t device_type = 0x0302;  // Temperature Sensor (Matter Device Library)
    uint8_t commissioning_mode = 1;  // Always in commissioning mode for this device
    
    // Advertise the commissionable node
    int result = dns_sd_advertise_commissionable_node(
        device_discriminator,
        vendor_id,
        product_id,
        device_type,
        commissioning_mode
    );
    
    if (result == 0) {
        printf("\n✓ Device is now discoverable via DNS-SD\n");
        printf("  Use 'dns-sd -B _matterc._udp' to verify\n");
        printf("====================================\n\n");
    } else {
        printf("[PlatformManager] ERROR: Failed to start DNS-SD advertisement\n");
    }
    
    return result;
}

void platform_manager_stop_dns_sd_advertisement(void) {
    if (!platform_initialized) {
        return;
    }
    
    printf("Stopping DNS-SD advertisement...\n");
    dns_sd_stop();
}

bool platform_manager_is_dns_sd_advertising(void) {
    if (!platform_initialized) {
        return false;
    }
    
    return dns_sd_is_advertising();
}

} // extern "C"
