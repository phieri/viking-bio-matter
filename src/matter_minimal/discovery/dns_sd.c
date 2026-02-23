/*
 * dns_sd.c
 * DNS-SD stub — mDNS removed for CYW43 init diagnostic.
 *
 * mdns_resp_init() calls igmp_joingroup_netif_all() and
 * mld6_joingroup_netif_all() which attempt to send multicast join
 * packets on the CYW43 netif immediately after cyw43_arch_init().
 * There is also a LWIP_NUM_NETIF_CLIENT_DATA mismatch: DHCP + IGMP +
 * MDNS require 3 client-data slots but the current lwipopts.h only
 * provides 2, risking out-of-bounds memory writes.
 *
 * This stub eliminates all mDNS/lwIP-multicast activity so we can
 * determine whether mDNS initialisation is the root cause of the hang.
 */

#include "dns_sd.h"
#include <stdio.h>

int dns_sd_init(void) {
    printf("DNS-SD: mDNS removed for diagnostics — dns_sd_init() is a no-op\n");
    return 0;
}

int dns_sd_advertise_commissionable_node(
    uint16_t discriminator,
    uint16_t vendor_id,
    uint16_t product_id,
    uint16_t device_type,
    uint8_t commissioning_mode
) {
    (void)discriminator;
    (void)vendor_id;
    (void)product_id;
    (void)device_type;
    (void)commissioning_mode;
    printf("DNS-SD: advertisement skipped (mDNS removed for diagnostics)\n");
    return 0;
}

void dns_sd_stop(void) {
    /* no-op */
}

bool dns_sd_is_advertising(void) {
    return false;
}
