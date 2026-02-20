/*
 * voice_recorder.ino - Main Entry Point
 * 
 * XIAO ESP32S3 Sense Voice Recorder & Summarizer
 * 
 * Build with: ./build.sh compile
 * Upload with: ./build.sh upload
 */

#include <WiFi.h>
#include "config.h"
#include "wifi_manager.h"
#include "audio_recorder.h"
#include "whisper_client.h"
#include "llm_client.h"
#include "email_client.h"

// State machine
enum State {
    STATE_WIFI_SETUP,
    STATE_IDLE,
    STATE_RECORDING,
    STATE_UPLOADING,
    STATE_SUMMARIZING,
    STATE_EMAILING,
    STATE_ERROR
};

State currentState = STATE_WIFI_SETUP;
String lastError = "";

// Components
WiFiManager wifiManager;
AudioRecorder recorder;
WhisperClient whisper;
LLMClient llm;
EmailClient email;

// Button handling
volatile bool buttonPressed = false;
volatile unsigned long buttonPressTime = 0;
volatile unsigned long lastButtonTime = 0;
volatile bool buttonHeld = false;
const unsigned long debounceDelay = 200;
const unsigned long longPressTime = 3000;  // 3 seconds to reset WiFi

// LED patterns
unsigned long lastLedBlink = 0;
bool ledState = false;

void IRAM_ATTR buttonISR() {
    unsigned long now = millis();

    if (digitalRead(BUTTON_PIN) == LOW) {
        // Button pressed - only accept if outside debounce window
        if (now - lastButtonTime > debounceDelay) {
            buttonPressTime = now;
            buttonPressed = false;
            buttonHeld = true;
            lastButtonTime = now;  // Start debounce window from this press
        }
        // If debounced: ignore entirely - don't update lastButtonTime
    } else {
        // Button released
        if (buttonHeld) {
            unsigned long pressDuration = now - buttonPressTime;
            if (pressDuration < longPressTime) {
                buttonPressed = true;  // Short press
            }
            // Long press handled in loop()
            buttonHeld = false;
            lastButtonTime = now;
        }
        // If press was debounced (buttonHeld=false): ignore release too
    }
}

void checkLongPress() {
    // Check if button is being held (not in ISR to avoid blocking)
    if (buttonHeld && digitalRead(BUTTON_PIN) == LOW) {
        if (millis() - buttonPressTime >= longPressTime) {
            // Long press detected!
            buttonHeld = false;
            
            Serial.println("\n========================================");
            Serial.println("  LONG PRESS - RESETTING WIFI!");
            Serial.println("========================================\n");
            
            // Flash LED rapidly to indicate reset
            for (int i = 0; i < 10; i++) {
                digitalWrite(LED_PIN, LOW);
                delay(50);
                digitalWrite(LED_PIN, HIGH);
                delay(50);
            }
            
            // Clear saved WiFi credentials
            wifiManager.forgetWiFi();
            
            Serial.println("WiFi credentials cleared. Restarting...");
            delay(500);
            ESP.restart();
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  XIAO Voice Recorder & Summarizer");
    Serial.println("========================================\n");
    
    // Initialize LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);  // LED off (inverted)
    
    // Initialize button with interrupt
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonISR, CHANGE);
    
    // Start WiFi Manager (will enter AP mode if no saved credentials)
    Serial.println("Starting WiFi Manager...");
    if (wifiManager.begin()) {
        Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
        initAfterWiFi();
        // Clear any button state that accumulated during init (GPIO0 picks up noise)
        buttonHeld = false;
        buttonPressed = false;
        lastButtonTime = 0;
        currentState = STATE_IDLE;
    } else {
        Serial.printf("\nAP Mode: Connect to '%s'\n", wifiManager.getAPSSID().c_str());
        Serial.println("Then open http://192.168.4.1 in your browser");
        currentState = STATE_WIFI_SETUP;
    }
}

void initAfterWiFi() {
    // Initialize audio recorder
    if (!recorder.begin()) {
        Serial.println("Audio recorder init failed!");
        currentState = STATE_ERROR;
        lastError = "Audio init failed";
        return;
    }
    
    // Set API keys
    whisper.setApiKey(GROQ_API_KEY);
    llm.setApiKey(GROQ_API_KEY);
    email.setApiKey(SENDGRID_API_KEY);
    
    Serial.println("\n========================================");
    Serial.println("  Ready! Press button to record.");
    Serial.println("========================================\n");
    
    // Quick LED flash to indicate ready
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, LOW);
        delay(100);
        digitalWrite(LED_PIN, HIGH);
        delay(100);
    }
}

void loop() {
    // Check for long press (reset WiFi)
    checkLongPress();
    
    // Handle state machine
    switch (currentState) {
        case STATE_WIFI_SETUP:
            handleWiFiSetup();
            break;
            
        case STATE_IDLE:
            handleIdle();
            break;
            
        case STATE_RECORDING:
            handleRecording();
            break;
            
        case STATE_UPLOADING:
            handleUploading();
            break;
            
        case STATE_SUMMARIZING:
            handleSummarizing();
            break;
            
        case STATE_EMAILING:
            handleEmailing();
            break;
            
        case STATE_ERROR:
            handleError();
            break;
    }
    
    // Update LED based on state
    updateLED();
}

void handleWiFiSetup() {
    // Process captive portal requests
    wifiManager.update();
    
    // Check if we got connected
    if (wifiManager.isConnected()) {
        Serial.println("\nWiFi connected!");
        Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
        
        initAfterWiFi();
        // Clear any button state that accumulated during init (GPIO0 picks up noise)
        buttonHeld = false;
        buttonPressed = false;
        lastButtonTime = 0;
        currentState = STATE_IDLE;
    }

    // LED: slow pulse in AP mode
    static unsigned long lastPulse = 0;
    static int brightness = 0;
    static bool increasing = true;
    
    if (millis() - lastPulse > 10) {
        if (increasing) {
            brightness += 5;
            if (brightness >= 255) increasing = false;
        } else {
            brightness -= 5;
            if (brightness <= 0) increasing = true;
        }
        analogWrite(LED_PIN, 255 - brightness);  // Inverted
        lastPulse = millis();
    }
}

