#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <getopt.h>
#include <errno.h>
#include <sys/select.h>

#include "viking_bio_protocol.h"
#include "matter_bridge.h"

// Default configuration
#define DEFAULT_SERIAL_DEVICE "/dev/ttyUSB0"
#define DEFAULT_BAUD_RATE B9600
#define DEFAULT_SETUP_CODE "20202021"
#define DEFAULT_DISCRIMINATOR 3840
#define SERIAL_BUFFER_SIZE 256

// Global flag for clean shutdown
static volatile bool keep_running = true;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    keep_running = false;
}

// Configure serial port for Viking Bio protocol
int configure_serial_port(const char *device, speed_t baud_rate) {
    int fd = open(device, O_RDONLY | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "Error opening serial device %s: %s\n", device, strerror(errno));
        return -1;
    }
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        fprintf(stderr, "Error getting serial attributes: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Set baud rate
    cfsetispeed(&tty, baud_rate);
    cfsetospeed(&tty, baud_rate);
    
    // 8N1 mode
    tty.c_cflag &= ~PARENB;  // No parity
    tty.c_cflag &= ~CSTOPB;  // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;      // 8 data bits
    
    // No hardware flow control
    tty.c_cflag &= ~CRTSCTS;
    
    // Enable receiver, ignore modem control lines
    tty.c_cflag |= CREAD | CLOCAL;
    
    // Raw input mode
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    // Raw output mode
    tty.c_oflag &= ~OPOST;
    
    // Non-blocking reads with timeout
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;  // 0.1 second timeout
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        fprintf(stderr, "Error setting serial attributes: %s\n", strerror(errno));
        close(fd);
        return -1;
    }
    
    // Flush any existing data
    tcflush(fd, TCIOFLUSH);
    
    printf("Serial port %s configured successfully (9600 8N1)\n", device);
    return fd;
}

// Print usage information
void print_usage(const char *prog_name) {
    printf("Viking Bio Matter Bridge - Host Application\n\n");
    printf("Usage: %s [OPTIONS]\n\n", prog_name);
    printf("Options:\n");
    printf("  -d, --device <path>        Serial device path (default: %s)\n", DEFAULT_SERIAL_DEVICE);
    printf("  -s, --setup-code <code>    Matter setup code (default: %s)\n", DEFAULT_SETUP_CODE);
    printf("  -r, --discriminator <num>  Matter discriminator (default: %d)\n", DEFAULT_DISCRIMINATOR);
    printf("  -h, --help                 Show this help message\n");
    printf("\nExamples:\n");
    printf("  %s\n", prog_name);
    printf("  %s -d /dev/ttyAMA0 -s 34567890 -r 2000\n", prog_name);
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Configuration
    const char *serial_device = DEFAULT_SERIAL_DEVICE;
    const char *setup_code = DEFAULT_SETUP_CODE;
    uint16_t discriminator = DEFAULT_DISCRIMINATOR;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"device", required_argument, 0, 'd'},
        {"setup-code", required_argument, 0, 's'},
        {"discriminator", required_argument, 0, 'r'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "d:s:r:h", long_options, NULL)) != -1) {
        switch (opt) {
            case 'd':
                serial_device = optarg;
                break;
            case 's':
                setup_code = optarg;
                break;
            case 'r':
                discriminator = (uint16_t)atoi(optarg);
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }
    
    // Print banner
    printf("====================================\n");
    printf("Viking Bio Matter Bridge\n");
    printf("====================================\n");
    printf("Serial Device: %s\n", serial_device);
    printf("Setup Code: %s\n", setup_code);
    printf("Discriminator: %u\n", discriminator);
    printf("====================================\n\n");
    
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize Viking Bio protocol
    viking_bio_init();
    printf("Viking Bio protocol initialized\n");
    
    // Initialize Matter bridge
    if (!matter_bridge_init(setup_code, discriminator)) {
        fprintf(stderr, "Failed to initialize Matter bridge\n");
        return 1;
    }
    
    // Open serial port
    int serial_fd = configure_serial_port(serial_device, DEFAULT_BAUD_RATE);
    if (serial_fd < 0) {
        matter_bridge_shutdown();
        return 1;
    }
    
    printf("\nStarting main loop...\n");
    printf("Waiting for Viking Bio data on %s\n\n", serial_device);
    
    // Main loop
    uint8_t buffer[SERIAL_BUFFER_SIZE];
    size_t buffer_pos = 0;
    
    while (keep_running) {
        // Check for serial data
        fd_set readfds;
        struct timeval timeout;
        
        FD_ZERO(&readfds);
        FD_SET(serial_fd, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms timeout
        
        int ret = select(serial_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(serial_fd, &readfds)) {
            // Read available data
            ssize_t bytes_read = read(serial_fd, buffer + buffer_pos, 
                                     SERIAL_BUFFER_SIZE - buffer_pos - 1);
            
            if (bytes_read > 0) {
                buffer_pos += bytes_read;
                
                // Try to parse Viking Bio data
                viking_bio_data_t data;
                if (viking_bio_parse_data(buffer, buffer_pos, &data)) {
                    // Valid data received
                    printf("Viking Bio data: Flame=%s, Speed=%u%%, Temp=%u°C, Error=0x%02X\n",
                           data.flame_detected ? "ON" : "OFF",
                           data.fan_speed,
                           data.temperature,
                           data.error_code);
                    
                    // Update Matter attributes
                    matter_bridge_update_flame(data.flame_detected);
                    matter_bridge_update_fan_speed(data.fan_speed);
                    matter_bridge_update_temperature((int16_t)data.temperature);
                    
                    // Reset buffer after successful parse
                    buffer_pos = 0;
                } else if (buffer_pos >= SERIAL_BUFFER_SIZE - 1) {
                    // Buffer full without valid data, reset
                    printf("Warning: Buffer full without valid data, resetting\n");
                    buffer_pos = 0;
                }
            } else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                fprintf(stderr, "Error reading serial port: %s\n", strerror(errno));
                break;
            }
        }
        
        // Run Matter event loop
        matter_bridge_run_event_loop(10);  // 10ms timeout
    }
    
    // Cleanup
    printf("\nShutting down...\n");
    close(serial_fd);
    matter_bridge_shutdown();
    printf("Shutdown complete\n");
    
    return 0;
}
