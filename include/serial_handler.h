#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// UART configuration for TTL serial input
#define UART_ID uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define SERIAL_BUFFER_SIZE 256

// Function prototypes
void serial_handler_init(void);
void serial_handler_task(void);
bool serial_handler_data_available(void);
size_t serial_handler_read(uint8_t *buffer, size_t max_length);

#endif // SERIAL_HANDLER_H
