# Viking Bio Matter Bridge - Coding Agent Instructions

## Quick Reference

**Document Status**: Validated February 2026 | **Last Major Update**: Feb 22, 2026  
**Repository**: [phieri/viking-bio-matter](https://github.com/phieri/viking-bio-matter)  
**Purpose**: Matter bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W

### Critical Information for New Agents

| Category | Key Information |
|----------|-----------------|
| **Build Time** | ~32s total (SDK init once, then 26s builds) |
| **Firmware Size** | ~590KB text, ~75KB bss (±20KB variance OK); artifacts versioned as `viking_bio_matter-<git-version>.*` |
| **CRITICAL Requirement** | Initialize submodules: `git submodule update --init --recursive` |
| **Environment Variable** | `PICO_SDK_PATH` MUST be absolute path, set before cmake |
| **Target Board** | Pico W ONLY (not standard Pico) - WiFi required |
| **Pico SDK Version** | 2.2.0 (mbedTLS 3.6.2) |
| **CYW43 Arch** | `pico_cyw43_arch_lwip_poll` MUST be used — `threadsafe_background` hangs in `cyw43_arch_init()` on this hardware |
| **WiFi Provisioning** | Via BLE commissioning (chip-tool) — NO hardcoded credentials in source |
| **CI Build Dir** | `build-matter/` (not `build/`) |
| **Security Status** | ✅ All critical issues fixed (Feb 15, 2026) |

### Common Errors → Quick Fix

| Error Message | Quick Fix |
|---------------|-----------|
| "libs/pico-lfs does not contain CMakeLists.txt" | `git submodule update --init --recursive` |
| "PICO_SDK_PATH not set" | `export PICO_SDK_PATH=/absolute/path/to/pico-sdk` |
| "arm-none-eabi-gcc not found" | `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi` |
| "Missing lwip.h or mbedtls.h" | `cd pico-sdk && git submodule update --init` |
| "This firmware requires PICO_BOARD=pico_w" | Use Pico W, not standard Pico |
| Device hangs at startup / WiFi init stall | Ensure `pico_cyw43_arch_lwip_poll` is linked (not threadsafe_background) in **all** CMakeLists |
| BTstack infinite scan / LittleFS corruption at init | Ensure `ble_adapter_init()` overrides TLV backend after `storage_adapter_init()` |
| LWIP_ASSERT panic in `mdns_resp_init()` | Set `LWIP_NUM_NETIF_CLIENT_DATA=2` in lwipopts.h |

## Getting Started (First-Time Agents)

**Quick Start Workflow**:
1. ✅ Install prerequisites (ARM toolchain, CMake)
2. ✅ Clone and initialize **both** Pico SDK and repository submodules (CRITICAL)
3. ✅ Set `PICO_SDK_PATH` to absolute path (CRITICAL)
4. ✅ Build to verify setup (`mkdir build-matter && cd build-matter && cmake .. && make -j$(nproc)`)
5. ✅ Check firmware size is ~590KB text, ~75KB bss
6. ✅ Explore codebase with helper scripts (`./run.sh`, `./setup.sh`, `tools/`)

**Common First-Time Mistakes**:
- ❌ Forgetting `git submodule update --init --recursive` → CMake fails with "libs/pico-lfs does not contain CMakeLists.txt"
- ❌ Setting relative `PICO_SDK_PATH` → CMake fails to find SDK
- ❌ Using standard Pico instead of Pico W → "This firmware requires PICO_BOARD=pico_w"
- ❌ Linking `pico_cyw43_arch_lwip_threadsafe_background` anywhere → hangs in `cyw43_arch_init()`
- ❌ Not initializing Pico SDK submodules → Missing lwIP/mbedTLS headers
- ❌ Hardcoding WiFi credentials in source → never do this; use BLE commissioning instead

## Repository Overview

**Purpose**: Matter (CHIP) bridge firmware for Viking Bio 20 burner on Raspberry Pi Pico W. Reads TTL serial (9600 baud) and exposes flame/fan/temp via Matter over WiFi.

**Languages**: C (firmware), C++ (Matter platform port), Python (tools)  
**Target**: RP2040/Pico W | **Framework**: Pico SDK 2.2.0, minimal Matter implementation (`src/matter_minimal/`, no external SDK)

**Key Features**:
- Matter protocol (no external SDK) — `ENABLE_MATTER=1` compile definition (always enabled)
- LittleFS storage with wear leveling (last 256KB flash, `libs/pico-lfs` submodule)
- BLE commissioning for WiFi provisioning (BTstack — fully functional, `pico_btstack_ble` + `pico_btstack_cyw43` linked)
- BTstack persistent state stored in LittleFS via custom `btstack_tlv_littlefs` backend (no dedicated BTstack flash bank)
- Cooperative single-threaded poll loop on Core 0 — `pico_multicore` is NOT linked
- Event-driven main loop with 1-second periodic timer and event flags
- Interrupt-driven serial with circular buffer, 30-second stale data timeout
- SHA256-based Matter PIN derivation per device MAC

## Build Commands (Validated Feb 2026)

### Prerequisites (ALWAYS run first on fresh environment)
```bash
sudo apt-get update && sudo apt-get install -y cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Clone Pico SDK (use exact version for compatibility)
git clone --depth 1 --branch 2.2.0 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git submodule update --init && cd ..  # ~30s

# Set PICO_SDK_PATH (CRITICAL: Must be absolute path)
export PICO_SDK_PATH=$(pwd)/pico-sdk  # CRITICAL: Must be absolute path, set before cmake

# Clone repository and initialize ALL submodules
git clone https://github.com/phieri/viking-bio-matter.git
cd viking-bio-matter
git submodule update --init --recursive  # CRITICAL: Initializes libs/pico-lfs
```

**Verified Versions**: CMake 3.17+, ARM GCC 13.2.1, Pico SDK 2.2.0

### Build
```bash
# CRITICAL: Initialize repository submodules first!
git submodule update --init --recursive  # Initializes libs/pico-lfs

mkdir build-matter && cd build-matter
export PICO_SDK_PATH=/absolute/path/to/pico-sdk  # REQUIRED
cmake .. && make -j$(nproc)
```

**Timing** (8 cores): Submodule init 5s | SDK init 30s (one-time) | CMake 1.3s | Build 26s | Incremental ~1s  
**Artifacts** (`build-matter/`): `viking_bio_matter-<git-version>.uf2`, `.elf`, `.bin`, `.hex`  
Note: Artifact filenames always include the git version tag (e.g., `viking_bio_matter-abc1234.uf2`). Use `ls *.uf2` to find them.  
**Size**: text ~590KB, bss ~75KB, data 0KB (±20KB variance acceptable)

### Pre-Commit Validation (ALL MUST PASS)

```bash
export PICO_SDK_PATH=/absolute/path/to/pico-sdk
git submodule update --init --recursive
rm -rf build-matter && mkdir build-matter && cd build-matter
cmake .. && make -j$(nproc)
arm-none-eabi-size viking_bio_matter-*.elf   # text ~590KB, bss ~75KB
cd ..
git diff | grep -i -E "password|ssid|secret|key" || echo "OK"  # must output "OK"
git diff | grep -E "TODO|HACK|FIXME"   # only OK: crypto_adapter.cpp CTR_DRBG (non-critical)
git submodule status  # all initialized (no leading '-')
```

### Common Build Errors

1. **"PICO_SDK_PATH not set"** → `export PICO_SDK_PATH=/absolute/path` (verify: `echo $PICO_SDK_PATH`)
2. **"arm-none-eabi-gcc not found"** → `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi`
3. **"This firmware requires PICO_BOARD=pico_w"** → Only Pico W supported (WiFi required)
4. **Missing lwIP/mbedTLS headers** → `cd pico-sdk && git submodule update --init` (~30s)
5. **"libs/pico-lfs does not contain a CMakeLists.txt file"** → `git submodule update --init --recursive`
6. **Device hangs at startup / cyw43_arch_init() never returns** → See WiFi Init Hang error below

## Project Structure

**Root**: CMakeLists.txt (enforces pico_w, defines ENABLE_MATTER=1), setup.sh, run.sh

**Source** (`src/`):
- `main.c` - Entry point, event-driven main loop on Core 0 (cooperative poll)
- `serial_handler.c` - UART0 RX (GP1), interrupt-driven, circular buffer (hardware_sync)
- `viking_bio_protocol.c` - Parser: binary `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]` or text `F:1,S:50,T:75\n`
- `matter_bridge.cpp` - Matter bridge: initializes platform, manages WiFi connect, updates attributes
- `version.c` - Firmware version information (git-describe based)
- `matter_minimal/` - Minimal Matter stack (TLV codec, UDP transport, PASE SPAKE2+, interaction model, clusters, DNS-SD)
  - `codec/` - TLV and message encoding/decoding
  - `transport/` - UDP transport layer with lwIP (links `pico_cyw43_arch_lwip_poll`)
  - `security/` - PASE (SPAKE2+) and session management (AES-128-CCM)
  - `interaction/` - Read handler, subscribe handler, report generator
  - `clusters/` - OnOff, LevelControl, Temperature, Descriptor, NetworkCommissioning
  - `discovery/` - DNS-SD/mDNS advertising (links `pico_cyw43_arch_lwip_poll`)

**Platform** (`platform/pico_w_chip_port/`):
- `network_adapter.cpp` - WiFi/lwIP STA mode (loads credentials from flash via storage_adapter)
- `ble_adapter.cpp` - BTstack BLE commissioning (fully functional; Matter UUID 0000FFF6-...)
- `btstack_tlv_littlefs.cpp` - BTstack TLV backend backed by LittleFS (keys stored as `/btk_XXXXXXXX`)
- `storage_adapter.cpp` - LittleFS storage (last 256KB flash), key-value pairs, wear leveling
- `crypto_adapter.cpp` - mbedTLS wrapper (CTR_DRBG wrapper stubbed — hw RNG works, SHA256/AES work)
- `platform_manager.cpp` - Platform init coordinator, discriminator management
- `matter_attributes.cpp/.h` - Thread-safe attribute store (cooperative model, no locks)
- `matter_reporter.cpp/.h` - Attribute reporting/subscription notifications
- `matter_network_transport.cpp/.h` - Network-layer Matter message transport
- `matter_network_subscriber.cpp/.h` - Subscription handling
- `config/` - lwipopts.h, mbedtls_config.h, btstack_config.h
- `CHIPDevicePlatformConfig.h` - Matter device config (PIN from MAC, discriminator from storage)

**Tests** (`tests/`):
- `codec/` - TLV tests (host-runnable via CMake on non-Pico platform)
- `transport/`, `security/`, `interaction/`, `clusters/`, `storage/` - require Pico W hardware

**Tools & Examples**:
- `tools/derive_pin.py` - Generate Matter PIN from MAC address (SHA256-based)
- `examples/viking_bio_simulator.py` - Serial simulator for testing without hardware

**Libraries** (`libs/`):
- `pico-lfs/` - **Git submodule** (LittleFS port for Pico, MUST be initialized with `--recursive`)

**Documentation** (`docs/`):
- `MINIMAL_MATTER_ARCHITECTURE.md`, `BLE_COMMISSIONING_SUMMARY.md`, `SUBSCRIPTIONS_TESTING.md`, `DNS_SD_IMPLEMENTATION.md`

## Initialization Order (CRITICAL)

The initialization sequence in `main.c` and `platform_manager_init()` must be followed exactly:

```
main():
  stdio_init_all()
  sleep_ms(8000)          ← 8-second startup delay (USB enumeration + hardware settling)
  version_print_info()
  viking_bio_init()
  serial_handler_init()
  matter_bridge_init()    ← calls platform_manager_init() internally
    └─ platform_manager_init():
         Step 1/4: crypto_adapter_init()
         Step 2/4: storage_adapter_init()   ← LittleFS mounted; discriminator loaded/generated
         Step 3/4: network_adapter_init()   ← cyw43_arch_init() + btstack_cyw43_init()
                   ble_adapter_init()       ← OVERRIDES TLV backend with LittleFS backend
                   dns_sd_init()
         Step 4/4: matter_attributes_init() + register clusters
  watchdog_enable(8000ms)
  add_repeating_timer_ms(1000, periodic_timer_callback)
  [main loop: cyw43_arch_poll() → serial → Matter → periodic checks]
```

**Why this order matters**:
- `storage_adapter_init()` (LittleFS) MUST come before `ble_adapter_init()`
- `ble_adapter_init()` overrides the default BTstack TLV backend (set by `cyw43_arch_init()`) with the LittleFS-backed backend — if reversed, BTstack scans corrupt data and hangs indefinitely

## GPIO & Protocols

**Pins**: GP0=UART TX (unused), GP1=UART RX (Viking Bio input), LED=CYW43 chip (not GPIO 25 — use `cyw43_arch_gpio_put`)

**Serial**: Binary 6 bytes `[0xAA][FLAGS][SPEED][TEMP_H][TEMP_L][0x55]` or text `F:1,S:50,T:75\n`  
**Matter clusters** on endpoint 1:
- OnOff (0x0006) — flame bool
- LevelControl (0x0008) — fan speed 0-100%
- TemperatureMeasurement (0x0402) — centidegrees int16
- Diagnostics — TotalOperationalHours, DeviceEnabledState, NumberOfActiveFaults

**Commissioning**: PIN derived from MAC (`SHA256(MAC||"VIKINGBIO-2026") % 100000000`), discriminator randomly generated on first boot (0xF00-0xFFF, saved to flash), tool: `python3 tools/derive_pin.py <MAC>`

## WiFi Provisioning

WiFi credentials are **never hardcoded**. They are provisioned via BLE commissioning and stored in flash:

```bash
chip-tool pairing ble-wifi <node-id> <ssid> <password> <discriminator> <pin>
```

After successful commissioning, credentials are saved to flash in LittleFS (`/wifi_credentials` key). On subsequent boots, the firmware auto-connects using stored credentials.

**Test Simulator**: `pip3 install pyserial && python3 examples/viking_bio_simulator.py /dev/ttyUSB0`  
**Monitor Device**: `screen /dev/ttyACM0 115200` or `./run.sh monitor`  
**Helper Scripts**: `./setup.sh` (setup wizard), `./run.sh monitor|simulate|commissioning`

## CYW43 / lwIP Architecture (CRITICAL)

**Use `pico_cyw43_arch_lwip_poll` everywhere** — NOT `pico_cyw43_arch_lwip_threadsafe_background`.

This applies to:
- Top-level CMakeLists.txt `target_link_libraries`
- `src/matter_minimal/transport/CMakeLists.txt`
- `src/matter_minimal/discovery/CMakeLists.txt`

If any sub-library links `threadsafe_background`, it transitively overrides the top-level choice and re-introduces the startup hang.

**Why**: `threadsafe_background` claims a hardware alarm and user IRQ in its async-context setup. On this hardware this causes `cyw43_arch_init()` to hang indefinitely regardless of initialization ordering.

**Main loop requirement**: Call `cyw43_arch_poll()` at the top of every main loop iteration to drive CYW43 and lwIP processing.

## BTstack / BLE Architecture

BTstack (`pico_btstack_ble` + `pico_btstack_cyw43`) is **fully linked** and functional.

**Key design**: BTstack persistent state is stored in LittleFS via the custom `btstack_tlv_littlefs` backend (file `platform/pico_w_chip_port/btstack_tlv_littlefs.cpp`). Keys stored as `/btk_XXXXXXXX`.

**Why not the default flash-bank TLV**: The default BTstack TLV backend (`btstack_tlv_flash_bank`) scans from the beginning of its flash region. When it encounters LittleFS data (which occupies the last 256KB), the scan never terminates → infinite loop at WiFi init. The LittleFS-backed TLV eliminates this by storing BTstack data alongside LittleFS files.

**PICO_FLASH_BANK_TOTAL_SIZE / PICO_FLASH_BANK_STORAGE_OFFSET are intentionally NOT defined** (see CMakeLists.txt). No dedicated BTstack flash-bank region is needed.

**BLE events**: Driven by `cyw43_arch_poll()` in the main loop (not a background thread).

## CI/CD

**Workflow**: `.github/workflows/build-firmware.yml`  
**Jobs**: build-matter (Ubuntu, ARM GCC, Pico SDK 2.2.0 w/submodules cached, cmake in `build-matter/`, make ~26s, uploads artifacts)  
**Triggers**: push main/develop, PR to main, manual  
**Artifacts**: Actions tab → run → "viking-bio-matter-firmware-matter"  
**Paths-ignore**: Workflow skips on `**.md` and `docs/**` changes only (lines 6-13); manual `workflow_dispatch` always runs

**CI checklist** — all must be true for CI to pass:
- `PICO_SDK_PATH` set (workflow line 54)
- ARM toolchain installed (lines 32-35)
- Pico SDK submodules initialized (`submodules: recursive` on SDK checkout, line 51)
- **Repository submodules initialized** (`submodules: recursive` on repo checkout, line 30)

## Key Facts

1. **ENABLE_MATTER=1**: IS defined as a compile definition in CMakeLists.txt. Matter is always compiled in; firmware requires Pico W.
2. **Single-threaded**: `pico_multicore` is NOT linked. All tasks run on Core 0 via cooperative polling. `matter_bridge_task()` is called from the main loop.
3. **Event-driven loop**: Uses `volatile uint32_t event_flags` with `EVENT_SERIAL_DATA`, `EVENT_MATTER_MSG`, `EVENT_TIMEOUT_CHECK`, `EVENT_LED_UPDATE`. 1-second repeating timer sets timeout/LED flags and wakes CPU via `__sev()`.
4. **Interrupt serial**: `hardware_sync` lib (save/restore_interrupts), circular buffer with 30-second stale data timeout.
5. **PIN per device**: `SHA256(MAC||"VIKINGBIO-2026") % 100000000`, tool: `python3 tools/derive_pin.py <MAC>`
6. **No OTA**: Physical USB only (BOOTSEL + copy .uf2)
7. **Flash**: Last 256KB for LittleFS (Matter storage, WiFi creds, BTstack state), wear leveling via pico-lfs submodule
8. **RAM limits**: 264KB → max 5 Matter fabrics. Hardware RNG (`get_rand_32()`) works; mbedTLS CTR_DRBG wrapper stubbed (not needed).
9. **Optimizations**: `-O3 -ffast-math -fno-signed-zeros -fno-trapping-math -funroll-loops`, LTO off (SDK wrapper issues)
10. **Discriminator**: Random 0xF00-0xFFF on first boot, persisted to flash at `/discriminator`
11. **lwIP config**: `MEM_SIZE=8KB` required to avoid mDNS init failures; `LWIP_NUM_NETIF_CLIENT_DATA=2` required (MLD6 takes slot 0, mDNS takes slot 1)
12. **DNS-SD**: Instance names are 16-char uppercase hex from fresh 64-bit hardware RNG each advertising start
13. **Versioned artifacts**: Post-build CMake renames all output files to `viking_bio_matter-<git-version>.*`

## Common Code Changes

1. **Serial Protocol**: Edit `src/viking_bio_protocol.c` + `include/viking_bio_protocol.h`, update `viking_bio_data_t`, test with `python3 examples/viking_bio_simulator.py /dev/ttyUSB0`
2. **Matter Clusters**: Edit `src/matter_bridge.cpp` + `platform/pico_w_chip_port/platform_manager.cpp` (register attributes), clean rebuild
3. **GPIO**: Edit `include/serial_handler.h` + `src/serial_handler.c` + README.md
4. **WiFi**: Provisioned via BLE commissioning — no source changes needed for credentials

## Testing & Validation

**Unit Tests**: Most tests require Pico W hardware. TLV codec tests (`tests/codec/`) can run on host via CMake on non-Pico platform.

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
- `./setup.sh` - Interactive setup wizard
- `./run.sh monitor` - Monitor device serial output
- `./run.sh simulate` - Run Viking Bio simulator
- `./run.sh commissioning` - Show Matter commissioning info
- `tools/derive_pin.py <MAC>` - Generate Matter PIN from device MAC address

## Troubleshooting Guide

### Build Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| "PICO_SDK_PATH not set" | Environment variable not exported | `export PICO_SDK_PATH=/absolute/path/to/pico-sdk` (must be absolute) |
| "arm-none-eabi-gcc not found" | ARM toolchain not installed | `sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi` |
| "This firmware requires PICO_BOARD=pico_w" | Wrong board selected | Only Pico W supported (WiFi required) |
| "Missing lwip.h" or "mbedtls.h" | Pico SDK submodules not initialized | `cd pico-sdk && git submodule update --init` |
| "libs/pico-lfs does not contain CMakeLists.txt" | Repository submodules not initialized | `git submodule update --init --recursive` |
| Build succeeds but size changed significantly | Optimization flags or dependencies changed | Check CMakeLists.txt optimization flags and target_link_libraries |
| Linker errors about undefined references | Missing library dependencies | Ensure pico-lfs/mbedtls/lwip/btstack linked in CMakeLists.txt |

### Runtime Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| Device hangs at startup (WiFi init) | Wrong CYW43 arch variant | Ensure **all** CMakeLists link `pico_cyw43_arch_lwip_poll` (not threadsafe_background) |
| Device not connecting to WiFi | No credentials provisioned yet | Use BLE commissioning: `chip-tool pairing ble-wifi <node> <ssid> <password> <discriminator> <pin>` |
| Serial data not received | Wrong GPIO or baud rate | Verify GP1/UART0 at 9600 baud, check wiring (requires 5V→3.3V level shifter) |
| Matter pairing fails | Wrong PIN or discriminator | Use `tools/derive_pin.py <MAC>` for PIN; discriminator printed on boot serial output |
| Device crashes after WiFi connect | RAM exhaustion | Check fabric count (max 5), check `CHIPDevicePlatformConfig.h` |
| Flash storage errors | Wear leveling exhausted | Flash has limited write cycles; storage uses last 256KB with LittleFS |
| Stale data in Matter | Viking Bio not sending data | Firmware auto-clears attributes after 30s timeout |
| LWIP_ASSERT panic in mdns_resp_init | LWIP_NUM_NETIF_CLIENT_DATA too low | Set `LWIP_NUM_NETIF_CLIENT_DATA=2` in lwipopts.h |
| mDNS init failure / memory errors | MEM_SIZE too small | Set `MEM_SIZE=(8*1024)` in lwipopts.h |

### CI/CD Issues

| Symptom | Root Cause | Solution |
|---------|------------|----------|
| CI fails with "CMakeLists.txt not found" | Submodules not checked out | Add `submodules: recursive` to `actions/checkout@v6` |
| CI skips on code changes | paths-ignore too broad | Check `.github/workflows/build-firmware.yml` lines 6-13 |
| SDK cache miss every run | Cache key doesn't match | Verify `PICO_SDK_VERSION` env var matches SDK tag (2.2.0) |
| Build timeout in CI | Build too slow or hanging | Check for infinite loops; verify `-j$(nproc)` used; check CYW43 arch |

## Architecture Summary

**Data Flow**: Viking Bio 20 → UART0 (9600 baud, GP1) → Interrupt handler → Circular buffer → Protocol parser → Matter bridge → Minimal Matter stack (`src/matter_minimal/`) → WiFi/lwIP → Matter controller

**Main Loop (Core 0 only)**:
```
while(true):
  watchdog_update()
  cyw43_arch_poll()           ← drives WiFi/lwIP/BTstack
  serial_handler_task()
  if serial data: parse → update Matter attributes
  matter_bridge_task()        ← process incoming Matter messages
  if EVENT_TIMEOUT_CHECK: check stale data, check BLE stop condition
  if idle: sleep up to 100ms (dynamic)
```

**Platform init order** (see Initialization Order section for full detail):
`crypto → storage/LittleFS → network/CYW43 → BLE (overrides TLV) → DNS-SD → Matter attributes`

**Limitations**: No OTA, WiFi only (no Thread/Ethernet), last 256KB flash for LittleFS (shared: Matter, WiFi creds, BTstack TLV), max 5 fabrics (264KB RAM), discriminator testing range 0xF00-0xFFF, CTR_DRBG wrapper stubbed (hw RNG sufficient), requires 5V→3.3V level shifter for serial

## Trust & Verify

These instructions validated Feb 22, 2026 via code inspection. Only search/explore if: incomplete for your task, errors not documented, or `git log` shows significant recent changes. Key files to check for latest state: `CMakeLists.txt`, `src/main.c`, `platform/pico_w_chip_port/platform_manager.cpp`.

## Recent Improvements & Changes

### WiFi Init Hang Fix & BTstack TLV Backend (Feb 2026)

Multiple related issues were resolved together:

**Problem 1: `cyw43_arch_init()` hanging indefinitely**
- Root cause: `pico_cyw43_arch_lwip_threadsafe_background` claims a hardware alarm and user IRQ in its async-context setup, causing an infinite wait on this hardware regardless of init order.
- Fix: Use `pico_cyw43_arch_lwip_poll` in top-level CMakeLists.txt AND in all sub-library CMakeLists (transport, discovery). Sub-library linkage of threadsafe_background overrides the top-level choice transitively.

**Problem 2: BTstack infinite scan on startup**
- Root cause: Default `btstack_tlv_flash_bank` scans LittleFS data at startup and never terminates (LittleFS data looks like garbage to BTstack).
- Fix: New `btstack_tlv_littlefs.cpp` backend stores BTstack TLV tags as LittleFS files (`/btk_XXXXXXXX`). `ble_adapter_init()` overrides the default TLV backend with this implementation. `PICO_FLASH_BANK_TOTAL_SIZE` / `PICO_FLASH_BANK_STORAGE_OFFSET` are intentionally NOT defined.

**Problem 3: `pico_multicore` linking hang**
- Root cause: Linking `pico_multicore` enabled `ASYNC_CONTEXT_THREADSAFE_BACKGROUND_MULTI_CORE=1`, causing `cyw43_arch_init()` to hang.
- Fix: Removed `pico_multicore`. All tasks run on Core 0 via cooperative polling.

### Main Loop Architecture (Feb 2026)

- Replaced simple polling loop with event-driven architecture using `volatile uint32_t event_flags`
- 1-second repeating hardware timer sets `EVENT_TIMEOUT_CHECK` and `EVENT_LED_UPDATE`
- `__sev()` wakes CPU from WFE when events arrive from interrupt context
- Dynamic sleep (up to 100ms) when no work pending for power efficiency
- LED behavior: 200ms tick on serial data; constant on when WiFi+commissioned but no data; off otherwise

### Platform Architecture (Feb 2026)

New platform source files added:
- `matter_attributes.cpp/.h` — cooperative single-threaded attribute store (no lock-based sync)
- `matter_reporter.cpp/.h` — attribute reporting to Matter subscribers
- `matter_network_transport.cpp/.h` — network-layer Matter message transport
- `matter_network_subscriber.cpp/.h` — subscription session management
- `btstack_tlv_littlefs.cpp/.h` — LittleFS-backed BTstack TLV backend
- `src/version.c` — firmware version (git-describe, embedded at compile time)

Matter attribute store now includes Diagnostics cluster: TotalOperationalHours, DeviceEnabledState, NumberOfActiveFaults.

### Versioned Firmware Artifacts (Feb 2026)

CMakeLists.txt now renames all output files post-build:
```
viking_bio_matter.uf2  →  viking_bio_matter-<git-version>.uf2
```
Use `ls build-matter/*.uf2` to find the artifact. The `.elf` size measurement command must use a wildcard: `arm-none-eabi-size build-matter/viking_bio_matter-*.elf`.

### Pico SDK Upgrade to 2.2.0 (Feb 16, 2026)
- Upgraded from Pico SDK 1.5.1 to 2.2.0 (latest stable release)
- Updated mbedTLS from 2.x to 3.6.2; added `MBEDTLS_ALLOW_PRIVATE_ACCESS` for ECP structure access
- CI/CD updated with SDK caching at version 2.2.0

### BLE Commissioning (Feb 16, 2026)
- BLE is fully functional (NOT a stub); uses BTstack with LittleFS-backed TLV
- Auto-starts BLE advertising on boot when no WiFi credentials in flash
- Auto-stops BLE when WiFi connected AND device commissioned
- Matter-compliant BLE service UUID 0000FFF6-0000-1000-8000-00805F9B34FB

### Security Hardening (Feb 15, 2026)
- All 11 critical, 5 high-priority, 6 medium-priority security issues resolved
- PASE upgraded to full SPAKE2+ with proper elliptic curve operations (`pA - w0*M`)
- Buffer overflow protections, race condition fixes, watchdog timer (8s timeout)
- See `SECURITY_NOTES.md` for detailed documentation

## Documented Errors & Resolutions

### CI Build Failure - Feb 13, 2026

**Error**: `CMake Error: The source directory .../libs/pico-lfs does not contain a CMakeLists.txt file`

**Root Cause**: `actions/checkout@v6` without `submodules: recursive` → `libs/pico-lfs` not initialized.

**Resolution**: Added `submodules: recursive` to repo checkout step in `.github/workflows/build-firmware.yml` (line 30).

**Validation**:
```bash
git submodule status  # should show commit hash, not leading '-'
ls -la libs/pico-lfs/CMakeLists.txt
```

### WiFi Initialization Hang (Fixed Feb 2026)

**Symptom**: Device hangs forever at `cyw43_arch_init()` — no USB output after startup message.

**Root Cause (A)**: `pico_cyw43_arch_lwip_threadsafe_background` linked in sub-library CMakeLists (transport or discovery) overrides the top-level `lwip_poll` choice.

**Root Cause (B)**: `pico_multicore` linked → enables multi-core background IRQ path in async context.

**Root Cause (C)**: BTstack TLV flash bank scan on startup before LittleFS init (infinite scan of LittleFS data).

**Resolution**:
1. All CMakeLists (top-level + sub-libraries) link only `pico_cyw43_arch_lwip_poll`
2. Removed `pico_multicore` from target_link_libraries
3. Added `btstack_tlv_littlefs` backend; `ble_adapter_init()` overrides TLV after `storage_adapter_init()` mounts LittleFS

**Lesson**: If startup hangs at WiFi init, immediately check: (1) `grep -r "threadsafe_background" */CMakeLists.txt` — must return nothing; (2) `grep "pico_multicore" CMakeLists.txt` — must return nothing; (3) initialization order (storage before BLE).

### LWIP_ASSERT Panic in mdns_resp_init (Fixed Feb 2026)

**Symptom**: Hard fault or assertion failure during Matter DNS-SD initialization.

**Root Cause**: `LWIP_NUM_NETIF_CLIENT_DATA` was 1. With `LWIP_IPV6=1` (MLD6 claims slot 0) and `LWIP_MDNS_RESPONDER=1` (mDNS claims slot 1), there is exactly one slot too few → `LWIP_ASSERT` in `mdns_resp_init()`.

**Resolution**: Set `LWIP_NUM_NETIF_CLIENT_DATA=2` in `platform/pico_w_chip_port/config/lwipopts.h`. Also set `MEM_SIZE=(8*1024)` to avoid mDNS startup allocation failures.

### PASE Security Fix - Feb 15, 2026

**Issue**: PASE used simplified SPAKE2+ (copied `pA` directly instead of computing `pA - w0*M`).

**Resolution**: Implemented proper elliptic curve point subtraction in `src/matter_minimal/security/pase.c`. See `SECURITY_NOTES.md` for details.

### Other Known Issues

**Crypto RNG**: `crypto_adapter_random()` wrapper returns -1 (CTR_DRBG stubbed). This is intentional — RP2040 hardware RNG (`get_rand_32()`) is used directly for all critical operations including PASE. No fix needed.

**Firmware Size Variance**: If size changes >±20KB, check: optimization flags in CMakeLists.txt (`-O3`, `-ffast-math`, etc.), no debug code left enabled, all required libraries linked.
