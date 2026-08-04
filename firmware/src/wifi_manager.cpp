#include <Arduino.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include "app_config.h"
#include "config.h"
#include "display.h"
#include "logger.h"
#include "wifi_manager.h"

namespace {
Preferences preferences;
DNSServer dnsServer;
WebServer portalServer(Config::PORTAL_HTTP_PORT);
WiFiManager::State currentState = WiFiManager::State::Idle;
String storedSsid;
String storedPassword;
String portalName;
uint32_t lastRetryAt = 0;

String page(const String& message = "") {
    String html = "<!doctype html><html><head><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<style>body{font-family:Arial;background:#f3f4f6;padding:24px}"
                  ".c{max-width:440px;margin:auto;background:white;padding:24px;border-radius:14px}"
                  "input,button{width:100%;box-sizing:border-box;padding:12px;margin-top:10px}"
                  "button{background:#111827;color:white;border:0;border-radius:8px}</style></head>"
                  "<body><div class='c'><h2>EInk WiFi Setup</h2>";
    if (!message.isEmpty()) html += "<p>" + message + "</p>";
    html += "<form method='POST' action='/save'>"
            "<input name='ssid' placeholder='SSID' required value='" + storedSsid + "'>"
            "<input name='password' type='password' placeholder='Password'>"
            "<input name='server' placeholder='Server URL' required value='" + AppConfig::serverUrl() + "'>"
            "<button>Luu va khoi dong lai</button></form></div></body></html>";
    return html;
}

void loadCredentials() {
    preferences.begin(Config::PREF_WIFI_NAMESPACE, true);
    storedSsid = preferences.getString(Config::PREF_KEY_SSID, "");
    storedPassword = preferences.getString(Config::PREF_KEY_PASSWORD, "");
    preferences.end();
}

void saveCredentials(const String& ssid, const String& password) {
    preferences.begin(Config::PREF_WIFI_NAMESPACE, false);
    preferences.putString(Config::PREF_KEY_SSID, ssid);
    preferences.putString(Config::PREF_KEY_PASSWORD, password);
    preferences.end();
}

bool connectStation() {
    if (storedSsid.isEmpty()) return false;
    currentState = WiFiManager::State::Connecting;
    Display::showConnecting(storedSsid);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.persistent(false);
    WiFi.begin(storedSsid.c_str(), storedPassword.c_str());

    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - started < Config::WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        currentState = WiFiManager::State::Connected;
        Display::showConnected(WiFi.localIP().toString(), WiFi.RSSI());
        Logger::info("WiFi connected: " + WiFi.localIP().toString());
        return true;
    }

    WiFi.disconnect(true);
    return false;
}

void startPortal() {
    currentState = WiFiManager::State::Portal;
    const uint64_t chipId = ESP.getEfuseMac();
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%04X", static_cast<uint16_t>(chipId & 0xFFFF));
    portalName = "EInk-Setup-" + String(suffix);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(portalName.c_str());
    dnsServer.start(Config::PORTAL_DNS_PORT, "*", WiFi.softAPIP());

    portalServer.on("/", HTTP_GET, []() {
        portalServer.send(200, "text/html; charset=utf-8", page());
    });

    portalServer.on("/save", HTTP_POST, []() {
        const String ssid = portalServer.arg("ssid");
        const String password = portalServer.arg("password");
        String serverUrl = portalServer.arg("server");
        serverUrl.trim();
        while (serverUrl.endsWith("/")) {
            serverUrl.remove(serverUrl.length() - 1);
        }
        if (ssid.isEmpty() || serverUrl.isEmpty()) {
            portalServer.send(400, "text/html; charset=utf-8", page("SSID required."));
            return;
        }
        saveCredentials(ssid, password);
        AppConfig::setServerUrl(serverUrl);
        portalServer.send(200, "text/html; charset=utf-8", page("Saved. Restarting..."));
        delay(1000);
        ESP.restart();
    });

    portalServer.onNotFound([]() {
        portalServer.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
        portalServer.send(302, "text/plain", "");
    });

    portalServer.begin();
    Display::showPortal(portalName, WiFi.softAPIP().toString());
}

}

namespace WiFiManager {
void begin() {
    loadCredentials();
    if (!connectStation()) startPortal();
}

void loop() {
    if (currentState == State::Portal) {
        dnsServer.processNextRequest();
        portalServer.handleClient();
        delay(2);
        return;
    }

    if (currentState == State::Connected && WiFi.status() != WL_CONNECTED) {
        currentState = State::Connecting;
        lastRetryAt = millis();
    }

    if (currentState == State::Connecting &&
        millis() - lastRetryAt >= Config::WIFI_RETRY_INTERVAL_MS) {
        lastRetryAt = millis();
        if (!connectStation()) startPortal();
    }
}

State state() { return currentState; }
bool isConnected() { return WiFi.status() == WL_CONNECTED; }
String ipAddress() { return isConnected() ? WiFi.localIP().toString() : WiFi.softAPIP().toString(); }
int32_t rssi() { return isConnected() ? WiFi.RSSI() : 0; }
String accessPointName() { return portalName; }
}
