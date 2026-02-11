#include "matter_bridge.h"
#include <iostream>
#include <cstring>

#ifdef ENABLE_MATTER
// Include Matter SDK headers when available
// These will be defined based on MATTER_ROOT environment variable
#include <app/server/Server.h>
#include <app/util/attribute-storage.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <platform/CHIPDeviceLayer.h>
#include <credentials/DeviceAttestationCredsProvider.h>
#include <credentials/examples/DeviceAttestationCredsExample.h>
#endif

MatterBridge::MatterBridge() 
    : initialized_(false), 
      running_(false),
      current_flame_(false),
      current_fan_speed_(0),
      current_temperature_(0) {
}

MatterBridge::~MatterBridge() {
    shutdown();
}

bool MatterBridge::initialize(uint32_t setupCode) {
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::DeviceLayer;
    
    std::cout << "Initializing Matter stack with setup code: " << setupCode << std::endl;
    
    // Initialize CHIP stack
    CHIP_ERROR err = PlatformMgr().InitChipStack();
    if (err != CHIP_NO_ERROR) {
        std::cerr << "Failed to initialize CHIP stack: " << err << std::endl;
        return false;
    }
    
    // Initialize device attestation credentials
    SetDeviceAttestationCredentialsProvider(Examples::GetExampleDACProvider());
    
    initialized_ = true;
    std::cout << "Matter stack initialized successfully" << std::endl;
    return true;
#else
    std::cout << "Matter support not enabled (ENABLE_MATTER not defined)" << std::endl;
    std::cout << "Running in stub mode - no actual Matter functionality" << std::endl;
    initialized_ = true;
    return true;
#endif
}

bool MatterBridge::start() {
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::DeviceLayer;
    
    if (!initialized_) {
        std::cerr << "Matter bridge not initialized" << std::endl;
        return false;
    }
    
    std::cout << "Starting Matter server..." << std::endl;
    
    // Start the server
    CHIP_ERROR err = chip::Server::GetInstance().Init();
    if (err != CHIP_NO_ERROR) {
        std::cerr << "Failed to start Matter server: " << err << std::endl;
        return false;
    }
    
    // Start the platform event loop in a separate thread
    PlatformMgr().StartEventLoopTask();
    
    running_ = true;
    std::cout << "Matter server started and ready for commissioning" << std::endl;
    return true;
#else
    std::cout << "Matter stub mode - server 'started' (no-op)" << std::endl;
    running_ = true;
    return true;
#endif
}

void MatterBridge::updateFlame(bool flameOn) {
    if (current_flame_ == flameOn) {
        return; // No change
    }
    
    current_flame_ = flameOn;
    std::cout << "Flame state changed: " << (flameOn ? "ON" : "OFF") << std::endl;
    
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::app::Clusters;
    
    // Update On/Off cluster attribute (Cluster 0x0006)
    // Endpoint 1 is the Viking Bio device endpoint
    EmberAfStatus status = OnOff::Attributes::OnOff::Set(1, flameOn);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
        std::cerr << "Failed to update OnOff attribute: " << status << std::endl;
    }
#endif
}

void MatterBridge::updateFanSpeed(uint8_t speedPercent) {
    if (current_fan_speed_ == speedPercent) {
        return; // No change
    }
    
    current_fan_speed_ = speedPercent;
    std::cout << "Fan speed changed: " << static_cast<int>(speedPercent) << "%" << std::endl;
    
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::app::Clusters;
    
    // Update Level Control cluster attribute (Cluster 0x0008)
    // Map 0-100% to 0-254 (Matter level range)
    uint8_t matterLevel = (speedPercent * 254) / 100;
    
    EmberAfStatus status = LevelControl::Attributes::CurrentLevel::Set(1, matterLevel);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
        std::cerr << "Failed to update CurrentLevel attribute: " << status << std::endl;
    }
#endif
}

void MatterBridge::updateTemperature(int16_t tempCelsius) {
    if (current_temperature_ == tempCelsius) {
        return; // No change
    }
    
    current_temperature_ = tempCelsius;
    std::cout << "Temperature changed: " << tempCelsius << "°C" << std::endl;
    
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::app::Clusters;
    
    // Update Temperature Measurement cluster attribute (Cluster 0x0402)
    // Matter expects temperature in 0.01°C units
    int16_t matterTemp = tempCelsius * 100;
    
    EmberAfStatus status = TemperatureMeasurement::Attributes::MeasuredValue::Set(1, matterTemp);
    if (status != EMBER_ZCL_STATUS_SUCCESS) {
        std::cerr << "Failed to update MeasuredValue attribute: " << status << std::endl;
    }
#endif
}

void MatterBridge::processEvents() {
#ifdef ENABLE_MATTER
    using namespace chip::DeviceLayer;
    
    // The event loop is running in its own thread, so we don't need to do much here
    // Just yield to allow other threads to run
    PlatformMgr().ScheduleWork([](intptr_t) {
        // Periodic work can be done here if needed
    });
#endif
}

bool MatterBridge::isRunning() const {
    return running_;
}

void MatterBridge::shutdown() {
#ifdef ENABLE_MATTER
    using namespace chip;
    using namespace chip::DeviceLayer;
    
    if (running_) {
        std::cout << "Shutting down Matter server..." << std::endl;
        PlatformMgr().StopEventLoopTask();
        PlatformMgr().Shutdown();
        running_ = false;
        initialized_ = false;
    }
#else
    running_ = false;
    initialized_ = false;
#endif
}
