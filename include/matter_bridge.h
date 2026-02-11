#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

// Matter cluster attribute definitions
typedef struct {
    bool flame_state;
    uint8_t fan_speed;
    uint16_t temperature;
    uint32_t last_update_time;
} matter_attributes_t;

// Function prototypes
void matter_bridge_init(void);
void matter_bridge_update_attributes(const viking_bio_data_t *data);
void matter_bridge_task(void);
void matter_bridge_get_attributes(matter_attributes_t *attrs);

// Individual attribute update functions for Matter clusters
void matter_bridge_update_flame(bool flame_on);
void matter_bridge_update_fan_speed(uint8_t speed);
void matter_bridge_update_temperature(uint16_t temp);

#endif // MATTER_BRIDGE_H
