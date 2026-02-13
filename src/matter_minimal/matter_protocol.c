/*
 * matter_protocol.c
 * Matter Protocol Coordinator implementation
 */

#include "matter_protocol.h"
#include "codec/message_codec.h"
#include "transport/udp_transport.h"
#include "security/session_mgr.h"
#include "security/pase.h"
#include "commissioning/network_commissioning.h"
#include "interaction/interaction_model.h"
#include "interaction/read_handler.h"
#include "interaction/subscribe_handler.h"
#include "clusters/descriptor.h"
#include "clusters/onoff.h"
#include "clusters/level_control.h"
#include "clusters/temperature.h"
#include <string.h>
#include <stdio.h>

// Internal state
static bool initialized = false;

/**
 * Initialize Matter protocol stack
 */
int matter_protocol_init(void) {
    if (initialized) {
        return 0;
    }
    
    // Initialize layers from bottom to top
    
    // 1. Transport layer (UDP)
    if (matter_transport_init() < 0) {
        return -1;
    }
    
    // 2. Message codec
    matter_message_codec_init();
    
    // 3. Security layer
    if (session_mgr_init() < 0) {
        return -1;
    }
    
    // 4. Commissioning layer
    if (commissioning_init() < 0) {
        return -1;
    }
    
    // 5. Interaction layer
    if (read_handler_init() < 0) {
        return -1;
    }
    
    if (subscribe_handler_init() < 0) {
        return -1;
    }
    
    // 5. Cluster implementations
    if (cluster_descriptor_init() < 0) {
        return -1;
    }
    
    if (cluster_onoff_init() < 0) {
        return -1;
    }
    
    if (cluster_level_control_init() < 0) {
        return -1;
    }
    
    if (cluster_temperature_init() < 0) {
        return -1;
    }
    
    initialized = true;
    return 0;
}

/**
 * Process PASE message (Secure Channel Protocol)
 */
static int process_pase_message(const matter_message_t *msg,
                               const char *source_ip, uint16_t source_port) {
    uint8_t response_payload[1024];
    size_t response_len;
    uint8_t session_id;
    
    // Handle PASE message through commissioning system
    int result = commissioning_handle_pase_message(msg->protocol_opcode,
                                                   msg->payload, msg->payload_length,
                                                   response_payload, sizeof(response_payload),
                                                   &response_len, &session_id);
    
    if (result < 0) {
        return -1; // Error
    }
    
    if (result == 1) {
        // PASE completed successfully, session established
        printf("Matter Protocol: PASE commissioning completed\n");
    }
    
    // Send response if we have one
    if (response_len > 0) {
        // Determine response opcode based on request
        uint8_t response_opcode = msg->protocol_opcode + 1; // Response is typically request + 1
        
        return matter_protocol_send(source_ip, source_port,
                                   PROTOCOL_SECURE_CHANNEL,
                                   response_opcode,
                                   response_payload, response_len);
    }
    
    return 0;
}

/**
 * Process ReadRequest (Interaction Model Protocol)
 */
static int process_read_request(const matter_message_t *msg,
                               const char *source_ip, uint16_t source_port) {
    uint8_t response_payload[1024];
    size_t response_len;
    
    // Process the ReadRequest and generate ReadResponse
    if (read_handler_process_request(msg->payload, msg->payload_length,
                                     response_payload, sizeof(response_payload),
                                     &response_len) < 0) {
        return -1;
    }
    
    // Send response back to controller
    return matter_protocol_send(source_ip, source_port,
                               PROTOCOL_INTERACTION_MODEL,
                               OP_REPORT_DATA,
                               response_payload, response_len);
}

/**
 * Process SubscribeRequest (Interaction Model Protocol)
 */
static int process_subscribe_request(const matter_message_t *msg,
                                     const char *source_ip, uint16_t source_port) {
    uint8_t response_payload[1024];
    size_t response_len;
    
    // Process the SubscribeRequest and generate SubscribeResponse
    if (subscribe_handler_process_request(msg->payload, msg->payload_length,
                                         response_payload, sizeof(response_payload),
                                         &response_len, msg->header.session_id) < 0) {
        return -1;
    }
    
    // Send response back to controller
    return matter_protocol_send(source_ip, source_port,
                               PROTOCOL_INTERACTION_MODEL,
                               OP_SUBSCRIBE_RESPONSE,
                               response_payload, response_len);
}

