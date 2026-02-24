/*
 * matter_protocol.h
 * Matter Protocol Coordinator
 * Handles message routing between transport, security, and interaction layers
 */

#ifndef MATTER_PROTOCOL_H
#define MATTER_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize Matter protocol stack
 * Initializes all layers: transport, security, clusters, and read handler
 * 
 * @return 0 on success, -1 on failure
 */
int matter_protocol_init(void);

/**
 * Start commissioning mode
 * Enables device for commissioning with the given PIN
 * 
 * @param setup_pin 8-digit setup PIN (null-terminated)
 * @param discriminator Discriminator value (12 bits, e.g., 3840)
 * @return 0 on success, -1 on error
 */
int matter_protocol_start_commissioning(const char *setup_pin, uint16_t discriminator);

/**
 * Check if device is commissioned
 * 
 * @return true if commissioned, false otherwise
 */
bool matter_protocol_is_commissioned(void);

/**
 * Process incoming Matter messages
 * Main loop task that handles message routing
 * Call this periodically from the main loop
 * 
 * @return Number of messages processed, or -1 on error
 */
int matter_protocol_task(void);

/**
 * Process a raw Matter message received over BLE (COBLe channel).
 *
 * Decodes, decrypts (if secured) and routes the message through the same
 * handlers as UDP messages.  The encoded response (if any) is placed in
 * *output and should be sent back to the BLE controller via
 * ble_adapter_send_data().
 *
 * @param input       Raw bytes of the received Matter message
 * @param input_len   Length of input
 * @param output      Buffer to receive the encoded response
 * @param output_size Size of output buffer
 * @param output_len  Set to the actual response length (0 = no response)
 * @return 0 on success, -1 on failure
 */
int matter_protocol_process_ble_message(const uint8_t *input, size_t input_len,
                                         uint8_t *output, size_t output_size,
                                         size_t *output_len);

/**
 * Deinitialize Matter protocol stack
 * Cleans up all resources
 */
void matter_protocol_deinit(void);

/**
 * Send a Matter message
 * Wrapper for sending messages through the stack
 * 
 * @param dest_ip Destination IP address
 * @param dest_port Destination port
 * @param protocol_id Protocol ID
 * @param opcode Protocol opcode
 * @param payload Payload data
 * @param payload_len Length of payload
 * @return 0 on success, -1 on failure
 */
int matter_protocol_send(const char *dest_ip, uint16_t dest_port,
                        uint16_t protocol_id, uint8_t opcode,
                        const uint8_t *payload, size_t payload_len);

#ifdef __cplusplus
}
#endif

#endif // MATTER_PROTOCOL_H
