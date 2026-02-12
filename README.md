# Viking Bio Matter Bridge

A Matter (CHIP) bridge for the Viking Bio 20 burner, built for Raspberry Pi Pico W. This firmware reads TTL serial data from the burner and exposes flame status and fan speed through the Matter protocol.

## Features

- **Serial Communication**: Reads TTL serial data at 9600 baud from Viking Bio 20 burner
- **Flame Detection**: Reports real-time flame status
- **Fan Speed Monitoring**: Reports current fan speed (0-100%)
- **Temperature Monitoring**: Reports burner temperature
- **Matter Bridge**: Exposes burner data through Matter protocol over WiFi
- **WiFi Connectivity**: Connects to your local network for Matter communication

## Hardware Requirements

- **Raspberry Pi Pico W** (WiFi required for Matter)
- Viking Bio 20 burner with TTL serial output
- USB cable for power and debugging

## Wiring

Connect the Viking Bio 20 TTL serial output to the Raspberry Pi Pico W:

```mermaid
graph LR
    subgraph VB[Viking Bio 20 Burner]
        VB_TX[TX - Serial Out]
        VB_GND[GND]
    end
    
    LEVEL_SHIFTER[Level Shifter<br/>or<br/>Voltage Divider<br/>5V → 3.3V]
    
    subgraph PICO[Raspberry Pi Pico W]
        PICO_GP1[GP1 - UART0 RX]
        PICO_GND[GND]
        PICO_USB[USB Port]
    end
    
    VB_TX -->|5V TTL| LEVEL_SHIFTER
    LEVEL_SHIFTER -->|3.3V| PICO_GP1
    VB_GND --> PICO_GND
    PICO_USB -.->|Power & Debug| PICO
    
    style VB fill:#e1f5ff
    style PICO fill:#ffe1f5
    style LEVEL_SHIFTER fill:#FFB6C1
    style VB_TX fill:#90EE90
    style PICO_GP1 fill:#90EE90
    style VB_GND fill:#FFD700
    style PICO_GND fill:#FFD700

```
**Note**: The Pico W RX pin (GP1) expects 3.3V logic levels. The Viking Bio 20's TTL output voltage should be verified before connecting directly. If it outputs 5V TTL (which is common), a level shifter (e.g., bi-directional logic level converter) or voltage divider (two resistors: 2kΩ from TX to RX, 1kΩ from RX to GND) is required for safe voltage conversion. The diagram above shows the configuration with level shifting, which is the recommended safe approach.

## Serial Protocol

The firmware supports two serial data formats:

### Binary Protocol (Recommended)
```
[0xAA] [FLAGS] [FAN_SPEED] [TEMP_HIGH] [TEMP_LOW] [0x55]
```
- `FLAGS`: bit 0 = flame detected, bits 1-7 = error codes
- `FAN_SPEED`: 0-100 (percentage)
- `TEMP_HIGH, TEMP_LOW`: Temperature in Celsius (16-bit big-endian)

### Text Protocol (Fallback)
```
F:1,S:50,T:75\n
```
- `F`: Flame status (0=off, 1=on)
- `S`: Fan speed (0-100%)
- `T`: Temperature (°C)

## Building Firmware

### Prerequisites

1. Install the Pico SDK:
   ```bash
   git clone https://github.com/raspberrypi/pico-sdk.git
   cd pico-sdk
   git submodule update --init
   export PICO_SDK_PATH=$(pwd)
   ```

