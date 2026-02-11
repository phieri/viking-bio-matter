#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "matter_bridge.h"

// Matter attributes storage
static matter_attributes_t attributes = {
    .flame_state = false,
    .fan_speed = 0,
    .temperature = 0,
    .last_update_time = 0
};

// Matter bridge state
static bool initialized = false;

#ifdef ENABLE_MATTER
// Full Matter implementation with platform integration
extern "C" {
int platform_manager_init(void);
int platform_manager_connect_wifi(const char *ssid, const char *password);
bool platform_manager_is_wifi_connected(void);
void platform_manager_print_commissioning_info(void);
void platform_manager_task(void);
}

void matter_bridge_init(void) {
    printf("\n");
    printf("==========================================\n");
    printf("  Viking Bio Matter Bridge - Full Mode\n");
    printf("==========================================\n\n");
    
    // Initialize Matter platform
    printf("Initializing Matter platform for Pico W...\n");
    if (platform_manager_init() != 0) {
        printf("ERROR: Failed to initialize Matter platform\n");
        return;
    }
    
    // Connect to WiFi
    printf("Connecting to WiFi...\n");
    if (platform_manager_connect_wifi(NULL, NULL) != 0) {
        printf("ERROR: Failed to connect to WiFi\n");
        printf("Update WiFi credentials in platform/pico_w_chip_port/network_adapter.cpp\n");
        return;
    }
    
    // Print commissioning information
    platform_manager_print_commissioning_info();
    
    // Initialize attribute storage
    memset(&attributes, 0, sizeof(attributes));
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
        // In full Matter SDK: call ChipDeviceEvent to trigger attribute report
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
        // In full Matter SDK: call ChipDeviceEvent to trigger attribute report
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
        // In full Matter SDK: call ChipDeviceEvent to trigger attribute report
    }
}

#else
// Stub implementation when Matter is disabled
void matter_bridge_init(void) {
    printf("Matter Bridge initialized (stub mode - Matter disabled)\n");
    printf("Build with -DENABLE_MATTER=ON for full Matter support\n");
    
    memset(&attributes, 0, sizeof(attributes));
    initialized = true;
}

void matter_bridge_update_flame(bool flame_on) {
    if (!initialized) {
        return;
    }
    
    if (attributes.flame_state != flame_on) {
        attributes.flame_state = flame_on;
        printf("Matter: Flame state changed to %s (stub mode)\n", 
               flame_on ? "ON" : "OFF");
    }
}

void matter_bridge_update_fan_speed(uint8_t speed) {
    if (!initialized) {
        return;
    }
    
    if (attributes.fan_speed != speed) {
        attributes.fan_speed = speed;
        printf("Matter: Fan speed changed to %d%% (stub mode)\n", speed);
    }
}

void matter_bridge_update_temperature(uint16_t temp) {
    if (!initialized) {
        return;
    }
    
    if (attributes.temperature != temp) {
        attributes.temperature = temp;
        printf("Matter: Temperature changed to %d°C (stub mode)\n", temp);
    }
}
#endif

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
    
#ifdef ENABLE_MATTER
    // Process Matter platform tasks
    platform_manager_task();
#endif
    
    // Periodic Matter stack processing would happen here
    // In full SDK: ChipDeviceEvent processing, network polling, etc.
}

void matter_bridge_get_attributes(matter_attributes_t *attrs) {
    if (attrs != NULL && initialized) {
        memcpy(attrs, &attributes, sizeof(matter_attributes_t));
    }
}
