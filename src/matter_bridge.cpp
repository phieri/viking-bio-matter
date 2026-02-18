#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/critical_section.h"
#include "matter_bridge.h"
#include "../platform/pico_w_chip_port/platform_manager.h"
#include "../platform/pico_w_chip_port/matter_attributes.h"
#include "../platform/pico_w_chip_port/matter_reporter.h"
#include "../platform/pico_w_chip_port/matter_network_subscriber.h"
#include "../platform/pico_w_chip_port/matter_network_transport.h"
#include "matter_minimal/interaction/subscription_bridge.h"
#include "matter_minimal/matter_protocol.h"

// Forward declare storage functions
extern "C" {
    int storage_adapter_has_wifi_credentials(void);
    int storage_adapter_load_operational_hours(uint32_t *hours);
    int storage_adapter_save_operational_hours(uint32_t hours);
}

// Matter attributes storage
static matter_attributes_t attributes = {
    .flame_state = false,
    .fan_speed = 0,
    .temperature = 0,
    .last_update_time = 0,
    .total_operational_hours = 0,
    .device_enabled_state = 1,  // 1 = enabled (no errors)
    .number_of_active_faults = 0,
    .error_code = 0
};

// Tracking for operational hours calculation
static bool last_flame_state = false;
static uint32_t flame_on_timestamp = 0;  // Timestamp when flame turned on (milliseconds)

// Matter bridge state
static bool initialized = false;

// Critical section for thread-safe attribute access
static critical_section_t bridge_lock;

