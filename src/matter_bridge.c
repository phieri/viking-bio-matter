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

void matter_bridge_init(void) {
    // Initialize Matter stack (simplified for now)
    // In a full implementation, this would initialize the Matter SDK
    // and set up the device as a bridge with appropriate clusters
    
    memset(&attributes, 0, sizeof(attributes));
    initialized = true;
    
    printf("Matter Bridge initialized\n");
    printf("Device ready to report flame status and fan speed\n");
}

void matter_bridge_update_attributes(const viking_bio_data_t *data) {
    if (!initialized || data == NULL || !data->valid) {
        return;
    }
    
    // Update attributes from Viking Bio data
    bool attributes_changed = false;
    
    if (attributes.flame_state != data->flame_detected) {
        attributes.flame_state = data->flame_detected;
        attributes_changed = true;
        printf("Matter: Flame state changed to %s\n", 
               attributes.flame_state ? "ON" : "OFF");
    }
    
    if (attributes.fan_speed != data->fan_speed) {
        attributes.fan_speed = data->fan_speed;
        attributes_changed = true;
        printf("Matter: Fan speed changed to %d%%\n", attributes.fan_speed);
    }
    
    if (attributes.temperature != data->temperature) {
        attributes.temperature = data->temperature;
        attributes_changed = true;
        printf("Matter: Temperature changed to %d°C\n", attributes.temperature);
    }
    
    if (attributes_changed) {
        attributes.last_update_time = to_ms_since_boot(get_absolute_time());
        
        // In a full implementation, this would trigger Matter attribute reports
        // to subscribed controllers
    }
}

void matter_bridge_task(void) {
    if (!initialized) {
        return;
    }
    
    // Periodic Matter stack processing
    // In a full implementation, this would call Matter SDK event loop functions
    // to handle network events, attribute subscriptions, commands, etc.
}

void matter_bridge_get_attributes(matter_attributes_t *attrs) {
    if (attrs != NULL && initialized) {
        memcpy(attrs, &attributes, sizeof(matter_attributes_t));
    }
}
