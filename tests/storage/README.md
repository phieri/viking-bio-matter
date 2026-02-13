# Storage Adapter Tests

## Overview

This directory contains test cases for the LittleFS-based storage adapter. These tests validate key operations including:

- Basic write and read operations
- Overwriting existing keys
- Deleting keys
- WiFi credential storage
- Multiple key handling
- Storage clearing
- Power cycle persistence

## Hardware Requirements

**Important:** These tests require Pico W hardware and cannot run on a host PC because they:
- Use LittleFS with actual flash memory operations
- Require the Pico's flash memory hardware
- Test wear leveling behavior specific to physical flash

## Running Tests

### Option 1: Add to Main Firmware (Recommended)

1. Include the test file in your build by adding to `CMakeLists.txt`:
   ```cmake
   set(SOURCES
       # ... existing sources ...
       tests/storage/test_storage_adapter.c
   )
   ```

2. Add the test directory to includes:
   ```cmake
   target_include_directories(viking_bio_matter PRIVATE
       # ... existing includes ...
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/storage
   )
   ```

3. In `src/main.c`, call the test runner:
   ```c
   #ifdef PICO_PLATFORM
   extern int run_all_storage_tests(void);
   #endif
   
   int main() {
       stdio_init_all();
       
       #ifdef PICO_PLATFORM
       // Run storage tests
       run_all_storage_tests();
       #endif
       
       // ... rest of main code ...
   }
   ```

4. Build and flash firmware:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   # Copy viking_bio_matter.uf2 to Pico W in BOOTSEL mode
   ```

5. Monitor USB serial output (115200 baud) to see test results.

### Option 2: Manual Testing

Alternatively, you can manually test each function by:

1. Flash the normal firmware
2. Use the USB serial interface or add debug commands
3. Call each storage function and verify behavior

## Test Coverage

### Test 1: Basic Write and Read
- Writes a 5-byte value to a key
- Reads it back and verifies data integrity
- **Validates:** Basic LittleFS file operations

### Test 2: Overwrite Existing Key
- Writes data to a key
- Overwrites with different data
- Verifies only the latest data is present
- **Validates:** LittleFS overwrite capability with wear leveling

### Test 3: Delete Key
- Writes data to a key
- Deletes the key
- Verifies key no longer exists
- **Validates:** LittleFS file deletion

### Test 4: WiFi Credentials Save and Load
- Saves WiFi SSID and password
- Loads them back
- Verifies exact match
- **Validates:** High-level WiFi credential API

### Test 5: Multiple Keys
- Writes three different keys with different data sizes
- Reads each back independently
- Verifies all data is correct
- **Validates:** LittleFS multi-file capability

### Test 6: Clear All Storage
- Writes data
- Clears entire storage
- Verifies data is gone
- **Validates:** LittleFS format operation

### Test 7: Power Cycle Persistence
- Writes data
- Simulates power cycle (note: requires actual power cycle for full test)
- Verifies data persists
- **Validates:** Data persistence and wear leveling across power cycles

## Expected Output

When tests run successfully, you should see output like:

```
╔═══════════════════════════════════════════════╗
║   LittleFS Storage Adapter Test Suite        ║
╚═══════════════════════════════════════════════╝

Initializing storage...
✓ Storage initialized successfully

=== Test: Basic Write and Read ===
✓ PASSED

=== Test: Overwrite Existing Key ===
✓ PASSED

... (more tests) ...

╔═══════════════════════════════════════════════╗
║              Test Summary                     ║
╠═══════════════════════════════════════════════╣
║  Total Tests:  7                              ║
║  Passed:       7                              ║
║  Failed:       0                              ║
╚═══════════════════════════════════════════════╝
```

## Debugging Tests

If tests fail:

1. **Check LittleFS initialization:**
   - Verify storage offset doesn't overlap with code
   - Check PICO_SDK_PATH is set correctly
   - Ensure pico-lfs submodule is initialized

2. **Monitor flash usage:**
   - Use `arm-none-eabi-size viking_bio_matter.elf`
   - Ensure code + data < 1.75MB (leaving 256KB for storage)

3. **Check for flash conflicts:**
   - Verify no other code writes to the storage region
   - Check STORAGE_FLASH_OFFSET in storage_adapter.cpp

4. **Enable LittleFS debug:**
   - Remove `LFS_NO_DEBUG=1` from CMakeLists.txt
   - Rebuild and check detailed LittleFS logs

## Wear Leveling Validation

To fully validate wear leveling:

1. Write to the same key many times (e.g., 10,000+ iterations)
2. Monitor flash wear using LittleFS wear statistics
3. Verify data is being written to different physical blocks
4. Confirm no single block is excessively worn

Example stress test (add to your code):

```c
void stress_test_wear_leveling(void) {
    uint8_t data[32];
    for (int i = 0; i < 10000; i++) {
        memset(data, i & 0xFF, sizeof(data));
        storage_adapter_write("stress_test", data, sizeof(data));
        if (i % 1000 == 0) {
            printf("Iteration %d\n", i);
        }
    }
}
```

## Power Cycle Testing

For Test 7 (Power Cycle Persistence), perform these manual steps:

1. Run the firmware with Test 7 enabled
2. Let it write data
3. **Physically power cycle** the Pico W (disconnect and reconnect USB)
4. Run a simple read test on boot to verify data persists
5. This validates both LittleFS persistence and wear leveling effectiveness

## Notes

- Tests modify flash memory - run on development boards only
- Each test run may take a few seconds due to flash operations
- Test 6 (Clear All) erases all stored data
- For production, consider running tests only during initial device provisioning
