/*
 * matter_protocol.h
 * Matter Protocol Coordinator
 * Handles message routing between transport, security, and interaction layers
 */

#ifndef MATTER_PROTOCOL_H
#define MATTER_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

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
 * Process incoming Matter messages
 * Main loop task that handles message routing
 * Call this periodically from the main loop
 * 
 * @return Number of messages processed, or -1 on error
 */
int matter_protocol_task(void);

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
