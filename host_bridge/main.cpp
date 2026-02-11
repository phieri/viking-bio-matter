#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <getopt.h>
#include <sys/select.h>
#include "viking_bio_protocol.h"
#include "matter_bridge.h"

// Global flag for clean shutdown
static volatile bool g_running = true;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    g_running = false;
}

// Configuration structure
struct Config {
    const char* serial_device;
    int baud_rate;
    const char* setup_code;
    bool verbose;
    
    Config() : serial_device("/dev/ttyUSB0"),
               baud_rate(9600),
               setup_code("20202021"),
               verbose(false) {}
};

// Print usage information
void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("\nOptions:\n");
    printf("  -d, --device <path>      Serial device path (default: /dev/ttyUSB0)\n");
    printf("  -b, --baud <rate>        Baud rate (default: 9600)\n");
    printf("  -s, --setup-code <code>  Matter setup code (default: 20202021)\n");
    printf("  -v, --verbose            Verbose output\n");
    printf("  -h, --help               Show this help message\n");
    printf("\nExample:\n");
    printf("  %s -d /dev/ttyAMA0 -b 9600 -s 34567890\n", program_name);
}

// Open and configure serial port
int open_serial_port(const char* device, int baud_rate) {
    int fd = open(device, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("Failed to open serial device");
        return -1;
    }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        perror("Failed to get serial attributes");
        close(fd);
        return -1;
    }

    // Configure baud rate
    speed_t speed = B9600;
    switch (baud_rate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 38400:  speed = B38400;  break;
        case 57600:  speed = B57600;  break;
        case 115200: speed = B115200; break;
        default:
            fprintf(stderr, "Unsupported baud rate: %d\n", baud_rate);
            close(fd);
            return -1;
    }

    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    // Configure 8N1 mode
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    tty.c_iflag &= ~IGNBRK;                         // disable break processing
    tty.c_lflag = 0;                                // no signaling chars, no echo,
                                                    // no canonical processing
    tty.c_oflag = 0;                                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;                            // read doesn't block
    tty.c_cc[VTIME] = 5;                            // 0.5 seconds read timeout

    tty.c_iflag &= ~(IXON | IXOFF | IXANY);         // shut off xon/xoff ctrl
    tty.c_cflag |= (CLOCAL | CREAD);                // ignore modem controls,
                                                    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);              // shut off parity
    tty.c_cflag &= ~CSTOPB;                         // one stop bit
    tty.c_cflag &= ~CRTSCTS;                        // no hardware flow control

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("Failed to set serial attributes");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char* argv[]) {
    Config config;

    // Parse command-line arguments
    static struct option long_options[] = {
        {"device",     required_argument, 0, 'd'},
        {"baud",       required_argument, 0, 'b'},
        {"setup-code", required_argument, 0, 's'},
        {"verbose",    no_argument,       0, 'v'},
        {"help",       no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "d:b:s:vh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'd':
                config.serial_device = optarg;
                break;
            case 'b':
                config.baud_rate = atoi(optarg);
                break;
            case 's':
                config.setup_code = optarg;
                break;
            case 'v':
                config.verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    printf("=== Viking Bio Matter Bridge (Host) ===\n");
    printf("Serial device: %s\n", config.serial_device);
    printf("Baud rate: %d\n", config.baud_rate);
    printf("Matter setup code: %s\n", config.setup_code);
    printf("\n");

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize Viking Bio protocol parser
    viking_bio_init();

    // Open serial port
    printf("Opening serial port %s...\n", config.serial_device);
    int serial_fd = open_serial_port(config.serial_device, config.baud_rate);
    if (serial_fd < 0) {
        fprintf(stderr, "Failed to open serial port %s\n", config.serial_device);
        return 1;
    }
    printf("Serial port opened successfully\n");

    // Initialize Matter bridge
    MatterBridge bridge;
    if (!bridge.init(config.setup_code)) {
        fprintf(stderr, "Failed to initialize Matter bridge\n");
        close(serial_fd);
        return 1;
    }

    // Start Matter server
    if (!bridge.start()) {
        fprintf(stderr, "Failed to start Matter server\n");
        close(serial_fd);
        return 1;
    }

    printf("\n=== Bridge is running ===\n");
    printf("Waiting for commissioning...\n");
    printf("Use QR code or setup code: %s\n", config.setup_code);
    printf("Press Ctrl+C to stop\n\n");

    // Main loop
    uint8_t buffer[256];
    viking_bio_data_t viking_data;
    viking_bio_data_t last_viking_data = {0};
    
    while (g_running) {
        // Read from serial port
        ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
        
        if (bytes_read > 0) {
            if (config.verbose) {
                printf("Read %zd bytes from serial\n", bytes_read);
            }
            
            // Parse Viking Bio data
            if (viking_bio_parse_data(buffer, bytes_read, &viking_data)) {
                // Check if data has changed
                bool changed = (viking_data.flame_detected != last_viking_data.flame_detected) ||
                              (viking_data.fan_speed != last_viking_data.fan_speed) ||
                              (viking_data.temperature != last_viking_data.temperature);
                
                if (changed) {
                    printf("Flame: %s, Fan Speed: %d%%, Temp: %d°C",
                           viking_data.flame_detected ? "ON " : "OFF",
                           viking_data.fan_speed,
                           viking_data.temperature);
                    
                    if (viking_data.error_code != 0) {
                        printf(", Error: 0x%02X", viking_data.error_code);
                    }
                    printf("\n");
                    
                    // Update Matter attributes
                    bridge.updateFlame(viking_data.flame_detected);
                    bridge.updateFanSpeed(viking_data.fan_speed);
                    bridge.updateTemperature(viking_data.temperature);
                    
                    last_viking_data = viking_data;
                }
            }
        } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Serial read error");
            break;
        }
        
        // Process Matter events
        bridge.process();
        
        // Small delay to prevent busy-waiting
        usleep(100000); // 100ms
    }

    printf("\nShutting down...\n");
    bridge.stop();
    close(serial_fd);
    printf("Goodbye!\n");

    return 0;
}
