#include <iostream>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <getopt.h>
#include "matter_bridge.h"
#include "viking_bio_protocol.h"

// Global flag for graceful shutdown
static volatile bool g_running = true;

// Signal handler for clean shutdown
void signalHandler(int signum) {
    std::cout << "\nReceived signal " << signum << ", shutting down..." << std::endl;
    g_running = false;
}

// Configuration structure
struct Config {
    const char* serial_device = "/dev/ttyUSB0";
    int baud_rate = 9600;
    uint32_t setup_code = 20202021;
    bool help = false;
};

// Parse command line arguments
bool parseArgs(int argc, char** argv, Config& config) {
    static struct option long_options[] = {
        {"device", required_argument, 0, 'd'},
        {"baud", required_argument, 0, 'b'},
        {"setup-code", required_argument, 0, 's'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "d:b:s:h", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                config.serial_device = optarg;
                break;
            case 'b':
                config.baud_rate = atoi(optarg);
                break;
            case 's':
                config.setup_code = strtoul(optarg, nullptr, 10);
                break;
            case 'h':
                config.help = true;
                return true;
            default:
                return false;
        }
    }
    
    return true;
}

// Print usage information
void printUsage(const char* program_name) {
    std::cout << "Viking Bio Matter Bridge - Raspberry Pi Zero Host Application\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --device PATH      Serial device path (default: /dev/ttyUSB0)\n";
    std::cout << "  -b, --baud RATE        Baud rate (default: 9600)\n";
    std::cout << "  -s, --setup-code CODE  Matter setup code (default: 20202021)\n";
    std::cout << "  -h, --help             Show this help message\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " --device /dev/ttyAMA0\n";
    std::cout << "  " << program_name << " --device /dev/ttyUSB0 --setup-code 12345678\n\n";
}

// Configure serial port using termios
int openSerial(const char* device, int baud_rate) {
    int fd = open(device, O_RDONLY | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        std::cerr << "Error opening serial device " << device << ": " 
                  << strerror(errno) << std::endl;
        return -1;
    }
    
    // Configure serial port
    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        std::cerr << "Error getting serial port attributes: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }
    
    // Set baud rate
    speed_t speed;
    switch (baud_rate) {
        case 9600: speed = B9600; break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default:
            std::cerr << "Unsupported baud rate: " << baud_rate << std::endl;
            close(fd);
            return -1;
    }
    
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    
    // 8N1 mode
    options.c_cflag &= ~PARENB;  // No parity
    options.c_cflag &= ~CSTOPB;  // 1 stop bit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;      // 8 data bits
    
    // Raw input mode
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;
    
    // Read timeout (deciseconds)
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;  // 0.1 second timeout
    
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        std::cerr << "Error setting serial port attributes: " << strerror(errno) << std::endl;
        close(fd);
        return -1;
    }
    
    // Flush any existing data
    tcflush(fd, TCIOFLUSH);
    
    std::cout << "Serial port " << device << " opened at " << baud_rate << " baud" << std::endl;
    return fd;
}

int main(int argc, char** argv) {
    // Parse command line arguments
    Config config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }
    
    if (config.help) {
        printUsage(argv[0]);
        return 0;
    }
    
    std::cout << "=== Viking Bio Matter Bridge ===" << std::endl;
    std::cout << "Host-based Matter bridge for Raspberry Pi Zero" << std::endl;
    std::cout << "Serial device: " << config.serial_device << std::endl;
    std::cout << "Baud rate: " << config.baud_rate << std::endl;
    std::cout << "Setup code: " << config.setup_code << std::endl;
    std::cout << std::endl;
    
    // Setup signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Open serial port
    int serial_fd = openSerial(config.serial_device, config.baud_rate);
    if (serial_fd < 0) {
        return 1;
    }
    
    // Initialize Viking Bio protocol parser
    viking_bio_init();
    
    // Initialize Matter bridge
    MatterBridge bridge;
    if (!bridge.initialize(config.setup_code)) {
        std::cerr << "Failed to initialize Matter bridge" << std::endl;
        close(serial_fd);
        return 1;
    }
    
    if (!bridge.start()) {
        std::cerr << "Failed to start Matter bridge" << std::endl;
        close(serial_fd);
        return 1;
    }
    
    std::cout << "\nBridge running. Press Ctrl+C to stop." << std::endl;
    std::cout << "Waiting for Viking Bio serial data..." << std::endl;
    
    // Main loop
    uint8_t buffer[256];
    viking_bio_data_t last_data = {
        .flame_detected = false,
        .fan_speed = 0,
        .temperature = 0,
        .error_code = 0,
        .valid = false
    };
    
    while (g_running) {
        // Read from serial port
        int bytes_read = read(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            // Parse Viking Bio data
            viking_bio_data_t data;
            if (viking_bio_parse_data(buffer, bytes_read, &data)) {
                if (data.valid) {
                    // Check what changed and update Matter attributes
                    if (data.flame_detected != last_data.flame_detected) {
                        bridge.updateFlame(data.flame_detected);
                    }
                    
                    if (data.fan_speed != last_data.fan_speed) {
                        bridge.updateFanSpeed(data.fan_speed);
                    }
                    
                    if (data.temperature != last_data.temperature) {
                        bridge.updateTemperature(data.temperature);
                    }
                    
                    last_data = data;
                }
            }
        } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            std::cerr << "Error reading from serial port: " << strerror(errno) << std::endl;
            break;
        }
        
        // Process Matter events
        bridge.processEvents();
        
        // Small delay to prevent busy-waiting
        usleep(10000); // 10ms
    }
    
    std::cout << "\nShutting down..." << std::endl;
    bridge.shutdown();
    close(serial_fd);
    
    std::cout << "Bridge stopped." << std::endl;
    return 0;
}
