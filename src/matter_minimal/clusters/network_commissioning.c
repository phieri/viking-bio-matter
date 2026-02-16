/*
 * network_commissioning.c
 * Matter Network Commissioning Cluster (0x0031)
 * WiFi Network Commissioning implementation
 */

#include "network_commissioning.h"
#include <string.h>
#include <stdio.h>

// Forward declarations for platform network functions
extern int storage_adapter_save_wifi_credentials(const char *ssid, const char *password);
extern int network_adapter_save_and_connect(const char *ssid, const char *password);
extern bool network_adapter_is_connected(void);

// Network commissioning state
static bool initialized = false;
static network_commissioning_status_t last_status = NETWORK_STATUS_SUCCESS;
static char last_network_id[33] = {0};  // Last SSID
static bool interface_enabled = true;

/**
 * Initialize Network Commissioning cluster
 */
int cluster_network_commissioning_init(void) {
    if (initialized) {
        return 0;
    }
    
    initialized = true;
    last_status = NETWORK_STATUS_SUCCESS;
    memset(last_network_id, 0, sizeof(last_network_id));
    interface_enabled = true;
    
    printf("Network Commissioning cluster initialized\n");
    return 0;
}

/**
 * Handle AddOrUpdateWiFiNetwork command (0x02)
 * Format:
 *   - SSID (octstr)
 *   - Credentials (octstr)
 *   - Breadcrumb (uint64, optional)
 */
int network_commissioning_add_or_update_wifi(
    const uint8_t *ssid, size_t ssid_len,
    const uint8_t *credentials, size_t credentials_len,
    uint8_t *response_buffer, size_t max_response_len,
    size_t *actual_response_len)
{
    if (!ssid || ssid_len == 0 || ssid_len > 32) {
        printf("NetworkCommissioning: Invalid SSID (len=%zu)\n", ssid_len);
        last_status = NETWORK_STATUS_OUT_OF_RANGE;
        return -1;
    }
    
    if (!credentials || credentials_len > 64) {
        printf("NetworkCommissioning: Invalid credentials (len=%zu)\n", credentials_len);
        last_status = NETWORK_STATUS_OUT_OF_RANGE;
        return -1;
    }
    
    // Convert to null-terminated strings
    char ssid_str[33] = {0};
    char password_str[65] = {0};
    
    memcpy(ssid_str, ssid, ssid_len);
    ssid_str[ssid_len] = '\0';
    
    if (credentials_len > 0) {
        memcpy(password_str, credentials, credentials_len);
        password_str[credentials_len] = '\0';
    }
    
    printf("NetworkCommissioning: AddOrUpdateWiFiNetwork\n");
    printf("  SSID: %s\n", ssid_str);
    printf("  Credentials: %s\n", password_str[0] ? "***" : "(none)");
    
    // Save credentials to storage
    int storage_result = storage_adapter_save_wifi_credentials(ssid_str, password_str);
    if (storage_result != 0) {
        // Map storage error to specific network commissioning status
        // Storage adapter logs detailed error, we provide high-level status
        if (ssid_len == 0 || ssid_len > 32 || credentials_len > 64) {
            // Length validation failed (should have been caught earlier)
            last_status = NETWORK_STATUS_OUT_OF_RANGE;
            printf("NetworkCommissioning: Failed to save credentials - parameter out of range\n");
        } else {
            // General storage failure (disk full, I/O error, corruption, etc.)
            last_status = NETWORK_STATUS_OTHER_CONNECTION_FAILURE;
            printf("NetworkCommissioning: Failed to save credentials - storage error\n");
        }
        return -1;
    }
    
    // Store last network ID
    strncpy(last_network_id, ssid_str, sizeof(last_network_id) - 1);
    last_status = NETWORK_STATUS_SUCCESS;
    
    // Build NetworkConfigResponse (0x05)
    // Simple response: [status] [network_index (optional)]
    if (response_buffer && max_response_len > 0) {
        response_buffer[0] = (uint8_t)NETWORK_STATUS_SUCCESS;
        *actual_response_len = 1;
    }
    
    printf("NetworkCommissioning: WiFi credentials saved successfully\n");
    return 0;
}

/**
 * Handle ConnectNetwork command (0x06)
 * Connects to the specified network
 */
