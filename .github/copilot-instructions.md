# Viking Bio Matter Bridge - Coding Agent Instructions

## Getting Started (First-Time Agents)

**Read This First**: These instructions are validated and comprehensive. Follow them exactly to avoid common pitfalls.

**Quick Start Workflow**:
1. ✅ Install prerequisites (ARM toolchain, CMake)
2. ✅ Clone and initialize **both** Pico SDK and repository submodules (CRITICAL)
3. ✅ Set `PICO_SDK_PATH` to absolute path (CRITICAL)
4. ✅ Build to verify setup (`mkdir build && cd build && cmake .. && make`)
5. ✅ Check firmware size is ~544KB text, ~72KB bss
6. ✅ Explore codebase with helper scripts (`./run.sh`, `./setup.sh`, `tools/`)

**Common First-Time Mistakes**:
- ❌ Forgetting to run `git submodule update --init --recursive` → CMake fails with "libs/pico-lfs does not contain CMakeLists.txt"
- ❌ Setting relative `PICO_SDK_PATH` → CMake fails to find SDK
- ❌ Using standard Pico instead of Pico W → Compile error "This firmware requires PICO_BOARD=pico_w"
- ❌ Committing WiFi credentials → Security issue
- ❌ Not initializing Pico SDK submodules → Missing lwIP/mbedTLS headers

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W. Reads TTL serial (9600 baud) and exposes flame/fan/temp via Matter over WiFi.

**Size**: ~532KB binary, 544KB text, 72KB bss | **Languages**: C (firmware), C++ (Matter port), Python (tools) | **Target**: RP2040/Pico W | **Framework**: Pico SDK 1.5.1, minimal Matter implementation (src/matter_minimal/, no external SDK)

**Key Features**: Matter protocol (no external SDK), LittleFS storage with wear leveling (last 256KB flash), SoftAP WiFi commissioning, interrupt-driven serial with circular buffer, SHA256-based Matter PIN generation

## Build Commands (Validated Feb 2026)

### Prerequisites (ALWAYS run first on fresh environment)
```bash
sudo apt-get update && sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Clone Pico SDK (use exact version for compatibility)
git clone --depth 1 --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init && cd ..  # ~30s

# Set PICO_SDK_PATH (CRITICAL: Must be absolute path)
export PICO_SDK_PATH=$(pwd)/pico-sdk  # CRITICAL: Must be absolute path, set before cmake

# Clone repository and initialize ALL submodules
git clone https://github.com/phieri/viking-bio-matter.git
cd viking-bio-matter
git submodule update --init --recursive  # CRITICAL: Initializes libs/pico-lfs
```

**Verified Versions**: CMake 3.13+, ARM GCC 13.2.1, Pico SDK 1.5.1 (exact)

### Build (Matter Always Enabled - No Flag)
```bash
# CRITICAL: Initialize repository submodules first!
git submodule update --init --recursive  # Initializes libs/pico-lfs

mkdir build && cd build
export PICO_SDK_PATH=/absolute/path/to/pico-sdk  # REQUIRED
cmake .. && make -j$(nproc)
```

**Timing** (8 cores): Submodule init 5s | SDK init 30s (one-time) | CMake 1.3s | Build 26s | Incremental 0.4s  
**Artifacts** (build/): viking_bio_matter.uf2 (1.1MB), .elf (919KB), .bin (532KB), .hex (1.5MB)  
**Size**: text 544KB, bss 72KB, data 0KB

### Common Build Errors

1. **"PICO_SDK_PATH not set"** → `export PICO_SDK_PATH=/absolute/path` (verify: `echo $PICO_SDK_PATH`)
2. **"arm-none-eabi-gcc not found"** → `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`
3. **"This firmware requires PICO_BOARD=pico_w"** → Only Pico W supported (WiFi required), not standard Pico
4. **Missing lwIP/mbedTLS headers** → `cd pico-sdk && git submodule update --init` (~30s)
5. **"libs/pico-lfs does not contain a CMakeLists.txt file"** → Repository submodules not initialized: `git submodule update --init --recursive` (CRITICAL in CI)
6. **After WiFi credential changes** → Clean build: `rm -rf build && mkdir build && cd build && cmake .. && make`

#### Error Workaround Details

