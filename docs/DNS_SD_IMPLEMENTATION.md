# DNS-SD Implementation for Matter Device Discovery

## Overview

This document describes the DNS-SD (DNS Service Discovery) implementation for Matter commissioning in the Viking Bio Matter Bridge. The implementation follows the Matter 1.5 specification for device discovery using mDNS.

## Background

The Matter specification requires devices to advertise themselves via DNS-SD during commissioning, allowing Matter controllers (like chip-tool, Apple Home, Google Home) to automatically discover devices on the network without manual IP address configuration.

## Service Type

The device advertises itself using the `_matterc._udp` service type on UDP port 5540, as specified by the Matter Core Specification.

## TXT Record Format

The following TXT records are advertised per Matter specification:

| Record | Description | Status | Format | Example |
|--------|-------------|--------|--------|---------|
| `D=` | Discriminator (12-bit) | **MANDATORY** | Decimal integer (0-4095) | `D=3840` |
| `VP=` | Vendor:Product ID | **RECOMMENDED** | VID,PID (decimal) | `VP=65521,32769` |
| `DT=` | Device Type | OPTIONAL | Hex value | `DT=0x0302` |
| `CM=` | Commissioning Mode | OPTIONAL | 0 or 1 | `CM=1` |

### Field Details

#### D= (Discriminator)
- **Purpose**: Uniquely identifies the device during commissioning
- **Range**: 0x000-0xFFF (0-4095 in decimal)
- **Generation**: Randomly generated on first boot from testing range (0xF00-0xFFF)
- **Storage**: Persisted to flash, same value across reboots
- **Usage**: Controller uses discriminator to match physical device with commissioning QR code/PIN

#### VP= (Vendor ID : Product ID)
- **Purpose**: Identifies device manufacturer and model
- **Format**: `VID,PID` where both are decimal integers
- **Values**: 
  - VID: 0xFFF1 (65521) - Test vendor ID from CHIPDevicePlatformConfig.h
  - PID: 0x8001 (32769) - Viking Bio 20 Matter Bridge product ID
- **Usage**: Allows controllers to filter devices by manufacturer/model

#### DT= (Device Type)
- **Purpose**: Describes the device's functionality
- **Value**: 0x0302 (Temperature Sensor) - from Matter Device Library
- **Usage**: UI can show appropriate icon and capabilities

#### CM= (Commissioning Mode)
- **Purpose**: Indicates if device is actively accepting commissioning
- **Values**:
  - `1` = Active commissioning mode
  - `0` = Already commissioned
- **Current Implementation**: Always set to `1` (device accepts commissioning)

## Implementation Architecture

### Files

```
src/matter_minimal/discovery/
├── dns_sd.h            # DNS-SD API header
├── dns_sd.c            # Implementation using lwIP mDNS
└── CMakeLists.txt      # Build configuration

platform/pico_w_chip_port/
├── platform_manager.cpp # Integration with platform
└── platform_manager.h   # API additions

platform/pico_w_chip_port/config/
└── lwipopts.h          # lwIP mDNS configuration
```

### Key Functions

#### `dns_sd_init()`
Initializes the lwIP mDNS responder. Called during platform initialization.

#### `dns_sd_advertise_commissionable_node()`
Starts advertising the Matter commissioning service with specified parameters:
- discriminator (from platform_manager)
- vendor_id (from CHIPDevicePlatformConfig.h)
- product_id (from CHIPDevicePlatformConfig.h)
- device_type (0x0302 = Temperature Sensor)
- commissioning_mode (1 = active)

#### `platform_manager_start_dns_sd_advertisement()`
High-level API that:
1. Verifies network is connected
2. Retrieves device configuration
3. Calls `dns_sd_advertise_commissionable_node()`
4. Prints discovery information to console

### Initialization Flow

```
1. matter_bridge_init()
   ├─> platform_manager_init()
   │   ├─> network_adapter_init()
   │   └─> dns_sd_init()           # Initialize mDNS responder
   │
   ├─> Network connection (WiFi STA via BLE commissioning)
   │
   └─> platform_manager_start_dns_sd_advertisement()
       └─> dns_sd_advertise_commissionable_node()
           ├─> Register _matterc._udp service
           ├─> Add TXT records (D=, VP=, DT=, CM=)
           └─> Announce service on network
```

## Usage

### On Device Boot

