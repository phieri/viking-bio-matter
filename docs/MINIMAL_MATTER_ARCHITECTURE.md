# Minimal Matter Protocol Architecture

## Overview

This document describes the architecture for a minimal Matter protocol implementation for the Viking Bio Matter Bridge on Raspberry Pi Pico W. This implementation uses a lightweight, purpose-built protocol stack without requiring the full connectedhomeip SDK.

## Design Goals

1. **Small footprint**: Target 50-80KB binary size increase
2. **Essential features only**: Read-only sensor device with attribute reporting
3. **No external SDK**: Self-contained implementation using only Pico SDK and mbedTLS
4. **WiFi only**: No Thread, BLE, or Ethernet complexity
5. **Simple commissioning**: QR code + PIN over WiFi

## Architecture Layers

### Layer 1: Transport (UDP over lwIP)

**Purpose**: Send and receive Matter messages over UDP/IPv6

**Components**:
- `udp_transport.c`: UDP socket management
- Listens on ports:
  - 5540: Operational messages (post-commissioning)
  - 5550: Commissioning messages
- Message RX/TX queues
- IPv6 multicast for discovery (optional, can use unicast only)

**API**:
```c
int matter_transport_init(void);
int matter_transport_send(const uint8_t *data, size_t len, const ip_addr_t *dest, uint16_t port);
int matter_transport_receive(uint8_t *buffer, size_t max_len, size_t *actual_len);
void matter_transport_deinit(void);
```

**Dependencies**:
- Pico SDK lwIP (already integrated via network_adapter.cpp)
- No new dependencies

**Size estimate**: ~800 LOC, ~10KB binary

---

### Layer 2: Message Codec

**Purpose**: Encode and decode Matter protocol messages

**Components**:
- `message_codec.c`: Matter message framing
- `tlv.c`: Tag-Length-Value encoder/decoder

**Matter Message Format**:
```
[Header (8 bytes)] [Payload (variable, TLV)] [Footer (optional MIC)]
```

Header fields:
- Flags (1 byte): Version, security flags
- Session ID (2 bytes)
- Message counter (4 bytes)
- Source/Dest node ID (optional, 8 bytes each)

**TLV (Tag-Length-Value) Format**:
Matter uses TLV for all structured data. Essential types:
- Signed integers (int8, int16, int32, int64)
- Unsigned integers (uint8, uint16, uint32, uint64)
- Boolean
- UTF-8 string
- Byte string
- Null
- Structure (nested TLV)
- Array
- List

**API**:
```c
// Message codec
int matter_message_encode(const matter_message_t *msg, uint8_t *buffer, size_t max_len, size_t *actual_len);
int matter_message_decode(const uint8_t *buffer, size_t len, matter_message_t *msg);

// TLV codec
int tlv_encode_uint8(uint8_t tag, uint8_t value, uint8_t *buffer, size_t max_len);
int tlv_encode_int16(uint8_t tag, int16_t value, uint8_t *buffer, size_t max_len);
int tlv_encode_bool(uint8_t tag, bool value, uint8_t *buffer, size_t max_len);
int tlv_decode(const uint8_t *buffer, size_t len, tlv_element_t *element);
```

**Dependencies**:
- Standard C library only

**Size estimate**: ~1500 LOC, ~15KB binary

---

### Layer 3: Security (PASE/CASE)

**Purpose**: Secure session establishment and message encryption

**Components**:
- `pase.c`: Password-Authenticated Session Establishment (commissioning)
- `case.c`: Certificate-Authenticated Session Establishment (operational)
- `session_mgr.c`: Session key management and message encryption/decryption

**PASE Flow (Commissioning)**:
1. Controller sends PBKDFParamRequest
2. Device responds with PBKDFParamResponse (iterations, salt)
3. Both derive session key using SPAKE2+ protocol + setup PIN
4. Exchange encrypted messages using derived key