2. Install ARM toolchain:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
   
   # macOS
   brew install cmake arm-none-eabi-gcc
   ```

### Build Steps

1. **Initialize the connectedhomeip submodule:**
   ```bash
   git submodule update --init --recursive third_party/connectedhomeip
   ```

2. **Configure WiFi credentials** in `platform/pico_w_chip_port/network_adapter.cpp`:
   ```cpp
   #define WIFI_SSID "YourNetworkName"
   #define WIFI_PASSWORD "YourPassword"
   ```

3. **Build the firmware:**
   ```bash
   mkdir build
   cd build
   cmake ..
   make
   ```

This generates Matter-enabled firmware that:
- Connects to WiFi on boot
- Initializes the Matter stack
- Prints commissioning QR code and PIN
- Exposes Viking Bio data as Matter attributes
- Can be commissioned by Matter controllers (e.g., chip-tool)

See [platform/pico_w_chip_port/README.md](platform/pico_w_chip_port/README.md) for detailed Matter configuration and commissioning instructions.

### Flashing the Firmware

1. Hold the BOOTSEL button on the Pico W while connecting it via USB
2. The Pico W will appear as a mass storage device
3. Copy `build/viking_bio_matter.uf2` to the Pico W
4. The Pico W will automatically reboot with the new firmware

## GitHub Actions

The firmware is automatically built on push to `main` or `develop` branches. Build artifacts are available in the Actions tab.

## Usage

1. Flash the firmware to your Raspberry Pi Pico W
2. Connect the Viking Bio 20 serial output to the Pico W (see Wiring section)
3. Power the Pico W via USB
4. Connect to the Pico's USB serial to view commissioning info:
   ```bash
   screen /dev/ttyACM0 115200
   # Or use Thonny IDE (Tools > Serial)
   ```
   
   You'll see:
   ```
   ====================================
       Matter Commissioning Info
   ====================================
   Device MAC:     28:CD:C1:00:00:01
   Setup PIN Code: 24890840
   Discriminator:  3840 (0x0F00)
   
   ⚠️  IMPORTANT:
      PIN is derived from device MAC.
      Use tools/derive_pin.py to compute
      the PIN from the MAC address above.
   ====================================
   ```
   
   **Note**: The Setup PIN is unique per device, derived from its MAC address.
   You can compute it offline using:
   ```bash
   python3 tools/derive_pin.py 28:CD:C1:00:00:01
   ```

5. Commission the device using chip-tool with the **printed PIN**:
   ```bash
   # Use the PIN from your device's serial output
   chip-tool pairing ble-wifi 1 MySSID MyPassword 24890840 3840
   ```

6. Control and monitor attributes:
   ```bash
   # Read flame status (OnOff cluster)
   chip-tool onoff read on-off 1 1
   
   # Read fan speed (LevelControl cluster)
   chip-tool levelcontrol read current-level 1 1
   
   # Read temperature (TemperatureMeasurement cluster)
   chip-tool temperaturemeasurement read measured-value 1 1
   ```

**Matter Clusters Exposed:**
- **OnOff (0x0006)**: Flame detected state
- **LevelControl (0x0008)**: Fan speed (0-100%)
- **TemperatureMeasurement (0x0402)**: Burner temperature

⚠️ **Security Note:** 
- The Setup PIN is **unique per device**, derived from the device MAC address using SHA-256 with product salt `VIKINGBIO-2026`.
- The discriminator 3840 is for **testing only**. For production deployments, use unique discriminators per device (0-4095, excluding reserved ranges) and update `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h`.
- The PIN derivation algorithm is documented in `tools/derive_pin.py` and can be computed offline from a printed MAC address.

For detailed Matter configuration and troubleshooting, see [platform/pico_w_chip_port/README.md](platform/pico_w_chip_port/README.md).

## Development

### Project Structure

```
viking-bio-matter/
├── src/
│   ├── main.c                 # Main application entry point
│   ├── serial_handler.c       # UART/serial communication
│   ├── viking_bio_protocol.c  # Viking Bio protocol parser
│   └── matter_bridge.c        # Matter bridge implementation
├── include/
│   ├── serial_handler.h
│   ├── viking_bio_protocol.h
│   └── matter_bridge.h
├── platform/
│   └── pico_w_chip_port/      # Matter platform port for Pico W
│       ├── network_adapter.cpp    # WiFi/lwIP integration
│       ├── storage_adapter.cpp    # Flash storage for fabrics
│       ├── crypto_adapter.cpp     # mbedTLS crypto
│       ├── platform_manager.cpp   # Platform coordination
│       └── README.md              # Detailed Matter documentation
├── third_party/
│   └── connectedhomeip/       # Matter SDK submodule (when initialized)
├── examples/
│   └── viking_bio_simulator.py # Serial data simulator for testing
├── CMakeLists.txt             # Build configuration
└── .github/
    └── workflows/
        └── build-firmware.yml # CI/CD pipeline
```

## Testing

### Serial Simulator

For testing without hardware, use the included simulator:

```bash
# Install dependencies
pip3 install pyserial

# Run simulator with binary protocol (default)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0

# Run simulator with text protocol
python3 examples/viking_bio_simulator.py -p text /dev/ttyUSB0

# Change update interval
python3 examples/viking_bio_simulator.py -i 2.0 /dev/ttyUSB0
```

### Manual Testing

Send test data directly:

```bash
# Binary protocol (hex)
echo -ne '\xAA\x01\x50\x00\x4B\x55' > /dev/ttyUSB0

# Text protocol
echo "F:1,S:80,T:75" > /dev/ttyUSB0
```

### Debug Output

Connect to the Pico's USB serial port:

```bash
screen /dev/ttyACM0 115200
```

Expected output:
```
Viking Bio Matter Bridge starting...
Setting up WiFi...
Connecting to WiFi: YourNetworkName
WiFi connected, IP: 192.168.1.xxx
====================================
   Matter Commissioning Info
