# Matter Attribute Management System

## Overview

This directory contains a functional Matter attribute management system for the Viking Bio 20 bridge. The implementation provides real attribute storage, change detection, and a subscription-based reporting mechanism.

## Components

### Core System

#### `matter_attributes.h` / `matter_attributes.cpp`
Thread-safe attribute storage and management system:
- **Attribute Registration**: Register Matter clusters/attributes with proper types
- **Value Storage**: Store and retrieve attribute values
- **Change Detection**: Automatically detect when values change
- **Subscription System**: Callback-based notification for attribute changes
- **Thread Safety**: Uses Pico SDK critical sections for concurrent access

#### `matter_reporter.cpp` / `matter_reporter.h`
Example subscriber that demonstrates the attribute reporting system:
- Subscribes to all attribute changes
- Formats and logs attribute reports
- Shows proper usage of the subscription API

### Platform Integration

#### `platform_manager.cpp`
- Initializes the attribute system on startup
- Registers the 3 Matter clusters for Viking Bio:
  - **OnOff** (0x0006): Flame detection state
  - **LevelControl** (0x0008): Fan speed percentage
  - **TemperatureMeasurement** (0x0402): Temperature in centidegrees
- Processes attribute reports in the main platform task loop

## Features

### 1. Proper Attribute Storage
Unlike the previous stub implementation, this system actually:
- Stores attribute values in memory
- Organizes by endpoint/cluster/attribute ID
- Supports multiple data types (bool, uint8, int16, uint32)

### 2. Change Detection
- Compares new values with cached values
- Only triggers reports when values actually change
- Reduces unnecessary network traffic

### 3. Subscription System
- Up to 4 simultaneous subscribers
- Callback-based notifications
- Easy to extend for network transport layers

### 4. Thread Safety
- Uses Pico SDK critical sections
- Safe for multi-threaded or interrupt-driven updates
- Prevents race conditions in attribute access

### 5. Matter Compliance
- Uses correct Matter cluster IDs
- Proper attribute types per Matter specification
- Temperature in centidegrees (Matter requirement)
- Endpoint-based organization

## Usage Example

### Registering an Attribute

```cpp
// During initialization
matter_attr_value_t initial_value;
initial_value.bool_val = false;

matter_attributes_register(
    1,                          // endpoint
    MATTER_CLUSTER_ON_OFF,      // cluster ID (0x0006)
    MATTER_ATTR_ON_OFF,         // attribute ID (0x0000)
    MATTER_TYPE_BOOL,           // data type
    &initial_value              // initial value
);
```

### Updating an Attribute

```cpp
// When value changes
matter_attr_value_t new_value;
new_value.bool_val = true;

matter_attributes_update(
    1,                          // endpoint
    MATTER_CLUSTER_ON_OFF,      // cluster ID
    MATTER_ATTR_ON_OFF,         // attribute ID
    &new_value                  // new value
);
```

### Subscribing to Changes

```cpp
void my_callback(uint8_t endpoint, uint32_t cluster_id,
                 uint32_t attribute_id, const matter_attr_value_t *value) {
    printf("Attribute changed: EP=%u, CL=0x%04X, AT=0x%04X\n",
           endpoint, cluster_id, attribute_id);
}

// Register subscriber
int subscriber_id = matter_attributes_subscribe(my_callback);
```

### Processing Reports

```cpp
// In main loop or platform task
matter_attributes_process_reports();
```

## Console Output Example

When Viking Bio data changes, you'll see:

```
Matter: OnOff cluster updated - Flame ON
Matter: Attribute changed (EP:1, CL:0x0006, AT:0x0000) = true
Matter Report Sent:
  Endpoint: 1
  Cluster:  0x0006
  Attribute: 0x0000
  Value: ON (OnOff)

Matter: LevelControl cluster updated - Fan speed 75%
Matter: Attribute changed (EP:1, CL:0x0008, AT:0x0000) = 75
Matter Report Sent:
  Endpoint: 1
  Cluster:  0x0008
  Attribute: 0x0000
  Value: 75% (Level)

Matter: TemperatureMeasurement cluster updated - 150°C
Matter: Attribute changed (EP:1, CL:0x0402, AT:0x0000) = 15000
Matter Report Sent:
  Endpoint: 1
  Cluster:  0x0402
  Attribute: 0x0000
  Value: 150.00°C (Temperature)
```

## Architecture

```
Viking Bio Data
    ↓
matter_bridge_update_*()
    ↓
matter_attributes_update()
    ├─ Compare with cached value
    ├─ Update if changed
    └─ Mark as "dirty"
    
platform_manager_task()
    ↓
matter_attributes_process_reports()
    ├─ Find all dirty attributes
    └─ Call subscriber callbacks
        ↓
    matter_reporter (example)
        └─ Log formatted report
```

## Memory Usage

- **Attributes**: 16 max (configurable via MAX_ATTRIBUTES)
- **Subscribers**: 4 max (configurable via MATTER_MAX_SUBSCRIBERS)
- **Per Attribute**: ~24 bytes (cluster_id, attribute_id, endpoint, type, value, dirty flag)
- **Total**: ~400 bytes + subscriber callback pointers

## Future Enhancements

### Network Transport Layer
Add a subscriber that sends attribute reports over WiFi:
```cpp
void network_reporter(uint8_t endpoint, uint32_t cluster_id,
                     uint32_t attribute_id, const matter_attr_value_t *value) {
    // Build Matter report message
    // Send via UDP/TCP to subscribed controllers
}
```

### Full Matter SDK Integration
Replace the attribute system with the full connectedhomeip SDK:
- Link Matter SDK libraries
- Implement DeviceLayer platform interface
- Use Matter's built-in attribute management
- Add full protocol support (BLE, Thread, etc.)

### Persistent Storage
Store attribute values in flash:
```cpp
// On attribute change
storage_adapter_write("attr_1_6_0", &value, sizeof(value));
```

## Testing

The system can be tested without hardware using the Viking Bio simulator:

```bash
python3 examples/viking_bio_simulator.py /dev/ttyUSB0
```

Monitor the console output to see attribute updates and reports being generated in real-time.

## Integration with Matter Controllers

While this implementation provides functional attribute management, it doesn't include network transport. To integrate with actual Matter controllers:

1. **Option A**: Add a network subscriber that sends reports via UDP/TCP
2. **Option B**: Integrate with the full connectedhomeip SDK
3. **Option C**: Use the attribute system as a bridge to a separate Matter stack

## Comparison: Stub vs Functional Implementation

### Previous Stub Implementation
```cpp
void platform_manager_report_onoff_change(uint8_t endpoint) {
    printf("Matter Report: Cluster 0x0006, ...\n");
    // That's it - just logging
}
```

### New Functional Implementation
```cpp
void platform_manager_report_onoff_change(uint8_t endpoint) {
    // Actual attribute update
    matter_attr_value_t value;
    value.bool_val = new_state;
    matter_attributes_update(1, 0x0006, 0x0000, &value);
    // ↓
    // Detects change, marks dirty, notifies subscribers
    // Subscribers can send actual network reports
}
```

## Conclusion

This implementation provides a **functional** Matter attribute management system that:
- ✅ Actually stores and tracks attribute values
- ✅ Detects changes before reporting
- ✅ Provides extensible subscription mechanism
- ✅ Uses correct Matter cluster/attribute structure
- ✅ Is thread-safe for embedded environments
- ✅ Can be extended with network transport or full SDK

It's no longer just logging - it's a working attribute management system ready for integration with network layers or the full Matter SDK.