**CASE Flow (Operational)**:
1. Controller sends Sigma1 (initiator random, public key)
2. Device sends Sigma2 (responder random, public key, encrypted credentials)
3. Controller sends Sigma3 (encrypted credentials)
4. Both derive session key using ECDH + credentials
5. Subsequent messages encrypted with session key

**Simplified Approach**:
- Implement PASE only (sufficient for basic commissioning)
- Store single fabric credentials in flash
- Use mbedTLS for:
  - SPAKE2+ (PASE key exchange)
  - AES-128-CCM (message encryption)
  - SHA-256 (hashing)
  - HKDF (key derivation)

**API**:
```c
// PASE
int pase_init(const char *setup_pin);
int pase_handle_pbkdf_request(const uint8_t *request, size_t len, uint8_t *response, size_t max_len, size_t *actual_len);
int pase_derive_session_key(uint8_t session_id, uint8_t *key_out);

// Session management
int session_create(uint8_t session_id, const uint8_t *key, size_t key_len);
int session_encrypt(uint8_t session_id, const uint8_t *plaintext, size_t len, uint8_t *ciphertext, size_t max_len);
int session_decrypt(uint8_t session_id, const uint8_t *ciphertext, size_t len, uint8_t *plaintext, size_t max_len);
void session_destroy(uint8_t session_id);
```

**Dependencies**:
- mbedTLS (already integrated via crypto_adapter.cpp)
- Pico SDK hardware_flash (already used by storage_adapter.cpp)

**Size estimate**: ~2000 LOC, ~20KB binary

---

### Layer 4: Interaction Model

**Purpose**: Handle Matter interaction protocol (read, write, subscribe, invoke)

**For a read-only sensor device, we only need**:
- **ReadRequest**: Controller reads attribute value
- **SubscribeRequest**: Controller subscribes to attribute changes
- **ReportData**: Device reports attribute changes to subscribers

**Components**:
- `read_handler.c`: Process ReadRequest, send ReadResponse
- `subscribe_handler.c`: Process SubscribeRequest, manage subscriptions, send ReportData

**Read Flow**:
1. Controller sends ReadRequest (cluster ID, attribute ID, endpoint)
2. Device looks up attribute value
3. Device sends ReadResponse with TLV-encoded value

**Subscribe Flow**:
1. Controller sends SubscribeRequest (cluster ID, attribute ID, min/max intervals)
2. Device stores subscription info
3. When attribute changes, device sends ReportData to controller

**API**:
```c
int interaction_handle_read_request(const uint8_t *request, size_t len, uint8_t *response, size_t max_len, size_t *actual_len);
int interaction_handle_subscribe_request(const uint8_t *request, size_t len, uint8_t *response, size_t max_len, size_t *actual_len);
int interaction_send_report(uint8_t endpoint, uint32_t cluster_id, uint32_t attr_id, const matter_attr_value_t *value);
void interaction_process_subscriptions(void); // Called periodically
```

**Dependencies**:
- TLV codec (layer 2)
- Matter attributes system (already exists in your platform/pico_w_chip_port/matter_attributes.cpp)

**Size estimate**: ~1200 LOC, ~12KB binary

---

### Layer 5: Clusters

**Purpose**: Implement Matter cluster specifications

**We need 4 clusters**:

#### 1. Descriptor Cluster (0x001D) - REQUIRED
Every Matter device must implement this on endpoint 0.

**Attributes**:
- DeviceTypeList: List of device types (0x0302 = Temperature Sensor)
- ServerList: List of server clusters on endpoint
- ClientList: List of client clusters (empty for sensor)
- PartsList: List of endpoints (just endpoint 1)

#### 2. OnOff Cluster (0x0006)
**Attributes**:
- OnOff (0x0000, bool): Flame detected state

#### 3. LevelControl Cluster (0x0008)
**Attributes**:
- CurrentLevel (0x0000, uint8): Fan speed 0-100%

