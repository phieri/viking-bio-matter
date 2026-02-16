# Pico W Matter (CHIP) Platform Port

This directory contains the Matter platform port for Raspberry Pi Pico W. It provides platform-specific implementations for networking, storage, and cryptography to support the minimal Matter protocol implementation.

## Overview

This platform port enables native Matter protocol support on the Pico W, exposing Viking Bio 20 burner data as Matter-compliant device attributes over WiFi.

## Architecture

The port consists of several adapter layers:

### 1. Network Adapter (`network_adapter.cpp`)
- Interfaces with CYW43439 WiFi chip on Pico W
- Uses pico_cyw43_arch and lwIP stack
- Provides WiFi station mode connectivity
- Manages network state and IP configuration

### 2. Storage Adapter (`storage_adapter.cpp`)
- Persistent storage for Matter fabric commissioning data
- Uses LittleFS on Pico's internal flash memory (last 256KB reserved)
- Provides wear leveling for extended flash memory lifespan
- Key-value store implementation with file-based storage
- Stores pairing info, access control lists, WiFi credentials, etc.

### 3. Crypto Adapter (`crypto_adapter.cpp`)
- Cryptographic operations using mbedTLS
- Random number generation with hardware entropy
- SHA-256, AES encryption/decryption
- Uses Pico's unique board ID for entropy seeding

### 4. Platform Manager (`platform_manager.cpp`)
- Coordinates all platform components
- Initializes adapters in proper order
- Provides high-level platform API
- Handles commissioning info display

## Building with Matter Support

### Prerequisites

1. **Pico SDK** (version 2.2.0 or later)
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

2. **ARM GCC Toolchain**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi
   
   # macOS
   brew install arm-none-eabi-gcc
   ```

**Note**: Pico SDK 2.2.0 includes mbedTLS 3.6.2, which requires compatibility flags defined in the project's mbedtls_config.h.

### Build Configuration

Build the firmware (Matter support is always enabled):

```bash
mkdir build
cd build
cmake ..
make
```

This produces `viking_bio_matter.uf2` with full Matter support for Pico W.

### WiFi Configuration

WiFi credentials are provisioned via **Matter BLE commissioning** and stored in flash.

- Recommended: commission with `chip-tool pairing ble-wifi ...`
- Optional: use platform storage APIs to pre-store credentials for unattended bootstrapping

## Flashing the Firmware

1. Hold BOOTSEL button on Pico W while connecting USB
2. Copy `build/viking_bio_matter.uf2` to the mounted drive
3. Pico will reboot automatically

## Commissioning

### 1. View Commissioning Info

Connect to Pico's USB serial port:

```bash
screen /dev/ttyACM0 115200
# Or use Thonny IDE (Tools > Serial)
```

On boot, you'll see commissioning details:

```
====================================
    Matter Commissioning Info
====================================
Device MAC:     28:CD:C1:00:00:01
Setup PIN Code: 24890840
Discriminator:  3912 (0xF48)

⚠️  IMPORTANT:
   PIN is derived from device MAC.
   Use tools/derive_pin.py to compute
   the PIN from the MAC address above.

⚠️  NOTE: Discriminator was randomly
   generated on first boot and saved
   to flash. Value is in testing range.
====================================
```

**Note**: The Setup PIN is deterministically derived from the device MAC address using SHA-256 hashing with the product salt `VIKINGBIO-2026`. This allows you to compute the PIN offline from a printed MAC address without needing device storage.

To derive the PIN from a MAC address:

```bash
# From repository root
python3 tools/derive_pin.py 28:CD:C1:00:00:01

# Output:
# Device MAC:     28:CD:C1:00:00:01
# Setup PIN Code: 24890840
```

**Python one-liner equivalent**:
```python
import hashlib
mac = bytes.fromhex("28CDC1000001")  # MAC without separators
salt = b"VIKINGBIO-2026"
hash_val = int.from_bytes(hashlib.sha256(mac + salt).digest()[:4], 'big')
pin = f"{hash_val % 100000000:08d}"
print(f"PIN: {pin}")
```

### 2. Commission with chip-tool

Install chip-tool from the Matter SDK or use your Matter controller:

```bash
# Option A: Install from package manager (if available)
sudo apt-get install chip-tool

# Option B: Build from source
git clone --depth 1 --branch v1.3-branch https://github.com/project-chip/connectedhomeip.git
cd connectedhomeip
./scripts/examples/gn_build_example.sh examples/chip-tool out/host
```

Commission the device using the **derived PIN** from the serial console:

```bash
# Replace 24890840 with the actual PIN from your device's serial output
./out/host/chip-tool pairing ble-wifi 1 MySSID MyPassword 3840 24890840
```

**Important**: The PIN changes per device (derived from MAC), so always check the serial console for the correct PIN.

### 3. Verify Commissioning

Read device attributes:

```bash
# Check OnOff cluster (flame status)
./out/host/chip-tool onoff read on-off 1 1

# Check LevelControl cluster (fan speed)
./out/host/chip-tool levelcontrol read current-level 1 1

