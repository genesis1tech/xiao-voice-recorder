# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Embedded firmware for **Seeed XIAO ESP32S3 Sense** — records voice via I2S microphone, transcribes with Groq Whisper API, summarizes with Groq LLM, and emails the result via SendGrid.

Build system: **Arduino CLI** (no Arduino IDE). Target board FQBN: `esp32:esp32:XIAO_ESP32S3`.

## Build Commands

Use `./build.sh` (preferred) or `make`:

```bash
./build.sh setup     # First-time: installs arduino-cli, ESP32 core, ArduinoJson lib
./build.sh compile   # Compile only (checks src/config.h exists)
./build.sh upload    # Upload to board (auto-detects USB port)
./build.sh monitor   # Serial monitor at 115200 baud
./build.sh all       # Compile + upload + monitor
./build.sh ports     # List detected boards
./build.sh clean     # Remove build/ directory
```

Compiled binary lands in `build/voice_recorder.ino.bin`. Port is auto-detected from `/dev/cu.usbmodem*` or `/dev/cu.wchusbserial*`.

## Configuration

`src/config.h` is **gitignored** (contains API keys). Copy `src/config.h.example` to `src/config.h` before compiling.

```bash
cp voice_recorder/config.h.example voice_recorder/config.h
```

Required fields: `GROQ_API_KEY`, `SENDGRID_API_KEY`, `EMAIL_TO`, `EMAIL_FROM`.

WiFi credentials are **not** stored in `config.h` — they are entered at runtime via the captive portal.

## Architecture

The firmware is a **state machine** with these states (defined in `voice_recorder.ino`):

```
STATE_WIFI_SETUP → STATE_IDLE → STATE_RECORDING → STATE_UPLOADING
                                                        ↓
                      STATE_IDLE ← STATE_EMAILING ← STATE_SUMMARIZING
                         ↑
                    STATE_ERROR (recovers to IDLE on button press)
```

**Module layout** (all `.h` files, header-only implementation):

| File | Class | Responsibility |
|------|-------|----------------|
| `config.h` | — | All pin assignments, timing constants, API key defines |
| `wifi_manager.h` | `WiFiManager` | AP mode captive portal (WebServer + DNSServer), credentials stored in ESP32 NVS via `Preferences` |
| `audio_recorder.h` | `AudioRecorder` | I2S mic capture, WAV file write to SD card |
| `whisper_client.h` | `WhisperClient` | HTTPS multipart upload to Groq Whisper API |
| `llm_client.h` | `LLMClient` | HTTPS POST to Groq chat completions for summarization |
| `email_client.h` | `EmailClient` | HTTPS POST to SendGrid API |

**Key hardware details:**
- LED on GPIO 21 is **active-low** (`LOW` = on, `HIGH` = off)
- Button uses `INPUT_PULLUP` + interrupt (`FALLING` edge) on `BUTTON_PIN` (D1/GPIO 2)
- Long press (3s) clears WiFi credentials and restarts
- I2S mic pins: BCLK=42, WS=41, DIN=40 (XIAO ESP32S3 Sense built-in PDM mic)
- SD card CS on GPIO 21 (shared with LED — verify in config if issues arise)
- Audio: 16kHz, 16-bit, mono WAV written to `/recording.wav` on SD

**WiFi flow:** On boot, `WiFiManager::begin()` tries saved credentials from NVS. If none or timeout, starts AP `VoiceRecorder-<MAC>` with captive portal at `192.168.4.1`. Credentials saved to NVS namespace `"wifi"` via `Preferences`.

## Libraries Required

- `esp32:esp32` core (includes SD, WiFi, WebServer, DNSServer, Preferences, I2S driver)
- `ArduinoJson` (installed separately via arduino-cli)

## Upload Troubleshooting

If upload fails: hold BOOT button, press RESET, release BOOT after "Connecting..." appears in terminal.
