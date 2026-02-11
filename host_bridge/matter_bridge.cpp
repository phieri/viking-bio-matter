#include "matter_bridge.h"
#include <cstdio>
#include <cstring>
#include <unistd.h>

// Conditional Matter SDK includes
#ifdef ENABLE_MATTER
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <app/EventLogging.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#include <lib/core/CHIPError.h>
#include <lib/support/CHIPMem.h>
#include <platform/CHIPDeviceLayer.h>
#include <platform/PlatformManager.h>

using namespace chip;
using namespace chip::app;
using namespace chip::Credentials;
using namespace chip::DeviceLayer;
#endif

MatterBridge::MatterBridge() 
    : initialized_(false)
    , running_(false)
    , flame_state_(false)
    , fan_speed_(0)
    , temperature_(0)
{
}

MatterBridge::~MatterBridge() {
    stop();
}

bool MatterBridge::init(const char* setup_code) {
#ifdef ENABLE_MATTER
    printf("Initializing Matter bridge...\n");

    // Initialize CHIP stack
    CHIP_ERROR err = Platform::MemoryInit();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize memory: %s\n", ErrorStr(err));
        return false;
    }

    err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize CHIP stack: %s\n", ErrorStr(err));
        return false;
    }

    // Initialize device attestation config
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());

    // Initialize Matter server
    static chip::CommonCaseDeviceServerInitParams initParams;
    err = initParams.InitializeStaticResourcesBeforeServerInit();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize static resources: %s\n", ErrorStr(err));
        return false;
    }

    err = chip::Server::GetInstance().Init(initParams);
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to initialize Matter server: %s\n", ErrorStr(err));
        return false;
    }

    printf("Matter bridge initialized successfully\n");
    initialized_ = true;
    return true;
#else
    printf("Matter bridge: ENABLE_MATTER not defined, running in stub mode\n");
    initialized_ = false;
    return false;
#endif
}

bool MatterBridge::start() {
#ifdef ENABLE_MATTER
    if (!initialized_) {
        fprintf(stderr, "Cannot start: Matter bridge not initialized\n");
        return false;
    }

    printf("Starting Matter server...\n");

    // Start the CHIP event loop
    CHIP_ERROR err = PlatformMgr().StartEventLoopTask();
    if (err != CHIP_NO_ERROR) {
        fprintf(stderr, "Failed to start event loop: %s\n", ErrorStr(err));
        return false;
    }

    running_ = true;
    printf("Matter server started and advertising\n");
    return true;
#else
    printf("Matter bridge: Not started (ENABLE_MATTER not defined)\n");
    return false;
#endif
}

void MatterBridge::stop() {
#ifdef ENABLE_MATTER
    if (running_) {
        printf("Stopping Matter bridge...\n");
        chip::Server::GetInstance().Shutdown();
        PlatformMgr().Shutdown();
        running_ = false;
        initialized_ = false;
    }
#endif
}

void MatterBridge::process() {
#ifdef ENABLE_MATTER
    // The event loop runs in its own thread, so we don't need to process manually
    // This function is kept for API compatibility
#endif
}

void MatterBridge::updateFlame(bool flame_on) {
    if (flame_state_ == flame_on) {
        return; // No change
    }

    flame_state_ = flame_on;

#ifdef ENABLE_MATTER
    if (!initialized_ || !running_) {
        return;
    }

    // Update On/Off cluster attribute (Cluster 0x0006, Attribute 0x0000)
    // Endpoint 1 is our dynamic endpoint for the Viking Bio bridge
    constexpr EndpointId kEndpoint = 1;
    
    EmberAfStatus status = Clusters::OnOff::Attributes::OnOff::Set(kEndpoint, flame_on);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
        printf("Matter: Updated flame state to %s\n", flame_on ? "ON" : "OFF");
    } else {
        fprintf(stderr, "Matter: Failed to update flame state (status: 0x%02x)\n", status);
    }
#else
    printf("Matter stub: Flame state changed to %s\n", flame_on ? "ON" : "OFF");
#endif
}

void MatterBridge::updateFanSpeed(uint8_t speed_percent) {
    if (fan_speed_ == speed_percent) {
        return; // No change
    }

    fan_speed_ = speed_percent;

#ifdef ENABLE_MATTER
    if (!initialized_ || !running_) {
        return;
    }

    // Update Level Control cluster attribute (Cluster 0x0008, Attribute 0x0000)
    // Matter uses 0-254 range, we convert from 0-100 percentage
    constexpr EndpointId kEndpoint = 1;
    uint8_t matter_level = static_cast<uint8_t>((speed_percent * 254) / 100);
    
    EmberAfStatus status = Clusters::LevelControl::Attributes::CurrentLevel::Set(kEndpoint, matter_level);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
        printf("Matter: Updated fan speed to %d%% (level: %d)\n", speed_percent, matter_level);
    } else {
        fprintf(stderr, "Matter: Failed to update fan speed (status: 0x%02x)\n", status);
    }
#else
    printf("Matter stub: Fan speed changed to %d%%\n", speed_percent);
#endif
}

void MatterBridge::updateTemperature(int16_t temp_celsius) {
    if (temperature_ == temp_celsius) {
        return; // No change
    }

    temperature_ = temp_celsius;

#ifdef ENABLE_MATTER
    if (!initialized_ || !running_) {
        return;
    }

    // Update Temperature Measurement cluster attribute (Cluster 0x0402, Attribute 0x0000)
    // Matter uses 0.01°C units (int16), so multiply by 100
    constexpr EndpointId kEndpoint = 1;
    int16_t matter_temp = temp_celsius * 100;
    
    EmberAfStatus status = Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(kEndpoint, matter_temp);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
        printf("Matter: Updated temperature to %d°C\n", temp_celsius);
    } else {
        fprintf(stderr, "Matter: Failed to update temperature (status: 0x%02x)\n", status);
    }
#else
    printf("Matter stub: Temperature changed to %d°C\n", temp_celsius);
#endif
}

bool MatterBridge::isCommissioned() const {
#ifdef ENABLE_MATTER
    if (!initialized_) {
        return false;
    }
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
#else
    return false;
#endif
}
