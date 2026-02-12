# Viking Bio Matter Bridge - Coding Agent Instructions

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W. Reads TTL serial (9600 baud) and exposes flame/fan/temp via Matter over WiFi.

**Size**: ~484KB core, 17 source files | **Languages**: C (firmware), C++ (Matter port), Python (tools) | **Target**: RP2040/Pico W | **Framework**: Pico SDK 1.5.1, minimal Matter implementation (src/matter_minimal/, no external SDK)

## Build Commands (Validated Feb 2026)

### Prerequisites (ALWAYS run first on fresh environment)
```bash
sudo apt-get update && sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential
git clone --depth 1 --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init && cd ..  # ~30s
export PICO_SDK_PATH=$(pwd)/pico-sdk  # CRITICAL: Must be absolute path, set before cmake
```

**Verified Versions**: CMake 3.13+, ARM GCC 13.2.1, Pico SDK 1.5.1 (exact)

### Build (Matter Always Enabled - No Flag)
```bash
mkdir build && cd build
export PICO_SDK_PATH=/absolute/path/to/pico-sdk  # REQUIRED
cmake .. && make -j$(nproc)
```

**Timing** (8 cores): SDK init 30s (one-time) | CMake 1.3s | Build 26s | Incremental 0.4s  
**Artifacts** (build/): viking_bio_matter.uf2 (837KB), .elf (765KB), .bin (419KB), .hex (1.2MB)  
**Size**: text 428KB, bss 54KB, data 0KB

### Common Build Errors

1. **"PICO_SDK_PATH not set"** → `export PICO_SDK_PATH=/absolute/path` (verify: `echo $PICO_SDK_PATH`)
2. **"arm-none-eabi-gcc not found"** → `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`
3. **"This firmware requires PICO_BOARD=pico_w"** → Only Pico W supported (WiFi required), not standard Pico
4. **Missing lwIP/mbedTLS headers** → `cd pico-sdk && git submodule update --init` (~30s)
5. **After WiFi credential changes** → Clean build: `rm -rf build && mkdir build && cd build && cmake .. && make`

### WiFi Config & Testing