# Check TemperatureMeasurement cluster
./out/host/chip-tool temperaturemeasurement read measured-value 1 1
```

## Matter Clusters

The device exposes three clusters:

### OnOff Cluster (0x0006, Endpoint 1)
- **Attribute**: OnOff (bool)
- **Maps to**: Flame detected state
- **Updates**: When flame state changes

### LevelControl Cluster (0x0008, Endpoint 1)
- **Attribute**: CurrentLevel (uint8, 0-100)
- **Maps to**: Fan speed percentage
- **Updates**: When fan speed changes

### TemperatureMeasurement Cluster (0x0402, Endpoint 1)
- **Attribute**: MeasuredValue (int16, centidegrees)
- **Maps to**: Burner temperature × 100
- **Updates**: When temperature changes

## Testing

### With Hardware

1. Connect Viking Bio 20 serial output to Pico GP1 (RX)
2. Flash Matter-enabled firmware
3. Monitor serial output for commissioning info
4. Commission device with chip-tool
5. Verify attribute updates when burner state changes

### With Simulator

Use the included simulator for testing without hardware:

```bash
# Run simulator (sends data to /dev/ttyUSB0)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0

# In another terminal, monitor Matter attributes
watch -n 1 './out/host/chip-tool onoff read on-off 1 1'
```

## Configuration

### Device Information

Edit `CHIPDevicePlatformConfig.h` to customize:

- **Vendor ID**: `CHIP_DEVICE_CONFIG_DEVICE_VENDOR_ID`
- **Product ID**: `CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_ID`
- **Device Name**: `CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME`

### Setup Credentials

⚠️ **SECURITY WARNING**: Default credentials are for testing only!

**Current implementation:**

1. **Discriminator**: Automatically randomized on first boot from testing range (0xF00-0xFFF) and persisted to flash storage. Each device gets a unique value.
2. **Setup PIN**: Unique per device, derived from MAC address using SHA-256 with product salt `VIKINGBIO-2026`. See `tools/derive_pin.py` for offline computation.

**For production:**

- The discriminator range 0xF00-0xFFF (3840-4095) is commonly used for testing
- To change the discriminator range, modify `DISCRIMINATOR_TEST_MIN` and `DISCRIMINATOR_TEST_MAX` in `platform_manager.cpp`
- The discriminator is stored in flash at key `/discriminator` and persists across reboots
- To force regeneration, delete the flash storage or call `storage_adapter_clear_discriminator()`
```

### Storage Configuration

Flash storage layout with LittleFS:

- **Location**: Last 256KB of Pico's 2MB flash
- **Size**: 256KB (configurable in `storage_adapter.cpp`)
- **Purpose**: Fabric commissioning data, ACLs, credentials
- **Filesystem**: LittleFS with automatic wear leveling
- **Features**:
  - Automatic wear leveling to extend flash lifespan
  - Power-loss resilient operations
  - Efficient space utilization
  - Thread-safe operations (when compiled with LFS_THREADSAFE=1)

Adjust storage size if needed:

```cpp
#define STORAGE_FLASH_OFFSET (PICO_FLASH_SIZE_BYTES - (256 * 1024))
#define STORAGE_FLASH_SIZE (256 * 1024)
```

**LittleFS Integration:**
- Uses [pico-lfs](https://github.com/tjko/pico-lfs) library
- Automatic formatting on first use
- Files stored with `/` prefix for LittleFS compatibility
- See `tests/storage/README.md` for testing procedures

## Limitations

- **No OTA support**: Firmware updates require physical USB access
- **Single fabric**: Limited to 5 fabrics max due to memory constraints
- **No Thread**: WiFi only (CYW43439 limitation)
- **No BLE commissioning**: Uses WiFi commissioning only
- **Test credentials**: Ships with test setup codes

## Troubleshooting

### WiFi Connection Fails

```
ERROR: Failed to connect to WiFi (error -1)
```

**Solutions:**
- Verify SSID and password in `network_adapter.cpp`
- Check WiFi is 2.4GHz (5GHz not supported)
- Ensure WPA2 security (WPA3 may not work)
- Check router MAC filtering

### Commissioning Timeout

**Solutions:**
- Ensure Pico and commissioning device on same network
- Check firewall isn't blocking mDNS/UDP port 5540
- Verify commissioning info matches (PIN, discriminator)
- Try manual pairing code instead of QR

### Flash Storage Errors

```
ERROR: Storage area exceeds flash size
```

**Solutions:**
- Reduce `STORAGE_FLASH_SIZE` in `storage_adapter.cpp`
- Ensure firmware + storage fit in 2MB flash
- Check `STORAGE_FLASH_OFFSET` calculation

### Build Errors

**PICO_SDK_PATH not set**

Solution: Export the PICO_SDK_PATH environment variable:
```bash
export PICO_SDK_PATH=/path/to/pico-sdk
```

## Development

### Adding Custom Clusters

1. Define cluster in `matter_bridge.c` (or create new file)
2. Register endpoints during initialization
3. Implement attribute getters/setters
4. Call update functions when data changes

### Debugging

Enable verbose logging:

```cpp
// In platform files
#define DEBUG_NETWORK 1
#define DEBUG_STORAGE 1
#define DEBUG_CRYPTO 1
```

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Pico W Documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html)
- [CYW43439 WiFi Chip](https://www.infineon.com/cms/en/product/wireless-connectivity/airoc-wi-fi-plus-bluetooth-combos/cyw43439/)

## License

This platform port is provided under the same license as the parent project (MIT License).