#### 4. TemperatureMeasurement Cluster (0x0402)
**Attributes**:
- MeasuredValue (0x0000, int16): Temperature in centidegrees (e.g., 2500 = 25.00°C)
- MinMeasuredValue (0x0001, int16): Minimum temp (optional, use 0)
- MaxMeasuredValue (0x0002, int16): Maximum temp (optional, use 10000 = 100°C)

**Implementation Strategy**:
- Each cluster is a simple module with read handlers
- Links to existing `matter_attributes` system (already stores values)
- No write handlers needed (read-only device)

**API**:
```c
int cluster_onoff_read(uint8_t endpoint, uint32_t attr_id, matter_attr_value_t *value);
int cluster_level_read(uint8_t endpoint, uint32_t attr_id, matter_attr_value_t *value);
int cluster_temperature_read(uint8_t endpoint, uint32_t attr_id, matter_attr_value_t *value);
int cluster_descriptor_read(uint8_t endpoint, uint32_t attr_id, matter_attr_value_t *value);
```

**Dependencies**:
- Matter attributes system (already exists)
- TLV codec for encoding list/structure types (Descriptor cluster)

**Size estimate**: ~1000 LOC, ~10KB binary

---

### Layer 6: Commissioning

**Purpose**: Add device to Matter fabric (network)

**Components**:
- `network_commissioning.c`: Network commissioning cluster (0x0031)

**Commissioning Flow (Simplified)**:
1. Controller discovers device via mDNS (service type `_matterc._udp`)
2. Controller establishes PASE session using setup PIN
3. Controller sends AddOrUpdateWiFiNetwork command (SSID, password)
4. Device connects to WiFi (already handled by network_adapter.cpp)
5. Controller sends CommissioningComplete command
6. Device stores fabric credentials in flash
7. Device reboots and is now commissioned

**Simplified Approach**:
- Skip mDNS discovery (use static IP or manual discovery)
- WiFi credentials hardcoded (already done in network_adapter.cpp)
- Minimal fabric storage (just node ID and root public key)

**API**:
```c
int commissioning_init(void);
int commissioning_handle_add_wifi(const uint8_t *ssid, size_t ssid_len, const uint8_t *password, size_t pass_len);
int commissioning_handle_complete(const uint8_t *fabric_data, size_t len);
bool commissioning_is_commissioned(void);
```

**Dependencies**:
- Storage adapter (already exists)
- Network adapter (already exists)

**Size estimate**: ~800 LOC, ~8KB binary

---

## Integration with Existing Code

### Keep These Files (Already Good)
- ✅ `platform/pico_w_chip_port/network_adapter.cpp` - WiFi/lwIP integration
- ✅ `platform/pico_w_chip_port/storage_adapter.cpp` - Flash storage
- ✅ `platform/pico_w_chip_port/crypto_adapter.cpp` - mbedTLS wrappers
- ✅ `platform/pico_w_chip_port/matter_attributes.cpp` - Attribute storage
- ✅ `platform/pico_w_chip_port/platform_manager.cpp` - Orchestration

### Removed Dependencies (Phase 7 Complete)
- ✅ `third_party/connectedhomeip/` - Successfully removed, no longer needed
- ✅ All CHIP SDK references removed from CMakeLists.txt

### Add New Directory Structure
```
src/matter_minimal/
├── transport/
│   └── udp_transport.c
├── codec/
│   ├── message_codec.c
│   └── tlv.c
├── security/
│   ├── pase.c
│   └── session_mgr.c
├── interaction/
│   ├── read_handler.c
│   └── subscribe_handler.c
├── clusters/
│   ├── descriptor.c
│   ├── onoff.c
│   ├── level_control.c
│   └── temperature.c
├── commissioning/
│   └── network_commissioning.c
└── matter_protocol.c (main coordinator)
```

### Modified Files
- `src/main.c`: Call `matter_protocol_init()` instead of old platform manager
- `CMakeLists.txt`: Link new matter_minimal library

---

## Data Flow

