#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "matter_bridge.h"
#include "../platform/pico_w_chip_port/platform_manager.h"
#include "../platform/pico_w_chip_port/matter_attributes.h"
#include "../platform/pico_w_chip_port/matter_reporter.h"
#include "../platform/pico_w_chip_port/matter_network_subscriber.h"
#include "../platform/pico_w_chip_port/matter_network_transport.h"
#include "matter_minimal/interaction/subscription_bridge.h"

// Matter attributes storage
static matter_attributes_t attributes = {
    .flame_state = false,
    .fan_speed = 0,
    .temperature = 0,
    .last_update_time = 0
};

// Matter bridge state
static bool initialized = false;

extern "C" {

void matter_bridge_init(void) {
    printf("\n");
    printf("==========================================\n");
    printf("  Viking Bio Matter Bridge - Full Mode\n");
    printf("==========================================\n\n");
    
    // Initialize Matter platform
    printf("Initializing Matter platform for Pico W...\n");
    if (platform_manager_init() != 0) {
        printf("ERROR: Failed to initialize Matter platform\n");
        printf("Device will continue in degraded mode (no Matter support)\n");
        initialized = false;
        return;
    }
    
    // Connect to WiFi
    printf("Connecting to WiFi...\n");
    if (platform_manager_connect_wifi(NULL, NULL) != 0) {
        printf("ERROR: Failed to connect to WiFi\n");
        printf("Update WiFi credentials in platform/pico_w_chip_port/network_adapter.cpp\n");
        printf("Device will continue without network connectivity\n");
        initialized = false;
        return;
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
    
    initialized = true;
    
    printf("✓ Matter Bridge fully initialized\n");
    printf("✓ Device is ready for commissioning\n");
    printf("✓ Monitoring Viking Bio serial data...\n\n");
}

void matter_bridge_update_flame(bool flame_on) {
    if (!initialized) {
        return;
    }
    
    if (attributes.flame_state != flame_on) {
        attributes.flame_state = flame_on;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
        
        printf("Matter: OnOff cluster updated - Flame %s\n", flame_on ? "ON" : "OFF");
        
        // Update Matter attribute
        matter_attr_value_t value;
        value.bool_val = flame_on;
        matter_attributes_update(1, MATTER_CLUSTER_ON_OFF, MATTER_ATTR_ON_OFF, &value);
    }
}

void matter_bridge_update_fan_speed(uint8_t speed) {
    if (!initialized) {
        return;
    }
    
    if (attributes.fan_speed != speed) {
        attributes.fan_speed = speed;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
        
        printf("Matter: LevelControl cluster updated - Fan speed %d%%\n", speed);
        
        // Update Matter attribute
        matter_attr_value_t value;
        value.uint8_val = speed;
        matter_attributes_update(1, MATTER_CLUSTER_LEVEL_CONTROL, MATTER_ATTR_CURRENT_LEVEL, &value);
    }
}

void matter_bridge_update_temperature(uint16_t temp) {
    if (!initialized) {
        return;
    }
    
    if (attributes.temperature != temp) {
        attributes.temperature = temp;
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
        
        printf("Matter: TemperatureMeasurement cluster updated - %d°C\n", temp);
        
        // Update Matter attribute (convert to centidegrees for Matter spec)
        matter_attr_value_t value;
        value.int16_val = (int16_t)(temp * 100); // Convert to centidegrees
        matter_attributes_update(1, MATTER_CLUSTER_TEMPERATURE_MEASUREMENT, MATTER_ATTR_MEASURED_VALUE, &value);
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
    
    // Process Matter platform tasks (includes attribute reporting)
    platform_manager_task();
}

void matter_bridge_get_attributes(matter_attributes_t *attrs) {
    if (attrs != NULL && initialized) {
        memcpy(attrs, &attributes, sizeof(matter_attributes_t));
    }
}

int matter_bridge_add_controller(const char *ip_address, uint16_t port) {
    if (!initialized) {
        printf("Matter: ERROR - Bridge not initialized\n");
        return -1;
    }
    
    return matter_network_transport_add_controller(ip_address, port);
}

} // extern "C"
