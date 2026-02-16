#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Matter cluster attribute definitions
typedef struct {
    bool flame_state;           // OnOff cluster: flame detected state
    uint8_t fan_speed;          // LevelControl cluster: fan speed 0-100%
    uint16_t temperature;       // TemperatureMeasurement cluster: temperature in °C
    uint32_t last_update_time;  // Timestamp of last attribute update (milliseconds)
} matter_attributes_t;

/**
 * Initialize the Matter bridge
 * Initializes platform, connects WiFi, and prints commissioning info
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
 * Processes Matter platform tasks
 * Call regularly in main loop
 * 
 * @return true if any work was done (messages processed), false if idle
 */
bool matter_bridge_task(void);

/**
 * Get current Matter attributes
 * Returns cached attributes without locking (safe for read-only access)
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

/**
 * Add a Matter controller to receive attribute reports over WiFi
 * @param ip_address Controller IP address (e.g., "192.168.1.100")
 * @param port UDP port (typically 5540 for Matter)
 * @return Controller ID on success, -1 on failure
 */
int matter_bridge_add_controller(const char *ip_address, uint16_t port);

#ifdef __cplusplus
}
#endif

#endif // MATTER_BRIDGE_H