### Startup Sequence
```
1. main.c calls matter_protocol_init()
2. matter_protocol_init() calls:
   - network_adapter_init()        [existing]
   - storage_adapter_init()        [existing]
   - crypto_adapter_init()         [existing]
   - matter_attributes_init()      [existing]
   - matter_transport_init()       [NEW]
   - pase_init(setup_pin)          [NEW]
   - commissioning_init()          [NEW]
3. main.c enters main loop
4. main loop calls:
   - serial_handler_read()         [existing]
   - viking_bio_parse_data()       [existing]
   - matter_attributes_update()    [existing]
   - matter_protocol_task()        [NEW - processes incoming Matter messages]
```

### Message Processing (Main Loop)
```
matter_protocol_task() {
  // Check for incoming UDP packets
  if (matter_transport_receive(buffer, &len) > 0) {
    // Decode Matter message
    matter_message_decode(buffer, len, &msg);
    
    // Decrypt if secured session
    if (msg.session_id != 0) {
      session_decrypt(msg.session_id, msg.payload, ...);
    }
    
    // Route to handler
    if (msg.protocol_id == PROTOCOL_INTERACTION_MODEL) {
      if (msg.opcode == OP_READ_REQUEST) {
        interaction_handle_read_request(msg.payload, ...);
      }
      else if (msg.opcode == OP_SUBSCRIBE_REQUEST) {
        interaction_handle_subscribe_request(msg.payload, ...);
      }
    }
    else if (msg.protocol_id == PROTOCOL_SECURE_CHANNEL) {
      if (msg.opcode == OP_PASE_PAKE1) {
        pase_handle_pake1(msg.payload, ...);
      }
    }
  }
  
  // Process attribute subscriptions
  interaction_process_subscriptions();
}
```

### Attribute Update (When Viking Bio Data Changes)
```c
// In main.c, after parsing serial data:
matter_attributes_update(endpoint, cluster_id, attr_id, &new_value);

// matter_attributes_update() [already exists] now triggers:
interaction_send_report(endpoint, cluster_id, attr_id, &new_value);

// interaction_send_report() [NEW]:
- Finds active subscriptions for this attribute
- Encodes ReportData message with new value (TLV)
- Encrypts message if secured session
- Sends via matter_transport_send()
```

---

## Implementation Phases

### Phase 1: Foundation (Week 1)
- **TLV codec** (`codec/tlv.c`)
  - Encoder for basic types (uint8, int16, bool)
  - Decoder for basic types
  - Unit tests (can run on host PC)
- **Message codec** (`codec/message_codec.c`)
  - Encode/decode Matter message header
  - Unsecured messages only (no encryption yet)
- **UDP transport** (`transport/udp_transport.c`)
  - Bind to port 5540
  - Send/receive raw UDP packets
  - Integration with lwIP

**Deliverable**: Can send/receive unsecured Matter messages

### Phase 2: Read Attributes (Week 2)
- **Clusters** (`clusters/onoff.c`, `level_control.c`, `temperature.c`, `descriptor.c`)
  - Link to existing matter_attributes system
  - Read handlers return TLV-encoded values
- **Interaction model** (`interaction/read_handler.c`)
  - Parse ReadRequest (TLV)
  - Call cluster read handler
  - Encode ReadResponse (TLV)
  - Send response via transport

**Deliverable**: Can respond to Matter read requests (unsecured)

### Phase 3: Security (Week 3)
- **PASE implementation** (`security/pase.c`)
  - SPAKE2+ using mbedTLS
  - PBKDF2 for PIN verification
  - Session key derivation
- **Session management** (`security/session_mgr.c`)
  - AES-128-CCM encryption/decryption
  - Session storage (up to 5 sessions)
- **Secure transport**
  - Encrypt outgoing messages
  - Decrypt incoming messages

**Deliverable**: Can establish PASE session and read attributes securely

### Phase 4: Commissioning (Week 4)
- **Network commissioning cluster** (`commissioning/network_commissioning.c`)
  - Handle AddOrUpdateWiFiNetwork
  - Handle CommissioningComplete
  - Store fabric info in flash
- **Commissioning flow**
  - QR code generation (already have in platform_manager)
  - PIN validation (already have in platform_manager)

