/*
 * dns_sd.c
 * DNS-SD (mDNS) service discovery for Matter commissioning
 * Implements _matterc._udp service advertisement as per Matter specification
 */

#include "dns_sd.h"
#include <stdio.h>
#include <string.h>
#include "lwip/apps/mdns.h"
#include "lwip/netif.h"
#include "pico/cyw43_arch.h"
#include "pico/rand.h"

// Matter service constants
#define MATTER_SERVICE_NAME "_matterc"
#define MATTER_PROTOCOL "_udp"
#define MATTER_PORT 5540

// TXT record buffer
#define TXT_BUFFER_SIZE 128

// Static state
static bool is_advertising = false;
static uint16_t current_discriminator = 0;
static uint16_t current_vendor_id = 0;
static uint16_t current_product_id = 0;
static uint16_t current_device_type = 0;
static uint8_t current_commissioning_mode = 0;
static char current_instance_name[17] = {0}; // 16 hex chars + null

// Generate a fresh 64-bit random instance name per Matter spec (16 upper-hex chars)
static void generate_instance_name(void) {
    uint32_t r1 = get_rand_32();
    uint32_t r2 = get_rand_32();
    uint8_t random_bytes[8];
    memcpy(random_bytes, &r1, sizeof(r1));
    memcpy(random_bytes + 4, &r2, sizeof(r2));

    // Uppercase hex string, zero-terminated
    snprintf(current_instance_name, sizeof(current_instance_name),
             "%02X%02X%02X%02X%02X%02X%02X%02X",
             random_bytes[0], random_bytes[1], random_bytes[2], random_bytes[3],
             random_bytes[4], random_bytes[5], random_bytes[6], random_bytes[7]);
}

// TXT record callback - adds Matter-specific TXT records
static void matter_txt_callback(struct mdns_service *service, void *txt_userdata) {
    char txt_buffer[TXT_BUFFER_SIZE];
    int len;
    
    (void)txt_userdata;  // Unused
    
    // D= (Discriminator) - MANDATORY
    // 12-bit value (0x000-0xFFF)
    len = snprintf(txt_buffer, sizeof(txt_buffer), "D=%u", current_discriminator);
    if (len > 0 && len < sizeof(txt_buffer)) {
        mdns_resp_add_service_txtitem(service, txt_buffer, len);
        printf("  DNS-SD: Added TXT record: %s\n", txt_buffer);
    }
    
    // VP= (Vendor ID:Product ID) - RECOMMENDED
    // Format: decimal VID,PID
    len = snprintf(txt_buffer, sizeof(txt_buffer), "VP=%u,%u", 
                   current_vendor_id, current_product_id);
    if (len > 0 && len < sizeof(txt_buffer)) {
        mdns_resp_add_service_txtitem(service, txt_buffer, len);
        printf("  DNS-SD: Added TXT record: %s\n", txt_buffer);
    }
    
    // DT= (Device Type) - OPTIONAL
    // 16-bit device type value
    if (current_device_type != 0) {
        len = snprintf(txt_buffer, sizeof(txt_buffer), "DT=0x%04X", current_device_type);
        if (len > 0 && len < sizeof(txt_buffer)) {
            mdns_resp_add_service_txtitem(service, txt_buffer, len);
            printf("  DNS-SD: Added TXT record: %s\n", txt_buffer);
        }
    }
    
    // CM= (Commissioning Mode) - Flag
    // 1 = in commissioning mode, 0 = commissioned
    len = snprintf(txt_buffer, sizeof(txt_buffer), "CM=%u", current_commissioning_mode);
    if (len > 0 && len < sizeof(txt_buffer)) {
        mdns_resp_add_service_txtitem(service, txt_buffer, len);
        printf("  DNS-SD: Added TXT record: %s\n", txt_buffer);
    }
}

int dns_sd_init(void) {
    printf("Initializing DNS-SD for Matter discovery...\n");
    
    // Initialize mDNS responder
    mdns_resp_init();
    
    printf("DNS-SD initialized\n");
    return 0;
}

int dns_sd_advertise_commissionable_node(
    uint16_t discriminator,
    uint16_t vendor_id,
    uint16_t product_id,
    uint16_t device_type,
    uint8_t commissioning_mode
) {
    struct netif *netif;
    char hostname[32];
    
    printf("Starting Matter DNS-SD advertisement...\n");
    
    // Store current values for TXT record callback
    current_discriminator = discriminator & 0xFFF;  // Ensure 12-bit
    current_vendor_id = vendor_id;
    current_product_id = product_id;
    current_device_type = device_type;
    current_commissioning_mode = commissioning_mode ? 1 : 0;
    generate_instance_name();
    
    // Get the network interface (station mode)
    netif = &cyw43_state.netif[CYW43_ITF_STA];
    if (!netif_is_up(netif)) {
        printf("[DNS-SD] ERROR: Network interface not up, cannot advertise DNS-SD\n");
        return -1;
    }
    
    // Generate hostname based on discriminator
    snprintf(hostname, sizeof(hostname), "matter-%04X", current_discriminator);
    
    // Add network interface to mDNS responder if not already added
    cyw43_arch_lwip_begin();
    
    if (!mdns_resp_netif_active(netif)) {
        printf("  Registering mDNS netif with hostname: %s\n", hostname);
        mdns_resp_add_netif(netif, hostname);
    }
    
    // Add Matter commissioning service with randomized instance name (Matter 1.5)
    printf("  Registering Matter service: %s.%s.%s.local (instance: %s)\n",
           hostname, MATTER_SERVICE_NAME, MATTER_PROTOCOL, current_instance_name);
    printf("  Port: %d\n", MATTER_PORT);
    printf("  TXT Records:\n");
    
    mdns_resp_add_service(
        netif,
        current_instance_name,
        MATTER_SERVICE_NAME,
        DNSSD_PROTO_UDP,
        MATTER_PORT,
        matter_txt_callback,
        NULL
    );
    
    // Announce the service immediately
    mdns_resp_announce(netif);
    
    cyw43_arch_lwip_end();
    
    is_advertising = true;
    
    printf("Matter DNS-SD advertisement started successfully\n");
    printf("  Device discoverable as: %s.local\n", hostname);
    printf("  Discriminator: %u (0x%03X)\n", current_discriminator, current_discriminator);
    printf("  Vendor ID: %u (0x%04X)\n", current_vendor_id, current_vendor_id);
    printf("  Product ID: %u (0x%04X)\n", current_product_id, current_product_id);
    printf("  Service Instance: %s\n", current_instance_name);
    if (current_device_type != 0) {
        printf("  Device Type: 0x%04X\n", current_device_type);
    }
    printf("  Commissioning Mode: %s\n", current_commissioning_mode ? "Active" : "Commissioned");
    
    return 0;
}

void dns_sd_stop(void) {
    struct netif *netif;
    
    if (!is_advertising) {
        return;
    }
    
    printf("Stopping Matter DNS-SD advertisement...\n");
    
    netif = &cyw43_state.netif[CYW43_ITF_STA];
    
    cyw43_arch_lwip_begin();
    
    // Remove network interface from mDNS responder
    if (mdns_resp_netif_active(netif)) {
        mdns_resp_remove_netif(netif);
    }
    
    cyw43_arch_lwip_end();
    
    is_advertising = false;
    
    printf("Matter DNS-SD advertisement stopped\n");
}

bool dns_sd_is_advertising(void) {
    return is_advertising;
}
