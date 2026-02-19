/*
 * audio_recorder.h - I2S Microphone Recording for XIAO ESP32S3 Sense
 */

#ifndef AUDIO_RECORDER_H
#define AUDIO_RECORDER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#include "config.h"

// WAV file header structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t fileSize;      // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmtSize;       // 16 for PCM
    uint16_t audioFormat;   // 1 for PCM
    uint16_t numChannels;   // 1 for mono
    uint32_t sampleRate;    // 16000
    uint32_t byteRate;      // sampleRate * numChannels * bitsPerSample/8
    uint16_t blockAlign;    // numChannels * bitsPerSample/8
    uint16_t bitsPerSample; // 16
    char data[4];           // "data"
    uint32_t dataSize;      // Number of bytes in data
} WAVHeader;

class AudioRecorder {
private:
    bool isRecording;
    bool isInitialized;
    File audioFile;
    uint32_t bytesWritten;
    unsigned long recordStartTime;
    
    // I2S configuration
    i2s_config_t i2s_config;
    i2s_pin_config_t pin_config;
    
    void createWAVHeader(WAVHeader* header, uint32_t dataSize);
    
public:
    AudioRecorder();
    ~AudioRecorder();
    
    bool begin();
    bool startRecording(const char* filename);
    bool stopRecording();
    void update();  // Call this in loop() while recording
    bool recording() { return isRecording; }
    uint32_t getBytesWritten() { return bytesWritten; }
    unsigned long getRecordDuration();
};

AudioRecorder::AudioRecorder() {
    isRecording = false;
    isInitialized = false;
    bytesWritten = 0;
}

AudioRecorder::~AudioRecorder() {
    if (isRecording) {
        stopRecording();
    }
}

bool AudioRecorder::begin() {
    DEBUG_PRINT("Initializing audio recorder...");
    
    // Initialize SD card
    if (!SD.begin(SD_CS)) {
        DEBUG_PRINT("SD card initialization failed!");
        return false;
    }
    DEBUG_PRINT("SD card initialized.");
    
    // Configure I2S
    i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };
    
    pin_config = {
        .bck_io_num = I2S_BCLK,
        .ws_io_num = I2S_WS,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DO
    };
    
    // Install I2S driver
    esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        DEBUG_PRINTF("I2S driver install failed: %d", err);
        return false;
    }
    
    err = i2s_set_pin(I2S_NUM_0, &pin_config);
    if (err != ESP_OK) {
        DEBUG_PRINTF("I2S set pin failed: %d", err);
        return false;
    }
    
    isInitialized = true;
    DEBUG_PRINT("Audio recorder initialized successfully.");
    return true;
}

bool AudioRecorder::startRecording(const char* filename) {
    if (!isInitialized) {
        DEBUG_PRINT("Recorder not initialized!");
        return false;
    }
    
    if (isRecording) {
        DEBUG_PRINT("Already recording!");
        return false;
    }
    
    // Delete existing file
    if (SD.exists(filename)) {
        SD.remove(filename);
    }
    
    // Open file for writing
    audioFile = SD.open(filename, FILE_WRITE);
    if (!audioFile) {
        DEBUG_PRINTF("Failed to create file: %s", filename);
        return false;
    }
    
    // Write placeholder WAV header (will update later)
    WAVHeader header;
    createWAVHeader(&header, 0);
    audioFile.write((uint8_t*)&header, sizeof(WAVHeader));
    
    bytesWritten = 0;
    isRecording = true;
    recordStartTime = millis();
    
    // Clear I2S buffer
    i2s_zero_dma_buffer(I2S_NUM_0);
    
    DEBUG_PRINTF("Recording started: %s", filename);
    return true;
}

void AudioRecorder::update() {
    if (!isRecording || !audioFile) return;
    
    // Buffer for I2S data
    uint8_t i2s_buffer[1024];
    size_t bytes_read = 0;
    
    // Read from I2S
    esp_err_t err = i2s_read(I2S_NUM_0, i2s_buffer, sizeof(i2s_buffer), &bytes_read, 100);
    
    if (err == ESP_OK && bytes_read > 0) {
        // Write to SD card
        size_t bytes_written = audioFile.write(i2s_buffer, bytes_read);
        bytesWritten += bytes_written;
        
        // Check max duration
        if (getRecordDuration() >= MAX_RECORDING_SECONDS) {
            DEBUG_PRINT("Max recording time reached");
            stopRecording();
        }
    }
}

bool AudioRecorder::stopRecording() {
    if (!isRecording) return true;
    
    isRecording = false;
    
    // Close file first
    audioFile.close();
    
    // Reopen to update header with correct size
    File file = SD.open(RECORDING_FILENAME, FILE_WRITE);
    if (file) {
        WAVHeader header;
        createWAVHeader(&header, bytesWritten);
        file.seek(0);
        file.write((uint8_t*)&header, sizeof(WAVHeader));
        file.close();
        DEBUG_PRINTF("Recording stopped. Size: %lu bytes, Duration: %lu sec", 
                     bytesWritten, getRecordDuration());
    }
    
    return true;
}

void AudioRecorder::createWAVHeader(WAVHeader* header, uint32_t dataSize) {
    memcpy(header->riff, "RIFF", 4);
    header->fileSize = dataSize + sizeof(WAVHeader) - 8;
    memcpy(header->wave, "WAVE", 4);
    memcpy(header->fmt, "fmt ", 4);
    header->fmtSize = 16;
    header->audioFormat = 1;  // PCM
    header->numChannels = CHANNELS;
    header->sampleRate = SAMPLE_RATE;
    header->bitsPerSample = SAMPLE_BITS;
    header->byteRate = SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8);
    header->blockAlign = CHANNELS * (SAMPLE_BITS / 8);
    memcpy(header->data, "data", 4);
    header->dataSize = dataSize;
}

unsigned long AudioRecorder::getRecordDuration() {
    if (bytesWritten == 0) return 0;
    // Duration = bytes / (sample_rate * channels * bytes_per_sample)
    return bytesWritten / (SAMPLE_RATE * CHANNELS * (SAMPLE_BITS / 8));
}

#endif // AUDIO_RECORDER_H
