#!/usr/bin/env python3
"""
Viking Bio 20 Serial Simulator

This script simulates the Viking Bio 20 burner serial output for testing
the Matter bridge firmware without actual hardware.

Usage:
    python3 viking_bio_simulator.py /dev/ttyUSB0

Requirements:
    pip3 install pyserial
"""

import serial
import time
import random
import argparse
import struct

class VikingBioSimulator:
    def __init__(self, port, baudrate=9600):
        """Initialize the simulator with serial port settings."""
        self.ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1
        )
        
        # Simulation state
        self.flame_on = False
        self.fan_speed = 0
        self.temperature = 20
        self.target_temp = 75
        
    def send_binary_packet(self):
        """Send a binary protocol packet."""
        # Build flags byte
        flags = 0x01 if self.flame_on else 0x00
        
        # Pack temperature as 16-bit value
        temp_high = (self.temperature >> 8) & 0xFF
        temp_low = self.temperature & 0xFF
        
        # Create packet
        packet = bytes([
            0xAA,                    # Start byte
            flags,                   # Flags
            self.fan_speed,          # Fan speed
            temp_high,               # Temperature high
            temp_low,                # Temperature low
            0x55                     # End byte
        ])
        
        self.ser.write(packet)
        print(f"Sent binary: Flame={self.flame_on}, Speed={self.fan_speed}%, Temp={self.temperature}°C")
        
    def send_text_packet(self):
        """Send a text protocol packet."""
        flame_val = 1 if self.flame_on else 0
        message = f"F:{flame_val},S:{self.fan_speed},T:{self.temperature}\n"
        self.ser.write(message.encode('ascii'))
        print(f"Sent text: {message.strip()}")
        
    def update_state(self):
        """Update the simulated burner state."""
        # Simulate burner startup sequence
        if not self.flame_on and random.random() < 0.1:
            self.flame_on = True
            self.fan_speed = 30
            print("🔥 Flame ignited!")
            
        # Simulate burner shutdown
        elif self.flame_on and random.random() < 0.05:
            self.flame_on = False
            self.fan_speed = 0
            self.temperature = max(20, self.temperature - 10)
            print("💨 Flame extinguished!")
            
        # Update fan speed based on flame state
        if self.flame_on:
            # Gradually increase fan speed
            if self.fan_speed < 80:
                self.fan_speed = min(80, self.fan_speed + random.randint(5, 15))
            # Slight variations
            else:
                self.fan_speed = max(70, min(90, self.fan_speed + random.randint(-5, 5)))
                
            # Heat up when flame is on
            if self.temperature < self.target_temp:
                self.temperature = min(self.target_temp, self.temperature + random.randint(1, 3))
        else:
            # Cool down when flame is off
            if self.temperature > 20:
                self.temperature = max(20, self.temperature - random.randint(1, 2))
                
    def run(self, protocol='binary', interval=2.0):
        """
        Run the simulator.
        
        Args:
            protocol: 'binary' or 'text'
            interval: Seconds between packets
        """
        print(f"Viking Bio 20 Simulator started on {self.ser.port}")
        print(f"Protocol: {protocol}, Interval: {interval}s")
        print("Press Ctrl+C to stop\n")
        
        try:
            while True:
                self.update_state()
                
                if protocol == 'binary':
                    self.send_binary_packet()
                else:
                    self.send_text_packet()
                    
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print("\n\nSimulator stopped.")
        finally:
            self.ser.close()

def main():
    parser = argparse.ArgumentParser(description='Viking Bio 20 Serial Simulator')
    parser.add_argument('port', help='Serial port (e.g., /dev/ttyUSB0 or COM3)')
    parser.add_argument('-b', '--baudrate', type=int, default=9600, help='Baud rate (default: 9600)')
    parser.add_argument('-p', '--protocol', choices=['binary', 'text'], default='binary',
                        help='Protocol to use (default: binary)')
    parser.add_argument('-i', '--interval', type=float, default=2.0,
                        help='Seconds between packets (default: 2.0)')
    
    args = parser.parse_args()
    
    simulator = VikingBioSimulator(args.port, args.baudrate)
    simulator.run(protocol=args.protocol, interval=args.interval)

if __name__ == '__main__':
    main()
