#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "serial_handler.h"
#include "viking_bio_protocol.h"
#include "matter_bridge.h"

// LED pin for status indication
#define LED_PIN PICO_DEFAULT_LED_PIN

int main() {
    // Initialize standard I/O
    stdio_init_all();
    
    // Initialize LED for status indication
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // Blink LED to indicate startup
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(200);
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
    }
    
    printf("Viking Bio Matter Bridge starting...\n");
    
    // Initialize components
    viking_bio_init();
    serial_handler_init();
    matter_bridge_init();
    
    printf("Initialization complete. Reading serial data...\n");
    
    // Main loop
    uint8_t buffer[SERIAL_BUFFER_SIZE];
    viking_bio_data_t viking_data;
    uint32_t last_blink = 0;
    bool led_state = false;
    
    while (true) {
        // Handle serial data
        serial_handler_task();
        
        // Check for available data
        if (serial_handler_data_available()) {
            size_t bytes_read = serial_handler_read(buffer, sizeof(buffer));
            
            if (bytes_read > 0) {
                // Parse Viking Bio data
                if (viking_bio_parse_data(buffer, bytes_read, &viking_data)) {
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
        
        // Update Matter bridge
        matter_bridge_task();
        
        // Blink LED every second to show activity
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_blink >= 1000) {
            led_state = !led_state;
            gpio_put(LED_PIN, led_state);
            last_blink = now;
        }
        
        // Small delay to prevent CPU spinning
        sleep_ms(10);
    }
    
    return 0;
}
