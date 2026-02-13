/*
 * udp_transport.c
 * Matter UDP transport implementation using lwIP
 * Integrates with existing network_adapter.cpp (WiFi/lwIP already configured)
 */

#include "udp_transport.h"
#include <string.h>
#include <stdio.h>

// lwIP headers
#include "lwip/udp.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"

/**
 * Receive Queue Entry
 */
typedef struct {
    uint8_t data[MATTER_TRANSPORT_MAX_PACKET];
    size_t length;
    matter_transport_addr_t source_addr;
    bool in_use;
} rx_queue_entry_t;

/**
 * Transport State
 */
static struct {
    struct udp_pcb *operational_pcb;    // UDP PCB for operational messages
    struct udp_pcb *commissioning_pcb;  // UDP PCB for commissioning messages
    rx_queue_entry_t rx_queue[MATTER_TRANSPORT_RX_QUEUE_SIZE];
    size_t rx_queue_head;               // Next entry to write
    size_t rx_queue_tail;               // Next entry to read
    size_t rx_queue_count;              // Number of entries in queue
    bool initialized;
} transport_state = {
    .operational_pcb = NULL,
    .commissioning_pcb = NULL,
    .rx_queue_head = 0,
    .rx_queue_tail = 0,
    .rx_queue_count = 0,
    .initialized = false
};

/**
 * Convert lwIP ip_addr_t to matter_transport_addr_t
 */
static void lwip_addr_to_transport_addr(const ip_addr_t *lwip_addr, uint16_t port,
                                        matter_transport_addr_t *transport_addr) {
    transport_addr->port = port;
    
    if (IP_IS_V6(lwip_addr)) {
        transport_addr->is_ipv6 = true;
        memcpy(transport_addr->addr, ip_2_ip6(lwip_addr)->addr, 16);
    } else {
        transport_addr->is_ipv6 = false;
        // Store IPv4 as IPv4-mapped IPv6 address
        memset(transport_addr->addr, 0, 10);
        transport_addr->addr[10] = 0xFF;
        transport_addr->addr[11] = 0xFF;
        uint32_t ipv4 = ip_2_ip4(lwip_addr)->addr;
        memcpy(&transport_addr->addr[12], &ipv4, 4);
    }
}

/**
 * Convert matter_transport_addr_t to lwIP ip_addr_t
 */
static void transport_addr_to_lwip_addr(const matter_transport_addr_t *transport_addr,
                                        ip_addr_t *lwip_addr) {
    if (transport_addr->is_ipv6) {
        IP_SET_TYPE(lwip_addr, IPADDR_TYPE_V6);
        memcpy(ip_2_ip6(lwip_addr)->addr, transport_addr->addr, 16);
    } else {
        IP_SET_TYPE(lwip_addr, IPADDR_TYPE_V4);
        // Extract IPv4 from IPv4-mapped IPv6 address
        uint32_t ipv4;
        memcpy(&ipv4, &transport_addr->addr[12], 4);
        ip_2_ip4(lwip_addr)->addr = ipv4;
    }
}

/**
 * UDP receive callback
 * Called by lwIP when UDP packet is received
 */
static void udp_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port) {
    (void)arg; // Unused
    (void)pcb; // Unused
    
    if (p == NULL) {
        return;
    }
    
    // Check if we have space in the queue
    if (transport_state.rx_queue_count >= MATTER_TRANSPORT_RX_QUEUE_SIZE) {
        printf("UDP transport: RX queue full, dropping packet\n");
        pbuf_free(p);
        return;
    }
    
    // Get next queue entry
    rx_queue_entry_t *entry = &transport_state.rx_queue[transport_state.rx_queue_head];
    
    // Check packet size
    if (p->tot_len > MATTER_TRANSPORT_MAX_PACKET) {
        printf("UDP transport: Packet too large (%u bytes), dropping\n", p->tot_len);
        pbuf_free(p);
        return;
    }
    
    // Copy packet data
    entry->length = p->tot_len;
    pbuf_copy_partial(p, entry->data, p->tot_len, 0);
    
    // Store source address
    lwip_addr_to_transport_addr(addr, port, &entry->source_addr);
    
    entry->in_use = true;
    
    // Update queue
    transport_state.rx_queue_head = (transport_state.rx_queue_head + 1) % MATTER_TRANSPORT_RX_QUEUE_SIZE;
    transport_state.rx_queue_count++;
    
    // Free pbuf
    pbuf_free(p);
}

