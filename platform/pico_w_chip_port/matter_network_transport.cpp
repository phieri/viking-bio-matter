/*
 * matter_network_transport.cpp
 * Network transport implementation for sending Matter attribute reports over WiFi
 */

#include "matter_network_transport.h"
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/ip_addr.h"

// Matter controllers storage
static matter_controller_t controllers[MAX_MATTER_CONTROLLERS];
static bool transport_initialized = false;
static uint32_t report_interval_ms = 0;  // No throttling by default

// UDP control block for sending reports
static struct udp_pcb *udp_pcb = NULL;

int matter_network_transport_init(void) {
    if (transport_initialized) {
        return 0;
    }
    
    // Clear controllers
    memset(controllers, 0, sizeof(controllers));
    
    // Create UDP socket for sending reports (must be inside lwIP lock)
    cyw43_arch_lwip_begin();
    udp_pcb = udp_new();
    if (!udp_pcb) {
        cyw43_arch_lwip_end();
        printf("[Matter Transport] ERROR: Failed to create UDP socket\n");
        return -1;
    }
    cyw43_arch_lwip_end();
    
    transport_initialized = true;
    printf("Matter Transport: Network transport initialized\n");
    
    return 0;
}

int matter_network_transport_add_controller(const char *ip_address, uint16_t port) {
    if (!transport_initialized || !ip_address) {
        return -1;
    }
    
    // Find free controller slot
    for (int i = 0; i < MAX_MATTER_CONTROLLERS; i++) {
        if (!controllers[i].active) {
            // Parse IP address
            ip_addr_t ipaddr;
            cyw43_arch_lwip_begin();
            bool ip_ok = ipaddr_aton(ip_address, &ipaddr);
            cyw43_arch_lwip_end();
            if (!ip_ok) {
                printf("[Matter Transport] ERROR: Invalid IP address: %s\n", ip_address);
                return -1;
            }
            
            controllers[i].ip_address = ip_addr_get_ip4_u32(&ipaddr);
            controllers[i].port = port;
            controllers[i].active = true;
            controllers[i].last_report_time = 0;
            
            printf("Matter Transport: Controller added [%d]: %s:%u\n", i, ip_address, port);
            return i;
        }
    }
    
    printf("[Matter Transport] ERROR: Maximum controllers reached\n");
    return -1;
}

void matter_network_transport_remove_controller(int controller_id) {
    if (controller_id < 0 || controller_id >= MAX_MATTER_CONTROLLERS) {
        return;
    }
    
    if (controllers[controller_id].active) {
        controllers[controller_id].active = false;
        printf("Matter Transport: Controller removed [%d]\n", controller_id);
    }
}

// Helper to format attribute value as string (JSON-safe)
static void format_attribute_value(const matter_attr_value_t *value, matter_attr_type_t type,
                                   char *buffer, size_t buffer_len) {
    switch (type) {
        case MATTER_TYPE_BOOL:
            // Boolean values are JSON keywords (no quotes needed)
            snprintf(buffer, buffer_len, "%s", value->bool_val ? "true" : "false");
            break;
        case MATTER_TYPE_UINT8:
            // Numeric value (no quotes needed)
            snprintf(buffer, buffer_len, "%u", value->uint8_val);
            break;
        case MATTER_TYPE_INT16:
            // Numeric value (no quotes needed)
            snprintf(buffer, buffer_len, "%d", value->int16_val);
            break;
        case MATTER_TYPE_UINT32:
            // Numeric value (no quotes needed)
            snprintf(buffer, buffer_len, "%" PRIu32, value->uint32_val);
            break;
        default:
            // Unknown type - use null
            snprintf(buffer, buffer_len, "null");
            break;
    }
}

// Determine attribute type from cluster/attribute IDs
static matter_attr_type_t get_attribute_type(uint32_t cluster_id, uint32_t attribute_id) {
    // OnOff cluster
    if (cluster_id == MATTER_CLUSTER_ON_OFF && attribute_id == MATTER_ATTR_ON_OFF) {
        return MATTER_TYPE_BOOL;
    }
    // LevelControl cluster
    if (cluster_id == MATTER_CLUSTER_LEVEL_CONTROL && attribute_id == MATTER_ATTR_CURRENT_LEVEL) {
        return MATTER_TYPE_UINT8;
    }
    // TemperatureMeasurement cluster
    if (cluster_id == MATTER_CLUSTER_TEMPERATURE_MEASUREMENT && attribute_id == MATTER_ATTR_MEASURED_VALUE) {
        return MATTER_TYPE_INT16;
    }
    return MATTER_TYPE_UINT32;
}

