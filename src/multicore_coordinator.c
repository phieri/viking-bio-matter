/*
 * multicore_coordinator.c
 * Multicore coordination implementation using Pico SDK multicore API
 */

#include "multicore_coordinator.h"
#include "matter_bridge.h"
#include "matter_minimal/matter_protocol.h"
#include "platform_manager.h"
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/sync.h"

// Inter-core queue for Viking Bio data
static queue_t viking_data_queue;

// Core 1 state
static volatile bool core1_running = false;
static volatile bool core1_should_exit = false;

// Statistics
static volatile uint32_t core1_messages_processed = 0;
static volatile uint32_t core1_data_updates_processed = 0;

// Event signaling for Matter tasks
static volatile bool matter_task_signaled = false;

/**
 * Core 1 entry point
 * Runs Matter protocol processing and network tasks
 */
static void core1_entry(void) {
    printf("Core 1: Started\n");
    core1_running = true;
    
    viking_bio_data_t data;
    
    while (!core1_should_exit) {
        bool work_done = false;
        
        // Process Viking Bio data from queue (non-blocking)
        if (queue_try_remove(&viking_data_queue, &data)) {
            // Update Matter attributes with Viking Bio data
            matter_bridge_update_attributes(&data);
            core1_data_updates_processed++;
            work_done = true;
        }
        
        // Process Matter protocol messages (network activity)
        // This handles incoming UDP messages, PASE, read requests, subscriptions
        int messages = matter_protocol_task();
        if (messages > 0) {
            core1_messages_processed += messages;
            work_done = true;
        }
        
        // Process Matter platform tasks (attribute reporting, maintenance)
        platform_manager_task();
        
        // Clear Matter task signal flag
        if (matter_task_signaled) {
            matter_task_signaled = false;
            work_done = true;
        }
        
        // Yield CPU if no work was done
        if (!work_done) {
            // Sleep briefly to avoid busy-waiting
            // Use tight_loop_contents() to allow other core to run
            sleep_us(100);  // 100 microseconds
        }
    }
    
    printf("Core 1: Exiting\n");
    core1_running = false;
}

int multicore_coordinator_init(void) {
    printf("Multicore: Initializing inter-core communication...\n");
    
    // Initialize queue for Viking Bio data
    queue_init(&viking_data_queue, sizeof(viking_bio_data_t), VIKING_DATA_QUEUE_SIZE);
    
    printf("Multicore: Queue initialized (size=%d)\n", VIKING_DATA_QUEUE_SIZE);
    return 0;
}

int multicore_coordinator_launch_core1(void) {
    if (core1_running) {
        printf("Multicore: Core 1 already running\n");
        return 0;
    }
    
    printf("Multicore: Launching core 1 for Matter/network processing...\n");
    
    // Launch core 1
    multicore_launch_core1(core1_entry);
    
    // Wait a bit for core 1 to start
    sleep_ms(100);
    
    if (core1_running) {
        printf("Multicore: Core 1 started successfully\n");
        printf("  - Core 0: Serial input, LED control, coordination\n");
        printf("  - Core 1: Matter protocol, network tasks, reporting\n");
        return 0;
    } else {
        printf("ERROR: Core 1 failed to start\n");
        return -1;
    }
}

int multicore_coordinator_send_data(const viking_bio_data_t *data) {
    if (!data || !core1_running) {
        return -1;
    }
    
    // Try to add to queue (non-blocking)
    if (queue_try_add(&viking_data_queue, data)) {
        return 0;
    } else {
        // Queue full - data will be dropped
        // This is acceptable as Viking Bio sends data frequently
        return -1;
    }
}

void multicore_coordinator_signal_matter_task(void) {
    matter_task_signaled = true;
}

bool multicore_coordinator_is_core1_running(void) {
    return core1_running;
}

void multicore_coordinator_get_stats(uint32_t *messages_processed, uint32_t *data_updates_processed) {
    if (messages_processed) {
        *messages_processed = core1_messages_processed;
    }
    if (data_updates_processed) {
        *data_updates_processed = core1_data_updates_processed;
    }
}