int matter_transport_init(void) {
    if (transport_state.initialized) {
        return MATTER_TRANSPORT_SUCCESS;
    }
    
    printf("Initializing Matter UDP transport...\n");
    
    // Initialize receive queue
    for (size_t i = 0; i < MATTER_TRANSPORT_RX_QUEUE_SIZE; i++) {
        transport_state.rx_queue[i].in_use = false;
        transport_state.rx_queue[i].length = 0;
    }
    transport_state.rx_queue_head = 0;
    transport_state.rx_queue_tail = 0;
    transport_state.rx_queue_count = 0;
    
    // Create UDP PCB for operational messages (port 5540)
    transport_state.operational_pcb = udp_new();
    if (transport_state.operational_pcb == NULL) {
        printf("ERROR: Failed to create operational UDP PCB\n");
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    // Bind operational PCB to port 5540
    err_t err = udp_bind(transport_state.operational_pcb, IP_ANY_TYPE, MATTER_PORT_OPERATIONAL);
    if (err != ERR_OK) {
        printf("ERROR: Failed to bind operational UDP PCB to port %d (err=%d)\n", 
               MATTER_PORT_OPERATIONAL, err);
        udp_remove(transport_state.operational_pcb);
        transport_state.operational_pcb = NULL;
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    // Set receive callback for operational PCB
    udp_recv(transport_state.operational_pcb, udp_recv_callback, NULL);
    printf("Operational UDP socket bound to port %d\n", MATTER_PORT_OPERATIONAL);
    
    // Create UDP PCB for commissioning messages (port 5550)
    transport_state.commissioning_pcb = udp_new();
    if (transport_state.commissioning_pcb == NULL) {
        printf("ERROR: Failed to create commissioning UDP PCB\n");
        udp_remove(transport_state.operational_pcb);
        transport_state.operational_pcb = NULL;
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    // Bind commissioning PCB to port 5550
    err = udp_bind(transport_state.commissioning_pcb, IP_ANY_TYPE, MATTER_PORT_COMMISSIONING);
    if (err != ERR_OK) {
        printf("ERROR: Failed to bind commissioning UDP PCB to port %d (err=%d)\n", 
               MATTER_PORT_COMMISSIONING, err);
        udp_remove(transport_state.operational_pcb);
        udp_remove(transport_state.commissioning_pcb);
        transport_state.operational_pcb = NULL;
        transport_state.commissioning_pcb = NULL;
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    // Set receive callback for commissioning PCB
    udp_recv(transport_state.commissioning_pcb, udp_recv_callback, NULL);
    printf("Commissioning UDP socket bound to port %d\n", MATTER_PORT_COMMISSIONING);
    
    transport_state.initialized = true;
    printf("Matter UDP transport initialized\n");
    
    return MATTER_TRANSPORT_SUCCESS;
}

void matter_transport_deinit(void) {
    if (!transport_state.initialized) {
        return;
    }
    
    printf("Deinitializing Matter UDP transport...\n");
    
    // Remove UDP PCBs
    if (transport_state.operational_pcb != NULL) {
        udp_remove(transport_state.operational_pcb);
        transport_state.operational_pcb = NULL;
    }
    
    if (transport_state.commissioning_pcb != NULL) {
        udp_remove(transport_state.commissioning_pcb);
        transport_state.commissioning_pcb = NULL;
    }
    
    // Clear receive queue
    for (size_t i = 0; i < MATTER_TRANSPORT_RX_QUEUE_SIZE; i++) {
        transport_state.rx_queue[i].in_use = false;
        transport_state.rx_queue[i].length = 0;
    }
    transport_state.rx_queue_head = 0;
    transport_state.rx_queue_tail = 0;
    transport_state.rx_queue_count = 0;
    
    transport_state.initialized = false;
    printf("Matter UDP transport deinitialized\n");
}

int matter_transport_send(const uint8_t *data, size_t length, 
                          const matter_transport_addr_t *dest_addr) {
    if (!transport_state.initialized) {
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    if (data == NULL || dest_addr == NULL || length == 0) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    if (length > MATTER_TRANSPORT_MAX_PACKET) {
        printf("ERROR: Packet too large (%zu bytes, max %d)\n", 
               length, MATTER_TRANSPORT_MAX_PACKET);
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    // Convert destination address to lwIP format
    ip_addr_t dest_ip;
    transport_addr_to_lwip_addr(dest_addr, &dest_ip);
    
    // Allocate pbuf
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, length, PBUF_RAM);
    if (p == NULL) {
        printf("ERROR: Failed to allocate pbuf for send\n");
        return MATTER_TRANSPORT_ERROR_NO_MEMORY;
    }
    
    // Copy data into pbuf
    memcpy(p->payload, data, length);
    
    // Send via operational PCB (use operational for all sends in this phase)
    err_t err = udp_sendto(transport_state.operational_pcb, p, &dest_ip, dest_addr->port);
    
    // Free pbuf
    pbuf_free(p);
    
    if (err != ERR_OK) {
        printf("ERROR: UDP send failed (err=%d)\n", err);
        return MATTER_TRANSPORT_ERROR_SEND_FAILED;
    }
    
    return MATTER_TRANSPORT_SUCCESS;
}

int matter_transport_receive(uint8_t *buffer, size_t buffer_size, 
                             size_t *actual_length, 
                             matter_transport_addr_t *source_addr,
                             int timeout_ms) {
    if (!transport_state.initialized) {
        return MATTER_TRANSPORT_ERROR_INIT;
    }
    
    if (buffer == NULL || actual_length == NULL || buffer_size == 0) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    // For now, ignore timeout and implement non-blocking receive
    // Full timeout support would require integration with FreeRTOS or polling loop
    (void)timeout_ms;
    
    // Check if we have data in queue
    if (transport_state.rx_queue_count == 0) {
        // Process lwIP (allow callbacks to run)
        cyw43_arch_poll();
        
        // Check again
        if (transport_state.rx_queue_count == 0) {
            return MATTER_TRANSPORT_ERROR_WOULD_BLOCK;
        }
    }
    
    // Get next entry from queue
    rx_queue_entry_t *entry = &transport_state.rx_queue[transport_state.rx_queue_tail];
    
    if (!entry->in_use) {
        return MATTER_TRANSPORT_ERROR_WOULD_BLOCK;
    }
    
    // Check buffer size
    if (entry->length > buffer_size) {
        printf("ERROR: Buffer too small for received packet (%zu bytes needed, %zu available)\n",
               entry->length, buffer_size);
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    // Copy data to output buffer
    memcpy(buffer, entry->data, entry->length);
    *actual_length = entry->length;
    
    // Copy source address if requested
    if (source_addr != NULL) {
        memcpy(source_addr, &entry->source_addr, sizeof(matter_transport_addr_t));
    }
    
    // Mark entry as free
    entry->in_use = false;
    
    // Update queue
    transport_state.rx_queue_tail = (transport_state.rx_queue_tail + 1) % MATTER_TRANSPORT_RX_QUEUE_SIZE;
    transport_state.rx_queue_count--;
    
    return MATTER_TRANSPORT_SUCCESS;
}

bool matter_transport_has_data(void) {
    if (!transport_state.initialized) {
        return false;
    }
    
    // Process lwIP to allow callbacks
    cyw43_arch_poll();
    
    return transport_state.rx_queue_count > 0;
}

size_t matter_transport_get_pending_count(void) {
    if (!transport_state.initialized) {
        return 0;
    }
    
    return transport_state.rx_queue_count;
}

int matter_transport_addr_from_ipv4(const char *addr_str, uint16_t port, 
                                    matter_transport_addr_t *addr) {
    if (addr_str == NULL || addr == NULL) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    ip4_addr_t ipv4_addr;
    if (!ip4addr_aton(addr_str, &ipv4_addr)) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    addr->is_ipv6 = false;
    addr->port = port;
    
    // Store as IPv4-mapped IPv6 address
    memset(addr->addr, 0, 10);
    addr->addr[10] = 0xFF;
    addr->addr[11] = 0xFF;
    memcpy(&addr->addr[12], &ipv4_addr.addr, 4);
    
    return MATTER_TRANSPORT_SUCCESS;
}

int matter_transport_addr_from_ipv6(const char *addr_str, uint16_t port, 
                                    matter_transport_addr_t *addr) {
    if (addr_str == NULL || addr == NULL) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    ip6_addr_t ipv6_addr;
    if (!ip6addr_aton(addr_str, &ipv6_addr)) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    addr->is_ipv6 = true;
    addr->port = port;
    memcpy(addr->addr, ipv6_addr.addr, 16);
    
    return MATTER_TRANSPORT_SUCCESS;
}

int matter_transport_addr_to_string(const matter_transport_addr_t *addr, 
                                    char *buffer, size_t buffer_size) {
    if (addr == NULL || buffer == NULL || buffer_size == 0) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    if (addr->is_ipv6) {
        ip6_addr_t ipv6_addr;
        memcpy(ipv6_addr.addr, addr->addr, 16);
        return snprintf(buffer, buffer_size, "[%s]:%u", 
                       ip6addr_ntoa(&ipv6_addr), addr->port);
    } else {
        // Extract IPv4 from IPv4-mapped IPv6
        ip4_addr_t ipv4_addr;
        memcpy(&ipv4_addr.addr, &addr->addr[12], 4);
        return snprintf(buffer, buffer_size, "%s:%u", 
                       ip4addr_ntoa(&ipv4_addr), addr->port);
    }
}

// Legacy wrapper functions for backward compatibility
int udp_transport_init(void) {
    return matter_transport_init();
}

void udp_transport_deinit(void) {
    matter_transport_deinit();
}

int udp_transport_send(const char *dest_ip, uint16_t dest_port, 
                      const uint8_t *data, size_t length) {
    if (!dest_ip || !data) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    matter_transport_addr_t addr;
    int result;
    
    // Try IPv4 first
    result = matter_transport_addr_from_ipv4(dest_ip, dest_port, &addr);
    if (result < 0) {
        // Try IPv6
        result = matter_transport_addr_from_ipv6(dest_ip, dest_port, &addr);
        if (result < 0) {
            return result;
        }
    }
    
    return matter_transport_send(data, length, &addr);
}

int udp_transport_recv(uint8_t *buffer, size_t buffer_size, size_t *actual_length,
                      char *source_ip, size_t source_ip_size, uint16_t *source_port) {
    if (!buffer || !actual_length) {
        return MATTER_TRANSPORT_ERROR_INVALID_PARAM;
    }
    
    matter_transport_addr_t source_addr;
    int result = matter_transport_receive(buffer, buffer_size, actual_length, 
                                         &source_addr, 0);  // 0 ms timeout = non-blocking
    
    if (result == MATTER_TRANSPORT_SUCCESS && source_ip && source_port) {
        // Convert address to string
        if (source_addr.is_ipv6) {
            ip6_addr_t ipv6_addr;
            memcpy(ipv6_addr.addr, source_addr.addr, 16);
            snprintf(source_ip, source_ip_size, "%s", ip6addr_ntoa(&ipv6_addr));
        } else {
            // Extract IPv4 from IPv4-mapped IPv6
            ip4_addr_t ipv4_addr;
            memcpy(&ipv4_addr.addr, &source_addr.addr[12], 4);
            snprintf(source_ip, source_ip_size, "%s", ip4addr_ntoa(&ipv4_addr));
        }
        *source_port = source_addr.port;
    }
    
    return result;
}
