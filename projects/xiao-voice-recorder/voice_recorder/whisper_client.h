/*
 * whisper_client.h - Whisper API Client (Groq)
 * 
 * Sends audio to Groq's Whisper API for transcription
 */

#ifndef WHISPER_CLIENT_H
#define WHISPER_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "config.h"

class WhisperClient {
private:
    WiFiClientSecure client;
    String apiKey;
    String transcript;
    bool success;
    String errorMessage;
    
    bool sendMultipartFormData(const uint8_t* audioData, size_t audioSize);
    bool parseResponse(const String& response);
    
public:
    WhisperClient();
    ~WhisperClient();
    
    void setApiKey(const char* key) { apiKey = String(key); }
    
    // Transcribe audio from a WAV buffer in memory
    bool transcribe(const uint8_t* audioData, size_t audioSize);
    
    // Get results
    String getTranscript() { return transcript; }
    bool succeeded() { return success; }
    String getError() { return errorMessage; }
};

WhisperClient::WhisperClient() {
    success = false;
    transcript = "";
    errorMessage = "";
}

WhisperClient::~WhisperClient() {
    client.stop();
}

bool WhisperClient::transcribe(const uint8_t* audioData, size_t audioSize) {
    DEBUG_PRINTF("Transcribing buffer: %d bytes", audioSize);

    if (!audioData || audioSize == 0) {
        errorMessage = "Empty audio buffer";
        DEBUG_PRINT(errorMessage);
        return false;
    }

    // Connect to Groq API
    DEBUG_PRINT("Connecting to Groq API...");

    client.setInsecure();  // Skip cert verification for now
    if (!client.connect("api.groq.com", 443)) {
        errorMessage = "Failed to connect to API";
        DEBUG_PRINT(errorMessage);
        return false;
    }

    DEBUG_PRINT("Connected! Sending audio...");

    // Send multipart form data
    success = sendMultipartFormData(audioData, audioSize);

    if (!success) {
        errorMessage = "Failed to send request";
        return false;
    }
    
    // Read response
    DEBUG_PRINT("Reading response...");
    String response = "";
    unsigned long startTime = millis();
    
    while ((client.connected() || client.available()) && (millis() - startTime < API_TIMEOUT)) {
        while (client.available()) {
            char c = client.read();
            response += c;
        }
        delay(10);
    }

    client.stop();
    
    // Parse response
    success = parseResponse(response);
    
    if (success) {
        DEBUG_PRINTF("Transcript length: %d chars", transcript.length());
        DEBUG_PRINTF("Transcript preview: %.100s...", transcript.c_str());
    }
    
    return success;
}

bool WhisperClient::sendMultipartFormData(const uint8_t* audioData, size_t audioSize) {
    String boundary = "----XIAOVoiceRecorder12345";
    
    // Build the multipart request
    String header = "--" + boundary + "\r\n";
    header += "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n";
    header += "Content-Type: audio/wav\r\n\r\n";
    
    String footer = "\r\n--" + boundary + "\r\n";
    footer += "Content-Disposition: form-data; name=\"model\"\r\n\r\n";
    footer += "whisper-large-v3-turbo\r\n";
    footer += "--" + boundary + "\r\n";
    footer += "Content-Disposition: form-data; name=\"language\"\r\n\r\n";
    footer += "en\r\n";
    footer += "--" + boundary + "--\r\n";
    
    size_t contentLength = header.length() + audioSize + footer.length();
    
    // Build HTTP request
    String httpRequest = "POST /openai/v1/audio/transcriptions HTTP/1.0\r\n";
    httpRequest += "Host: api.groq.com\r\n";
    httpRequest += "Authorization: Bearer " + apiKey + "\r\n";
    httpRequest += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    httpRequest += "Content-Length: " + String(contentLength) + "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";
    
    // Send HTTP headers
    client.print(httpRequest);
    
    // Send multipart header
    client.print(header);
    
    // Send audio data in chunks
    size_t sent = 0;
    const size_t chunkSize = 4096;
    while (sent < audioSize) {
        size_t toSend = min(chunkSize, audioSize - sent);
        size_t written = client.write(audioData + sent, toSend);
        if (written == 0) {
            DEBUG_PRINT("Error sending audio data");
            return false;
        }
        sent += written;
        
        // Progress indicator
        if (sent % 16384 == 0) {
            DEBUG_PRINTF("Sent %d / %d bytes", sent, audioSize);
        }
    }
    
    // Send multipart footer
    client.print(footer);
    
    DEBUG_PRINT("Request sent completely");
    return true;
}

bool WhisperClient::parseResponse(const String& response) {
    DEBUG_PRINT("Parsing response...");
    
    // Find the JSON body
    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart < 0) {
        errorMessage = "Invalid HTTP response";
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    String body = response.substring(bodyStart + 4);
    
    // Check for error
    if (body.indexOf("\"error\"") >= 0) {
        int errorStart = body.indexOf("\"message\":\"");
        if (errorStart >= 0) {
            errorStart += 11;
            int errorEnd = body.indexOf("\"", errorStart);
            errorMessage = body.substring(errorStart, errorEnd);
        } else {
            errorMessage = "API error";
        }
        DEBUG_PRINTF("API Error: %s", errorMessage.c_str());
        return false;
    }
    
    // Extract transcript
    int textStart = body.indexOf("\"text\":\"");
    if (textStart < 0) {
        errorMessage = "No transcript in response";
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    textStart += 8;
    int textEnd = body.indexOf("\",\"", textStart);
    if (textEnd < 0) {
        textEnd = body.indexOf("\"}", textStart);
    }
    
    if (textEnd < 0) {
        errorMessage = "Failed to parse transcript";
        return false;
    }
    
    transcript = body.substring(textStart, textEnd);
    
    // Unescape JSON string
    transcript.replace("\\n", "\n");
    transcript.replace("\\\"", "\"");
    transcript.replace("\\\\", "\\");
    
    return true;
}

#endif // WHISPER_CLIENT_H
