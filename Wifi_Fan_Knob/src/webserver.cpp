#include "webserver.h"
#include "config.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

// Port comes from config, so the server is created after config loads
static AsyncWebServer *server = nullptr;

// Reboot once the response has been delivered to the browser
static void restart_after_response(AsyncWebServerRequest *request) {
  request->onDisconnect([]() {
    Serial.println("[WEB] Restarting...");
    ESP.restart();
  });
}

void init_webserver() {
  server = new AsyncWebServer(config.webserver.port);

  // Serve only the UI page — /config.json in SPIFFS holds credentials
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!SPIFFS.exists("/index.html")) {
      request->send(500, "text/plain", "index.html missing from SPIFFS (run: pio run -t uploadfs)");
      return;
    }
    request->send(SPIFFS, "/index.html", "text/html");
  });

  // Save WiFi credentials, then reboot to connect
  server->on("/api/wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true)) {
      request->send(400, "text/plain", "Missing ssid");
      return;
    }
    String ssid = request->getParam("ssid", true)->value();
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
    if (ssid.length() == 0 || ssid.length() > 32 || pass.length() > 63) {
      request->send(400, "text/plain", "SSID must be 1-32 chars, password 0-63");
      return;
    }
    config.wifi.saveCredentials = true;
    setWiFiCredentials(ssid.c_str(), pass.c_str());
    restart_after_response(request);
    request->send(200, "text/plain", "Saved. Rebooting to connect...");
  });

  // Clear saved credentials, then reboot into AP mode
  server->on("/api/wifi/forget", HTTP_POST, [](AsyncWebServerRequest *request) {
    setWiFiCredentials("", "");
    restart_after_response(request);
    request->send(200, "text/plain", "Forgotten. Rebooting into AP mode...");
  });

  // Non-blocking scan: 202 while scanning (client polls), 200 + JSON when done
  server->on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      request->send(202, "text/plain", "Scan started");
      return;
    }
    if (n == WIFI_SCAN_RUNNING) {
      request->send(202, "text/plain", "Scanning");
      return;
    }

    DynamicJsonDocument doc(4096);
    JsonArray networks = doc.to<JsonArray>();
    for (int i = 0; i < n; i++) {
      JsonObject net = networks.createNestedObject();
      net["ssid"] = WiFi.SSID(i);
      net["rssi"] = WiFi.RSSI(i);
    }
    WiFi.scanDelete();

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
  });

  server->onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server->begin();
  Serial.printf("[WEB] Serving on port %u\n", config.webserver.port);
}