====================================
Device MAC:     28:CD:C1:00:00:01
Setup PIN Code: 24890840  (derived from MAC)
Discriminator:  3840 (0x0F00)
====================================
Flame: ON, Fan Speed: 80%, Temp: 75°C
Matter: OnOff cluster updated - Flame ON
Matter: LevelControl cluster updated - Fan speed 80%
```

### Testing Matter Integration

1. **Build and flash firmware:**
   ```bash
   cd build
   cmake .. && make
   # Flash viking_bio_matter.uf2 to Pico W
   ```

2. **Monitor commissioning info:**
   ```bash
   screen /dev/ttyACM0 115200
   ```
   
   Look for the commissioning information displayed on boot.

3. **Commission with chip-tool:**
   ```bash
   # Build chip-tool from Matter SDK (one-time setup)
   cd third_party/connectedhomeip
   ./scripts/examples/gn_build_example.sh examples/chip-tool out/host
   
   # Commission device using the PIN from your device's serial output
   ./out/host/chip-tool pairing code 1 <PIN>
   ```

4. **Test attribute reads:**
   ```bash
   # Flame status
   ./out/host/chip-tool onoff read on-off 1 1
   
   # Fan speed
   ./out/host/chip-tool levelcontrol read current-level 1 1
   
   # Temperature
   ./out/host/chip-tool temperaturemeasurement read measured-value 1 1
   ```

5. **Test attribute updates with simulator:**
   ```bash
   # In one terminal, run simulator
   python3 examples/viking_bio_simulator.py /dev/ttyUSB0
   
   # In another, watch attributes change
   watch -n 2 './out/host/chip-tool onoff read on-off 1 1'
   ```

## Known Limitations

1. **No OTA support**: Firmware updates require physical USB access (hold BOOTSEL button and copy .uf2 file)
2. **WiFi only**: No Thread or Ethernet support currently
3. **Simple storage**: Basic key-value store without wear leveling
4. **Limited fabrics**: Maximum 5 Matter fabrics due to memory constraints (264KB RAM on RP2040)
5. **Crypto limitations**: DRBG and RNG functions are stubbed due to Pico SDK 1.5.1 mbedTLS bugs (SHA256 and AES work correctly)

## Security Considerations

⚠️ **CRITICAL**: Default commissioning credentials are for TESTING ONLY

**Production deployment requires:**
1. Unique per-device discriminator (0-4095, excluding reserved ranges)
2. Device-specific setup PIN codes (each device has unique PIN derived from MAC)
3. Update `platform/pico_w_chip_port/CHIPDevicePlatformConfig.h`:
   - Discriminator 3840 is reserved for testing per Matter Core Specification 5.1.3.1
   - Production devices MUST use unique discriminators

**WiFi Security:**
- WiFi credentials are hardcoded in `platform/pico_w_chip_port/network_adapter.cpp`
- Never commit credentials to version control
- Consider using a secure provisioning method for production

## Future Enhancements

1. **Full Matter SDK Integration**
   - ✅ Commissioning flow
   - ✅ Attribute subscriptions
   - ✅ WiFi support (Pico W)
   - ⏳ Command handling for bidirectional control
   - ⏳ OTA firmware updates

2. **Network Connectivity**
   - ✅ WiFi support (Pico W)
   - ⏳ Thread support (with external radio)
   - ⏳ Ethernet support (with W5500 module)

3. **Advanced Features**
   - ⏳ OTA firmware updates over Matter
   - ⏳ Enhanced error reporting via Matter events
   - ⏳ Historical data logging
   - ⏳ Alarm notifications

4. **Protocol Extensions**
   - ⏳ Support for multiple Viking Bio devices
   - ⏳ Bidirectional communication (control burner)
   - ⏳ Enhanced diagnostics

5. **Production Readiness**
   - ⏳ Per-device unique commissioning credentials
   - ⏳ Secure boot and attestation
   - ⏳ Flash wear leveling for storage
   - ⏳ Watchdog and fault recovery

## References

- [Matter Specification](https://csa-iot.org/all-solutions/matter/)
- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [Project CHIP (connectedhomeip)](https://github.com/project-chip/connectedhomeip)
- [Viking Bio 20 Documentation](https://www.vikingbio.se/)
- [Platform Port README](platform/pico_w_chip_port/README.md) - Detailed Matter configuration

## License

This project is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License (CC BY-NC-SA 4.0). This means you can use, modify, and share this project for non-commercial purposes, as long as you provide attribution and share derivatives under the same license.

See the [LICENSE](LICENSE) file for details or visit https://creativecommons.org/licenses/by-nc-sa/4.0/

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.