#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "viking_bio_protocol.h"

// Matter bridge initialization and management
class MatterBridge {
public:
    MatterBridge();
    ~MatterBridge();

    // Initialize Matter stack
    // Returns true on success, false on failure
    bool init(const char* setup_code = nullptr);

    // Start the Matter server and begin advertising
    bool start();

    // Stop the Matter server
    void stop();

    // Process Matter events (call periodically)
    void process();

    // Update Matter attributes from Viking Bio data
    void updateFlame(bool flame_on);
    void updateFanSpeed(uint8_t speed_percent);
    void updateTemperature(int16_t temp_celsius);

    // Check if Matter stack is initialized
    bool isInitialized() const { return initialized_; }

    // Check if Matter is commissioned
    bool isCommissioned() const;

private:
    bool initialized_;
    bool running_;
    
    // Current attribute values
    bool flame_state_;
    uint8_t fan_speed_;
    int16_t temperature_;
};

#endif // MATTER_BRIDGE_H
