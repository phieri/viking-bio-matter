/*
 * network_commissioning.h
 * Matter Network Commissioning Cluster (0x0031)
 * WiFi Network Commissioning implementation
 */

#ifndef CLUSTER_NETWORK_COMMISSIONING_H
#define CLUSTER_NETWORK_COMMISSIONING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Network Commissioning Cluster ID
#define MATTER_CLUSTER_NETWORK_COMMISSIONING 0x0031

// Network Commissioning Commands
#define NETWORK_COMMISSIONING_CMD_SCAN_NETWORKS           0x00
#define NETWORK_COMMISSIONING_CMD_SCAN_NETWORKS_RESPONSE  0x01
#define NETWORK_COMMISSIONING_CMD_ADD_OR_UPDATE_WIFI      0x02
#define NETWORK_COMMISSIONING_CMD_ADD_OR_UPDATE_THREAD    0x03
#define NETWORK_COMMISSIONING_CMD_REMOVE_NETWORK          0x04
#define NETWORK_COMMISSIONING_CMD_NETWORK_CONFIG_RESPONSE 0x05
#define NETWORK_COMMISSIONING_CMD_CONNECT_NETWORK         0x06
#define NETWORK_COMMISSIONING_CMD_CONNECT_NETWORK_RESPONSE 0x07
#define NETWORK_COMMISSIONING_CMD_REORDER_NETWORK         0x08

// Network Commissioning Attributes
#define NETWORK_COMMISSIONING_ATTR_MAX_NETWORKS           0x0000
#define NETWORK_COMMISSIONING_ATTR_NETWORKS               0x0001
#define NETWORK_COMMISSIONING_ATTR_SCAN_MAX_TIME_SECONDS  0x0002
#define NETWORK_COMMISSIONING_ATTR_CONNECT_MAX_TIME_SECONDS 0x0003
#define NETWORK_COMMISSIONING_ATTR_INTERFACE_ENABLED      0x0004
#define NETWORK_COMMISSIONING_ATTR_LAST_NETWORKING_STATUS 0x0005
#define NETWORK_COMMISSIONING_ATTR_LAST_NETWORK_ID        0x0006
#define NETWORK_COMMISSIONING_ATTR_LAST_CONNECT_ERROR_VALUE 0x0007

// Network Commissioning Status Codes
typedef enum {
    NETWORK_STATUS_SUCCESS = 0x00,
    NETWORK_STATUS_OUT_OF_RANGE = 0x01,
    NETWORK_STATUS_BOUNDS_EXCEEDED = 0x02,
    NETWORK_STATUS_NETWORK_ID_NOT_FOUND = 0x03,
    NETWORK_STATUS_DUPLICATE_NETWORK_ID = 0x04,
    NETWORK_STATUS_NETWORK_NOT_FOUND = 0x05,
    NETWORK_STATUS_REGULATORY_ERROR = 0x06,
    NETWORK_STATUS_AUTH_FAILURE = 0x07,
    NETWORK_STATUS_UNSUPPORTED_SECURITY = 0x08,
    NETWORK_STATUS_OTHER_CONNECTION_FAILURE = 0x09,
    NETWORK_STATUS_IPV6_FAILED = 0x0A,
    NETWORK_STATUS_IP_BIND_FAILED = 0x0B,
    NETWORK_STATUS_UNKNOWN_ERROR = 0x0C
} network_commissioning_status_t;

/**
 * Handle AddOrUpdateWiFiNetwork command
 * 
 * @param ssid WiFi SSID
 * @param ssid_len SSID length
 * @param credentials WiFi password
 * @param credentials_len Password length
 * @param response_buffer Output buffer for response
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int network_commissioning_add_or_update_wifi(
    const uint8_t *ssid, size_t ssid_len,
    const uint8_t *credentials, size_t credentials_len,
    uint8_t *response_buffer, size_t max_response_len,
    size_t *actual_response_len);

/**
 * Handle ConnectNetwork command
 * Connects to the provisioned WiFi network
 * 
 * @param network_id Network ID (SSID)
 * @param network_id_len Network ID length
 * @param response_buffer Output buffer for response
 * @param max_response_len Maximum response buffer size
 * @param actual_response_len Actual response length written
 * @return 0 on success, -1 on error
 */
int network_commissioning_connect_network(
    const uint8_t *network_id, size_t network_id_len,
    uint8_t *response_buffer, size_t max_response_len,
    size_t *actual_response_len);

/**
 * Read Network Commissioning cluster attribute
 * 
 * @param attribute_id Attribute ID to read
 * @param value_out Output buffer for attribute value
 * @param max_value_len Maximum value buffer size
 * @param actual_value_len Actual value length written
 * @return 0 on success, -1 on error
 */
int cluster_network_commissioning_read(
    uint32_t attribute_id,
    uint8_t *value_out, size_t max_value_len,
    size_t *actual_value_len);

/**
 * Initialize Network Commissioning cluster
 * 
 * @return 0 on success, -1 on error
 */
int cluster_network_commissioning_init(void);

#ifdef __cplusplus
}
#endif

#endif // CLUSTER_NETWORK_COMMISSIONING_H
