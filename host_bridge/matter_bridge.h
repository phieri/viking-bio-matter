#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

#ifdef ENABLE_MATTER
// Matter SDK integration is available
#define MATTER_ENABLED 1
#else
// Stub implementation
#define MATTER_ENABLED 0
#endif

/**
 * @brief Initialize the Matter bridge
 * 
 * Initializes the Matter stack, creates endpoints, and sets up commissioning.
 * 
 * @param setup_code Matter setup code for commissioning (e.g., "20202021")
 * @param discriminator Commissioning discriminator value (0-4095)
 * @return true if initialization successful, false otherwise
 */
bool matter_bridge_init(const char *setup_code, uint16_t discriminator);

/**
 * @brief Shutdown the Matter bridge
 */
void matter_bridge_shutdown(void);

/**
 * @brief Run the Matter event loop
 * 
 * Should be called periodically from main loop or run in a dedicated thread.
 * Processes Matter events, network activity, and subscriptions.
 * 
 * @param timeout_ms Maximum time to block waiting for events (milliseconds)
 */
void matter_bridge_run_event_loop(uint32_t timeout_ms);

/**
 * @brief Update flame state attribute
 * 
 * Reports flame state change to subscribed Matter controllers.
 * Maps to On/Off cluster (0x0006).
 * 
 * @param flame_on true if flame detected, false otherwise
 */
void matter_bridge_update_flame(bool flame_on);

/**
 * @brief Update fan speed attribute
 * 
 * Reports fan speed change to subscribed Matter controllers.
 * Maps to Level Control cluster (0x0008).
 * 
 * @param speed Fan speed percentage (0-100)
 */
void matter_bridge_update_fan_speed(uint8_t speed);

/**
 * @brief Update temperature attribute
 * 
 * Reports temperature change to subscribed Matter controllers.
 * Maps to Temperature Measurement cluster (0x0402).
 * 
 * @param temperature Temperature in Celsius
 */
void matter_bridge_update_temperature(int16_t temperature);

/**
 * @brief Check if Matter bridge is commissioned
 * 
 * @return true if commissioned and connected, false otherwise
 */
bool matter_bridge_is_commissioned(void);

/**
 * @brief Get the QR code for commissioning
 * 
 * @param buffer Buffer to store QR code string
 * @param buffer_size Size of buffer
 * @return true if QR code generated, false otherwise
 */
bool matter_bridge_get_qr_code(char *buffer, size_t buffer_size);

#endif // MATTER_BRIDGE_H
