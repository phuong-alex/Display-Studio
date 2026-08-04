#include <Arduino.h>
#include <WebServer.h>
#include "app_config.h"
#include "device.h"
#include "device_web.h"
#include "logger.h"
#include "wifi_manager.h"

namespace {
WebServer server(80);

String page(const String& message = "") {
    String html = "<!doctype html><html><head><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<style>body{font-family:Arial;background:#f3f4f6;padding:24px}"
                  ".c{max-width:500px;margin:auto;background:white;padding:24px;border-radius:14px}"
                  "input,button{width:100%;box-sizing:border-box;padding:12px;margin-top:10px}"
                  "button{background:#111827;color:white;border:0;border-radius:8px}</style></head>"
                  "<body><div class='c'><h2>EInk Device</h2>";
    if (!message.isEmpty()) html += "<p>" + message + "</p>";
    html += "<p><b>Device ID:</b> " + Device::id() + "</p>";
    html += "<p><b>IP:</b> " + WiFiManager::ipAddress() + "</p>";
    html += "<form method='POST' action='/config'>"
            "<label>Server URL</label>"
            "<input name='server' value='" + AppConfig::serverUrl() + "' required>"
            "<button>Luu server</button></form></div></body></html>";
    return html;
}
}

namespace DeviceWeb {
void begin() {
    server.on("/", HTTP_GET, []() {
        server.sendHeader("Location", "/config", true);
        server.send(302, "text/plain", "");
    });

    server.on("/config", HTTP_GET, []() {
        server.send(200, "text/html; charset=utf-8", page());
    });

    server.on("/config", HTTP_POST, []() {
        String url = server.arg("server");
        url.trim();
        while (url.endsWith("/")) url.remove(url.length() - 1);
        if (url.isEmpty()) {
            server.send(400, "text/html; charset=utf-8", page("Server URL required."));
            return;
        }
        AppConfig::setServerUrl(url);
        server.send(200, "text/html; charset=utf-8", page("Saved."));
    });

    server.begin();
}

void loop() { server.handleClient(); }
}
