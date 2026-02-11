#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "viking_bio_protocol.h"

// Current Viking Bio data state
static viking_bio_data_t current_data = {
    .flame_detected = false,
    .fan_speed = 0,
    .temperature = 0,
    .error_code = 0,
    .valid = false
};

// Protocol constants
#define VIKING_BIO_START_BYTE 0xAA
#define VIKING_BIO_END_BYTE 0x55
#define VIKING_BIO_MIN_PACKET_SIZE 6

void viking_bio_init(void) {
    // Initialize data structure
    memset(&current_data, 0, sizeof(current_data));
    current_data.valid = false;
}

bool viking_bio_parse_data(const uint8_t *buffer, size_t length, viking_bio_data_t *data) {
    if (buffer == NULL || data == NULL || length < VIKING_BIO_MIN_PACKET_SIZE) {
        return false;
    }
    
    // Simple protocol parser
    // Format: [START_BYTE] [FLAGS] [FAN_SPEED] [TEMP_HIGH] [TEMP_LOW] [END_BYTE]
    // FLAGS bit 0: flame detected
    // FLAGS bit 1-7: error codes
    
    for (size_t i = 0; i < length - VIKING_BIO_MIN_PACKET_SIZE + 1; i++) {
        if (buffer[i] == VIKING_BIO_START_BYTE) {
            // Check for valid end byte
            if (buffer[i + 5] == VIKING_BIO_END_BYTE) {
                uint8_t flags = buffer[i + 1];
                uint8_t fan_speed = buffer[i + 2];
                uint8_t temp_high = buffer[i + 3];
                uint8_t temp_low = buffer[i + 4];
                
                // Parse data
                data->flame_detected = (flags & 0x01) != 0;
                // Clamp fan speed to valid range 0-100
                if (fan_speed > 100) {
                    data->fan_speed = 100;
                } else {
                    data->fan_speed = fan_speed;
                }
                data->temperature = ((uint16_t)temp_high << 8) | temp_low;
                data->error_code = (flags >> 1) & 0x7F;
                data->valid = true;
                
                // Update current state
                memcpy(&current_data, data, sizeof(viking_bio_data_t));
                
                return true;
            }
        }
    }
    
    // If no valid packet found, try simple text protocol fallback
    // Format: "F:1,S:50,T:75\n" (Flame:bool, Speed:%, Temp:°C)
    if (length > 10) {
        char str_buffer[256];
        size_t copy_len = length < sizeof(str_buffer) - 1 ? length : sizeof(str_buffer) - 1;
        memcpy(str_buffer, buffer, copy_len);
        str_buffer[copy_len] = '\0';
        
        int flame = 0, speed = 0, temp = 0;
        if (sscanf(str_buffer, "F:%d,S:%d,T:%d", &flame, &speed, &temp) == 3) {
            data->flame_detected = flame != 0;
            // Clamp fan speed to valid range 0-100
            if (speed < 0) {
                data->fan_speed = 0;
            } else if (speed > 100) {
                data->fan_speed = 100;
            } else {
                data->fan_speed = speed;
            }
            data->temperature = temp;
            data->error_code = 0;
            data->valid = true;
            
            // Update current state
            memcpy(&current_data, data, sizeof(viking_bio_data_t));
            
            return true;
        }
    }
    
    return false;
}

void viking_bio_get_current_data(viking_bio_data_t *data) {
    if (data != NULL) {
        memcpy(data, &current_data, sizeof(viking_bio_data_t));
    }
}