**pico-lfs Submodule Error (CI Failure Feb 13, 2026)**:
- **Symptom**: CMake fails with "The source directory .../libs/pico-lfs does not contain a CMakeLists.txt file"
- **Root Cause**: The repository has a git submodule at `libs/pico-lfs` that must be initialized before building
- **Solution**: Run `git submodule update --init --recursive` after cloning
- **CI Fix**: In GitHub Actions, use `actions/checkout@v6` with `submodules: recursive` parameter
- **Verification**: Check that `libs/pico-lfs/CMakeLists.txt` exists before building

### WiFi Config & Testing

**WiFi**: Edit `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11 (WIFI_SSID/WIFI_PASSWORD). ⚠️ Never commit creds.

**Test Simulator**: `pip3 install pyserial && python3 examples/viking_bio_simulator.py /dev/ttyUSB0`  
**Monitor Device**: `screen /dev/ttyACM0 115200` or `./run.sh monitor`  
**Helper Scripts**: `./setup.sh` (setup wizard), `./run.sh monitor|simulate|commissioning`

## Project Structure

**Root**: CMakeLists.txt (enforces pico_w), setup.sh, run.sh, .gitignore (excludes build/, pico-sdk/, *.uf2)

**Source**:
- `src/main.c` - Entry point, main loop (coordinator between serial, protocol, Matter bridge)
- `src/serial_handler.c` - UART0 RX (GP1), interrupt-driven, circular buffer (hardware_sync)
- `src/viking_bio_protocol.c` - Parser: binary `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]` or text `F:1,S:50,T:75\n`
- `src/matter_bridge.cpp` - Matter integration, 3 clusters: OnOff (0x0006), LevelControl (0x0008), TempMeasurement (0x0402)
- `src/matter_minimal/` - Minimal Matter stack implementation (TLV codec, UDP transport, PASE security, interaction model, clusters)
  - `codec/` - TLV and message encoding/decoding
  - `transport/` - UDP transport layer with lwIP
  - `security/` - PASE (SPAKE2+) and session management (AES-128-CCM)
  - `interaction/` - Read handler, subscribe handler, report generator
  - `clusters/` - OnOff, LevelControl, Temperature, Descriptor, NetworkCommissioning

**Platform** (`platform/pico_w_chip_port/`):
- `network_adapter.cpp` - WiFi/lwIP (hardcoded creds lines 10-11), SoftAP support (192.168.4.1)
- `storage_adapter.cpp` - LittleFS storage (last 256KB flash), key-value pairs, wear leveling
- `crypto_adapter.cpp` - mbedTLS wrapper (DRBG/RNG stubbed due to SDK bug, SHA256/AES work)
- `platform_manager.cpp` - Platform initialization coordinator, discriminator management
- `config/` - lwipopts.h (lwIP config, IPv6 enabled), mbedtls_config.h (crypto config)
- `CHIPDevicePlatformConfig.h` - Matter device config (PIN from MAC, discriminator from storage)

**Tests** (`tests/`):
- `codec/` - TLV tests (host-runnable)
- `transport/` - Message/UDP tests (requires Pico W)
- `security/` - PASE/session tests (requires Pico W)
- `interaction/` - Read/subscribe tests (requires Pico W)
- `clusters/` - Cluster tests (requires Pico W)
- `storage/` - LittleFS storage tests (requires Pico W)

**Tools & Examples**:
- `tools/derive_pin.py` - Generate Matter PIN from MAC address (SHA256-based)
- `examples/viking_bio_simulator.py` - Serial simulator for testing without hardware
- `setup.sh` - Interactive setup wizard
- `run.sh` - Device management (monitor, simulate, commissioning)

**Libraries** (`libs/`):
- `pico-lfs/` - **Git submodule** (LittleFS port for Pico, MUST be initialized)

**Documentation** (`docs/`):
- `MINIMAL_MATTER_ARCHITECTURE.md` - Matter stack architecture
- `PR_ROADMAP.md` - Phase-by-phase implementation roadmap (7 phases complete)
- `WIFI_COMMISSIONING_SUMMARY.md` - WiFi commissioning with SoftAP
- `SUBSCRIPTIONS_TESTING.md` - Matter subscriptions testing
- `REFERENCE_IMPLEMENTATIONS.md` - Reference implementations used

**Config**: 
- Build: CMakeLists.txt (lines 3-9: pico_w enforcement, lines 31-37: optimizations, line 23: pico-lfs submodule)
- Matter creds: platform/pico_w_chip_port/platform_manager.cpp (discriminator random 0xF00-0xFFF from storage, PIN from MAC)
- CI: .github/workflows/build-firmware.yml (Ubuntu, ARM GCC, Pico SDK 1.5.1)
- No linting config (follow existing style)

## GPIO & Protocols

**Pins**: GP0=UART TX (unused), GP1=UART RX (Viking Bio input), LED=CYW43 chip (not GPIO 25)

**Serial**: Binary 6 bytes or text "F:1,S:50,T:75\n"  
**Matter**: 3 clusters on endpoint 1 - OnOff (flame bool), LevelControl (speed 0-100%), Temperature (centidegrees int16)  
**Commissioning**: PIN derived from MAC (SHA256(MAC||"VIKINGBIO-2026")%100000000), discriminator randomly generated on first boot (0xF00-0xFFF, saved to flash), tool: `python3 tools/derive_pin.py <MAC>`

## CI/CD

**Workflow**: `.github/workflows/build-firmware.yml`  
**Jobs**: build-matter (Ubuntu, installs toolchain, clones SDK 1.5.1 w/submodules cached, cmake in build-matter/, make ~26s, uploads artifacts)  
**Triggers**: push main/develop, PR to main, manual  
**Artifacts**: Actions tab → run → "viking-bio-matter-firmware-matter"  
**CI Always Succeeds** if: PICO_SDK_PATH set (line 54), toolchain installed (lines 32-35), SDK submodules init (line 51), **repository submodules init** (line 30: `submodules: recursive`)

**Known CI Issues**:
- **Feb 13, 2026 Failure (Run 21999277166)**: CMake failed because `libs/pico-lfs` submodule was not initialized. Fixed by adding `submodules: recursive` to `actions/checkout@v6` in workflow.
- **Paths-ignore**: Workflow skips on **.md and docs/** changes only (lines 6-13), manual runs always work.

## Key Facts

1. **Matter always on**: No ENABLE_MATTER flag, firmware requires Pico W (WiFi), not standard Pico. Uses src/matter_minimal/, not external SDK.
2. **Interrupt serial**: hardware_sync lib (save/restore_interrupts), circular buffer
3. **PIN per device**: SHA256(MAC||"VIKINGBIO-2026")%100000000, `tools/derive_pin.py`
4. **No OTA**: Physical USB only (BOOTSEL + copy .uf2)
5. **Flash**: Last 256KB for Matter fabrics, simple key-value, with wear leveling
6. **RAM limits**: 264KB → max 5 fabrics. mbedTLS DRBG/RNG stubbed (SDK 1.5.1 bugs), SHA256/AES work.
7. **Optimizations**: `-O3 -ffast-math -fno-signed-zeros -fno-trapping-math -funroll-loops`, LTO off (SDK wrapper issues), `serial_handler_data_available()` inlined, `viking_bio_parse_data()` `__attribute__((hot))`
8. **Known TODOs**: crypto_adapter.cpp DRBG/RNG (documented SDK bug workaround) - do NOT remove

## Common Code Changes

1. **Serial Protocol**: Edit `src/viking_bio_protocol.c` + `include/viking_bio_protocol.h`, update `viking_bio_data_t`, test `python3 examples/viking_bio_simulator.py /dev/ttyUSB0`
2. **Matter Clusters**: Edit `src/matter_bridge.c` + `platform/pico_w_chip_port/platform_manager.cpp`, clean rebuild
3. **GPIO**: Edit `include/serial_handler.h` + `src/serial_handler.c` + README.md
4. **WiFi**: Edit `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11, clean rebuild, ⚠️ never commit creds

