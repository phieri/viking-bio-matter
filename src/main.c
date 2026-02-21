#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "hardware/timer.h"
#include "serial_handler.h"
#include "viking_bio_protocol.h"
#include "matter_bridge.h"
#include "network_adapter.h"
#include "platform_manager.h"
#include "matter_minimal/matter_protocol.h"
#include "multicore_coordinator.h"
#include "version.h"

// LED control for Pico W (CYW43 chip controls the LED)
// LED is now enabled using CYW43 architecture functions
#define LED_ENABLED 1
static bool main_led_available = false;
// Mirror the Pico SDK blink example: explicitly initialise the LED GPIO via CYW43
#define LED_INIT() do { if (!main_led_available && network_adapter_early_init() == 0) main_led_available = true; } while(0)
#define LED_SET(state) do { if (main_led_available) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (state)); } while(0)

// Event system for efficient interrupt-driven architecture
// Volatile since modified from interrupt context
volatile uint32_t event_flags = 0;

// Event flag definitions
#define EVENT_SERIAL_DATA    (1 << 0)  // Serial data received in UART interrupt
#define EVENT_MATTER_MSG     (1 << 1)  // Matter message needs processing
#define EVENT_TIMEOUT_CHECK  (1 << 2)  // Periodic timeout check needed
#define EVENT_LED_UPDATE     (1 << 3)  // LED state needs update

/**
 * Periodic timer callback - runs every 1 second
 * Sets event flags for periodic tasks (timeout checks, LED management)
 * Timer runs in interrupt context, so keep work minimal
 */
bool periodic_timer_callback(struct repeating_timer *t) {
    // Set event flags for periodic tasks
    event_flags |= EVENT_TIMEOUT_CHECK | EVENT_LED_UPDATE;
    __sev();  // Wake CPU from WFE if sleeping
    return true;  // Keep timer repeating
}

/**
 * Calculate time until next scheduled wakeup event
 * Returns duration in milliseconds until next event, or 100ms default
 */
