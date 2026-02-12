# Phase 4: Interaction Model Implementation

## Overview

Phase 4 implements the Matter Interaction Model for reading cluster attributes over the network. This phase adds support for:

- **ReadRequest/ReadResponse** interaction per Matter Spec Section 8.2
- **Descriptor Cluster** (0x001D) - Required on endpoint 0
- **OnOff Cluster** (0x0006) - Flame detection state  
- **LevelControl Cluster** (0x0008) - Fan speed (0-100%)
- **TemperatureMeasurement Cluster** (0x0402) - Temperature in centidegrees

## Architecture

### Components

```
matter_protocol.c (coordinator)
    ├── udp_transport (Phase 2) - UDP packet handling
    ├── session_mgr (Phase 3) - Encryption/decryption
    ├── message_codec (Phase 2) - Matter message encoding
    ├── read_handler - ReadRequest/ReadResponse processing
    └── clusters/
        ├── descriptor - Device structure information
        ├── onoff - Flame state (boolean)
        ├── level_control - Fan speed (0-100%)
        └── temperature - Temperature in centidegrees
```

### Data Flow

1. **Controller** sends ReadRequest (encrypted UDP packet)
2. **udp_transport** receives packet
3. **session_decrypt** decrypts payload  
4. **message_codec_decode** parses Matter message header
5. **read_handler_process_request** processes ReadRequest:
   - Parses TLV AttributePathList
   - Routes each path to appropriate cluster
   - Clusters read from `matter_attributes_get()`
   - Encodes ReadResponse with TLV
6. **message_codec_encode** wraps response
7. **session_encrypt** encrypts payload
8. **udp_transport_send** sends UDP packet back to controller

## File Structure

```
src/matter_minimal/
├── interaction/
│   ├── interaction_model.h      - Common IM types (protocol IDs, opcodes, status codes)
│   ├── read_handler.h/c          - ReadRequest/ReadResponse handler
│   └── CMakeLists.txt
├── clusters/
│   ├── descriptor.h/c            - Descriptor cluster (0x001D)
│   ├── onoff.h/c                 - OnOff cluster (0x0006)
│   ├── level_control.h/c         - LevelControl cluster (0x0008)
│   ├── temperature.h/c           - Temperature cluster (0x0402)
│   └── CMakeLists.txt
└── matter_protocol.h/c           - Protocol coordinator

tests/
├── interaction/
│   ├── test_read_handler.c       - ReadHandler unit tests (6 tests)
│   └── CMakeLists.txt
└── clusters/
    ├── test_clusters.c           - Cluster unit tests (16 tests)
    └── CMakeLists.txt
```

## Cluster Details

### Descriptor Cluster (0x001D)

**Required on endpoint 0** for all Matter devices per Device Library Spec Section 9.5.

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| DeviceTypeList | 0x0000 | array | [{device_type: 0x0302, revision: 1}] (Temperature Sensor) |
| ServerList | 0x0001 | array | [0x001D, 0x0006, 0x0008, 0x0402] |
| ClientList | 0x0002 | array | [] (empty - no client clusters) |
| PartsList | 0x0003 | array | [1] (endpoint 1) |

### OnOff Cluster (0x0006)

Matter Application Cluster Spec Section 1.5. Represents flame detection state.

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| OnOff | 0x0000 | bool | Flame detected (true/false) |

### LevelControl Cluster (0x0008)

Matter Application Cluster Spec Section 1.6. Represents fan speed.

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| CurrentLevel | 0x0000 | uint8 | Fan speed 0-100% |
| MinLevel | 0x0002 | uint8 | Minimum level (0) |
| MaxLevel | 0x0003 | uint8 | Maximum level (100) |

### TemperatureMeasurement Cluster (0x0402)

Matter Application Cluster Spec Section 2.3. Temperature in centidegrees (2500 = 25.00°C).

| Attribute | ID | Type | Description |
|-----------|------|------|-------------|
| MeasuredValue | 0x0000 | int16 | Temperature in centidegrees |
| MinMeasuredValue | 0x0001 | int16 | Minimum (0°C = 0) |
| MaxMeasuredValue | 0x0002 | int16 | Maximum (100°C = 10000) |
| Tolerance | 0x0003 | uint16 | Tolerance (±1°C = ±100) |