## Testing & Validation

### Running Tests

**Unit Tests**: Most tests require Pico W hardware and cannot run on host PC. Tests are in `tests/` directory:
- `tests/codec/` - TLV codec tests (can run on host)
- `tests/transport/` - UDP transport tests (requires hardware)
- `tests/security/` - PASE/session tests (requires hardware)
- `tests/interaction/` - Interaction model tests (requires hardware)
- `tests/clusters/` - Cluster tests (requires hardware)
- `tests/storage/` - Storage tests (requires hardware)

**Hardware Testing**:
```bash
# 1. Flash firmware to Pico W (hold BOOTSEL, connect USB, copy .uf2)
# 2. Monitor serial output
screen /dev/ttyACM0 115200
# or
./run.sh monitor

# 3. Run simulator for testing without actual burner
pip3 install pyserial
python3 examples/viking_bio_simulator.py /dev/ttyUSB0
```

**Helper Scripts**:
- `./setup.sh` - Interactive setup wizard (installs deps, downloads SDK, configures build)
- `./run.sh monitor` - Monitor device serial output
- `./run.sh simulate` - Run Viking Bio simulator
- `./run.sh commissioning` - Show Matter commissioning info
- `tools/derive_pin.py <MAC>` - Generate Matter PIN from device MAC address