extern "C" {

void matter_bridge_init(void) {
    printf("\n");
    printf("==========================================\n");
    printf("  Viking Bio Matter Bridge - Full Mode\n");
    printf("==========================================\n\n");
    
    // Initialize critical section for thread safety
    critical_section_init(&bridge_lock);
    
    // Load operational hours from flash storage
    uint32_t stored_hours = 0;
    if (storage_adapter_load_operational_hours(&stored_hours) == 0) {
        attributes.total_operational_hours = stored_hours;
        printf("Loaded operational hours from flash: %lu hours\n", 
               (unsigned long)stored_hours);
    } else {
        printf("No operational hours in storage, starting from 0\n");
        attributes.total_operational_hours = 0;
    }
    
    // Initialize Matter platform
    printf("Initializing Matter platform for Pico W...\n");
    if (platform_manager_init() != 0) {
        printf("[Matter] ERROR: Failed to initialize Matter platform\n");
        printf("Device will continue in degraded mode (no Matter support)\n");
        initialized = false;
        return;
    }
    
    // Check for WiFi credentials in storage
    bool has_credentials = storage_adapter_has_wifi_credentials();
    
    if (has_credentials) {
        // Try to connect with stored credentials
        printf("WiFi credentials found in flash. Connecting...\n");
        if (platform_manager_connect_wifi(NULL, NULL) == 0) {
            printf("Successfully connected to WiFi using stored credentials\n");
            
            // Start DNS-SD advertisement now that network is connected
            printf("\nStarting DNS-SD device discovery...\n");
            if (platform_manager_start_dns_sd_advertisement() != 0) {
                printf("WARNING: DNS-SD advertisement failed\n");
                printf("         Device will not be discoverable via mDNS\n");
            }
        } else {
            printf("WARNING: Failed to connect with stored credentials\n");
            printf("Starting commissioning mode for WiFi setup...\n");
            platform_manager_start_commissioning_mode();
        }
    } else {
        // No credentials - start commissioning mode (BLE)
        printf("No WiFi credentials found in storage\n");
        printf("Starting commissioning mode for WiFi setup...\n");
        if (platform_manager_start_commissioning_mode() != 0) {
            printf("[Matter] ERROR: Failed to start commissioning mode\n");
            printf("Device will continue without network connectivity\n");
            initialized = false;
            return;
        }
        
        // Start DNS-SD advertisement for device discovery
        printf("\nStarting DNS-SD device discovery...\n");
        if (platform_manager_start_dns_sd_advertisement() != 0) {
            printf("WARNING: DNS-SD advertisement failed\n");
            printf("         Device may not be discoverable via mDNS\n");
        }
    }
    
    // Print commissioning information
    platform_manager_print_commissioning_info();
    
    // Initialize Matter reporter (demonstrates attribute subscription)
    printf("Initializing Matter attribute reporter...\n");
    if (matter_reporter_init() != 0) {
        printf("WARNING: Matter reporter initialization failed\n");
    }
    
    // Initialize Matter network subscriber (sends reports over WiFi)
    printf("Initializing Matter network subscriber...\n");
    if (matter_network_subscriber_init() != 0) {
        printf("WARNING: Matter network subscriber initialization failed\n");
        printf("         Attribute reports will not be sent over WiFi\n");
    } else {
        // Add example controller (user should update this to their controller's IP)
        // For testing, you can add your Matter controller's IP address here
        // Example: matter_network_transport_add_controller("192.168.1.100", 5540);
        printf("\n");
        printf("To receive Matter attribute reports over WiFi:\n");
        printf("  1. Note your Matter controller's IP address\n");
        printf("  2. Call: matter_network_transport_add_controller(\"<IP>\", 5540)\n");
        printf("  3. Attribute changes will be sent as JSON over UDP\n");
        printf("\n");
    }
    
    // Initialize subscription bridge (connects matter_attributes to subscribe_handler)
    printf("Initializing Matter subscription bridge...\n");
    if (subscription_bridge_init() != 0) {
        printf("WARNING: Subscription bridge initialization failed\n");
        printf("         Subscriptions may not receive attribute updates\n");
    }
    
    // Initialize Matter protocol stack
    printf("Initializing Matter protocol stack...\n");
    if (matter_protocol_init() != 0) {
        printf("[Matter] ERROR: Failed to initialize Matter protocol stack\n");
        initialized = false;
        return;
    }
    
    // Start commissioning mode
    printf("Starting Matter commissioning...\n");
    uint8_t mac[6];
    platform_manager_get_network_info(NULL, 0, mac);
    
    // Derive setup PIN from MAC
    char setup_pin[9];
    if (platform_manager_derive_setup_pin(mac, setup_pin) != 0) {
        printf("[Matter] ERROR: Failed to derive setup PIN\n");
        initialized = false;
        return;
    }
    
    // Get discriminator from platform (loaded from storage or generated)
    uint16_t discriminator = platform_manager_get_discriminator();
    printf("Using discriminator: %u (0x%03X)\n", discriminator, discriminator);
    
    // Start commissioning with derived PIN and discriminator
    if (matter_protocol_start_commissioning(setup_pin, discriminator) != 0) {
        printf("[Matter] ERROR: Failed to start commissioning\n");
        initialized = false;
        return;
    }
    
    initialized = true;
    
    printf("✓ Matter Bridge fully initialized\n");
    printf("✓ Matter protocol stack running\n");
    printf("✓ Device is ready for commissioning\n");
    printf("✓ Monitoring Viking Bio serial data...\n\n");
}

void matter_bridge_update_flame(bool flame_on) {
    if (!initialized) {
        return;
    }
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    critical_section_enter_blocking(&bridge_lock);
    bool changed = (attributes.flame_state != flame_on);
    if (changed) {
        // Track operational hours when flame state changes
        if (last_flame_state && !flame_on) {
            // Flame turned OFF - accumulate hours
            if (flame_on_timestamp > 0) {
                uint32_t elapsed_ms = current_time - flame_on_timestamp;
                uint32_t elapsed_hours = elapsed_ms / (1000 * 60 * 60);  // Convert ms to hours
                attributes.total_operational_hours += elapsed_hours;
                
                // Save to flash every hour change (avoids excessive flash writes)
                if (elapsed_hours > 0) {
                    storage_adapter_save_operational_hours(attributes.total_operational_hours);
                    printf("Operational hours updated: %lu hours (added %lu)\n",
                           (unsigned long)attributes.total_operational_hours,
                           (unsigned long)elapsed_hours);
                }
            }
            flame_on_timestamp = 0;
        } else if (!last_flame_state && flame_on) {
            // Flame turned ON - start tracking
            flame_on_timestamp = current_time;
        }
        
        attributes.flame_state = flame_on;
        last_flame_state = flame_on;
        attributes.last_update_time = current_time;
    }
    critical_section_exit(&bridge_lock);
    
    if (changed) {
        printf("Matter: OnOff cluster updated - Flame %s\n", flame_on ? "ON" : "OFF");
        
        // Update Matter attribute
        matter_attr_value_t value;
        value.bool_val = flame_on;
        int ret = matter_attributes_update(1, MATTER_CLUSTER_ON_OFF, MATTER_ATTR_ON_OFF, &value);
        if (ret != 0) {
            printf("[Matter] ERROR: Failed to update OnOff attribute (ret=%d)\n", ret);
        } else {
            // Notify platform of attribute change
            platform_manager_report_onoff_change(1);
        }
        
        // Update operational hours attribute in Matter system
        matter_attr_value_t hours_value;
        hours_value.uint32_val = attributes.total_operational_hours;
        matter_attributes_update(1, MATTER_CLUSTER_DIAGNOSTICS, 
                               MATTER_ATTR_TOTAL_OPERATIONAL_HOURS, &hours_value);
    }
}

void matter_bridge_update_fan_speed(uint8_t speed) {
    if (!initialized) {
        return;
    }
    
    critical_section_enter_blocking(&bridge_lock);
    bool changed = (attributes.fan_speed != speed);
    if (changed) {
        attributes.fan_speed = speed;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
    }
    critical_section_exit(&bridge_lock);
    
    if (changed) {
        printf("Matter: LevelControl cluster updated - Fan speed %d%%\n", speed);
        
        // Update Matter attribute
        matter_attr_value_t value;
        value.uint8_val = speed;
        int ret = matter_attributes_update(1, MATTER_CLUSTER_LEVEL_CONTROL, MATTER_ATTR_CURRENT_LEVEL, &value);
        if (ret != 0) {
            printf("[Matter] ERROR: Failed to update LevelControl attribute (ret=%d)\n", ret);
        } else {
            // Notify platform of attribute change
            platform_manager_report_level_change(1);
        }
    }
}

void matter_bridge_update_temperature(uint16_t temp) {
    if (!initialized) {
        return;
    }
    
    critical_section_enter_blocking(&bridge_lock);
    bool changed = (attributes.temperature != temp);
    if (changed) {
        attributes.temperature = temp;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
    }
    critical_section_exit(&bridge_lock);
    
    if (changed) {
        printf("Matter: TemperatureMeasurement cluster updated - %d°C\n", temp);
        
        // Update Matter attribute (convert to centidegrees for Matter spec)
        matter_attr_value_t value;
        value.int16_val = (int16_t)(temp * 100); // Convert to centidegrees
        int ret = matter_attributes_update(1, MATTER_CLUSTER_TEMPERATURE_MEASUREMENT, MATTER_ATTR_MEASURED_VALUE, &value);
        if (ret != 0) {
            printf("[Matter] ERROR: Failed to update Temperature attribute (ret=%d)\n", ret);
        } else {
            // Notify platform of attribute change
            platform_manager_report_temperature_change(1);
        }
    }
}

void matter_bridge_update_attributes(const viking_bio_data_t *data) {
    if (!initialized || data == NULL || !data->valid) {
        return;
    }
    
    // Update individual attributes using specific update functions
    matter_bridge_update_flame(data->flame_detected);
    matter_bridge_update_fan_speed(data->fan_speed);
    matter_bridge_update_temperature(data->temperature);
    matter_bridge_update_diagnostics(data->error_code);
}

void matter_bridge_update_diagnostics(uint8_t error_code) {
    if (!initialized) {
        return;
    }
    
    critical_section_enter_blocking(&bridge_lock);
    bool changed = (attributes.error_code != error_code);
    if (changed) {
        attributes.error_code = error_code;
        
        // Map error code to DeviceEnabledState
        // 0 = no error (enabled), any other value = disabled
        attributes.device_enabled_state = (error_code == 0) ? 1 : 0;
        
        // Map error code to NumberOfActiveFaults
        // If error_code is non-zero, we have 1 fault
        attributes.number_of_active_faults = (error_code == 0) ? 0 : 1;
        
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
    }
    critical_section_exit(&bridge_lock);
    
    if (changed) {
        printf("Matter: Diagnostics cluster updated - Error code: 0x%02X, State: %s, Faults: %d\n",
               error_code,
               attributes.device_enabled_state ? "Enabled" : "Disabled",
               attributes.number_of_active_faults);
        
        // Update Matter attributes
        matter_attr_value_t value;
        
        // Update DeviceEnabledState
        value.uint8_val = attributes.device_enabled_state;
        int ret = matter_attributes_update(1, MATTER_CLUSTER_DIAGNOSTICS, 
                                          MATTER_ATTR_DEVICE_ENABLED_STATE, &value);
        if (ret != 0) {
            printf("[Matter] ERROR: Failed to update DeviceEnabledState (ret=%d)\n", ret);
        }
        
        // Update NumberOfActiveFaults
        value.uint8_val = attributes.number_of_active_faults;
        ret = matter_attributes_update(1, MATTER_CLUSTER_DIAGNOSTICS,
                                      MATTER_ATTR_NUMBER_OF_ACTIVE_FAULTS, &value);
        if (ret != 0) {
            printf("[Matter] ERROR: Failed to update NumberOfActiveFaults (ret=%d)\n", ret);
        }
        
        // Notify platform of attribute changes
        platform_manager_report_attribute_change(MATTER_CLUSTER_DIAGNOSTICS,
                                                MATTER_ATTR_DEVICE_ENABLED_STATE, 1);
        platform_manager_report_attribute_change(MATTER_CLUSTER_DIAGNOSTICS,
                                                MATTER_ATTR_NUMBER_OF_ACTIVE_FAULTS, 1);
    }
}

bool matter_bridge_task(void) {
    if (!initialized) {
        return false;
    }
    
    bool work_done = false;
    
    // Process Matter protocol messages (returns number of messages processed)
    int messages_processed = matter_protocol_task();
    if (messages_processed > 0) {
        work_done = true;
    }
    
    // Process Matter platform tasks (includes attribute reporting)
    platform_manager_task();
    
    return work_done;
}

void matter_bridge_get_attributes(matter_attributes_t *attrs) {
    if (attrs != NULL && initialized) {
        critical_section_enter_blocking(&bridge_lock);
        memcpy(attrs, &attributes, sizeof(matter_attributes_t));
        critical_section_exit(&bridge_lock);
    }
}

int matter_bridge_add_controller(const char *ip_address, uint16_t port) {
    if (!initialized) {
        printf("[Matter] ERROR: Bridge not initialized\n");
        return -1;
    }
    
    if (!ip_address) {
        printf("[Matter] ERROR: Invalid IP address (NULL)\n");
        return -1;
    }
    
    return matter_network_transport_add_controller(ip_address, port);
}

} // extern "C"
