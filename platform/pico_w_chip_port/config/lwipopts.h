/**
 * @file lwipopts.h
 * @brief lwIP configuration for Viking Bio Matter Bridge (Pico W)
 * 
 * Based on Pico SDK lwIP defaults for WiFi applications.
 * Vendored to resolve configuration header access issues.
 */

#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Platform specifics
#define NO_SYS                      1
#define LWIP_SOCKET                 0
#define LWIP_NETCONN                0

// Memory options - DO NOT use MEM_LIBC_MALLOC with threadsafe_background
#define MEM_LIBC_MALLOC             0
#define MEMP_NUM_TCP_SEG            32
#define MEMP_NUM_ARP_QUEUE          10
#define PBUF_POOL_SIZE              24
#define LWIP_ARP                    1
#define LWIP_ETHERNET               1
#define LWIP_ICMP                   1
#define LWIP_RAW                    1
#define TCP_MSS                     1460
#define TCP_WND                     (8 * TCP_MSS)
#define TCP_SND_BUF                 (8 * TCP_MSS)
#define TCP_SND_QUEUELEN            ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_NETIF_STATUS_CALLBACK  1
#define LWIP_NETIF_LINK_CALLBACK    1
#define LWIP_NETIF_HOSTNAME         1
#define LWIP_NETCONN                0
#define MEM_STATS                   0
#define SYS_STATS                   0
#define MEMP_STATS                  0
#define LINK_STATS                  0

// DHCP options
#define LWIP_DHCP                   1
#define LWIP_IPV4                   1
#define LWIP_IPV6                   1
#define LWIP_TCP                    1
#define LWIP_UDP                    1
#define LWIP_DNS                    1
#define LWIP_TCP_KEEPALIVE          1
#define LWIP_NETIF_TX_SINGLE_PBUF   1
#define DHCP_DOES_ARP_CHECK         0
#define LWIP_DHCP_DOES_ACD_CHECK    0

// mDNS options for Matter device discovery
#define LWIP_MDNS_RESPONDER         1
#define LWIP_IGMP                   1
#define LWIP_NUM_NETIF_CLIENT_DATA  1

// Threading
#define TCPIP_THREAD_STACKSIZE      1024
#define DEFAULT_THREAD_STACKSIZE    1024
#define DEFAULT_RAW_RECVMBOX_SIZE   8
#define TCPIP_MBOX_SIZE             8
#define LWIP_TIMEVAL_PRIVATE        0

// Checksums - let hardware handle if possible  
#define CHECKSUM_GEN_IP             0
#define CHECKSUM_GEN_UDP            0
#define CHECKSUM_GEN_TCP            0
#define CHECKSUM_GEN_ICMP           0
#define CHECKSUM_GEN_ICMP6          0
#define CHECKSUM_CHECK_IP           0
#define CHECKSUM_CHECK_UDP          0
#define CHECKSUM_CHECK_TCP          0
#define CHECKSUM_CHECK_ICMP         0
#define CHECKSUM_CHECK_ICMP6        0

#endif /* LWIPOPTS_H */