### Validating Changes

Before committing:
1. ✅ Build succeeds: `rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)`
2. ✅ Firmware size reasonable: `arm-none-eabi-size viking_bio_matter.elf` (text ~544KB ±20KB, bss ~72KB ±10KB)
3. ✅ No credential leaks: `git diff | grep -i -E "password|ssid|secret|key" || echo "OK"` → must output "OK"
4. ✅ Only acceptable TODOs: `git diff | grep -E "TODO|HACK|FIXME"` → only crypto_adapter.cpp DRBG/RNG (known SDK bug)
5. ✅ Submodules committed if changed: `git status libs/`
6. ✅ Update relevant docs (README.md, this file, platform docs)

## Troubleshooting Guide

### Build Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| "PICO_SDK_PATH not set" | Environment variable not exported | `export PICO_SDK_PATH=/absolute/path/to/pico-sdk` (must be absolute) |
| "arm-none-eabi-gcc not found" | ARM toolchain not installed | `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi` |
| "This firmware requires PICO_BOARD=pico_w" | Wrong board selected | Only Pico W supported (WiFi required), not standard Pico |
| "Missing lwip.h" or "mbedtls.h" | Pico SDK submodules not initialized | `cd pico-sdk && git submodule update --init` |
| "libs/pico-lfs does not contain CMakeLists.txt" | Repository submodules not initialized | `git submodule update --init --recursive` |
| Build succeeds but size changed significantly | Optimization flags or dependencies changed | Check `CMakeLists.txt` lines 31-37, verify all sources compiled |
| Linker errors about undefined references | Missing library dependencies | Check `CMakeLists.txt` target_link_libraries, ensure pico_lfs/mbedtls/lwip linked |

### Runtime Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| Device not connecting to WiFi | Wrong credentials or SoftAP mode | Check `network_adapter.cpp` lines 10-11, or use SoftAP commissioning |
| Serial data not received | Wrong GPIO or baud rate | Verify GP1/UART0 at 9600 baud, check wiring (requires level shifter for 5V TTL) |
| Matter pairing fails | Wrong PIN or discriminator | Use `tools/derive_pin.py <MAC>` to get correct PIN, discriminator is randomly generated (check device serial output) |
| Device crashes after WiFi connect | RAM exhaustion | Check fabric count (max 5), reduce Matter sessions in CHIPDevicePlatformConfig.h |
| Flash storage errors | Wear leveling exhausted | Flash has limited write cycles, storage uses last 256KB with LittleFS wear leveling |

