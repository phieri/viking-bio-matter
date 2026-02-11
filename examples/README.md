# Viking Bio Matter Bridge - Examples

## Viking Bio Simulator

The `viking_bio_simulator.py` script simulates the Viking Bio 20 burner's serial output, useful for testing the Matter bridge firmware without physical hardware.

### Installation

```bash
pip3 install pyserial
```

### Usage

Basic usage:
```bash
python3 viking_bio_simulator.py /dev/ttyUSB0
```

With options:
```bash
# Use text protocol instead of binary
python3 viking_bio_simulator.py /dev/ttyUSB0 -p text

# Increase update frequency to 0.5 seconds
python3 viking_bio_simulator.py /dev/ttyUSB0 -i 0.5

# Custom baud rate
python3 viking_bio_simulator.py /dev/ttyUSB0 -b 19200
```

### Testing with Raspberry Pi Pico

1. Connect the simulator to the Pico:
   - Connect USB-to-Serial adapter TX to Pico GP1 (RX)
   - Connect grounds together

2. Flash the firmware to the Pico

3. Run the simulator:
   ```bash
   python3 viking_bio_simulator.py /dev/ttyUSB0
   ```

4. Monitor Pico output:
   ```bash
   screen /dev/ttyACM0 115200
   ```

You should see the Pico receiving and parsing the simulated data.

### Simulator Behavior

The simulator mimics realistic burner operation:

- **Startup**: Randomly ignites flame and ramps up fan speed
- **Normal Operation**: Maintains 70-90% fan speed, heats to target temperature
- **Shutdown**: Randomly shuts down, fan stops, temperature cools down
- **Updates**: Sends data packets every 2 seconds (configurable)

### Protocol Formats

**Binary Protocol** (default):
```
[0xAA] [FLAGS] [FAN] [TEMP_H] [TEMP_L] [0x55]
```

**Text Protocol** (--protocol text):
```
F:1,S:80,T:75\n
```