## Testing with chip-tool

### Prerequisites

```bash
# Build chip-tool from Matter SDK
git clone --depth 1 --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git
cd connectedhomeip
./scripts/build/build_examples.py --target linux-x64-chip-tool build
export PATH=$PATH:$PWD/out/linux-x64-chip-tool/
```

### Commission Device

```bash
# Commission device (requires Phase 6 - commissioning flow)
# For Phase 4 testing, use unsecured read (session_id=0)
chip-tool pairing onnetwork 0x1234 20202021
```

### Read Attributes

#### OnOff Cluster (Flame State)

```bash
# Read flame detection state
chip-tool onoff read on-off 0x1234 1

# Expected output:
# [1645564800.123] CHIP:DMG: ReportDataMessage =
# [1645564800.123] CHIP:DMG: {
# [1645564800.123] CHIP:DMG:   AttributeReportIBs =
# [1645564800.123] CHIP:DMG:   [
# [1645564800.123] CHIP:DMG:     AttributeReportIB =
# [1645564800.123] CHIP:DMG:     {
# [1645564800.123] CHIP:DMG:       AttributeDataIB =
# [1645564800.123] CHIP:DMG:       {
# [1645564800.123] CHIP:DMG:         DataVersion = 0x0,
# [1645564800.123] CHIP:DMG:         AttributePathIB =
# [1645564800.123] CHIP:DMG:         {
# [1645564800.123] CHIP:DMG:           Endpoint = 0x1,
# [1645564800.123] CHIP:DMG:           Cluster = 0x6,
# [1645564800.123] CHIP:DMG:           Attribute = 0x0,
# [1645564800.123] CHIP:DMG:         }
# [1645564800.123] CHIP:DMG:         Data = true,
# [1645564800.123] CHIP:DMG:       }
# [1645564800.123] CHIP:DMG:     }
# [1645564800.123] CHIP:DMG:   ]
# [1645564800.123] CHIP:DMG: }
```

#### LevelControl Cluster (Fan Speed)

```bash
# Read current fan speed
chip-tool levelcontrol read current-level 0x1234 1

# Read min/max levels
chip-tool levelcontrol read min-level 0x1234 1
chip-tool levelcontrol read max-level 0x1234 1
```

#### TemperatureMeasurement Cluster

```bash
# Read measured temperature (in centidegrees)
chip-tool temperaturemeasurement read measured-value 0x1234 1

# Output example: 2500 = 25.00°C

# Read min/max/tolerance
chip-tool temperaturemeasurement read min-measured-value 0x1234 1
chip-tool temperaturemeasurement read max-measured-value 0x1234 1
chip-tool temperaturemeasurement read tolerance 0x1234 1
```

#### Descriptor Cluster

```bash
# Read device type list (endpoint 0)
chip-tool descriptor read device-type-list 0x1234 0
# Expected: [{device_type: 0x0302, revision: 1}]

# Read server list (clusters on endpoint)
chip-tool descriptor read server-list 0x1234 0
# Expected: [0x001D] (only Descriptor on endpoint 0)

chip-tool descriptor read server-list 0x1234 1
# Expected: [0x0006, 0x0008, 0x0402]

# Read parts list (sub-endpoints)
chip-tool descriptor read parts-list 0x1234 0
# Expected: [1]
```

### Read Multiple Attributes

```bash
# Read all attributes on endpoint 1
chip-tool any read-by-id 0x1234 1 0x0006 0x0000  # OnOff
chip-tool any read-by-id 0x1234 1 0x0008 0x0000  # CurrentLevel
chip-tool any read-by-id 0x1234 1 0x0402 0x0000  # MeasuredValue
```

## Unit Tests

### test_read_handler.c

Tests for ReadRequest/ReadResponse interaction:

