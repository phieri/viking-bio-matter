#!/bin/bash
# Setup script for Viking Bio Matter Bridge development

set -e

echo "========================================"
echo "Viking Bio Matter Bridge - Setup Script"
echo "========================================"
echo ""

# Check for required tools
echo "Checking for required tools..."

if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Please install CMake 3.13 or later."
    exit 1
fi
echo "✓ CMake found: $(cmake --version | head -1)"

if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "❌ ARM GCC toolchain not found."
    echo "   Please install: sudo apt-get install gcc-arm-none-eabi libnewlib-arm-none-eabi"
    exit 1
fi
echo "✓ ARM GCC found: $(arm-none-eabi-gcc --version | head -1)"

# Check for Pico SDK
echo ""
echo "Checking for Pico SDK..."

if [ -z "$PICO_SDK_PATH" ]; then
    echo "⚠️  PICO_SDK_PATH not set."
    echo "   Do you want to download it now? (y/n)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        echo "Downloading Pico SDK..."
        if [ ! -d "pico-sdk" ]; then
            git clone --depth 1 --branch 1.5.1 https://github.com/raspberrypi/pico-sdk.git
            cd pico-sdk
            git submodule update --init
            cd ..
        fi
        export PICO_SDK_PATH="$(pwd)/pico-sdk"
        echo "export PICO_SDK_PATH=$(pwd)/pico-sdk" >> ~/.bashrc
        echo "✓ Pico SDK downloaded and PICO_SDK_PATH set"
    else
        echo "Please set PICO_SDK_PATH manually:"
        echo "  export PICO_SDK_PATH=/path/to/pico-sdk"
        exit 1
    fi
else
    if [ ! -d "$PICO_SDK_PATH" ]; then
        echo "❌ PICO_SDK_PATH points to non-existent directory: $PICO_SDK_PATH"
        exit 1
    fi
    echo "✓ Pico SDK found at: $PICO_SDK_PATH"
fi

# Create build directory
echo ""
echo "Creating build directory..."
mkdir -p build
echo "✓ Build directory created"

# Configure
echo ""
echo "Configuring project..."
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cd ..
echo "✓ Configuration complete"

echo ""
echo "========================================"
echo "Setup complete!"
echo "========================================"
echo ""
echo "To build the firmware:"
echo "  cd build"
echo "  make"
echo ""
echo "The firmware file (viking_bio_matter.uf2) will be in the build directory."
echo ""
