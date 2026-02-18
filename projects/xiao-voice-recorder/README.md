# XIAO Voice Recorder & Summarizer

Record voice → Transcribe with Whisper → Summarize with LLM → Email summary

**Hardware:** Seeed XIAO ESP32S3 Sense  
**Build System:** Arduino CLI (no IDE required)

---

## Quick Start

```bash
# 1. Setup (first time only)
./build.sh setup

# 2. Configure credentials
cp src/config.h.example src/config.h
nano src/config.h  # Fill in your WiFi + API keys

# 3. Connect XIAO via USB-C

# 4. Build & upload
./build.sh all
```

---

## Project Structure

```
xiao-voice-recorder/
├── build.sh              # Build script (use this!)
├── Makefile              # Alternative: make compile/upload/etc
├── README.md
├── build/                # Compiled binaries (generated)
└── src/
    ├── voice_recorder.ino    # Main sketch
    ├── config.h.example      # Copy to config.h
    ├── audio_recorder.h      # I2S microphone capture
    ├── whisper_client.h      # Groq Whisper API
    ├── llm_client.h          # Groq LLM summarization
    └── email_client.h        # SendGrid email
```

---

## Build Commands

### Using build.sh (recommended)

```bash
./build.sh setup     # Install Arduino CLI, ESP32 core, libs
./build.sh compile   # Compile only
./build.sh upload    # Upload to board
./build.sh monitor   # Serial monitor
./build.sh all       # Compile + upload + monitor
./build.sh ports     # List connected boards
./build.sh clean     # Remove build artifacts
```

### Using Make

```bash
make setup    # First-time setup
make          # Compile
make upload   # Upload
make monitor  # Serial monitor
make all      # Everything
make clean    # Clean
```

### Raw Arduino CLI

```bash
# Install ESP32 core
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
arduino-cli lib install ArduinoJson

# Compile
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32S3 ./src/voice_recorder.ino

# Upload (replace port)
arduino-cli upload -p /dev/cu.usbmodem101 --fqbn esp32:esp32:XIAO_ESP32S3 ./src/voice_recorder.ino

# Monitor
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```

---

## Configuration

Copy `src/config.h.example` to `src/config.h` and fill in:

```cpp
// Groq API (free: https://console.groq.com)
#define GROQ_API_KEY "gsk_..."

// SendGrid (free 100/day: https://sendgrid.com)
#define SENDGRID_API_KEY "SG...."

// Email recipient
#define EMAIL_TO "you@example.com"
#define EMAIL_FROM "recorder@yourdomain.com"
```

**WiFi credentials NOT needed!** Device enters AP mode on first boot where you enter them via web portal.

---

## Hardware Setup

### Required

| Component | Notes |
|-----------|-------|
| XIAO ESP32S3 Sense | Has built-in mic + SD slot |
| MicroSD card | 4GB+ FAT32 formatted |
| USB-C cable | Data cable, not charge-only |

### Optional

| Component | Notes |
|-----------|-------|
| External button | Connect to D1 + GND |
| LiPo battery | 3.7V 500-1000mAh |
| 3D printed case | Your design |

### Wiring (if using external button)

```
Button Pin 1 → D1 (GPIO 2)
Button Pin 2 → GND
```

The built-in BOOT button can also be used (GPIO 0).

---

## Usage

### First Boot (WiFi Setup)

1. **Power on** - LED pulses slowly
2. **Connect phone** to WiFi network "VoiceRecorder-XXXXXX"
3. **Open browser** → automatically redirects to setup page (or go to http://192.168.4.1)
4. **Enter WiFi credentials** and submit
5. Device connects and saves credentials for next time

### Recording

1. **Power on** - LED flashes 3x when ready
2. **Press button** - Start recording (LED blinks fast)
3. **Press again** - Stop recording (or auto-stops at 5 min)
4. **Wait** - LED blinks slow during processing
5. **Done** - LED flashes 5x on success, email sent!

### LED Status

| Pattern | Meaning |
|---------|---------|
| Slow pulse | WiFi setup mode (connect your phone) |
| Off | Idle, ready |
| Fast blink | Recording |
| Slow blink | Processing (upload/summarize/email) |
| 5 quick flashes | Success! |
| 10 rapid flashes | WiFi reset (restarting) |
| Very slow blink | Error |

### Reset WiFi (Connect to New Network)

**Hold button for 3 seconds** → WiFi credentials cleared → Device restarts in AP mode

Use this at a new conference or if WiFi password changed.

---

## API Costs

| Service | Cost | Free Tier |
|---------|------|-----------|
| Groq Whisper | ~$0.03/hr audio | Yes |
| Groq LLM | ~$0.10/1M tokens | Yes |
| SendGrid | Free | 100 emails/day |

**Example:** 100 one-minute recordings = ~$0.50/month total

---

## Troubleshooting

### "No board detected"
- Use a data USB-C cable (not charge-only)
- Try different USB port
- Check `./build.sh ports`

### "SD card init failed"
- Format SD card as FAT32
- Check card is seated properly
- Try smaller capacity card (32GB max recommended)

### "WiFi connection failed"
- Check SSID/password in config.h
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)

### "API error"
- Verify API keys in config.h
- Check Groq/SendGrid dashboard for quota

### Upload fails
- Hold BOOT button while pressing RESET
- Release BOOT after "Connecting..." appears

---

## License

MIT - Do whatever you want with it.
