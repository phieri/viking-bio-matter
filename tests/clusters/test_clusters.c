/*
 * test_clusters.c
 * Unit tests for Matter cluster implementations
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/matter_minimal/clusters/descriptor.h"
#include "../../src/matter_minimal/clusters/onoff.h"
#include "../../src/matter_minimal/clusters/level_control.h"
#include "../../src/matter_minimal/clusters/temperature.h"
#include "../../src/matter_minimal/clusters/diagnostics.h"

// Test counter
static int tests_passed = 0;
static int tests_failed = 0;

// Mock matter_attributes_get for testing
int matter_attributes_get(uint8_t endpoint, uint32_t cluster_id,
                         uint32_t attribute_id, void *value) {
    if (endpoint != 1) return -1;
    
    switch (cluster_id) {
        case 0x0006: // OnOff
            if (attribute_id == 0x0000) {
                *(bool*)value = true;
                return 0;
            }
            break;
        case 0x0008: // LevelControl
            if (attribute_id == 0x0000) {
                *(uint8_t*)value = 75;
                return 0;
            }
            break;
        case 0x0402: // Temperature
            if (attribute_id == 0x0000) {
                *(int16_t*)value = 2500;  // 25.00°C
                return 0;
            }
            break;
        case 0x0033: // Diagnostics
            if (attribute_id == 0x0003) {  // TotalOperationalHours
                *(uint32_t*)value = 123;
                return 0;
            } else if (attribute_id == 0x0005) {  // DeviceEnabledState
                *(uint8_t*)value = 1;  // Enabled
                return 0;
            } else if (attribute_id == 0x0001) {  // NumberOfActiveFaults
                *(uint8_t*)value = 0;  // No faults
                return 0;
            }
            break;
    }
    return -1;
}

// Test: Descriptor DeviceTypeList
void test_descriptor_device_type_list(void) {
    printf("Test: Descriptor DeviceTypeList attribute...\n");
    
    device_type_entry_t types[4];
    size_t count;
    
    // Test endpoint 0 (Root Node)
    int result = cluster_descriptor_get_device_types(0, types, 4, &count);
    if (result == 0 && count == 1 && types[0].device_type == 0x0016) {
        printf("  ✓ Endpoint 0 device types correct (Root Node)\n");
        tests_passed++;
    } else {
        printf("  ✗ Endpoint 0 device types failed\n");
        tests_failed++;
    }
    
    // Test endpoint 1 (Temperature Sensor)
    result = cluster_descriptor_get_device_types(1, types, 4, &count);
    if (result == 0 && count == 1 && types[0].device_type == 0x0302) {
        printf("  ✓ Endpoint 1 device types correct (Temperature Sensor)\n");
        tests_passed++;
    } else {
        printf("  ✗ Endpoint 1 device types failed\n");
        tests_failed++;
    }
}

// Test: Descriptor ServerList
void test_descriptor_server_list(void) {
    printf("Test: Descriptor ServerList attribute...\n");
    
    uint32_t clusters[8];
    size_t count;
    
    // Test endpoint 0 (only Descriptor cluster)
    int result = cluster_descriptor_get_server_list(0, clusters, 8, &count);
    if (result == 0 && count == 1 && clusters[0] == 0x001D) {
        printf("  ✓ Endpoint 0 server list correct (Descriptor only)\n");
        tests_passed++;
    } else {
        printf("  ✗ Endpoint 0 server list failed\n");
        tests_failed++;
    }
    
    // Test endpoint 1 (OnOff, LevelControl, Temperature, Diagnostics)
    result = cluster_descriptor_get_server_list(1, clusters, 8, &count);
    if (result == 0 && count == 4 && 
        clusters[0] == 0x0006 && clusters[1] == 0x0008 && 
        clusters[2] == 0x0402 && clusters[3] == 0x0033) {
        printf("  ✓ Endpoint 1 server list correct (4 clusters)\n");
        tests_passed++;
    } else {
        printf("  ✗ Endpoint 1 server list failed (count=%zu)\n", count);
        tests_failed++;
    }
}

// Test: OnOff read flame state
void test_onoff_read_flame_state(void) {
    printf("Test: OnOff cluster read flame state...\n");
    
    attribute_value_t value;
    attribute_type_t type;
    
    int result = cluster_onoff_read(1, ATTR_ONOFF, &value, &type);
    
    if (result == 0 && type == ATTR_TYPE_BOOL && value.bool_val == true) {
        printf("  ✓ OnOff read succeeded (flame=true)\n");
        tests_passed++;
    } else {
        printf("  ✗ OnOff read failed\n");
        tests_failed++;
    }
    
    // Test unsupported endpoint
    result = cluster_onoff_read(0, ATTR_ONOFF, &value, &type);
    if (result < 0) {
        printf("  ✓ OnOff correctly rejects endpoint 0\n");
        tests_passed++;
    } else {
        printf("  ✗ OnOff should reject endpoint 0\n");
        tests_failed++;
    }
}

// Test: LevelControl read fan speed
void test_level_control_read_fan_speed(void) {
    printf("Test: LevelControl cluster read fan speed...\n");
    
    attribute_value_t value;
    attribute_type_t type;
    
    // Test CurrentLevel
    int result = cluster_level_control_read(1, ATTR_CURRENT_LEVEL, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT8 && value.uint8_val == 75) {
        printf("  ✓ CurrentLevel read succeeded (level=75)\n");
        tests_passed++;
    } else {
        printf("  ✗ CurrentLevel read failed\n");
        tests_failed++;
    }
    
    // Test MinLevel
    result = cluster_level_control_read(1, ATTR_MIN_LEVEL, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT8 && value.uint8_val == 0) {
        printf("  ✓ MinLevel read succeeded (min=0)\n");
        tests_passed++;
    } else {
        printf("  ✗ MinLevel read failed\n");
        tests_failed++;
    }
    
    // Test MaxLevel
    result = cluster_level_control_read(1, ATTR_MAX_LEVEL, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT8 && value.uint8_val == 100) {
        printf("  ✓ MaxLevel read succeeded (max=100)\n");
        tests_passed++;
    } else {
        printf("  ✗ MaxLevel read failed\n");
        tests_failed++;
    }
}

// Test: Temperature read value
void test_temperature_read_value(void) {
    printf("Test: TemperatureMeasurement cluster read value...\n");
    
    attribute_value_t value;
    attribute_type_t type;
    
    // Test MeasuredValue
    int result = cluster_temperature_read(1, ATTR_MEASURED_VALUE, &value, &type);
    if (result == 0 && type == ATTR_TYPE_INT16 && value.int16_val == 2500) {
        printf("  ✓ MeasuredValue read succeeded (2500 = 25.00°C)\n");
        tests_passed++;
    } else {
        printf("  ✗ MeasuredValue read failed\n");
        tests_failed++;
    }
    
    // Test MinMeasuredValue
    result = cluster_temperature_read(1, ATTR_MIN_MEASURED_VALUE, &value, &type);
    if (result == 0 && type == ATTR_TYPE_INT16 && value.int16_val == 0) {
        printf("  ✓ MinMeasuredValue read succeeded (0°C)\n");
        tests_passed++;
    } else {
        printf("  ✗ MinMeasuredValue read failed\n");
        tests_failed++;
    }
    
    // Test MaxMeasuredValue
    result = cluster_temperature_read(1, ATTR_MAX_MEASURED_VALUE, &value, &type);
    if (result == 0 && type == ATTR_TYPE_INT16 && value.int16_val == 10000) {
        printf("  ✓ MaxMeasuredValue read succeeded (100°C)\n");
        tests_passed++;
    } else {
        printf("  ✗ MaxMeasuredValue read failed\n");
        tests_failed++;
    }
    
    // Test Tolerance
    result = cluster_temperature_read(1, ATTR_TOLERANCE, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT16 && value.uint16_val == 100) {
        printf("  ✓ Tolerance read succeeded (±1°C)\n");
        tests_passed++;
    } else {
        printf("  ✗ Tolerance read failed\n");
        tests_failed++;
    }
}

// Test: Unsupported attribute handling
void test_unsupported_attribute_handling(void) {
    printf("Test: Unsupported attribute handling...\n");
    
    attribute_value_t value;
    attribute_type_t type;
    
    // Test unsupported attribute on OnOff
    int result = cluster_onoff_read(1, 0x9999, &value, &type);
    if (result < 0) {
        printf("  ✓ OnOff correctly rejects unsupported attribute\n");
        tests_passed++;
    } else {
        printf("  ✗ OnOff should reject unsupported attribute\n");
        tests_failed++;
    }
    
    // Test unsupported attribute on LevelControl
    result = cluster_level_control_read(1, 0x9999, &value, &type);
    if (result < 0) {
        printf("  ✓ LevelControl correctly rejects unsupported attribute\n");
        tests_passed++;
    } else {
        printf("  ✗ LevelControl should reject unsupported attribute\n");
        tests_failed++;
    }
    
    // Test unsupported attribute on Temperature
    result = cluster_temperature_read(1, 0x9999, &value, &type);
    if (result < 0) {
        printf("  ✓ Temperature correctly rejects unsupported attribute\n");
        tests_passed++;
    } else {
        printf("  ✗ Temperature should reject unsupported attribute\n");
        tests_failed++;
    }
}

// Test: Diagnostics cluster read attributes
void test_diagnostics_read_attributes(void) {
    printf("Test: Diagnostics cluster read attributes...\n");
    
    attribute_value_t value;
    attribute_type_t type;
    
    // Test TotalOperationalHours
    int result = cluster_diagnostics_read(1, ATTR_TOTAL_OPERATIONAL_HOURS, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT32 && value.uint32_val == 123) {
        printf("  ✓ TotalOperationalHours read succeeded (123 hours)\n");
        tests_passed++;
    } else {
        printf("  ✗ TotalOperationalHours read failed\n");
        tests_failed++;
    }
    
    // Test DeviceEnabledState
    result = cluster_diagnostics_read(1, ATTR_DEVICE_ENABLED_STATE, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT8 && value.uint8_val == 1) {
        printf("  ✓ DeviceEnabledState read succeeded (enabled)\n");
        tests_passed++;
    } else {
        printf("  ✗ DeviceEnabledState read failed\n");
        tests_failed++;
    }
    
    // Test NumberOfActiveFaults
    result = cluster_diagnostics_read(1, ATTR_NUMBER_OF_ACTIVE_FAULTS, &value, &type);
    if (result == 0 && type == ATTR_TYPE_UINT8 && value.uint8_val == 0) {
        printf("  ✓ NumberOfActiveFaults read succeeded (0 faults)\n");
        tests_passed++;
    } else {
        printf("  ✗ NumberOfActiveFaults read failed\n");
        tests_failed++;
    }
    
    // Test unsupported endpoint
    result = cluster_diagnostics_read(0, ATTR_TOTAL_OPERATIONAL_HOURS, &value, &type);
    if (result < 0) {
        printf("  ✓ Diagnostics correctly rejects endpoint 0\n");
        tests_passed++;
    } else {
        printf("  ✗ Diagnostics should reject endpoint 0\n");
        tests_failed++;
    }
    
    // Test unsupported attribute
    result = cluster_diagnostics_read(1, 0x9999, &value, &type);
    if (result < 0) {
        printf("  ✓ Diagnostics correctly rejects unsupported attribute\n");
        tests_passed++;
    } else {
        printf("  ✗ Diagnostics should reject unsupported attribute\n");
        tests_failed++;
    }
}

int main(void) {
    printf("\n========================================\n");
    printf("  Matter Cluster Tests\n");
    printf("========================================\n\n");
    
    // Initialize clusters
    if (cluster_descriptor_init() < 0 ||
        cluster_onoff_init() < 0 ||
        cluster_level_control_init() < 0 ||
        cluster_temperature_init() < 0 ||
        cluster_diagnostics_init() < 0) {
        printf("ERROR: Failed to initialize clusters\n");
        return 1;
    }
    
    // Run tests
    test_descriptor_device_type_list();
    test_descriptor_server_list();
    test_onoff_read_flame_state();
    test_level_control_read_fan_speed();
    test_temperature_read_value();
    test_diagnostics_read_attributes();
    test_unsupported_attribute_handling();
    
    // Print results
    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n\n");
    
    return tests_failed > 0 ? 1 : 0;
}
