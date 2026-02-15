/*
 * dns_sd.h
 * DNS-SD (mDNS) service discovery for Matter commissioning
 * Implements _matterc._udp service advertisement as per Matter specification
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * Initialize DNS-SD for Matter device discovery
 * Registers the _matterc._udp service with required TXT records
 * 
 * @return 0 on success, -1 on error
 */
int dns_sd_init(void);

/**
 * Start advertising the Matter commissioning service
 * Should be called after network is connected
 * 
 * @param discriminator 12-bit discriminator value (0x000-0xFFF)
 * @param vendor_id 16-bit vendor ID
 * @param product_id 16-bit product ID
 * @param device_type 16-bit device type (e.g., 0x0302 for temperature sensor)
 * @param commissioning_mode 1 if in commissioning mode, 0 otherwise
 * @return 0 on success, -1 on error
 */
int dns_sd_advertise_commissionable_node(
    uint16_t discriminator,
    uint16_t vendor_id,
    uint16_t product_id,
    uint16_t device_type,
    uint8_t commissioning_mode
);

/**
 * Stop advertising the Matter commissioning service
 */
void dns_sd_stop(void);

/**
 * Check if DNS-SD is currently advertising
 * 
 * @return true if advertising, false otherwise
 */
bool dns_sd_is_advertising(void);

#ifdef __cplusplus
}
#endif
