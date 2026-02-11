# Implementation Summary: Native Matter for Pico W

This document summarizes the implementation of native Matter (connectedhomeip) support for the Viking Bio 20 burner bridge on Raspberry Pi Pico W.

## What Was Implemented

### 1. Matter Platform Port

Created a complete Matter platform port at `platform/pico_w_chip_port/` with:

- **Network Adapter** (`network_adapter.cpp`): Integrates CYW43439 WiFi chip with lwIP stack
- **Storage Adapter** (`storage_adapter.cpp`): Flash-based persistent storage for fabric commissioning data
- **Crypto Adapter** (`crypto_adapter.cpp`): mbedTLS integration for cryptographic operations
- **Platform Manager** (`platform_manager.cpp`): Coordinates initialization and provides unified API

### 2. Build System Integration

- Added `ENABLE_MATTER` CMake option (default: OFF)
- Conditional compilation ensures backward compatibility
- When enabled, links Pico W libraries (pico_cyw43_arch, pico_mbedtls, pico_lwip)
- Standard builds unchanged, work on both Pico and Pico W

### 3. Matter Bridge Implementation

Updated `src/matter_bridge.c` with two modes:

**Stub Mode (ENABLE_MATTER=OFF):**
- Basic attribute tracking
- Console logging only
- No network required
- Compatible with Pico and Pico W

**Full Mode (ENABLE_MATTER=ON):**
- Complete Matter stack initialization
- WiFi connectivity
- Commissioning flow with QR code display
- Attribute updates trigger Matter reports
- Requires Pico W

### 4. Matter Clusters Exposed

Three standard Matter clusters on Endpoint 1:

1. **OnOff Cluster (0x0006)**
   - Maps to: Viking Bio flame detected state
   - Type: bool
   - Updates: When flame state changes

2. **LevelControl Cluster (0x0008)**
   - Maps to: Viking Bio fan speed
   - Type: uint8 (0-100%)
   - Updates: When fan speed changes

3. **TemperatureMeasurement Cluster (0x0402)**
   - Maps to: Viking Bio burner temperature
   - Type: int16 (centidegrees)
   - Updates: When temperature changes

### 5. Submodule Configuration

- Added `.gitmodules` with connectedhomeip at `third_party/connectedhomeip`
- Pinned to v1.3-branch for stability
- Updated `.gitignore` to exclude submodule build artifacts

### 6. Documentation

Comprehensive documentation added:

- **platform/pico_w_chip_port/README.md**: Detailed Matter platform guide
  - Build instructions
  - WiFi configuration
  - Commissioning workflow
  - Troubleshooting
  - Security warnings

- **README.md**: Updated with Matter build instructions and usage examples

- **ARCHITECTURE.md**: Updated data flow diagrams and implementation details

- **run.sh**: Helper script for monitoring, testing, and commissioning

### 7. CI/CD Integration

Updated `.github/workflows/build-firmware.yml` with parallel build jobs:

- **build-standard**: Builds with ENABLE_MATTER=OFF (default)
- **build-matter**: Builds with ENABLE_MATTER=ON (Pico W)

Both jobs produce separate firmware artifacts.

## Build Instructions

### Standard Build (Matter Disabled)

```bash
mkdir build && cd build
cmake .. -DENABLE_MATTER=OFF
make
```

Produces `viking_bio_matter.uf2` for Pico or Pico W with stub Matter bridge.

### Matter Build (Pico W Only)

```bash
# Initialize submodule
git submodule update --init --recursive third_party/connectedhomeip

# Configure WiFi in platform/pico_w_chip_port/network_adapter.cpp
# Update WIFI_SSID and WIFI_PASSWORD

# Build
mkdir build && cd build
cmake .. -DENABLE_MATTER=ON
make
```

Produces `viking_bio_matter.uf2` with full Matter support.

## Commissioning

1. Flash Matter-enabled firmware to Pico W
2. Connect via USB serial (115200 baud)
3. Note commissioning info displayed on boot:
   - Setup PIN: 20202021
   - Discriminator: 3840
   - Manual Code: 34970112332
   - QR Code: MT:Y.K9042C00KA0648G00

4. Commission with chip-tool:
   ```bash
   chip-tool pairing code 1 34970112332
   ```

5. Read attributes:
   ```bash
   chip-tool onoff read on-off 1 1
   chip-tool levelcontrol read current-level 1 1
   chip-tool temperaturemeasurement read measured-value 1 1
   ```

## Testing

### With Simulator

```bash
# Run Viking Bio simulator
python3 examples/viking_bio_simulator.py /dev/ttyUSB0

# Watch attributes update
watch -n 2 'chip-tool onoff read on-off 1 1'
```

### With Hardware

1. Connect Viking Bio 20 serial to Pico W GP1
2. Flash Matter firmware
3. Commission device
4. Verify attributes update when burner state changes

## Security Considerations

⚠️ **CRITICAL**: Default commissioning credentials are for TESTING ONLY

**Production deployment requires:**
1. Unique per-device discriminator
2. Device-specific setup PIN codes  
3. Update `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h`:
   - `CHIP_DEVICE_CONFIG_USE_TEST_SETUP_DISCRIMINATOR`
   - `CHIP_DEVICE_CONFIG_USE_TEST_SETUP_PIN_CODE`

## Known Limitations

1. **No OTA support**: Firmware updates require physical USB access
2. **Single WiFi only**: No Thread or Ethernet support
3. **Simple storage**: Basic key-value store without wear leveling
4. **Limited fabrics**: Maximum 5 fabrics due to memory constraints
5. **Test credentials**: Ships with default commissioning codes

## File Structure

```
viking-bio-matter/
├── platform/pico_w_chip_port/     # Matter platform port
│   ├── CHIPDevicePlatformConfig.h
│   ├── network_adapter.cpp
│   ├── storage_adapter.cpp
│   ├── crypto_adapter.cpp
│   ├── platform_manager.cpp
│   ├── platform_manager.h
│   ├── CMakeLists.txt
│   └── README.md
├── src/matter_bridge.c            # Updated with Matter integration
├── third_party/connectedhomeip/   # Matter SDK submodule
├── .gitmodules                    # Submodule configuration
├── CMakeLists.txt                 # Updated with ENABLE_MATTER
├── run.sh                         # Helper script
└── .github/workflows/
    └── build-firmware.yml         # CI with dual build jobs
```

## Next Steps

For future development:

1. **OTA Updates**: Implement over-the-air firmware updates
2. **Enhanced Storage**: Add wear leveling and garbage collection
3. **Production Security**: Implement per-device credential provisioning
4. **Command Support**: Add Matter command handlers for bidirectional control
5. **Additional Clusters**: Expose more burner diagnostics
6. **Thread Support**: Add Thread radio support for alternative to WiFi

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [connectedhomeip on GitHub](https://github.com/project-chip/connectedhomeip)
- [Pico W Documentation](https://www.raspberrypi.com/documentation/microcontrollers/raspberry-pi-pico.html)
- Platform README: `platform/pico_w_chip_port/README.md`

---

**Implementation Date**: February 2026  
**Branch**: copilot/featurematter-pico-native  
**Target**: Raspberry Pi Pico W with CYW43439 WiFi