int network_commissioning_connect_network(
    const uint8_t *network_id, size_t network_id_len,
    uint8_t *response_buffer, size_t max_response_len,
    size_t *actual_response_len)
{
    if (!network_id || network_id_len == 0 || network_id_len > 32) {
        printf("NetworkCommissioning: Invalid network ID\n");
        last_status = NETWORK_STATUS_OUT_OF_RANGE;
        return -1;
    }
    
    // Convert to null-terminated string
    char network_id_str[33] = {0};
    memcpy(network_id_str, network_id, network_id_len);
    network_id_str[network_id_len] = '\0';
    
    printf("NetworkCommissioning: ConnectNetwork\n");
    printf("  Network ID: %s\n", network_id_str);
    
    // Connect to the network using stored credentials
    // The network_adapter will load credentials from storage
    if (network_adapter_save_and_connect(network_id_str, NULL) != 0) {
        printf("NetworkCommissioning: Failed to connect to network\n");
        last_status = NETWORK_STATUS_OTHER_CONNECTION_FAILURE;
        
        // Build error response
        if (response_buffer && max_response_len >= 2) {
            response_buffer[0] = (uint8_t)NETWORK_STATUS_OTHER_CONNECTION_FAILURE;
            response_buffer[1] = 0;  // Error value (placeholder)
            *actual_response_len = 2;
        }
        return -1;
    }
    
    // Store last network ID
    strncpy(last_network_id, network_id_str, sizeof(last_network_id) - 1);
    last_status = NETWORK_STATUS_SUCCESS;
    
    // Build ConnectNetworkResponse (0x07)
    // Format: [status] [error_value (optional)] [debug_text (optional)]
    if (response_buffer && max_response_len > 0) {
        response_buffer[0] = (uint8_t)NETWORK_STATUS_SUCCESS;
        *actual_response_len = 1;
    }
    
    printf("NetworkCommissioning: Connected to network successfully\n");
    return 0;
}

/**
 * Read Network Commissioning cluster attribute
 */
int cluster_network_commissioning_read(
    uint32_t attribute_id,
    uint8_t *value_out, size_t max_value_len,
    size_t *actual_value_len)
{
    if (!value_out || !actual_value_len) {
        return -1;
    }
    
    switch (attribute_id) {
        case NETWORK_COMMISSIONING_ATTR_MAX_NETWORKS:
            // MaxNetworks: uint8 = 1 (only one WiFi network at a time)
            if (max_value_len < 1) return -1;
            value_out[0] = 1;
            *actual_value_len = 1;
            return 0;
            
        case NETWORK_COMMISSIONING_ATTR_INTERFACE_ENABLED:
            // InterfaceEnabled: bool
            if (max_value_len < 1) return -1;
            value_out[0] = interface_enabled ? 1 : 0;
            *actual_value_len = 1;
            return 0;
            
        case NETWORK_COMMISSIONING_ATTR_LAST_NETWORKING_STATUS:
            // LastNetworkingStatus: enum8
            if (max_value_len < 1) return -1;
            value_out[0] = (uint8_t)last_status;
            *actual_value_len = 1;
            return 0;
            
        case NETWORK_COMMISSIONING_ATTR_LAST_NETWORK_ID:
            // LastNetworkID: octstr (SSID)
            {
                size_t ssid_len = strlen(last_network_id);
                if (max_value_len < ssid_len) return -1;
                memcpy(value_out, last_network_id, ssid_len);
                *actual_value_len = ssid_len;
                return 0;
            }
            
        case NETWORK_COMMISSIONING_ATTR_LAST_CONNECT_ERROR_VALUE:
            // LastConnectErrorValue: int32 (0 if no error)
            if (max_value_len < 4) return -1;
            value_out[0] = 0;
            value_out[1] = 0;
            value_out[2] = 0;
            value_out[3] = 0;
            *actual_value_len = 4;
            return 0;
            
        case NETWORK_COMMISSIONING_ATTR_SCAN_MAX_TIME_SECONDS:
            // ScanMaxTimeSeconds: uint8 = 30
            if (max_value_len < 1) return -1;
            value_out[0] = 30;
            *actual_value_len = 1;
            return 0;
            
        case NETWORK_COMMISSIONING_ATTR_CONNECT_MAX_TIME_SECONDS:
            // ConnectMaxTimeSeconds: uint8 = 30
            if (max_value_len < 1) return -1;
            value_out[0] = 30;
            *actual_value_len = 1;
            return 0;
            
        default:
            printf("NetworkCommissioning: Unknown attribute 0x%04lX\n", 
                   (unsigned long)attribute_id);
            return -1;
    }
}
