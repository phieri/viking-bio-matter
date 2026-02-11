# Contributing to Viking Bio Matter Bridge

Thank you for your interest in contributing to the Viking Bio Matter Bridge project!

## Development Setup

1. **Clone the repository**
   ```bash
   git clone https://github.com/phieri/viking-bio-matter.git
   cd viking-bio-matter
   ```

2. **Install dependencies**
   - CMake 3.13 or later
   - ARM GCC toolchain (`gcc-arm-none-eabi`)
   - Pico SDK

3. **Run setup script** (optional)
   ```bash
   ./setup.sh
   ```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Testing

### Without Hardware

Use the Viking Bio simulator:

```bash
# Install dependencies
pip3 install pyserial

# Run simulator (requires USB-to-Serial adapter)
python3 examples/viking_bio_simulator.py /dev/ttyUSB0
```

### With Hardware

1. Flash firmware to Pico
2. Connect Viking Bio 20 serial output
3. Monitor USB serial output

## Code Style

- Follow existing code style
- Use meaningful variable names
- Add comments for complex logic
- Keep functions focused and small

## Commit Messages

Use clear, descriptive commit messages:

```
Add feature: brief description

Detailed explanation of what changed and why.
```

## Pull Requests

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Test thoroughly
5. Submit a pull request

### PR Guidelines

- Describe what the PR does
- Reference any related issues
- Include test results
- Update documentation if needed

## Areas for Contribution

### High Priority

- Full Matter SDK integration
- Network connectivity (WiFi/Thread)
- OTA firmware updates
- Comprehensive testing

### Medium Priority

- Enhanced error handling
- Additional Viking Bio protocol features
- Power optimization
- Historical data logging

### Low Priority

- Additional Matter clusters
- Web configuration interface
- Multiple device support

## Questions?

Feel free to open an issue for discussion!

## License

By contributing, you agree that your contributions will be licensed under the same license as the project (MIT License).