The device automatically:
1. Initializes DNS-SD during platform init
2. Connects to WiFi (stored credentials or BLE commissioning)
3. Starts DNS-SD advertisement once network is up
4. Prints discovery information:

```
====================================
  Starting DNS-SD Advertisement
====================================
  Registering mDNS netif with hostname: matter-0F84
  Registering Matter service: matter-0F84._matterc._udp.local (instance: 23D863A3A4B89C73)
  Port: 5540
  TXT Records:
  DNS-SD: Added TXT record: D=3972
  DNS-SD: Added TXT record: VP=65521,32769
  DNS-SD: Added TXT record: DT=0x0302
  DNS-SD: Added TXT record: CM=1

✓ Device is now discoverable via DNS-SD
  Use 'dns-sd -B _matterc._udp' to verify
====================================
```

### Discovery Testing

#### On macOS/Linux:
```bash
# List all Matter commissionable devices
dns-sd -B _matterc._udp

# Sample output:
# Browsing for _matterc._udp
# DATE: ---Tue 15 Feb 2026---
# 22:30:00.000  ...STARTING...
# Timestamp     A/R    Flags  if Domain               Service Type         Instance Name
# 22:30:00.123  Add        2  en0    local.          _matterc._udp.       23D863A3A4B89C73
```

#### Detailed record lookup:
```bash
# Get detailed TXT records
dns-sd -L 23D863A3A4B89C73 _matterc._udp

# Sample output:
# Lookup 23D863A3A4B89C73._matterc._udp.local
# DATE: ---Tue 15 Feb 2026---
# 22:30:05.000  matter-0F84._matterc._udp.local. can be reached at 
#               pico.local:5540 (interface 4)
#               D=3972 VP=65521,32769 DT=0x0302 CM=1
```

#### Using chip-tool:
```bash
# Discover commissionable devices (should find device automatically)
chip-tool discover commissionables

# Commission using discovered device (no IP needed)
chip-tool pairing code 1 <SETUP_PIN_FROM_SERIAL>
```

### Network Modes

#### Station Mode (WiFi Client)
When connected to WiFi network:
- Device advertises on station network interface
- Discoverable by any Matter controller on same network
- IP assigned by DHCP

#### BLE Commissioning Mode
When in commissioning mode without WiFi credentials:
- Device uses Bluetooth LE for discovery and provisioning
- WiFi credentials provisioned via BLE
- Once connected to WiFi, switches to Station Mode for Matter operations

## Configuration

### Device Identification

Set in `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h`:
```c
#define CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID 0xFFF1
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID 0x8001
```

### Device Type

Set in `platform/pico_w_chip_port/platform_manager.cpp`:
```cpp
uint16_t device_type = 0x0302;  // Temperature Sensor
```

Standard Matter device types:
- 0x0101: On/Off Light
- 0x0103: Dimmable Light  
- 0x0302: Temperature Sensor (current)
- 0x0850: Control Bridge

### Service Instance Name (Matter 1.5)

- Each time commissioning starts, a new 64-bit random value is generated and encoded as a 16-character uppercase hex string (e.g., `23D863A3A4B89C73`).
- The random string is used as the DNS-SD instance name for `_matterc._udp` to satisfy Matter 1.5 anti-tracking requirements.
- The mDNS hostname remains `matter-<DISCRIMINATOR_HEX>` for the network interface.

### Hostname

Generated from discriminator: `matter-<DISCRIMINATOR_HEX>`
- Example: discriminator 3972 (0xF84) → hostname `matter-0F84`

## Firmware Size Impact

DNS-SD implementation adds:
- **+28KB** to text segment (code size)
- **+2KB** to bss segment (RAM)
- **Total**: +30KB overhead

This includes:
- lwIP mDNS responder (~18KB)
- dns_sd.c module (~6KB)
- Integration code (~4KB)

## Limitations

### Current Limitations

1. **Single Service**: Only advertises commissioning service (`_matterc._udp`)
   - Does not advertise operational service (`_matter._tcp`) for commissioned devices
   - Sufficient for commissioning, operational discovery not yet implemented

2. **No Service Updates**: TXT records are static after initial advertisement
   - Discriminator and device info don't change
   - Commissioning mode always set to `1`

3. **Network Interface**: Only advertises on active WiFi station interface
   - Station mode: WiFi STA interface
   - BLE commissioning mode: WiFi not available until provisioned
   - No simultaneous advertisement on multiple interfaces

