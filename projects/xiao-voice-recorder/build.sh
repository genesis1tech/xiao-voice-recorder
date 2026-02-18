#!/bin/bash
#
# build.sh - Arduino CLI build script for XIAO Voice Recorder
#
# Usage:
#   ./build.sh setup     # First-time setup (install cores & libs)
#   ./build.sh compile   # Compile the sketch
#   ./build.sh upload    # Upload to board
#   ./build.sh monitor   # Serial monitor
#   ./build.sh all       # Compile + upload + monitor
#   ./build.sh ports     # List available ports
#   ./build.sh clean     # Clean build artifacts

set -e

# Configuration
FQBN="esp32:esp32:XIAO_ESP32S3"
SKETCH_DIR="./src"
SKETCH="$SKETCH_DIR/voice_recorder.ino"
BUILD_DIR="./build"
BAUD=115200

# Auto-detect port (macOS)
detect_port() {
    # Try common XIAO port patterns
    PORT=$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)
    if [ -z "$PORT" ]; then
        PORT=$(ls /dev/cu.wchusbserial* 2>/dev/null | head -1)
    fi
    if [ -z "$PORT" ]; then
        PORT=$(ls /dev/cu.SLAB_USBtoUART* 2>/dev/null | head -1)
    fi
    if [ -z "$PORT" ]; then
        echo "Error: No board detected. Connect your XIAO and try again."
        echo "Available ports:"
        ls /dev/cu.* 2>/dev/null || echo "  (none)"
        exit 1
    fi
    echo "$PORT"
}

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  XIAO Voice Recorder - Build System${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
}

cmd_setup() {
    print_header
    echo -e "${YELLOW}Setting up Arduino CLI environment...${NC}"
    echo ""
    
    # Check if arduino-cli is installed
    if ! command -v arduino-cli &> /dev/null; then
        echo -e "${RED}arduino-cli not found. Installing...${NC}"
        brew install arduino-cli || {
            echo "Install manually: brew install arduino-cli"
            exit 1
        }
    fi
    echo -e "${GREEN}✓ arduino-cli installed${NC}"
    
    # Initialize config if needed
    if [ ! -f ~/.arduino15/arduino-cli.yaml ]; then
        echo "Initializing arduino-cli config..."
        arduino-cli config init
    fi
    
    # Add ESP32 board URL
    echo "Adding ESP32 board manager URL..."
    arduino-cli config add board_manager.additional_urls \
        https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json 2>/dev/null || true
    
    # Update index
    echo "Updating core index..."
    arduino-cli core update-index
    
    # Install ESP32 core
    echo "Installing ESP32 core (this may take a while)..."
    arduino-cli core install esp32:esp32
    echo -e "${GREEN}✓ ESP32 core installed${NC}"
    
    # Install required libraries
    echo "Installing libraries..."
    arduino-cli lib install ArduinoJson
    echo -e "${GREEN}✓ ArduinoJson installed${NC}"
    
    # SD and WiFiClientSecure come with ESP32 core
    
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}  Setup complete!${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Copy src/config.h and fill in your credentials"
    echo "  2. Connect your XIAO ESP32S3"
    echo "  3. Run: ./build.sh all"
}

cmd_compile() {
    print_header
    echo -e "${YELLOW}Compiling...${NC}"
    echo "Board: $FQBN"
    echo "Sketch: $SKETCH"
    echo ""
    
    # Check config.h exists
    if [ ! -f "$SKETCH_DIR/config.h" ]; then
        echo -e "${RED}Error: config.h not found!${NC}"
        echo "Copy config.h.example to config.h and fill in your credentials."
        exit 1
    fi
    
    mkdir -p "$BUILD_DIR"
    
    arduino-cli compile \
        --fqbn "$FQBN" \
        --build-path "$BUILD_DIR" \
        --warnings default \
        "$SKETCH"
    
    echo ""
    echo -e "${GREEN}✓ Compilation successful!${NC}"
    
    # Show binary size
    if [ -f "$BUILD_DIR/voice_recorder.ino.bin" ]; then
        SIZE=$(ls -lh "$BUILD_DIR/voice_recorder.ino.bin" | awk '{print $5}')
        echo "Binary size: $SIZE"
    fi
}

cmd_upload() {
    print_header
    PORT=$(detect_port)
    echo -e "${YELLOW}Uploading...${NC}"
    echo "Port: $PORT"
    echo "Board: $FQBN"
    echo ""
    
    arduino-cli upload \
        --fqbn "$FQBN" \
        --port "$PORT" \
        --input-dir "$BUILD_DIR"
    
    echo ""
    echo -e "${GREEN}✓ Upload successful!${NC}"
}

cmd_monitor() {
    print_header
    PORT=$(detect_port)
    echo -e "${YELLOW}Starting serial monitor...${NC}"
    echo "Port: $PORT"
    echo "Baud: $BAUD"
    echo "Press Ctrl+C to exit"
    echo ""
    
    arduino-cli monitor \
        --port "$PORT" \
        --config baudrate=$BAUD
}

cmd_ports() {
    print_header
    echo -e "${YELLOW}Available boards:${NC}"
    echo ""
    arduino-cli board list
}

cmd_clean() {
    print_header
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf "$BUILD_DIR"
    echo -e "${GREEN}✓ Clean complete${NC}"
}

cmd_all() {
    cmd_compile
    echo ""
    cmd_upload
    echo ""
    echo "Starting monitor in 2 seconds..."
    sleep 2
    cmd_monitor
}

# Main
case "${1:-help}" in
    setup)
        cmd_setup
        ;;
    compile|build)
        cmd_compile
        ;;
    upload|flash)
        cmd_upload
        ;;
    monitor|serial)
        cmd_monitor
        ;;
    all)
        cmd_all
        ;;
    ports|list)
        cmd_ports
        ;;
    clean)
        cmd_clean
        ;;
    *)
        print_header
        echo "Usage: $0 <command>"
        echo ""
        echo "Commands:"
        echo "  setup     First-time setup (install cores & libs)"
        echo "  compile   Compile the sketch"
        echo "  upload    Upload to board"
        echo "  monitor   Serial monitor"
        echo "  all       Compile + upload + monitor"
        echo "  ports     List available ports"
        echo "  clean     Clean build artifacts"
        echo ""
        ;;
esac
