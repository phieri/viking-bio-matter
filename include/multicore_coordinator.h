/*
 * multicore_coordinator.h
 * Multicore coordination for Raspberry Pi Pico W
 * 
 * Architecture:
 * - Core 0: Serial input (UART interrupt), Viking Bio parsing, LED control, main coordination
 * - Core 1: Matter protocol processing, network tasks, DNS-SD, attribute reporting
 * 
 * Communication:
 * - Inter-core queue for Viking Bio data (core 0 → core 1)
 * - Event flags for signaling
 * - Existing critical sections for Matter attributes
 */

#ifndef MULTICORE_COORDINATOR_H
#define MULTICORE_COORDINATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// Maximum queue size for Viking Bio data between cores
#define VIKING_DATA_QUEUE_SIZE 8

/**
 * Initialize multicore coordination
 * Sets up inter-core communication primitives
 * @return 0 on success, -1 on failure
 */
int multicore_coordinator_init(void);

/**
 * Launch core 1 for network and Matter processing
 * Core 1 will handle:
 * - Matter protocol task (message processing)
 * - Matter attribute reporting
 * - Platform manager tasks
 * @return 0 on success, -1 on failure
 */
int multicore_coordinator_launch_core1(void);

/**
 * Send Viking Bio data from core 0 to core 1
 * Non-blocking: returns immediately if queue is full
 * @param data Pointer to Viking Bio data structure
 * @return 0 on success, -1 if queue is full or error
 */
int multicore_coordinator_send_data(const viking_bio_data_t *data);

/**
 * Signal core 1 to process Matter tasks
 * Called from core 0 when network activity may have occurred
 */
void multicore_coordinator_signal_matter_task(void);

/**
 * Check if core 1 is running
 * @return true if core 1 is active, false otherwise
 */
bool multicore_coordinator_is_core1_running(void);

/**
 * Get core 1 statistics
 * @param messages_processed Output: total messages processed by core 1
 * @param data_updates_processed Output: total Viking Bio updates processed
 */
void multicore_coordinator_get_stats(uint32_t *messages_processed, uint32_t *data_updates_processed);

#ifdef __cplusplus
}
#endif

#endif // MULTICORE_COORDINATOR_H