### CI/CD Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| CI fails with "CMakeLists.txt not found" | Submodules not checked out | Add `submodules: recursive` to `actions/checkout@v6` |
| CI skips on code changes | paths-ignore too broad | Check `.github/workflows/build-firmware.yml` lines 6-13, only **.md and docs/** ignored |
| SDK cache miss every run | Cache key doesn't match | Verify `PICO_SDK_VERSION` env var matches SDK tag (1.5.1) |
| Build timeout in CI | Build too slow or hanging | Check for infinite loops in code, increase timeout, verify -j$(nproc) used |

## Common Code Changes

1. **Serial Protocol**: Edit `src/viking_bio_protocol.c` + `include/viking_bio_protocol.h`, update `viking_bio_data_t`, test `python3 examples/viking_bio_simulator.py /dev/ttyUSB0`
2. **Matter Clusters**: Edit `src/matter_bridge.c` + `platform/pico_w_chip_port/platform_manager.cpp`, clean rebuild
3. **GPIO**: Edit `include/serial_handler.h` + `src/serial_handler.c` + README.md
4. **WiFi**: Edit `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11, clean rebuild, ⚠️ never commit creds

## Pre-Commit Validation (ALL MUST PASS)

1. ✅ `export PICO_SDK_PATH=/absolute/path/to/pico-sdk` → `echo $PICO_SDK_PATH` non-empty
2. ✅ `rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc)` → ~26s, artifacts exist
3. ✅ `arm-none-eabi-size viking_bio_matter.elf` → text ~544KB, bss ~72KB (±20KB OK due to LittleFS)
4. ✅ `git diff | grep -i -E "password|ssid|secret|key" || echo "OK"` → must output "OK"
5. ✅ `git diff | grep -E "TODO|HACK|FIXME"` → only OK: crypto_adapter.cpp DRBG/RNG (known)
6. ✅ .gitignore excludes build/, pico-sdk/, *.uf2
7. ✅ Update docs: Build→README.md+this, APIs→code comments, Matter→platform/pico_w_chip_port/README.md
8. ✅ Repository submodules: `git submodule status` shows all initialized (especially libs/pico-lfs)

**One-liner** (repo root, ~32s total):
```bash
export PICO_SDK_PATH=$(pwd)/pico-sdk && git submodule update --init --recursive && rm -rf build && mkdir build && cd build && cmake .. && make -j$(nproc) && ls -lh *.uf2 *.elf *.bin *.hex && arm-none-eabi-size viking_bio_matter.elf
```

## Architecture Summary

**Data Flow**: Viking Bio 20 → UART0 (9600 baud, GP1) → Circular buffer → Protocol parser → Matter bridge → Minimal Matter stack (src/matter_minimal/) → WiFi/lwIP → Matter controller

**Components**: serial_handler.c (interrupt RX, hardware_sync), viking_bio_protocol.c (binary/text parser with fallback), matter_bridge.cpp (3 clusters + NetworkCommissioning), main.c (coordinator), platform port (network/storage/crypto/manager), matter_minimal stack (7 phases complete)

**Limitations**: No OTA, WiFi only (no Thread/Ethernet), LittleFS storage (last 256KB, wear leveling), max 5 fabrics (RAM 264KB), discriminator randomized on first boot (0xF00-0xFFF testing range), DRBG/RNG stubbed (documented SDK 1.5.1 bug, SHA256/AES work), requires 5V→3.3V level shifter for serial input

## Trust & Verify

These instructions validated Feb 2026 via actual builds. Only search/explore if: incomplete for your task, errors not documented, repo changed significantly (`git log`). Otherwise follow exactly to minimize exploration/failures.

## Documented Errors & Resolutions

### CI Build Failure - Feb 13, 2026 (Workflow Run 21999277166)

**Error**: 
```
CMake Error at CMakeLists.txt:23 (add_subdirectory):
  The source directory
    /home/runner/work/viking-bio-matter/viking-bio-matter/libs/pico-lfs
  does not contain a CMakeLists.txt file.
```

**Root Cause**: The `actions/checkout@v6` in `.github/workflows/build-firmware.yml` did not have `submodules: recursive` parameter, so the `libs/pico-lfs` git submodule was not initialized during checkout.

**Resolution**: 
1. Added `submodules: recursive` to line 30 of `.github/workflows/build-firmware.yml`
2. Verified fix: Subsequent workflow runs succeeded (e.g., run 21999606656, 22016393422)

**Key Lesson**: This repository has **one git submodule** (`libs/pico-lfs`) that MUST be initialized before building. The submodule itself has a nested submodule (`littlefs`). Always use `git submodule update --init --recursive` when cloning or in CI.

**Validation**:
```bash
# Check submodule status (should show commit hash, not '-')
git submodule status

# Should output (no leading '-'):
# 8cddb6847393f67ba20a494bb318d3c7564a0a18 libs/pico-lfs (heads/master)

# Verify CMakeLists.txt exists
ls -la libs/pico-lfs/CMakeLists.txt
```

### Other Known Issues

**Pico SDK Submodules**: If you see "lwip.h: No such file or directory" or similar mbedTLS errors, the Pico SDK submodules are not initialized. Run `cd pico-sdk && git submodule update --init`.

**WiFi Credentials**: Hardcoded in `platform/pico_w_chip_port/network_adapter.cpp` lines 10-11. For production, use SoftAP commissioning (192.168.4.1, SSID: VikingBio-Setup, Password: vikingbio2026) or environment variables. Never commit real credentials.

**Firmware Size Changes**: If firmware size changes significantly (>±20KB text), verify:
- No accidental debug code left enabled
- Optimization flags intact in CMakeLists.txt lines 31-37 (`-O3`, `-ffast-math`, etc.)
- No unnecessary libraries linked
- LittleFS included (adds ~100KB but required for storage)