**WiFi**: Edit `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11 (WIFI_SSID/WIFI_PASSWORD). ⚠️ Never commit creds.

**Test Simulator**: `pip3 install pyserial && python3 examples/viking_bio_simulator.py /dev/ttyUSB0`  
**Monitor Device**: `screen /dev/ttyACM0 115200` or `./run.sh monitor`  
**Helper Scripts**: `./setup.sh` (setup wizard), `./run.sh monitor|simulate|commissioning`

## Project Structure

**Root**: CMakeLists.txt (enforces pico_w), setup.sh, run.sh, .gitignore (excludes build/, pico-sdk/, *.uf2)

**Source**:
- `src/main.c` - Entry point, main loop
- `src/serial_handler.c` - UART0 RX (GP1), interrupt-driven, circular buffer
- `src/viking_bio_protocol.c` - Parser: binary `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]` or text `F:1,S:50,T:75\n`
- `src/matter_bridge.cpp` - Matter integration, 3 clusters: OnOff (0x0006), LevelControl (0x0008), TempMeasurement (0x0402)
- `src/matter_minimal/` - TLV codec, UDP transport, PASE security, interaction model, clusters

**Platform** (`platform/pico_w_chip_port/`):
- `network_adapter.cpp` - WiFi/lwIP (creds lines 10-11)
- `storage_adapter.cpp` - Flash key-value (last 256KB)
- `crypto_adapter.cpp` - mbedTLS (DRBG/RNG stubbed, SHA256/AES OK)
- `platform_manager.cpp` - Init coordinator
- `config/` - lwipopts.h, mbedtls_config.h

**Config**: 
- Build: CMakeLists.txt (lines 3-9: pico_w enforcement, lines 22-34: optimizations)
- Matter creds: platform/pico_w_chip_port/CHIPDevicePlatformConfig.h (disc 3840=test only, PIN from MAC)
- No linting config

## GPIO & Protocols

**Pins**: GP0=UART TX (unused), GP1=UART RX (Viking Bio input), LED=CYW43 chip (not GPIO 25)

**Serial**: Binary 6 bytes or text "F:1,S:50,T:75\n"  
**Matter**: 3 clusters on endpoint 1 - OnOff (flame bool), LevelControl (speed 0-100%), Temperature (centidegrees int16)  
**Commissioning**: PIN derived from MAC (SHA256(MAC||"VIKINGBIO-2026")%100000000), discriminator 3840 (⚠️ test only), tool: `python3 tools/derive_pin.py <MAC>`

## CI/CD

**Workflow**: `.github/workflows/build-firmware.yml`  
**Jobs**: build-matter (Ubuntu, installs toolchain, clones SDK 1.5.1 w/submodules cached, cmake in build-matter/, make ~26s, uploads artifacts)  
**Triggers**: push main/develop, PR to main, manual  
**Artifacts**: Actions tab → run → "viking-bio-matter-firmware-matter"  
**CI Always Succeeds** if: PICO_SDK_PATH set (line 46), toolchain installed (lines 24-27), SDK submodules init (line 43)

## Key Facts

1. **Matter always on**: No ENABLE_MATTER flag, firmware requires Pico W (WiFi), not standard Pico. Uses src/matter_minimal/, not external SDK.
2. **Interrupt serial**: hardware_sync lib (save/restore_interrupts), circular buffer
3. **PIN per device**: SHA256(MAC||"VIKINGBIO-2026")%100000000, `tools/derive_pin.py`
4. **No OTA**: Physical USB only (BOOTSEL + copy .uf2)
5. **Flash**: Last 256KB for Matter fabrics, simple key-value, no wear leveling
6. **RAM limits**: 264KB → max 5 fabrics. mbedTLS DRBG/RNG stubbed (SDK 1.5.1 bugs), SHA256/AES work.
7. **Optimizations**: `-O3 -ffast-math -fno-signed-zeros -fno-trapping-math -funroll-loops`, LTO off (SDK wrapper issues), `serial_handler_data_available()` inlined, `viking_bio_parse_data()` `__attribute__((hot))`
8. **Known TODOs**: crypto_adapter.cpp DRBG/RNG (documented SDK bug workaround) - do NOT remove

## Common Code Changes

1. **Serial Protocol**: Edit `src/viking_bio_protocol.c` + `include/viking_bio_protocol.h`, update `viking_bio_data_t`, test `python3 examples/viking_bio_simulator.py /dev/ttyUSB0`
2. **Matter Clusters**: Edit `src/matter_bridge.c` + `platform/pico_w_chip_port/platform_manager.cpp`, clean rebuild
3. **GPIO**: Edit `include/serial_handler.h` + `src/serial_handler.c` + README.md
4. **WiFi**: Edit `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11, clean rebuild, ⚠️ never commit creds

## Pre-Commit Validation (ALL MUST PASS)

1. ✅ `export PICO_SDK_PATH=/absolute/path/to/pico-sdk` → `echo $PICO_SDK_PATH` non-empty
2. ✅ `rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)` → ~26s, artifacts exist
3. ✅ `arm-none-eabi-size viking_bio_matter.elf` → text ~428KB, bss ~54KB (±10KB OK)
4. ✅ `git diff | grep -i -E "password|ssid|secret|key" || echo "OK"` → must output "OK"
5. ✅ `git diff | grep -E "TODO|HACK|FIXME"` → only OK: crypto_adapter.cpp DRBG/RNG (known)
6. ✅ .gitignore excludes build/, pico-sdk/, *.uf2
7. ✅ Update docs: Build→README.md+this, APIs→code comments, Matter→platform/pico_w_chip_port/README.md

**One-liner** (repo root, ~27s total):
```bash
export PICO_SDK_PATH=$(pwd)/pico-sdk && rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc) && ls -lh *.uf2 *.elf *.bin *.hex && arm-none-eabi-size viking_bio_matter.elf
```

## Architecture Summary

**Data Flow**: Viking Bio 20 → UART0 (9600 baud, GP1) → Circular buffer → Protocol parser → Matter bridge → Minimal Matter stack (src/matter_minimal/) → WiFi/lwIP → Matter controller

**Components**: serial_handler.c (interrupt RX), viking_bio_protocol.c (binary/text parser), matter_bridge.cpp (3 clusters), main.c (coordinator), platform port (network/storage/crypto/manager)

**Limitations**: No OTA, WiFi only (no Thread/Ethernet), simple storage (no wear leveling), max 5 fabrics (RAM), test discriminator 3840, DRBG/RNG stubbed

## Trust & Verify

These instructions validated Feb 2026 via actual builds. Only search/explore if: incomplete for your task, errors not documented, repo changed significantly (`git log`). Otherwise follow exactly to minimize exploration/failures.
