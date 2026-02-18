# Makefile for XIAO Voice Recorder
# 
# Usage:
#   make setup    - First-time setup
#   make          - Compile
#   make upload   - Upload to board
#   make monitor  - Serial monitor
#   make all      - Compile + upload + monitor
#   make clean    - Clean build

FQBN := esp32:esp32:XIAO_ESP32S3
SKETCH := ./src/voice_recorder.ino
BUILD_DIR := ./build
BAUD := 115200

# Auto-detect port
PORT := $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -1)
ifeq ($(PORT),)
PORT := $(shell ls /dev/cu.wchusbserial* 2>/dev/null | head -1)
endif

.PHONY: all setup compile upload monitor ports clean help

help:
	@echo "XIAO Voice Recorder - Build System"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Targets:"
	@echo "  setup    - First-time setup (cores & libs)"
	@echo "  compile  - Compile sketch (default)"
	@echo "  upload   - Upload to board"
	@echo "  monitor  - Serial monitor"
	@echo "  all      - Compile + upload + monitor"
	@echo "  ports    - List available ports"
	@echo "  clean    - Clean build artifacts"

setup:
	@echo "Setting up Arduino CLI..."
	arduino-cli config init 2>/dev/null || true
	arduino-cli config add board_manager.additional_urls \
		https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json 2>/dev/null || true
	arduino-cli core update-index
	arduino-cli core install esp32:esp32
	arduino-cli lib install ArduinoJson
	@echo "Setup complete!"

compile: $(BUILD_DIR)
	@echo "Compiling for $(FQBN)..."
	arduino-cli compile --fqbn $(FQBN) --build-path $(BUILD_DIR) $(SKETCH)
	@echo "Done! Binary: $(BUILD_DIR)/voice_recorder.ino.bin"

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

upload:
	@if [ -z "$(PORT)" ]; then \
		echo "Error: No board detected"; \
		echo "Connect your XIAO and try again"; \
		exit 1; \
	fi
	@echo "Uploading to $(PORT)..."
	arduino-cli upload --fqbn $(FQBN) --port $(PORT) --input-dir $(BUILD_DIR)

monitor:
	@if [ -z "$(PORT)" ]; then \
		echo "Error: No board detected"; \
		exit 1; \
	fi
	@echo "Serial monitor on $(PORT) @ $(BAUD) baud"
	@echo "Press Ctrl+C to exit"
	arduino-cli monitor --port $(PORT) --config baudrate=$(BAUD)

all: compile upload
	@sleep 2
	@$(MAKE) monitor

ports:
	arduino-cli board list

clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"
