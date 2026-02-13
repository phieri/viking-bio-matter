#ifndef UDP_TRANSPORT_H
#define UDP_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Matter UDP Port Numbers
 */
#define MATTER_PORT_OPERATIONAL     5540    // Operational messages (post-commissioning)
#define MATTER_PORT_COMMISSIONING   5550    // Commissioning messages (PASE)

/**
 * Transport Configuration
 */
#define MATTER_TRANSPORT_RX_QUEUE_SIZE  4   // Number of receive buffers
#define MATTER_TRANSPORT_MAX_PACKET     1280 // Maximum UDP packet size (IPv6 MTU)

/**
 * Transport Error Codes
 */
#define MATTER_TRANSPORT_SUCCESS            0
#define MATTER_TRANSPORT_ERROR_INIT         -1
#define MATTER_TRANSPORT_ERROR_NO_MEMORY    -2
#define MATTER_TRANSPORT_ERROR_INVALID_PARAM -3
#define MATTER_TRANSPORT_ERROR_NOT_CONNECTED -4
#define MATTER_TRANSPORT_ERROR_TIMEOUT      -5
#define MATTER_TRANSPORT_ERROR_WOULD_BLOCK  -6
#define MATTER_TRANSPORT_ERROR_SEND_FAILED  -7

/**
 * Transport Address Structure
 * Supports both IPv4 and IPv6 addresses
 */
typedef struct {
    uint8_t addr[16];   // IPv6 address (or IPv4-mapped IPv6)
    uint16_t port;      // Port number
    bool is_ipv6;       // true for IPv6, false for IPv4
} matter_transport_addr_t;

/**
 * Initialize Matter UDP transport
 * Sets up UDP sockets for operational and commissioning ports
 * 
 * @return MATTER_TRANSPORT_SUCCESS on success, error code on failure
 */
int matter_transport_init(void);

/**
 * Deinitialize Matter UDP transport
 * Closes all UDP sockets and frees resources
 */
void matter_transport_deinit(void);

/**
 * Send a Matter message via UDP
 * 
 * @param data Buffer containing message data
 * @param length Length of message in bytes
 * @param dest_addr Destination address
 * @return MATTER_TRANSPORT_SUCCESS on success, error code on failure
 */
int matter_transport_send(const uint8_t *data, size_t length, 
                          const matter_transport_addr_t *dest_addr);

/**
 * Receive a Matter message via UDP (non-blocking)
 * 
 * @param buffer Buffer to store received message
 * @param buffer_size Size of buffer
 * @param actual_length Pointer to store actual received length
 * @param source_addr Pointer to store source address (can be NULL)
 * @param timeout_ms Timeout in milliseconds (0 = immediate return, -1 = blocking)
 * @return MATTER_TRANSPORT_SUCCESS on success, error code on failure
 */
int matter_transport_receive(uint8_t *buffer, size_t buffer_size, 
                             size_t *actual_length, 
                             matter_transport_addr_t *source_addr,
                             int timeout_ms);

/**
 * Check if transport has pending received data
 * 
 * @return true if data is available, false otherwise
 */
bool matter_transport_has_data(void);

/**
 * Get number of pending received messages
 * 
 * @return Number of messages in receive queue
 */
size_t matter_transport_get_pending_count(void);

/**
 * Helper: Create transport address from IPv4 address string
 * 
 * @param addr_str IPv4 address string (e.g., "192.168.1.100")
 * @param port Port number
 * @param addr Output address structure
 * @return MATTER_TRANSPORT_SUCCESS on success, error code on failure
 */
int matter_transport_addr_from_ipv4(const char *addr_str, uint16_t port, 
                                    matter_transport_addr_t *addr);

/**
 * Helper: Create transport address from IPv6 address string
 * 
 * @param addr_str IPv6 address string (e.g., "fe80::1")
 * @param port Port number
 * @param addr Output address structure
 * @return MATTER_TRANSPORT_SUCCESS on success, error code on failure
 */
int matter_transport_addr_from_ipv6(const char *addr_str, uint16_t port, 
                                    matter_transport_addr_t *addr);

/**
 * Helper: Convert transport address to string
 * 
 * @param addr Transport address
 * @param buffer Output buffer for string
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int matter_transport_addr_to_string(const matter_transport_addr_t *addr, 
                                    char *buffer, size_t buffer_size);

// Legacy wrapper functions for backward compatibility with simplified API
int udp_transport_init(void);
void udp_transport_deinit(void);
int udp_transport_send(const char *dest_ip, uint16_t dest_port, 
                      const uint8_t *data, size_t length);
int udp_transport_recv(uint8_t *buffer, size_t buffer_size, size_t *actual_length,
                      char *source_ip, size_t source_ip_size, uint16_t *source_port);

#ifdef __cplusplus
}
#endif

#endif // UDP_TRANSPORT_H
