#include "matter_bridge.h"
#include <stdio.h>
#include <string.h>

#if MATTER_ENABLED

// Include Matter SDK headers
#include <app/server/Server.h>
#include <app/util/af.h>
#include <app/clusters/on-off-server/on-off-server.h>
#include <app/clusters/level-control/level-control.h>
#include <app/clusters/temperature-measurement-server/temperature-measurement-server.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <lib/support/CHIPMem.h>
#include <platform/CHIPDeviceLayer.h>
#include <setup_payload/QRCodeSetupPayloadGenerator.h>
#include <setup_payload/SetupPayload.h>

using namespace chip;
using namespace chip::app;
using namespace chip::DeviceLayer;

// Matter endpoint configuration
static constexpr EndpointId kBridgeEndpoint = 1;

// Current attribute values
static bool current_flame_state = false;
static uint8_t current_fan_speed = 0;
static int16_t current_temperature = 0;

bool matter_bridge_init(const char *setup_code, uint16_t discriminator) {
    printf("Initializing Matter bridge...\n");
    
    // Initialize CHIP memory
    CHIP_ERROR err = Platform::MemoryInit();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize CHIP memory: %s\n", ErrorStr(err));
        return false;
    }
    
    // Initialize Device Layer
    err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize CHIP stack: %s\n", ErrorStr(err));
        return false;
    }
    
    // Set device attestation credentials
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
    
    // Configure commissioning parameters
    static chip::CommonCaseDeviceServerInitParams initParams;
    (void)initParams.InitializeStaticResourcesBeforeServerInit();
    
    // Initialize server
    err = chip::Server::GetInstance().Init(initParams);
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize Matter server: %s\n", ErrorStr(err));
        return false;
    }
    
    // Print commissioning information
    chip::PayloadContents payload;
    payload.setUpPINCode = strtoul(setup_code, nullptr, 10);
    payload.discriminator = discriminator;
    
    char qr_code[256];
    if (GetQRCode(qr_code, sizeof(qr_code), payload) == CHIP_NO_ERROR) {
        printf("\n=== Matter Commissioning ===\n");
        printf("Setup Code: %s\n", setup_code);
        printf("Discriminator: %u\n", discriminator);
        printf("QR Code: %s\n", qr_code);
        printf("===========================\n\n");
    }
    
    printf("Matter bridge initialized successfully\n");
    return true;
}

void matter_bridge_shutdown(void) {
    printf("Shutting down Matter bridge...\n");
    chip::Server::GetInstance().Shutdown();
    PlatformMgr().Shutdown();
    Platform::MemoryShutdown();
}

void matter_bridge_run_event_loop(uint32_t timeout_ms) {
    // Run the Matter event loop
    PlatformMgr().RunEventLoop();
}

void matter_bridge_update_flame(bool flame_on) {
    if (current_flame_state != flame_on) {
        current_flame_state = flame_on;
        printf("Matter: Flame state changed to %s\n", flame_on ? "ON" : "OFF");
        
        // Update On/Off cluster attribute
        Clusters::OnOff::Attributes::OnOff::Set(kBridgeEndpoint, flame_on);
    }
}

void matter_bridge_update_fan_speed(uint8_t speed) {
    if (current_fan_speed != speed) {
        current_fan_speed = speed;
        printf("Matter: Fan speed changed to %u%%\n", speed);
        
        // Map 0-100% to 0-254 for Level Control cluster
        uint8_t level = (speed * 254) / 100;
        Clusters::LevelControl::Attributes::CurrentLevel::Set(kBridgeEndpoint, level);
    }
}

void matter_bridge_update_temperature(int16_t temperature) {
    if (current_temperature != temperature) {
        current_temperature = temperature;
        printf("Matter: Temperature changed to %d°C\n", temperature);
        
        // Temperature in Matter is in hundredths of degrees Celsius
        int16_t matter_temp = temperature * 100;
        Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(kBridgeEndpoint, matter_temp);
    }
}

bool matter_bridge_is_commissioned(void) {
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

bool matter_bridge_get_qr_code(char *buffer, size_t buffer_size) {
    if (buffer == NULL || buffer_size == 0) {
        return false;
    }
    
    chip::PayloadContents payload;
    // These would be configured from setup parameters
    payload.setUpPINCode = 20202021;
    payload.discriminator = 3840;
    
    return GetQRCode(buffer, buffer_size, payload) == CHIP_NO_ERROR;
}

#else // !MATTER_ENABLED

// Stub implementations when Matter is not enabled
bool matter_bridge_init(const char *setup_code, uint16_t discriminator) {
    printf("Matter bridge not enabled (ENABLE_MATTER not defined)\n");
    printf("This is a stub implementation. To enable Matter:\n");
    printf("  1. Install connectedhomeip SDK\n");
    printf("  2. Set MATTER_ROOT environment variable\n");
    printf("  3. Build with cmake -DENABLE_MATTER=ON\n");
    return true;
}

void matter_bridge_shutdown(void) {
    // Stub
}

void matter_bridge_run_event_loop(uint32_t timeout_ms) {
    // Stub - sleep to avoid busy loop
    usleep(timeout_ms * 1000);
}

void matter_bridge_update_flame(bool flame_on) {
    printf("Matter stub: Flame state = %s\n", flame_on ? "ON" : "OFF");
}

void matter_bridge_update_fan_speed(uint8_t speed) {
    printf("Matter stub: Fan speed = %u%%\n", speed);
}

void matter_bridge_update_temperature(int16_t temperature) {
    printf("Matter stub: Temperature = %d°C\n", temperature);
}

bool matter_bridge_is_commissioned(void) {
    return false;
}

bool matter_bridge_get_qr_code(char *buffer, size_t buffer_size) {
    if (buffer && buffer_size > 0) {
        strncpy(buffer, "MT:STUB000000000000", buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
    }
    return false;
}

#endif // MATTER_ENABLED
