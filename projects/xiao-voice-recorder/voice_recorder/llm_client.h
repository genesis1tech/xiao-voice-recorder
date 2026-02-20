/*
 * llm_client.h - LLM Client for Summarization (Groq)
 * 
 * Sends transcript to LLM for summarization
 */

#ifndef LLM_CLIENT_H
#define LLM_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class LLMClient {
private:
    WiFiClientSecure client;
    String apiKey;
    String summary;
    bool success;
    String errorMessage;
    
    bool sendRequest(const String& transcript);
    bool parseResponse(const String& response);
    
public:
    LLMClient();
    ~LLMClient();
    
    void setApiKey(const char* key) { apiKey = String(key); }
    
    // Summarize transcript
    bool summarize(const String& transcript);
    
    // Get results
    String getSummary() { return summary; }
    bool succeeded() { return success; }
    String getError() { return errorMessage; }
};

LLMClient::LLMClient() {
    success = false;
    summary = "";
    errorMessage = "";
}

LLMClient::~LLMClient() {
    client.stop();
}

bool LLMClient::summarize(const String& transcript) {
    DEBUG_PRINTF("Summarizing transcript (%d chars)...", transcript.length());
    
    if (transcript.length() == 0) {
        errorMessage = "Empty transcript";
        return false;
    }
    
    // Connect to Groq API
    DEBUG_PRINT("Connecting to Groq LLM API...");
    
    client.setInsecure();
    if (!client.connect("api.groq.com", 443)) {
        errorMessage = "Failed to connect to API";
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    DEBUG_PRINT("Connected! Sending transcript...");
    
    // Send request
    success = sendRequest(transcript);
    
    if (!success) {
        errorMessage = "Failed to send request";
        client.stop();
        return false;
    }
    
    // Read response
    DEBUG_PRINT("Reading LLM response...");
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
        DEBUG_PRINTF("Summary length: %d chars", summary.length());
        DEBUG_PRINTF("Summary preview: %.100s...", summary.c_str());
    }
    
    return success;
}

bool LLMClient::sendRequest(const String& transcript) {
    // Build the JSON request
    DynamicJsonDocument doc(8192);
    
    doc["model"] = "llama-3.3-70b-versatile";
    doc["temperature"] = 0.3;
    doc["max_tokens"] = 1024;
    
    // System prompt
    doc["messages"][0]["role"] = "system";
    doc["messages"][0]["content"] = 
        "You are a helpful assistant that summarizes voice recordings. "
        "Create a clear, concise summary of the following transcript. "
        "Include:\n"
        "1. Main topics discussed (bullet points)\n"
        "2. Key decisions or action items\n"
        "3. Any important details or deadlines mentioned\n"
        "Keep the summary organized and easy to scan. "
        "Format in plain text suitable for email.";
    
    // User message (transcript)
    doc["messages"][1]["role"] = "user";
    doc["messages"][1]["content"] = "Please summarize this conversation:\n\n" + transcript;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Build HTTP request
    String httpRequest = "POST /openai/v1/chat/completions HTTP/1.0\r\n";
    httpRequest += "Host: api.groq.com\r\n";
    httpRequest += "Authorization: Bearer " + apiKey + "\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += "Content-Length: " + String(jsonString.length()) + "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";
    httpRequest += jsonString;
    
    // Send request
    client.print(httpRequest);
    
    DEBUG_PRINTF("Request sent (%d bytes)", jsonString.length());
    return true;
}

bool LLMClient::parseResponse(const String& response) {
    DEBUG_PRINT("Parsing LLM response...");
    
    // Find the JSON body
    int bodyStart = response.indexOf("\r\n\r\n");
    if (bodyStart < 0) {
        errorMessage = "Invalid HTTP response";
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    String body = response.substring(bodyStart + 4);
    
    // Parse JSON
    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, body);
    
    if (error) {
        errorMessage = "JSON parse error: " + String(error.c_str());
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    // Check for API error
    if (doc.containsKey("error")) {
        errorMessage = doc["error"]["message"].as<String>();
        DEBUG_PRINTF("API Error: %s", errorMessage.c_str());
        return false;
    }
    
    // Extract summary from choices[0].message.content
    if (!doc.containsKey("choices") || doc["choices"].size() == 0) {
        errorMessage = "No choices in response";
        return false;
    }
    
    summary = doc["choices"][0]["message"]["content"].as<String>();
    
    return true;
}

#endif // LLM_CLIENT_H
