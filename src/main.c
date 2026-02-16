#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/watchdog.h"
#include "serial_handler.h"
#include "viking_bio_protocol.h"
#include "matter_bridge.h"
#include "network_adapter.h"

// LED control for Pico W (CYW43 chip controls the LED)
// LED is now enabled using CYW43 architecture functions
#define LED_ENABLED 1
#define LED_INIT() do { /* LED initialized as part of cyw43_arch_init */ } while(0)
#define LED_SET(state) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, (state))

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    printf("Viking Bio Matter Bridge starting...\n");
    
    // Initialize components in order
    printf("Initializing Viking Bio protocol parser...\n");
    viking_bio_init();
    
    printf("Initializing serial handler...\n");
    serial_handler_init();
    
    printf("Initializing Matter bridge...\n");
    matter_bridge_init();
    
    // Initialize LED for status indication (after Matter init for Pico W)
    LED_INIT();
    
    // Blink LED to indicate startup complete
    for (int i = 0; i < 3; i++) {
        LED_SET(1);
        sleep_ms(200);
        LED_SET(0);
        sleep_ms(200);
    }
    
    printf("Initialization complete. Reading serial data...\n");
    
    // Enable watchdog with 8 second timeout for system reliability
    // The watchdog must be updated at least once every 8 seconds
    watchdog_enable(8000, false);
    printf("Watchdog enabled with 8 second timeout\n");
    
    // Main loop
    uint8_t buffer[SERIAL_BUFFER_SIZE];
    viking_bio_data_t viking_data;
    uint32_t last_blink = 0;
    bool led_state = false;
    bool timeout_triggered = false;  // Track if timeout has been triggered
    bool softap_timeout_handled = false;  // Track if SoftAP timeout has been handled to prevent re-execution
    uint32_t led_tick_off_time = 0;  // Timestamp when LED tick should turn off
    bool led_tick_active = false;    // Track if LED tick is active
    
    while (true) {
        // Update watchdog to prevent system reset
        watchdog_update();
        
        // Handle serial data
        serial_handler_task();
        
        // Check for available data
        if (serial_handler_data_available()) {
            size_t bytes_read = serial_handler_read(buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                // Parse Viking Bio data
                if (viking_bio_parse_data(buffer, bytes_read, &viking_data)) {
                    uint32_t now = to_ms_since_boot(get_absolute_time());
                    // Turn on LED for 200ms to indicate serial message received
                    LED_SET(1);
                    led_tick_active = true;
                    led_tick_off_time = now + 200;
                    // Reset heartbeat timer to prevent immediate turn-on after tick
                    last_blink = now;
                    
                    // Check if data resumed after timeout
                    if (timeout_triggered) {
                        printf("Viking Bio: Data resumed after timeout\n");
                        timeout_triggered = false;
                    }
                    
                    // Update Matter attributes
                    matter_bridge_update_attributes(&viking_data);
                    
                    // Log data to USB serial
                    printf("Flame: %s, Fan Speed: %d%%, Temp: %d°C\n",
                           viking_data.flame_detected ? "ON" : "OFF",
                           viking_data.fan_speed,
                           viking_data.temperature);
                }
            }
        }
        
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
            matter_bridge_update_attributes(&cleared_data);
        }
        
        // Update Matter bridge
        matter_bridge_task();
        
        // Check for SoftAP timeout (auto-disable after 30 minutes)
        // Only check once to avoid repeated calls to stop function
        if (!softap_timeout_handled && network_adapter_softap_timeout_expired()) {
            softap_timeout_handled = true;  // Set flag first to prevent re-entry
            printf("\n===========================================\n");
            printf("SoftAP TIMEOUT: 30 minutes have elapsed\n");
            printf("Automatically disabling SoftAP for security\n");
            printf("===========================================\n\n");
            
            if (network_adapter_stop_softap() == 0) {
                printf("✓ SoftAP disabled successfully\n");
                printf("  Device will continue operating without SoftAP\n");
                printf("  To re-enable commissioning mode, restart device\n\n");
            } else {
                printf("ERROR: Failed to disable SoftAP\n\n");
            }
        }
        
        // Turn off LED tick after 200ms
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (led_tick_active && now >= led_tick_off_time) {
            LED_SET(0);
            led_tick_active = false;
        }
        
        // Blink LED every second to show activity (only if not in tick mode)
        if (!led_tick_active && now - last_blink >= 1000) {
            led_state = !led_state;
            LED_SET(led_state);
            last_blink = now;
        }
        
        // Small delay to prevent CPU spinning
        sleep_ms(10);
    }
    
    return 0;
}
