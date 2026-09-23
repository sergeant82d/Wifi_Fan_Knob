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

// Form field as String, or "" if absent
static String form_value(AsyncWebServerRequest *request, const char *name) {
  return request->hasParam(name, true) ? request->getParam(name, true)->value() : String();
}

// Parse an integer form field within [min, max]; false if absent/invalid
static bool form_int(AsyncWebServerRequest *request, const char *name, long min, long max, long &out) {
  String v = form_value(request, name);
  if (v.length() == 0) return false;
  char *end;
  out = strtol(v.c_str(), &end, 10);
  return *end == '\0' && out >= min && out <= max;
}

// Apply Config-tab form to config; returns error text, or "" on success
static String apply_config_form(AsyncWebServerRequest *request) {
  String tz = form_value(request, "tz");
  if (tz != "EST" && tz != "CST" && tz != "MST" && tz != "PST" && tz != "UTC") return "Invalid time zone";
  String fmt = form_value(request, "time_format");
  if (fmt != "12h" && fmt != "24h") return "Invalid time format";

  long min_pwm, max_pwm, mqtt_port, brightness;
  if (!form_int(request, "min_pwm", 0, 255, min_pwm)) return "Min PWM must be 0-255";
  if (!form_int(request, "max_pwm", 0, 255, max_pwm)) return "Max PWM must be 0-255";
  if (min_pwm > max_pwm) return "Min PWM must not exceed Max PWM";
  if (!form_int(request, "mqtt_port", 1, 65535, mqtt_port)) return "MQTT port must be 1-65535";
  if (!form_int(request, "brightness", 10, 100, brightness)) return "Brightness must be 10-100";

  String broker = form_value(request, "mqtt_broker");
  String user = form_value(request, "mqtt_user");
  String pass = form_value(request, "mqtt_pass");
  if (broker.length() == 0 || broker.length() >= sizeof(config.mqtt.broker)) return "MQTT broker required (max 63 chars)";
  if (user.length() >= sizeof(config.mqtt.username)) return "MQTT username too long (max 31)";
  if (pass.length() >= sizeof(config.mqtt.password)) return "MQTT password too long (max 31)";

  strlcpy(config.display.timezone, tz.c_str(), sizeof(config.display.timezone));
  strlcpy(config.display.timeFormat, fmt.c_str(), sizeof(config.display.timeFormat));
  config.fan.calibration.minPwm = min_pwm;
  config.fan.calibration.maxPwm = max_pwm;
  strlcpy(config.mqtt.broker, broker.c_str(), sizeof(config.mqtt.broker));
  config.mqtt.port = mqtt_port;
  strlcpy(config.mqtt.username, user.c_str(), sizeof(config.mqtt.username));
  if (pass.length() > 0) {  // Blank = keep existing password (it is never sent to the browser)
    strlcpy(config.mqtt.password, pass.c_str(), sizeof(config.mqtt.password));
  }
  config.mqtt.discoveryEnabled = form_value(request, "mqtt_discovery") == "1";
  config.display.brightness = brightness;
  return "";
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

  // Current settings for the Config tab (no passwords)
  server->on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getConfigAsJson());
  });

  // Save Config tab; all fields validated before anything is changed
  server->on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request) {
    String error = apply_config_form(request);
    if (error.length() > 0) {
      request->send(400, "text/plain", error);
      return;
    }
    if (!saveConfig()) {
      request->send(500, "text/plain", "Failed to write config to SPIFFS");
      return;
    }
    request->send(200, "text/plain", "Configuration saved.");
  });

  // Factory reset (includes WiFi credentials), then reboot
  server->on("/api/config/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
    setDefaultConfig();
    saveConfig();
    restart_after_response(request);
    request->send(200, "text/plain", "Reset to defaults. Rebooting into AP mode...");
  });

  server->onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server->begin();
  Serial.printf("[WEB] Serving on port %u\n", config.webserver.port);
}