/**
 * Route incoming message to appropriate handler
 */
static int route_message(const matter_message_t *msg,
                        const char *source_ip, uint16_t source_port) {
    switch (msg->protocol_id) {
        case PROTOCOL_SECURE_CHANNEL:
            return process_pase_message(msg, source_ip, source_port);
        
        case PROTOCOL_INTERACTION_MODEL:
            switch (msg->protocol_opcode) {
                case OP_READ_REQUEST:
                    return process_read_request(msg, source_ip, source_port);
                
                case OP_SUBSCRIBE_REQUEST:
                    return process_subscribe_request(msg, source_ip, source_port);
                
                case OP_WRITE_REQUEST:
                case OP_INVOKE_REQUEST:
                    // Not implemented yet
                    return 0;
                
                default:
                    return -1;
            }
        
        default:
            // Unknown protocol
            return -1;
    }
}

/**
 * Process incoming Matter messages
 */
int matter_protocol_task(void) {
    if (!initialized) {
        return -1;
    }
    
    uint8_t buffer[MATTER_MAX_MESSAGE_SIZE];
    char source_ip[40];
    uint16_t source_port;
    size_t recv_len;
    int messages_processed = 0;
    
    // Check subscription intervals for periodic reporting
    // Note: In production, this would use actual time from pico SDK
    // For now, we pass 0 to indicate time checking is not active
    subscribe_handler_check_intervals(0);
    
    // Process all available messages
    while (udp_transport_recv(buffer, sizeof(buffer), &recv_len,
                             source_ip, sizeof(source_ip), &source_port) == 0) {
        matter_message_t msg;
        
        // Decode Matter message header
        if (matter_message_decode(buffer, recv_len, &msg) < 0) {
            continue;
        }
        
        // Check if message needs decryption
        if (msg.header.session_id != 0) {
            // Secured message - decrypt payload
            uint8_t plaintext[MATTER_MAX_PAYLOAD_SIZE];
            size_t plaintext_len;
            
            if (session_decrypt(msg.header.session_id,
                              msg.payload, msg.payload_length,
                              plaintext, sizeof(plaintext),
                              &plaintext_len) == 0) {
                // Update message with decrypted payload
                msg.payload = plaintext;
                msg.payload_length = plaintext_len;
            } else {
                // Decryption failed
                continue;
            }
        }
        
        // Route message to appropriate handler
        if (route_message(&msg, source_ip, source_port) == 0) {
            messages_processed++;
        }
    }
    
    return messages_processed;
}

/**
 * Send a Matter message
 */
int matter_protocol_send(const char *dest_ip, uint16_t dest_port,
                        uint16_t protocol_id, uint8_t opcode,
                        const uint8_t *payload, size_t payload_len) {
    if (!initialized || !dest_ip || !payload) {
        return -1;
    }
    
    uint8_t buffer[MATTER_MAX_MESSAGE_SIZE];
    size_t encoded_len;
    
    // Build Matter message
    matter_message_t msg;
    memset(&msg, 0, sizeof(msg));
    
    msg.header.flags = MATTER_MSG_VERSION;
    msg.header.session_id = 0;  // Unsecured for now
    msg.header.security_flags = 0;
    msg.header.message_counter = matter_message_get_next_counter();
    msg.protocol_id = protocol_id;
    msg.protocol_opcode = opcode;
    msg.exchange_id = matter_message_get_next_exchange_id();
    msg.payload = payload;
    msg.payload_length = payload_len;
    
    // Encode message
    if (matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_len) < 0) {
        return -1;
    }
    
    // Send via transport
    return udp_transport_send(dest_ip, dest_port, buffer, encoded_len);
}

/**
 * Start commissioning mode
 * Enables device for commissioning with the given PIN
 */
int matter_protocol_start_commissioning(const char *setup_pin, uint16_t discriminator) {
    if (!initialized) {
        return -1;
    }
    
    return commissioning_start(setup_pin, discriminator);
}

/**
 * Check if device is commissioned
 */
bool matter_protocol_is_commissioned(void) {
    return commissioning_is_commissioned();
}

/**
 * Deinitialize Matter protocol stack
 */
void matter_protocol_deinit(void) {
    if (!initialized) {
        return;
    }
    
    // Clean up in reverse order
    commissioning_deinit();
    udp_transport_deinit();
    
    initialized = false;
}
