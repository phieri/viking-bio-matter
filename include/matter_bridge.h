#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

// Matter cluster attribute definitions
typedef struct {
    bool flame_state;           // OnOff cluster: flame detected state
    uint8_t fan_speed;          // LevelControl cluster: fan speed 0-100%
    uint16_t temperature;       // TemperatureMeasurement cluster: temperature in °C
    uint32_t last_update_time;  // Timestamp of last attribute update (milliseconds)
} matter_attributes_t;

/**
 * Initialize the Matter bridge
 * In full mode (ENABLE_MATTER=ON): Initializes platform, connects WiFi, prints commissioning info
 * In stub mode: Just logs initialization message
 */
void matter_bridge_init(void);

/**
 * Update all Matter attributes from Viking Bio data
 * Calls individual update functions for each attribute
 * 
 * @param data Viking Bio data to publish to Matter clusters (must not be NULL and valid)
 */
void matter_bridge_update_attributes(const viking_bio_data_t *data);

/**
 * Periodic task for Matter bridge processing
 * Processes platform tasks when in full Matter mode
 * Call regularly in main loop
 */
void matter_bridge_task(void);

/**
 * Get current Matter attributes
 * 
 * @param attrs Output structure to receive attributes (must not be NULL)
 */
void matter_bridge_get_attributes(matter_attributes_t *attrs);

// Individual attribute update functions for Matter clusters

/**
 * Update OnOff cluster with flame state
 * @param flame_on True if flame is detected, false otherwise
 */
void matter_bridge_update_flame(bool flame_on);

/**
 * Update LevelControl cluster with fan speed
 * @param speed Fan speed percentage (0-100)
 */
void matter_bridge_update_fan_speed(uint8_t speed);

/**
 * Update TemperatureMeasurement cluster with temperature
 * @param temp Temperature in degrees Celsius
 */
void matter_bridge_update_temperature(uint16_t temp);

#endif // MATTER_BRIDGE_H