1. **test_parse_read_request_single_attribute** - Parse single attribute path
2. **test_parse_read_request_multiple_attributes** - Parse multiple paths
3. **test_encode_read_response_success** - Encode successful response
4. **test_encode_read_response_unsupported_cluster** - Encode error response
5. **test_read_all_clusters** - Read from all 4 clusters
6. **test_read_request_response_roundtrip** - Full roundtrip test

**Run tests:**

```bash
cd tests/interaction/build
./test_read_handler

# Expected output:
# ========================================
#   Matter ReadHandler Tests
# ========================================
# 
# Test: Parse ReadRequest with single attribute...
#   ✓ Single attribute read succeeded
# Test: Parse ReadRequest with multiple attributes...
#   ✓ Multiple attribute read succeeded
# ...
# ========================================
#   Results: 6 passed, 0 failed
# ========================================
```

### test_clusters.c

Tests for cluster implementations:

1. **test_descriptor_device_type_list** - Verify device types for endpoints 0 and 1
2. **test_descriptor_server_list** - Verify server cluster lists
3. **test_onoff_read_flame_state** - Read OnOff attribute
4. **test_level_control_read_fan_speed** - Read CurrentLevel, MinLevel, MaxLevel
5. **test_temperature_read_value** - Read MeasuredValue, Min, Max, Tolerance
6. **test_unsupported_attribute_handling** - Verify error handling

**Run tests:**

```bash
cd tests/clusters/build
./test_clusters

# Expected output:
# ========================================
#   Matter Cluster Tests
# ========================================
# 
# Test: Descriptor DeviceTypeList attribute...
#   ✓ Endpoint 0 device types correct (Root Node)
#   ✓ Endpoint 1 device types correct (Temperature Sensor)
# ...
# ========================================
#   Results: 16 passed, 0 failed
# ========================================
```

## Integration with Existing Code

### matter_attributes.cpp

Clusters read attribute values via `matter_attributes_get()`:

```c
// In onoff.c
bool onoff_state = false;
matter_attributes_get(endpoint, CLUSTER_ONOFF, ATTR_ONOFF, &onoff_state);
```

This integrates with the existing attribute storage system from platform/pico_w_chip_port/.

### TLV Codec (Phase 1)

All ReadRequest/ReadResponse encoding uses the TLV codec:

```c
// Encode attribute path
tlv_encode_structure_start(&writer, 0);
tlv_encode_uint8(&writer, 0, endpoint);
tlv_encode_uint32(&writer, 2, cluster_id);
tlv_encode_uint32(&writer, 3, attribute_id);
tlv_encode_container_end(&writer);
```

### Message Codec (Phase 2)

Matter message header encoding/decoding:

```c
matter_message_encode(&msg, buffer, sizeof(buffer), &encoded_len);
matter_message_decode(buffer, recv_len, &msg);
```

### Session Manager (Phase 3)

Encryption/decryption for secured messages:

```c
session_decrypt(session_id, ciphertext, ciphertext_len, plaintext, max_len, &actual_len);
session_encrypt(session_id, plaintext, plaintext_len, ciphertext, max_len, &actual_len);
```

## Known Limitations

1. **Array attributes**: Descriptor attributes (DeviceTypeList, ServerList, etc.) return simplified counts instead of full arrays. Full array encoding requires additional TLV list handling.

2. **Unsecured only**: Phase 4 supports session_id=0 (unsecured) for testing. Full commissioning and secured sessions require Phase 6.

3. **No subscriptions**: Only ReadRequest is implemented. Subscribe/Report will be added in Phase 5.

4. **No writes**: WriteRequest/WriteResponse not yet implemented.

5. **No commands**: InvokeRequest/InvokeResponse not yet implemented.

## Next Steps

- **Phase 5**: Implement Subscribe/Report for push-based attribute updates
- **Phase 6**: Implement full commissioning flow (PASE, commissioning cluster)
- **Phase 7**: Add write support for control (commands to burner)

## References

- Matter Core Specification Section 8: Interaction Model
- Matter Device Library Specification Section 9.5: Descriptor Cluster
- Matter Application Cluster Specification:
  - Section 1.5: OnOff Cluster
  - Section 1.6: LevelControl Cluster
  - Section 2.3: TemperatureMeasurement Cluster