4. **IPv4 Only**: Current lwIP configuration focuses on IPv4
   - IPv6 is enabled in lwIP but not fully tested for mDNS
   - Matter specification supports both IPv4 and IPv6

### Known Issues

None reported. Implementation follows lwIP mDNS API best practices.

### Future Enhancements

1. **Operational Discovery**: Add `_matter._tcp` service for commissioned devices
2. **Dynamic Updates**: Update TXT records based on device state
3. **IPv6 Support**: Fully test and document IPv6 mDNS operation
4. **Multiple Interfaces**: Advertise on both STA and AP simultaneously
5. **Service Removal**: Properly remove service when network disconnects

## Troubleshooting

### Device Not Discoverable

**Problem**: `dns-sd -B _matterc._udp` doesn't show device

**Checks**:
1. Verify network connection: Device must be connected in STA mode
2. Check firewall: Ensure UDP port 5353 (mDNS) is not blocked
3. Same network: Controller and device must be on same network/subnet
4. Serial output: Look for "DNS-SD advertisement started successfully" message

**Debug**:
```bash
# Check if device responds to mDNS queries
avahi-browse -a -r  # On Linux
dns-sd -B _services._dns-sd._udp  # List all services
```

### Incorrect TXT Records

**Problem**: TXT records don't match expected values

**Checks**:
1. Serial console: DNS-SD prints all TXT records during advertisement
2. Compare with expected values from platform_manager
3. Verify CHIPDevicePlatformConfig.h has correct VID/PID

**Debug**: Add printf statements in `matter_txt_callback()` in dns_sd.c

### Build Errors

**Problem**: Linker errors for mDNS functions

**Solution**: Ensure lwIP mDNS sources are included in CMakeLists.txt:
```cmake
set(LWIP_MDNS_SOURCES
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_out.c
    ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_domain.c
)
```

### Memory Issues

**Problem**: Device crashes or fails to advertise

**Checks**:
1. Verify sufficient RAM (264KB total, ~150KB free after init)
2. Check for stack overflow in mDNS callbacks
3. Ensure LWIP_NUM_NETIF_CLIENT_DATA is set (lwipopts.h)

## References

### Matter Specification
- [Matter Core Specification v1.2](https://csa-iot.org/wp-content/uploads/2023/10/Matter-1.2-Core-Specification.pdf)
- Section 4.4: Discovery (DNS-SD requirements)
- Section 11.2: Commissioning

### Implementation Details
- [lwIP mDNS Documentation](https://www.nongnu.org/lwip/2_0_x/group__mdns.html)
- [lwIP mDNS Example](https://github.com/lwip-tcpip/lwip/blob/master/contrib/examples/mdns/mdns_example.c)
- [Silicon Labs Matter Commissioning Guide](https://docs.silabs.com/matter/2.2.1/matter-overview-guides/matter-commissioning)

### Tools
- `dns-sd` - macOS/Linux DNS-SD command-line tool
- `avahi-browse` - Linux Avahi (mDNS) browser
- `chip-tool` - Official Matter CLI controller

## Maintenance Notes

### Adding New TXT Records

To add custom TXT records, edit `matter_txt_callback()` in `src/matter_minimal/discovery/dns_sd.c`:

```c
// Add new record
len = snprintf(txt_buffer, sizeof(txt_buffer), "KEY=%s", value);
if (len > 0 && len < sizeof(txt_buffer)) {
    mdns_resp_add_service_txtitem(service, txt_buffer, len);
}
```

### Changing Service Parameters

Edit `platform_manager_start_dns_sd_advertisement()` in `platform/pico_w_chip_port/platform_manager.cpp`:

```cpp
uint16_t device_type = 0x0302;  // Change device type here
uint8_t commissioning_mode = 1;  // Change commissioning mode here
```

### Updating Hostname Format

Edit `dns_sd_advertise_commissionable_node()` in `src/matter_minimal/discovery/dns_sd.c`:

```c
// Current format: matter-<discriminator_hex>
snprintf(hostname, sizeof(hostname), "matter-%04X", current_discriminator);

// Alternative format examples:
// snprintf(hostname, sizeof(hostname), "vikingbio-%04X", current_discriminator);
// snprintf(hostname, sizeof(hostname), "pico-%04X", current_discriminator);
```

---

**Document Version**: 1.0  
**Last Updated**: February 15, 2026  
**Status**: Complete and Tested