int matter_network_transport_send_report(uint8_t endpoint, uint32_t cluster_id,
                                         uint32_t attribute_id, const matter_attr_value_t *value) {
    if (!transport_initialized || !udp_pcb || !value) {
        return -1;
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    int sent_count = 0;
    
    // Build Matter attribute report message (simplified JSON format)
    // Buffer size calculated to handle maximum possible message:
    // - Fixed text: ~100 bytes
    // - Cluster ID (4 hex digits with 0x prefix): 6 bytes
    // - Attribute ID (4 hex digits with 0x prefix): 6 bytes
    // - Value string (max): 64 bytes (generous for numeric values)
    // - Timestamp (10 digits): 10 bytes
    // - Overhead/formatting: 20 bytes
    // Total: ~206 bytes, use 512 for safety margin
    char message[512];
    char value_str[64];  // Generous size for any numeric value
    matter_attr_type_t type = get_attribute_type(cluster_id, attribute_id);
    format_attribute_value(value, type, value_str, sizeof(value_str));
    
    // Construct JSON message with proper formatting
    // Note: All values are either JSON keywords (true/false/null) or numeric,
    // so no string escaping is needed. Using %04 for 4-digit hex formatting
    // (sufficient for current cluster/attribute IDs, which are 16-bit values)
    int msg_len = snprintf(message, sizeof(message),
                          "{\"type\":\"attribute-report\",\"endpoint\":%u,\"cluster\":\"0x%04" PRIx32 "\","
                          "\"attribute\":\"0x%04" PRIx32 "\",\"value\":%s,\"timestamp\":%" PRIu32 "}\n",
                          endpoint, cluster_id, attribute_id, value_str, now);
    
    // Check for truncation
    if (msg_len >= (int)sizeof(message)) {
        printf("[Matter Transport] ERROR: Message truncated\n");
        return -1;
    }
    
    // Send to all active controllers
    for (int i = 0; i < MAX_MATTER_CONTROLLERS; i++) {
        if (!controllers[i].active) {
            continue;
        }
        
        // Check throttling
        if (report_interval_ms > 0) {
            uint32_t elapsed = now - controllers[i].last_report_time;
            if (elapsed < report_interval_ms) {
                continue;  // Skip this controller, too soon
            }
        }
        
        // Prepare destination address and send inside lwIP lock
        cyw43_arch_lwip_begin();
        ip_addr_t dest_addr;
        ip_addr_set_ip4_u32(&dest_addr, controllers[i].ip_address);

        // Allocate buffer for UDP packet (use msg_len for efficiency)
        struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, msg_len, PBUF_RAM);
        if (!p) {
            cyw43_arch_lwip_end();
            printf("[Matter Transport] ERROR: Failed to allocate pbuf\n");
            continue;
        }

        // Copy message to buffer (use msg_len to avoid redundant strlen)
        memcpy(p->payload, message, msg_len);
        
        // Send UDP packet
        err_t err = udp_sendto(udp_pcb, p, &dest_addr, controllers[i].port);
        
        // Free buffer
        pbuf_free(p);
        
        if (err == ERR_OK) {
            controllers[i].last_report_time = now;
            sent_count++;
        } else {
            printf("[Matter Transport] ERROR: Failed to send to controller [%d], error %d\n", i, err);
        }
        cyw43_arch_lwip_end();
    }
    
    if (sent_count > 0) {
        printf("Matter Transport: Sent report to %d controller(s)\n", sent_count);
    }
    
    return sent_count;
}

int matter_network_transport_get_controller_count(void) {
    if (!transport_initialized) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < MAX_MATTER_CONTROLLERS; i++) {
        if (controllers[i].active) {
            count++;
        }
    }
    return count;
}

void matter_network_transport_set_report_interval(uint32_t interval_ms) {
    report_interval_ms = interval_ms;
    printf("Matter Transport: Report interval set to %" PRIu32 " ms\n", interval_ms);
}

void matter_network_transport_task(void) {
    // Network transport tasks (if any polling is needed in the future)
    // Currently, UDP is handled by lwIP in background
}
