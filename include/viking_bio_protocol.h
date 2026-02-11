#ifndef VIKING_BIO_PROTOCOL_H
#define VIKING_BIO_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Viking Bio 20 serial protocol definitions
#define VIKING_BIO_BAUD_RATE 9600
#define VIKING_BIO_DATA_BITS 8
#define VIKING_BIO_STOP_BITS 1
#define VIKING_BIO_PARITY UART_PARITY_NONE

// Data packet structure
typedef struct {
    bool flame_detected;
    uint8_t fan_speed;      // 0-100 percentage
    uint16_t temperature;   // Temperature in Celsius
    uint8_t error_code;     // Error code if any
    bool valid;             // Data validity flag
} viking_bio_data_t;

// Function prototypes
void viking_bio_init(void);
bool viking_bio_parse_data(const uint8_t *buffer, size_t length, viking_bio_data_t *data);
void viking_bio_get_current_data(viking_bio_data_t *data);

#endif // VIKING_BIO_PROTOCOL_H
