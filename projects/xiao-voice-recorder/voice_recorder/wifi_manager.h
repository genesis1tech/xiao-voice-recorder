/*
 * wifi_manager.h - WiFi Manager with AP Mode & Captive Portal
 * 
 * Automatically enters AP mode if WiFi can't connect
 * Captive portal for entering credentials
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config.h"

// AP Configuration
#define AP_SSID_PREFIX "VoiceRecorder-"
#define AP_PASSWORD ""  // Open network (or set a password)
#define AP_CHANNEL 1
#define AP_MAX_CONN 4

// Portal configuration
#define DNS_PORT 53
#define WEB_PORT 80

class WiFiManager {
private:
    WebServer server;
    DNSServer dnsServer;
    Preferences preferences;
    
    String apSSID;
    String savedSSID;
    String savedPassword;
    
    bool inAPMode;
    bool connected;
    unsigned long connectStartTime;
    
    void startAP();
    void stopAP();
    void setupCaptivePortal();
    void handleRoot();
    void handleSave();
    void handleNotFound();
    bool loadCredentials();
    bool saveCredentials(const String& ssid, const String& password);
    
public:
    WiFiManager();
    ~WiFiManager();
    
    bool begin();
    void update();
    
    bool isConnected() { return connected; }
    bool inAPModeNow() { return inAPMode; }
    String getAPSSID() { return apSSID; }
    String getSSID() { return savedSSID; }

    bool reconnect();
    void forgetWiFi();
};

WiFiManager::WiFiManager() : server(WEB_PORT) {
    inAPMode = false;
    connected = false;
    apSSID = String(AP_SSID_PREFIX) + String((uint32_t)ESP.getEfuseMac(), HEX);
}

WiFiManager::~WiFiManager() {
    if (inAPMode) {
        stopAP();
    }
}

bool WiFiManager::begin() {
    DEBUG_PRINT("Starting WiFi Manager...");
    
    // Load saved credentials
    if (loadCredentials()) {
        DEBUG_PRINTF("Found saved WiFi: %s", savedSSID.c_str());
        
        // Try to connect
        DEBUG_PRINT("Connecting to saved WiFi...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        
        connectStartTime = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - connectStartTime > WIFI_CONNECT_TIMEOUT) {
                DEBUG_PRINT("Connection timeout, starting AP mode...");
                break;
            }
            delay(500);
            DEBUG_PRINT(".");
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            connected = true;
            WiFi.setSleep(false);  // Prevent modem sleep dropping connection
            DEBUG_PRINTF("\nWiFi connected! IP: %s", WiFi.localIP().toString().c_str());
            return true;
        }
    }

    // Start AP mode
    startAP();
    return false;
}

bool WiFiManager::reconnect() {
    if (WiFi.status() == WL_CONNECTED) return true;
    if (savedSSID.length() == 0) return false;

    DEBUG_PRINT("WiFi dropped, reconnecting...");
    WiFi.disconnect();
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > WIFI_CONNECT_TIMEOUT) {
            DEBUG_PRINT("Reconnect timeout");
            return false;
        }
        delay(500);
    }
    WiFi.setSleep(false);
    DEBUG_PRINTF("Reconnected! IP: %s", WiFi.localIP().toString().c_str());
    return true;
}

void WiFiManager::startAP() {
    DEBUG_PRINT("Starting Access Point...");
    
    WiFi.mode(WIFI_AP_STA);  // APSTA so scanNetworks() doesn't drop AP clients
    WiFi.softAP(apSSID.c_str(), AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONN);
    
    // Start DNS server (captive portal)
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    // Setup web server routes
    setupCaptivePortal();
    server.begin();
    
    inAPMode = true;
    
    DEBUG_PRINTF("\n========================================");
    DEBUG_PRINTF("\n  AP MODE: Connect to '%s'", apSSID.c_str());
    DEBUG_PRINTF("\n  Then open: http://192.168.4.1");
    DEBUG_PRINTF("\n========================================\n");
}

void WiFiManager::stopAP() {
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);  // Switch fully to station mode
    inAPMode = false;
}

void WiFiManager::setupCaptivePortal() {
    server.on("/", std::bind(&WiFiManager::handleRoot, this));
    server.on("/save", HTTP_POST, std::bind(&WiFiManager::handleSave, this));
    server.onNotFound(std::bind(&WiFiManager::handleNotFound, this));
}

void WiFiManager::handleRoot() {
    // Scan for networks
    int n = WiFi.scanNetworks();

    String html = "<!DOCTYPE html><html><head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1'>"
        "<title>Voice Recorder - WiFi Setup</title>"
        "<style>"
        "* { box-sizing: border-box; margin: 0; padding: 0; }"
        "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;"
        "  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
        "  min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }"
        ".card { background: white; border-radius: 16px; padding: 32px; width: 100%;"
        "  max-width: 400px; box-shadow: 0 10px 40px rgba(0,0,0,0.2); }"
        ".icon { text-align: center; font-size: 48px; margin-bottom: 16px; }"
        "h1 { font-size: 24px; margin-bottom: 8px; color: #333; }"
        ".subtitle { color: #666; margin-bottom: 20px; font-size: 14px; }"
        ".net-list { margin-bottom: 16px; max-height: 220px; overflow-y: auto;"
        "  border: 2px solid #e0e0e0; border-radius: 8px; }"
        ".net { display: flex; align-items: center; justify-content: space-between;"
        "  padding: 12px 14px; cursor: pointer; border-bottom: 1px solid #f0f0f0;"
        "  transition: background 0.15s; }"
        ".net:last-child { border-bottom: none; }"
        ".net:hover { background: #f5f5f5; }"
        ".net.selected { background: #f0f3ff; }"
        ".net-name { font-size: 15px; color: #333; font-weight: 500; }"
        ".net-rssi { font-size: 12px; color: #999; }"
        ".lock { font-size: 12px; color: #bbb; margin-left: 6px; }"
        ".form-group { margin-bottom: 14px; }"
        "label { display: block; font-weight: 600; margin-bottom: 6px; color: #444; font-size: 14px; }"
        "input { width: 100%; padding: 12px 16px; border: 2px solid #e0e0e0;"
        "  border-radius: 8px; font-size: 16px; }"
        "input:focus { outline: none; border-color: #667eea; }"
        "button { width: 100%; padding: 14px;"
        "  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);"
        "  color: white; border: none; border-radius: 8px; font-size: 16px;"
        "  font-weight: 600; cursor: pointer; }"
        ".none { padding: 16px; text-align: center; color: #999; font-size: 14px; }"
        "</style></head><body><div class='card'>"
        "<div class='icon'>&#x1F3A4;</div>"
        "<h1>Voice Recorder</h1>"
        "<p class='subtitle'>Tap a network, enter password, connect.</p>";

    // Network list
    if (n <= 0) {
        html += "<div class='net-list'><div class='none'>No networks found</div></div>";
    } else {
        html += "<div class='net-list'>";
        for (int i = 0; i < n; i++) {
            String ssid   = WiFi.SSID(i);
            int    rssi   = WiFi.RSSI(i);
            bool   secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);

            // Signal bars: ▂▄▆█ rough mapping
            String bars;
            if      (rssi >= -55) bars = "&#x2588;&#x2588;&#x2588;&#x2588;";
            else if (rssi >= -65) bars = "&#x2588;&#x2588;&#x2588;&#x2591;";
            else if (rssi >= -75) bars = "&#x2588;&#x2588;&#x2591;&#x2591;";
            else                  bars = "&#x2588;&#x2591;&#x2591;&#x2591;";

            ssid.replace("'", "\\'");  // escape for JS
            html += "<div class='net' onclick=\"selectNet('" + ssid + "')\">"
                    "<span class='net-name'>" + WiFi.SSID(i) + "</span>"
                    "<span class='net-rssi'>" + bars + (secure ? " &#x1F512;" : "") + "</span>"
                    "</div>";
        }
        html += "</div>";
    }

    html += "<form action='/save' method='POST'>"
            "<div class='form-group'>"
            "<label for='ssid'>Network</label>"
            "<input type='text' id='ssid' name='ssid' placeholder='Network name' required>"
            "</div>"
            "<div class='form-group'>"
            "<label for='password'>Password</label>"
            "<input type='password' id='password' name='password' placeholder='WiFi password'>"
            "</div>"
            "<button type='submit'>Connect</button>"
            "</form>"
            "<script>"
            "function selectNet(s){"
            "  document.getElementById('ssid').value=s;"
            "  document.querySelectorAll('.net').forEach(function(el){el.classList.remove('selected');});"
            "  event.currentTarget.classList.add('selected');"
            "  document.getElementById('password').focus();"
            "}"
            "</script>"
            "</div></body></html>";

    server.send(200, "text/html", html);
}

void WiFiManager::handleSave() {
    if (!server.hasArg("ssid") || server.arg("ssid").length() == 0) {
        server.send(400, "text/plain", "SSID required");
        return;
    }
    
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    DEBUG_PRINTF("Received WiFi credentials: %s", ssid.c_str());
    
    // Save credentials
    saveCredentials(ssid, password);
    
    // Send success page
    String html = R"(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>Connecting...</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            padding: 20px;
        }
        .card {
            background: white;
            border-radius: 16px;
            padding: 32px;
            text-align: center;
            max-width: 400px;
        }
        .spinner {
            width: 50px;
            height: 50px;
            border: 4px solid #e0e0e0;
            border-top-color: #667eea;
            border-radius: 50%;
            animation: spin 1s linear infinite;
            margin: 0 auto 20px;
        }
        @keyframes spin { to { transform: rotate(360deg); } }
        h1 { color: #333; margin-bottom: 8px; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="card">
        <div class="spinner"></div>
        <h1>Connecting...</h1>
        <p>Your device is connecting to WiFi. This page will close automatically.</p>
    </div>
</body>
</html>
)";
    
    server.send(200, "text/html", html);
    delay(1000);
    
    // Stop AP (switches to WIFI_STA) and try to connect
    stopAP();

    WiFi.begin(ssid.c_str(), password.c_str());
    
    connectStartTime = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - connectStartTime > 15000) {
            DEBUG_PRINT("Connection failed, restarting AP...");
            ESP.restart();  // Restart to try again
            break;
        }
        delay(500);
        DEBUG_PRINT(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        connected = true;
        WiFi.setSleep(false);
        DEBUG_PRINTF("\nConnected! IP: %s", WiFi.localIP().toString().c_str());
    }
}

void WiFiManager::handleNotFound() {
    // Redirect all requests to root (captive portal behavior)
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}

void WiFiManager::update() {
    if (inAPMode) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

bool WiFiManager::loadCredentials() {
    preferences.begin("wifi", true);
    savedSSID = preferences.getString("ssid", "");
    savedPassword = preferences.getString("password", "");
    preferences.end();
    
    return savedSSID.length() > 0;
}

bool WiFiManager::saveCredentials(const String& ssid, const String& password) {
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.end();
    
    savedSSID = ssid;
    savedPassword = password;
    
    DEBUG_PRINTF("Saved WiFi credentials: %s", ssid.c_str());
    return true;
}

void WiFiManager::forgetWiFi() {
    DEBUG_PRINT("Clearing saved WiFi credentials...");
    
    preferences.begin("wifi", false);
    preferences.clear();
    preferences.end();
    
    savedSSID = "";
    savedPassword = "";
    
    DEBUG_PRINT("WiFi credentials cleared!");
}

#endif // WIFI_MANAGER_H