**Deliverable**: Can commission device into Matter fabric

### Phase 5: Subscriptions (Week 5)
- **Subscribe handler** (`interaction/subscribe_handler.c`)
  - Parse SubscribeRequest
  - Store subscription info
  - Generate ReportData on attribute changes
- **Integration**
  - matter_attributes_update() triggers reports
  - Subscription expiry/renewal

**Deliverable**: Controller can subscribe to attribute changes

### Phase 6: Polish & Testing (Week 6)
- Error handling
- Logging and diagnostics
- Integration testing with chip-tool
- Documentation updates
- Performance optimization

---

## Size Budget

| Component | LOC | Binary Size (est) |
|-----------|-----|-------------------|
| UDP transport | 500 | 8 KB |
| Message codec | 600 | 10 KB |
| TLV codec | 900 | 12 KB |
| PASE | 1200 | 15 KB |
| Session mgr | 800 | 10 KB |
| Read handler | 600 | 8 KB |
| Subscribe handler | 600 | 8 KB |
| Clusters (4x) | 1000 | 10 KB |
| Commissioning | 800 | 8 KB |
| Protocol coordinator | 400 | 5 KB |
| **Total** | **7400** | **~94 KB** |

**Current firmware**: 717 KB  
**Target firmware**: 811 KB  
**Flash capacity**: 2048 KB  
**Margin**: 1237 KB (60% free)

✅ **Comfortably fits within Pico W flash limits**

---

## Testing Strategy

### Unit Tests (Host PC)
- TLV encoder/decoder
- Message codec
- Cluster read logic
- PASE key derivation (test vectors from Matter spec)

### Integration Tests (Hardware)
- WiFi connectivity
- UDP send/receive
- Attribute read (unsecured)
- Attribute read (secured)
- Commissioning flow
- Subscription reporting

### Tools
- **chip-tool**: Official Matter CLI controller for commissioning and testing
- **Wireshark**: Packet capture and analysis (Matter dissector plugin)
- **Python scripts**: Custom test clients using socket library

---

## Risks & Mitigations

### Risk 1: PASE Implementation Complexity
**Mitigation**: Use mbedTLS SPAKE2+ implementation or port from ESP-Matter (BSD license)

### Risk 2: TLV Edge Cases
**Mitigation**: Test with official Matter test vectors from spec

### Risk 3: Memory Constraints (264 KB RAM)
**Mitigation**: 
- Limit to 5 concurrent sessions
- Use static buffers (no dynamic allocation)
- Monitor stack usage

### Risk 4: Interoperability Issues
**Mitigation**: Test with multiple controllers (chip-tool, Apple Home, Google Home)

### Risk 5: Matter Spec Changes
**Mitigation**: Target Matter 1.0 specification (stable, widely adopted)

---

## References

- [Matter Core Specification v1.0](https://csa-iot.org/developer-resource/specifications-download-request/)
- [Project CHIP GitHub](https://github.com/project-chip/connectedhomeip) - reference implementation
- [ESP-Matter](https://github.com/espressif/esp-matter) - minimal implementation example
- [lwIP Documentation](https://www.nongnu.org/lwip/2_1_x/index.html)
- [mbedTLS API Reference](https://mbed-tls.readthedocs.io/)

---

## Success Criteria

A successful minimal Matter implementation will:

1. ✅ Build without connectedhomeip SDK dependency
2. ✅ Firmware size < 850 KB
3. ✅ Commission via chip-tool using QR code + PIN
4. ✅ Read flame, speed, temperature attributes
5. ✅ Subscribe to attribute changes
6. ✅ Receive real-time updates when Viking Bio data changes
7. ✅ Survive reboot (persist fabric credentials)
8. ✅ Work with at least 2 Matter controllers (chip-tool + 1 other)

---

**Last Updated**: 2026-02-12 19:32:03  
**Status**: Architecture Design  
**Next Step**: Phase 1 - TLV Codec Implementation