void handleIdle() {
    // LED solid off in idle
    digitalWrite(LED_PIN, HIGH);
    
    if (buttonPressed) {
        buttonPressed = false;
        Serial.println("\n>> Button pressed - starting recording...");
        
        if (recorder.startRecording()) {
            currentState = STATE_RECORDING;
            Serial.println("Recording... (press button again to stop)");
        } else {
            lastError = "Failed to start recording";
            currentState = STATE_ERROR;
        }
    }
}

void handleRecording() {
    // Capture audio continuously
    recorder.update();
    
    // Show progress every second
    static unsigned long lastProgress = 0;
    if (millis() - lastProgress > 1000) {
        Serial.printf("Recording: %lu sec, %lu bytes\n", 
                      recorder.getRecordDuration(), 
                      recorder.getBytesWritten());
        lastProgress = millis();
    }
    
    // Stop on button press or max duration
    if (buttonPressed || !recorder.recording()) {
        buttonPressed = false;
        recorder.stopRecording();
        
        Serial.printf("\nRecording complete: %lu seconds\n", recorder.getRecordDuration());
        Serial.println("Uploading to Whisper API...");
        
        currentState = STATE_UPLOADING;
    }
}

void handleUploading() {
    if (!wifiManager.reconnect()) {
        lastError = "WiFi lost, could not reconnect";
        currentState = STATE_ERROR;
        return;
    }
    if (whisper.transcribe(recorder.getBuffer(), recorder.getBufferSize())) {
        Serial.println("\nTranscription complete!");
        Serial.println("----------------------------------------");
        Serial.println(whisper.getTranscript());
        Serial.println("----------------------------------------");
        Serial.println("\nGenerating summary...");
        
        currentState = STATE_SUMMARIZING;
    } else {
        lastError = "Transcription failed: " + whisper.getError();
        currentState = STATE_ERROR;
    }
}

void handleSummarizing() {
    if (!wifiManager.reconnect()) {
        lastError = "WiFi lost before summarizing";
        currentState = STATE_ERROR;
        return;
    }
    String transcript = whisper.getTranscript();
    
    if (llm.summarize(transcript)) {
        Serial.println("\nSummary generated!");
        Serial.println("----------------------------------------");
        Serial.println(llm.getSummary());
        Serial.println("----------------------------------------");
        Serial.println("\nSending email...");
        
        currentState = STATE_EMAILING;
    } else {
        lastError = "Summary failed: " + llm.getError();
        currentState = STATE_ERROR;
    }
}

void handleEmailing() {
    if (!wifiManager.reconnect()) {
        lastError = "WiFi lost before emailing";
        currentState = STATE_ERROR;
        return;
    }
    String summary = llm.getSummary();
    String transcript = whisper.getTranscript();
    
    // Build email body
    String body = "=== VOICE RECORDING SUMMARY ===\n\n";
    body += summary;
    body += "\n\n=== FULL TRANSCRIPT ===\n\n";
    body += transcript;
    body += "\n\n---\nRecorded by XIAO Voice Recorder";
    
    // Generate subject with timestamp
    String subject = EMAIL_SUBJECT;
    subject += " - ";
    subject += String(millis() / 1000);  // Simple timestamp
    
    if (email.send(EMAIL_TO, subject, body)) {
        Serial.println("\n========================================");
        Serial.println("  SUCCESS! Email sent.");
        Serial.println("========================================\n");
        
        // Success flash
        for (int i = 0; i < 5; i++) {
            digitalWrite(LED_PIN, LOW);
            delay(50);
            digitalWrite(LED_PIN, HIGH);
            delay(50);
        }
        
        currentState = STATE_IDLE;
    } else {
        lastError = "Email failed: " + email.getError();
        currentState = STATE_ERROR;
    }
}

void handleError() {
    Serial.println("\n========================================");
    Serial.println("  ERROR: " + lastError);
    Serial.println("========================================\n");
    Serial.println("Press button to try again.");
    
    // Error flash pattern (slow blink)
    for (int i = 0; i < 10; i++) {
        digitalWrite(LED_PIN, LOW);
        delay(500);
        digitalWrite(LED_PIN, HIGH);
        delay(500);
    }
    
    // Reset to idle
    currentState = STATE_IDLE;
}

void updateLED() {
    unsigned long now = millis();
    
    switch (currentState) {
        case STATE_WIFI_SETUP:
            // Handled in handleWiFiSetup() with analog pulse
            break;
            
        case STATE_IDLE:
            // LED off
            digitalWrite(LED_PIN, HIGH);
            break;
            
        case STATE_RECORDING:
            // Fast blink (recording)
            if (now - lastLedBlink > 200) {
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState ? LOW : HIGH);
                lastLedBlink = now;
            }
            break;
            
        case STATE_UPLOADING:
        case STATE_SUMMARIZING:
        case STATE_EMAILING:
            // Medium blink (processing)
            if (now - lastLedBlink > 500) {
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState ? LOW : HIGH);
                lastLedBlink = now;
            }
            break;
            
        case STATE_ERROR:
            // Slow blink (error)
            if (now - lastLedBlink > 1000) {
                ledState = !ledState;
                digitalWrite(LED_PIN, ledState ? LOW : HIGH);
                lastLedBlink = now;
            }
            break;
    }
}
