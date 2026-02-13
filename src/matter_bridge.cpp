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

// Forward declare storage function
extern "C" {
    int storage_adapter_has_wifi_credentials(void);
}

// Matter attributes storage
static matter_attributes_t attributes = {
    .flame_state = false,
    .fan_speed = 0,
    .temperature = 0,
    .last_update_time = 0
};

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
    
    // Initialize Matter platform
    printf("Initializing Matter platform for Pico W...\n");
    if (platform_manager_init() != 0) {
        printf("ERROR: Failed to initialize Matter platform\n");
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
        } else {
            printf("WARNING: Failed to connect with stored credentials\n");
            printf("Starting commissioning mode for WiFi setup...\n");
            platform_manager_start_commissioning_mode();
        }
    } else {
        // No credentials - start commissioning mode
        printf("No WiFi credentials found in storage\n");
        printf("Starting commissioning mode for WiFi setup...\n");
        if (platform_manager_start_commissioning_mode() != 0) {
            printf("ERROR: Failed to start commissioning mode\n");
            printf("Device will continue without network connectivity\n");
            initialized = false;
            return;
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
        printf("ERROR: Failed to initialize Matter protocol stack\n");
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
        printf("ERROR: Failed to derive setup PIN\n");
        initialized = false;
        return;
    }
    
    // Start commissioning with derived PIN and discriminator
    if (matter_protocol_start_commissioning(setup_pin, 3840) != 0) {
        printf("ERROR: Failed to start commissioning\n");
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
    
    critical_section_enter_blocking(&bridge_lock);
    bool changed = (attributes.flame_state != flame_on);
    if (changed) {
        attributes.flame_state = flame_on;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
    }
    critical_section_exit(&bridge_lock);
    
    if (changed) {
        printf("Matter: OnOff cluster updated - Flame %s\n", flame_on ? "ON" : "OFF");
        
        // Update Matter attribute
        matter_attr_value_t value;
        value.bool_val = flame_on;
        int ret = matter_attributes_update(1, MATTER_CLUSTER_ON_OFF, MATTER_ATTR_ON_OFF, &value);
        if (ret != 0) {
            printf("ERROR: Failed to update OnOff attribute (ret=%d)\n", ret);
        } else {
            // Notify platform of attribute change
            platform_manager_report_onoff_change(1);
        }
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
            printf("ERROR: Failed to update LevelControl attribute (ret=%d)\n", ret);
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
            printf("ERROR: Failed to update Temperature attribute (ret=%d)\n", ret);
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
}

void matter_bridge_task(void) {
    if (!initialized) {
        return;
    }
    
    // Process Matter protocol messages
    matter_protocol_task();
    
    // Process Matter platform tasks (includes attribute reporting)
    platform_manager_task();
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
        printf("Matter: ERROR - Bridge not initialized\n");
        return -1;
    }
    
    if (!ip_address) {
        printf("Matter: ERROR - Invalid IP address (NULL)\n");
        return -1;
    }
    
    return matter_network_transport_add_controller(ip_address, port);
}

} // extern "C"