uint32_t calculate_next_wakeup(uint32_t led_tick_off_time, bool led_tick_active) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t next_wakeup = UINT32_MAX;
    
    // LED tick timeout
    if (led_tick_active && led_tick_off_time > now) {
        uint32_t led_delta = led_tick_off_time - now;
        next_wakeup = (led_delta < next_wakeup) ? led_delta : next_wakeup;
    }
    
    // Default to 100ms if nothing scheduled soon (cap for responsiveness)
    return (next_wakeup == UINT32_MAX) ? 100 : next_wakeup;
}

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize CYW43 chip early so the LED is available for diagnostic blinks
    // throughout the rest of the startup sequence
    if (network_adapter_early_init() != 0) {
        printf("[Main] ERROR: Failed to initialize CYW43 chip - LED unavailable\n");
    } else {
        main_led_available = true;
        // 1 quick blink: CYW43 chip up, LED confirmed working
        LED_SET(1); sleep_ms(100); LED_SET(0); sleep_ms(100);
    }

    // Print version information
    printf("\n");
    version_print_info();
    
    printf("Viking Bio Matter Bridge starting...\n");
    
    // Initialize components in order
    printf("Initializing Viking Bio protocol parser...\n");
    viking_bio_init();
    
    printf("Initializing serial handler...\n");
    serial_handler_init();
    // 2 quick blinks: serial handler ready
    for (int i = 0; i < 2; i++) {
        LED_SET(1); sleep_ms(100); LED_SET(0); sleep_ms(100);
    }

    printf("Initializing Matter bridge...\n");
    matter_bridge_init();

    // platform_manager_init() may have successfully initialized CYW43 as a
    // fallback safety net; refresh local LED availability state.
    if (!main_led_available && network_adapter_early_init() == 0) {
        main_led_available = true;
    }
    
    // Initialize LED for status indication (after Matter init for Pico W)
    LED_INIT();
    
    printf("Initialization complete. Reading serial data...\n");
    
    // Initialize multicore coordination
    printf("\nInitializing multicore support...\n");
    if (multicore_coordinator_init() != 0) {
        printf("[Main] ERROR: Failed to initialize multicore coordinator\n");
        printf("[Main] Device will continue in single-core mode\n");
    } else {
        // Launch core 1 for Matter/network processing
        if (multicore_coordinator_launch_core1() == 0) {
            printf("[OK] Multicore enabled: Core 0 (serial/LED), Core 1 (Matter/network)\n");
        } else {
            printf("WARNING: Failed to launch core 1\n");
            printf("         Device will continue in single-core mode\n");
        }
    }
    
    // Slower blink LED to indicate startup complete
    for (int i = 0; i < 3; i++) {
        LED_SET(1);
        sleep_ms(200);
        LED_SET(0);
        sleep_ms(200);
    }
    
    // Enable watchdog with 8 second timeout for system reliability
    // The watchdog must be updated at least once every 8 seconds
    watchdog_enable(8000, false);
    printf("Watchdog enabled with 8 second timeout\n");
    
    // Initialize hardware timer for periodic tasks (1 second interval)
    struct repeating_timer timer;
    if (!add_repeating_timer_ms(1000, periodic_timer_callback, NULL, &timer)) {
        printf("WARNING: Failed to initialize periodic timer\n");
        printf("         Falling back to polling for periodic tasks\n");
    } else {
        printf("Periodic timer enabled (1 second interval)\n");
    };
    
    // Main loop - event-driven architecture
    uint8_t buffer[SERIAL_BUFFER_SIZE];
    viking_bio_data_t viking_data;
    bool timeout_triggered = false;  // Track if timeout has been triggered
    bool ble_commissioning_stopped = false;  // Track if BLE has been stopped after WiFi connection
    uint32_t led_tick_off_time = 0;  // Timestamp when LED tick should turn off
    bool led_tick_active = false;    // Track if LED tick is active
    uint32_t led_grace_period_end = 0;  // Timestamp when grace period after tick ends
    uint32_t queue_overflow_count = 0;  // Track queue overflows for rate-limited logging
    
    while (true) {
        // Track if any work was done this iteration
        bool work_done = false;
        
        // Update watchdog to prevent system reset (must be done every loop iteration)
        watchdog_update();
        
        // Process serial data events
        // Note: serial_handler_task() polls hardware but is lightweight
        serial_handler_task();
        
        if ((event_flags & EVENT_SERIAL_DATA) || serial_handler_data_available()) {
            // Clear serial event flag
            event_flags &= ~EVENT_SERIAL_DATA;
            
            size_t bytes_read = serial_handler_read(buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                // Parse Viking Bio data
                if (viking_bio_parse_data(buffer, bytes_read, &viking_data)) {
                    uint32_t now = to_ms_since_boot(get_absolute_time());
                    // Turn on LED for 200ms to indicate serial message received
                    LED_SET(1);
                    led_tick_active = true;
                    led_tick_off_time = now + 200;
                    
                    // Check if data resumed after timeout
                    if (timeout_triggered) {
                        printf("Viking Bio: Data resumed after timeout\n");
                        timeout_triggered = false;
                    }
                    
                    // Send Viking Bio data to core 1 for Matter attribute updates
                    // Falls back to direct call if multicore not running
                    if (multicore_coordinator_is_core1_running()) {
                        if (multicore_coordinator_send_data(&viking_data) != 0) {
                            // Queue full - count overflow and log periodically (every 10 overflows)
                            queue_overflow_count++;
                            if (queue_overflow_count % 10 == 1) {
                                printf("Warning: Viking Bio data queue full (%lu overflows)\n", 
                                       (unsigned long)queue_overflow_count);
                            }
                        }
                    } else {
                        // Fallback: update attributes directly on core 0
                        matter_bridge_update_attributes(&viking_data);
                    }
                    
                    // Log data to USB serial
                    printf("Flame: %s, Fan Speed: %d%%, Temp: %d°C\n",
                           viking_data.flame_detected ? "ON" : "OFF",
                           viking_data.fan_speed,
                           viking_data.temperature);
                }
                work_done = true;
            }
        }
        
        // Process Matter messages
        // Only run on core 0 if multicore is not active (fallback mode)
        if (!multicore_coordinator_is_core1_running() && matter_bridge_task()) {
            work_done = true;
        }
        
        // Periodic timeout check (triggered by 1-second timer)
        if (event_flags & EVENT_TIMEOUT_CHECK) {
            event_flags &= ~EVENT_TIMEOUT_CHECK;
            
            // Check for data timeout (Viking Bio unit powered off)
            if (!timeout_triggered && viking_bio_is_data_stale(VIKING_BIO_TIMEOUT_MS)) {
                timeout_triggered = true;
                printf("Viking Bio: No data received for 30s - clearing attributes\n");
                
                // Create cleared data structure
                viking_bio_data_t cleared_data = {
                    .flame_detected = false,
                    .fan_speed = 0,
                    .temperature = 0,
                    .error_code = 0,
                    .valid = true
                };
                
                // Update Matter attributes with cleared state
                // Send to core 1 if multicore is running, otherwise update directly
                if (multicore_coordinator_is_core1_running()) {
                    multicore_coordinator_send_data(&cleared_data);
                } else {
                    matter_bridge_update_attributes(&cleared_data);
                }
            }
            
            // Check if WiFi is connected and BLE commissioning should be stopped
            if (!ble_commissioning_stopped && network_adapter_is_connected() && matter_protocol_is_commissioned()) {
                ble_commissioning_stopped = true;  // Set flag first to prevent re-entry
                
                printf("\n");
                printf("====================================\n");
                printf("  WiFi Connected & Commissioned\n");
                printf("====================================\n");
                printf("Stopping BLE commissioning mode...\n");
                
                // Stop BLE commissioning after WiFi is connected and device is commissioned
                if (platform_manager_stop_commissioning_mode() == 0) {
                    printf("[OK] BLE commissioning stopped successfully\n");
                    printf("Device will continue operating over WiFi\n");
                } else {
                    printf("WARNING: Failed to stop BLE commissioning\n");
                }
                
                printf("====================================\n\n");
            }
            
            work_done = true;
        }
        
        // LED update (triggered by timer or LED tick)
        if (event_flags & EVENT_LED_UPDATE) {
            event_flags &= ~EVENT_LED_UPDATE;
            work_done = true;
        }
        
        // Handle LED tick timeout and steady state
        uint32_t now = to_ms_since_boot(get_absolute_time());
        
        // Turn off LED tick after 200ms
        if (led_tick_active && now >= led_tick_off_time) {
            LED_SET(0);
            led_tick_active = false;
            // Set grace period: keep LED off for 800ms after tick to make it visible
            led_grace_period_end = now + 800;
        }
        
        // LED behavior: Constantly ON when connected to WiFi + Matter fabric but not receiving serial data
        // When receiving serial data, show 200ms tick instead
        // When not connected/commissioned, slow blink to show device is alive
        if (!led_tick_active && now >= led_grace_period_end) {
            // Check if connected to WiFi and commissioned to Matter fabric
            if (network_adapter_is_connected() && matter_protocol_is_commissioned()) {
                // Keep LED constantly on to indicate ready state
                LED_SET(1);
            } else {
                // Not fully connected/commissioned - slow blink to indicate
                // the device is alive and waiting for commissioning
                LED_SET((now / 500) % 2 == 0);
            }
        }
        
        // Dynamic sleep if no work was done
        if (!work_done) {
            uint32_t sleep_duration = calculate_next_wakeup(led_tick_off_time, led_tick_active);
            if (sleep_duration > 0) {
                // Cap sleep at 100ms for responsiveness
                uint32_t capped_sleep = (sleep_duration < 100) ? sleep_duration : 100;
                sleep_ms(capped_sleep);
            }
        }
    }
    
    return 0;
}
