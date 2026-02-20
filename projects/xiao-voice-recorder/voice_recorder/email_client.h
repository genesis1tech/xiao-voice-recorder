/*
 * email_client.h - Email Client (SendGrid API)
 * 
 * Sends summary via email
 */

#ifndef EMAIL_CLIENT_H
#define EMAIL_CLIENT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"

class EmailClient {
private:
    WiFiClientSecure client;
    String apiKey;
    bool success;
    String errorMessage;
    
    bool sendRequest(const String& to, const String& subject, const String& body);
    
public:
    EmailClient();
    ~EmailClient();
    
    void setApiKey(const char* key) { apiKey = String(key); }
    
    // Send email
    bool send(const String& to, const String& subject, const String& body);
    
    // Get status
    bool succeeded() { return success; }
    String getError() { return errorMessage; }
};

EmailClient::EmailClient() {
    success = false;
    errorMessage = "";
}

EmailClient::~EmailClient() {
    client.stop();
}

bool EmailClient::send(const String& to, const String& subject, const String& body) {
    DEBUG_PRINTF("Sending email to: %s", to.c_str());
    DEBUG_PRINTF("Subject: %s", subject.c_str());
    
    // Connect to SendGrid API
    DEBUG_PRINT("Connecting to SendGrid API...");
    
    client.setInsecure();
    if (!client.connect("api.sendgrid.com", 443)) {
        errorMessage = "Failed to connect to SendGrid";
        DEBUG_PRINT(errorMessage);
        return false;
    }
    
    DEBUG_PRINT("Connected! Sending email...");
    
    // Build JSON request
    DynamicJsonDocument doc(4096);
    
    doc["personalizations"][0]["to"][0]["email"] = to;
    doc["from"]["email"] = EMAIL_FROM;
    doc["subject"] = subject;
    doc["content"][0]["type"] = "text/plain";
    doc["content"][0]["value"] = body;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Build HTTP request
    String httpRequest = "POST /v3/mail/send HTTP/1.0\r\n";
    httpRequest += "Host: api.sendgrid.com\r\n";
    httpRequest += "Authorization: Bearer " + apiKey + "\r\n";
    httpRequest += "Content-Type: application/json\r\n";
    httpRequest += "Content-Length: " + String(jsonString.length()) + "\r\n";
    httpRequest += "Connection: close\r\n";
    httpRequest += "\r\n";
    httpRequest += jsonString;
    
    // Send request
    client.print(httpRequest);
    
    DEBUG_PRINTF("Request sent (%d bytes)", jsonString.length());
    
    // Read response (SendGrid returns 202 on success)
    String response = "";
    unsigned long startTime = millis();

    while ((client.connected() || client.available()) && (millis() - startTime < 15000)) {
        while (client.available()) {
            char c = client.read();
            response += c;
        }
        delay(10);
    }
    
    client.stop();
    
    // Check response status
    if (response.indexOf("202 Accepted") >= 0) {
        DEBUG_PRINT("Email sent successfully!");
        success = true;
        return true;
    } else if (response.indexOf("202") >= 0) {
        // Sometimes just "202" appears
        DEBUG_PRINT("Email sent successfully!");
        success = true;
        return true;
    } else {
        // Extract error if present
        int bodyStart = response.indexOf("\r\n\r\n");
        if (bodyStart >= 0) {
            errorMessage = response.substring(bodyStart + 4);
        } else {
            errorMessage = "Email send failed";
        }
        DEBUG_PRINTF("Email error: %s", errorMessage.c_str());
        DEBUG_PRINTF("Full response: %s", response.c_str());
        return false;
    }
}

#endif // EMAIL_CLIENT_H
