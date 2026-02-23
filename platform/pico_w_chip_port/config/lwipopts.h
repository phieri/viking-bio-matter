/**
 * @file lwipopts.h
 * @brief lwIP configuration for Viking Bio Matter Bridge (Pico W)
 *
 * Aligned with the official Pico W SDK example reference:
 *   pico-examples/pico_w/wifi/lwipopts_examples_common.h
 *
 * Key corrections from the previous version:
 *
 * 1. MEM_LIBC_MALLOC = 1 (was 0)
 *    With pico_cyw43_arch_lwip_poll the cooperative poll loop means there
 *    is no concurrency, so using system malloc/free is safe and is the
 *    approach used by all official Pico W WiFi examples.  The old setting
 *    (MEM_LIBC_MALLOC=0) pre-allocated a fixed 8KB internal heap pool in
 *    BSS; combined with a 24-pbuf pool (~36KB static) and IPv6 state this
 *    exhausted the Pico W's 264KB SRAM before cyw43_arch_init() finished.
 *
 * 2. LWIP_IPV6 removed (was 1 / enabled)
 *    IPv6 allocates large static tables: ND6 neighbor cache, per-netif
 *    IPv6 address lists, MLD6 multicast group state, routing tables.
 *    This adds ~10-20 KB of BSS that is not needed for Matter over WiFi.
 *    NOTE: If IPv6 is re-enabled later, also set LWIP_NUM_NETIF_CLIENT_DATA
 *    to at least 2 (MLD6 claims slot 1) and 3 if mDNS is re-enabled.
 *
 * 3. Checksum overrides removed (CHECKSUM_GEN/CHECK_* = 0 removed)
 *    The CYW43439 chip has NO hardware TCP/IP checksum offload; all
 *    checksums are computed in software by lwIP on the RP2040.  Setting
 *    CHECKSUM_GEN_* = 0 caused every outgoing packet to carry a zero
 *    (invalid) checksum, which remote hosts silently drop.
 *
 * 4. LWIP_NUM_NETIF_CLIENT_DATA = 1
 *    With IPv6 and mDNS both disabled, only DHCP (slot 0) claims a
 *    netif client_data slot.  The previous value of 2 was over-allocated.
 *
 * 5. Removed TCPIP_THREAD_STACKSIZE / DEFAULT_THREAD_STACKSIZE
 *    These thread-stack defines are irrelevant with NO_SYS = 1 and were
 *    unnecessary noise in the configuration.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Platform specifics
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Memory — use system malloc/free (safe with cooperative poll loop)
#define MEM_LIBC_MALLOC             1
#define MEM_ALIGNMENT               4
#define MEM_SIZE                    4000

// Pool sizes (matches pico-examples reference)
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24

// Network features
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1

// TCP
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))

// Callbacks / hostname
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1

// Stats (disabled for smaller footprint)
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

// Protocols
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
// LWIP_IPV6 intentionally not defined (defaults to 0):
// enabling it allocates ~10-20 KB of static IPv6 state tables that are
// not needed for Matter over WiFi on the Pico W's 264 KB SRAM.
// If re-enabled, set LWIP_NUM_NETIF_CLIENT_DATA >= 2.
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// mDNS removed for CYW43 init diagnostic.
// To re-enable: set LWIP_MDNS_RESPONDER=1, LWIP_IGMP=1, and
// LWIP_NUM_NETIF_CLIENT_DATA=2 (DHCP=slot0, mDNS=slot1).
// If IPv6 is also re-enabled add another slot for MLD6 (total=3).
#define LWIP_MDNS_RESPONDER         0
// IGMP (IPv4 multicast) is only needed for mDNS; disable it explicitly
// so there is no ambiguity about how many netif client_data slots are used.
// NOTE: IPv4 IGMP does NOT claim a client_data slot itself (it uses
// netif->igmp_mac_filter), but disabling it here keeps the intent clear.
#define LWIP_IGMP                   0

// With IPv6, mDNS, and IGMP all disabled, only DHCP uses a netif
// client_data slot (slot 0).
#define LWIP_NUM_NETIF_CLIENT_DATA  1

#define LWIP_TIMEVAL_PRIVATE        0

#endif /* LWIPOPTS_H */
