#ifndef MATTER_BRIDGE_H
#define MATTER_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>

// Matter bridge initialization and control functions
class MatterBridge {
public:
    MatterBridge();
    ~MatterBridge();
    
    // Initialize Matter stack with setup code
    bool initialize(uint32_t setupCode);
    
    // Start Matter server and commissioning
    bool start();
    
    // Update attribute values (called when Viking Bio data changes)
    void updateFlame(bool flameOn);
    void updateFanSpeed(uint8_t speedPercent);
    void updateTemperature(int16_t tempCelsius);
    
    // Process Matter event loop (call periodically)
    void processEvents();
    
    // Check if Matter stack is running
    bool isRunning() const;
    
    // Shutdown Matter stack
    void shutdown();

private:
    bool initialized_;
    bool running_;
    
    // Current attribute values
    bool current_flame_;
    uint8_t current_fan_speed_;
    int16_t current_temperature_;
};

#endif // MATTER_BRIDGE_H
