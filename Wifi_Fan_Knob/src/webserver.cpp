#include "webserver.h"
#include "config.h"
#include "fan_control.h"
#include "power.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_app_desc.h>

// Port comes from config, so the server is created after config loads
static AsyncWebServer *server = nullptr;

// web/index.html embedded by board_build.embed_txtfiles (NUL-terminated)
extern const uint8_t index_html_start[] asm("_binary_web_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_web_index_html_end");

// Error from the current/last OTA upload ("" = OK), and its HTTP status
static String ota_error;
static int ota_error_code = 500;

// Web login (HTTP Basic, config.webserver). Every endpoint that changes something
// requires it; read-only GETs stay open. No login set = changes refused until one is set.
static bool login_set() {
  return config.webserver.password[0] != '\0';
}

static bool authorized(AsyncWebServerRequest *request) {
  return login_set() && request->authenticate(config.webserver.username, config.webserver.password);
}

// Sends 403/401 and returns false unless the request carries the web login
static bool require_login(AsyncWebServerRequest *request) {
  if (!login_set()) {
    request->send(403, "text/plain", "No login set. Set one on the Config tab first.");
    return false;
  }
  if (!authorized(request)) {
    request->send(401, "text/plain", "Login required (log in at the top of the page)");
    return false;
  }
  return true;
}

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

  // Routes use exact matching: a plain "/api/wifi" would also swallow "/api/wifi/forget" etc.

  // UI page from flash (excluding the appended NUL)
  server->on(AsyncURIMatcher::exact("/"), HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html_start, index_html_end - index_html_start - 1);
  });

  // Save WiFi credentials, then reboot to connect
  server->on(AsyncURIMatcher::exact("/api/wifi"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
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
  server->on(AsyncURIMatcher::exact("/api/wifi/forget"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    setWiFiCredentials("", "");
    restart_after_response(request);
    request->send(200, "text/plain", "Forgotten. Rebooting into AP mode...");
  });

  // Non-blocking scan: 202 while scanning (client polls), 200 + JSON when done
  server->on(AsyncURIMatcher::exact("/api/wifi/scan"), HTTP_GET, [](AsyncWebServerRequest *request) {
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

  // Live network status for the page header and WiFi tab
  server->on(AsyncURIMatcher::exact("/api/status"), HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<1024> doc;
    if (WiFi.status() == WL_CONNECTED) {
      doc["mode"] = "WiFi";
      doc["ip"] = WiFi.localIP().toString();
      doc["ssid"] = WiFi.SSID();
      doc["rssi"] = WiFi.RSSI();
      doc["mac"] = WiFi.macAddress();
    } else {
      doc["mode"] = "AP mode";
      doc["ip"] = WiFi.softAPIP().toString();
      doc["ssid"] = WiFi.softAPSSID();
      doc["rssi"] = nullptr;  // No station link in AP mode
      doc["mac"] = WiFi.softAPmacAddress();
    }
    doc["target_rpm"] = fan_get_target();
    doc["min_rpm"] = config.fan.minRpm;
    doc["max_rpm"] = config.fan.maxRpm;
    doc["rpm_step"] = config.fan.rpmStep;
    doc["fan_controller"] = fan_controller_present();
    doc["power_mode"] = power_is_standby() ? "Standby" : "Active";
    doc["mqtt_connected"] = false;  // MQTT not implemented yet
    doc["fw_version"] = config.firmwareVersion;
    char build_id[9];  // First 8 hex chars of firmware ELF SHA-256: unique per build
    esp_app_get_elf_sha256(build_id, sizeof(build_id));
    doc["fw_build"] = build_id;
    doc["chip_id"] = config.chipId;
    doc["app_slot"] = esp_ota_get_running_partition()->label;
    doc["login_set"] = login_set();
    doc["ap_ssid"] = "WiFi-Fan-Knob-" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
    doc["hotspot_on"] = (WiFi.getMode() & WIFI_AP) != 0;
    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
  });

  // Set target RPM from the Home tab (same target the knob adjusts)
  server->on(AsyncURIMatcher::exact("/api/fan"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    long rpm;
    if (!form_int(request, "rpm", config.fan.minRpm, config.fan.maxRpm, rpm)) {
      request->send(400, "text/plain", "rpm must be " + String(config.fan.minRpm) + "-" + String(config.fan.maxRpm));
      return;
    }
    fan_set_target(rpm);
    if (rpm > 0 && power_is_standby()) power_request_standby(false);  // Fan only runs when awake
    request->send(200, "text/plain", "Target set to " + String(fan_get_target()) + " RPM");
  });

  // OTA firmware upload: stream .bin into the spare app slot, validate, reboot.
  // On any error the running firmware is untouched.
  server->on(AsyncURIMatcher::exact("/api/ota"), HTTP_POST,
    [](AsyncWebServerRequest *request) {
      if (ota_error.length() > 0 || !Update.isFinished()) {
        String msg = ota_error.length() > 0 ? ota_error : String("Upload incomplete");
        Serial.printf("[OTA] Failed: %s\n", msg.c_str());
        request->send(ota_error_code, "text/plain", "Update failed: " + msg);
        return;
      }
      Serial.println("[OTA] Success, rebooting");
      restart_after_response(request);
      request->send(200, "text/plain", "Update OK. Rebooting...");
    },
    [](AsyncWebServerRequest *request, const String &filename, size_t index, uint8_t *data, size_t len, bool final) {
      if (index == 0) {
        Serial.printf("[OTA] Receiving %s\n", filename.c_str());
        ota_error = "";
        ota_error_code = 500;
        if (Update.isRunning()) Update.abort();  // Leftover from an interrupted upload
        if (!login_set()) {
          ota_error = "No login set. Set one on the Config tab first.";
          ota_error_code = 403;
        } else if (!authorized(request)) {
          ota_error = "Login required (log in at the top of the page)";
          ota_error_code = 401;
        } else if (len == 0 || data[0] != 0xE9) {
          // Update lib would report this as "Decryption error"
          ota_error = "Not an ESP32 firmware image (expected firmware.bin)";
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          ota_error = Update.errorString();
        }
      }
      if (ota_error.length() == 0 && Update.write(data, len) != len) {
        ota_error = Update.errorString();
        Update.abort();
      }
      if (final && ota_error.length() == 0) {
        if (Update.end(true)) {  // Validates image before marking it bootable
          Serial.printf("[OTA] Wrote %u bytes\n", index + len);
        } else {
          ota_error = Update.errorString();
        }
      }
    });

  // Standby (on=1) / wake (on=0); applied by loop()
  server->on(AsyncURIMatcher::exact("/api/standby"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    bool on = form_value(request, "on") == "1";
    power_request_standby(on);
    request->send(200, "text/plain", on ? "Standby" : "Awake");
  });

  // Check credentials (page login bar)
  server->on(AsyncURIMatcher::exact("/api/login"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    request->send(200, "text/plain", "OK");
  });

  // Set/change the web login. First login needs no auth; changing it needs the current one.
  server->on(AsyncURIMatcher::exact("/api/login/set"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (login_set() && !authorized(request)) {
      request->send(401, "text/plain", "Log in with the current login first");
      return;
    }
    String user = form_value(request, "new_user");
    String pass = form_value(request, "new_pass");
    if (user.length() == 0 || user.length() >= sizeof(config.webserver.username)) {
      request->send(400, "text/plain", "Username must be 1-31 chars");
      return;
    }
    if (pass.length() < 8 || pass.length() >= sizeof(config.webserver.password)) {
      request->send(400, "text/plain", "Password must be 8-31 chars");
      return;
    }
    strlcpy(config.webserver.username, user.c_str(), sizeof(config.webserver.username));
    strlcpy(config.webserver.password, pass.c_str(), sizeof(config.webserver.password));
    if (!saveConfig()) {
      request->send(500, "text/plain", "Failed to write config to SPIFFS");
      return;
    }
    Serial.println("[WEB] Web login updated");
    request->send(200, "text/plain", "Login saved.");
  });

  // Hotspot (AP) password; takes effect next time the hotspot starts
  server->on(AsyncURIMatcher::exact("/api/wifi/ap"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    String pass = form_value(request, "ap_pass");
    if (pass.length() < 8 || pass.length() >= sizeof(config.wifi.apPassword)) {
      request->send(400, "text/plain", "Hotspot password must be 8-63 chars");
      return;
    }
    strlcpy(config.wifi.apPassword, pass.c_str(), sizeof(config.wifi.apPassword));
    if (!saveConfig()) {
      request->send(500, "text/plain", "Failed to write config to SPIFFS");
      return;
    }
    Serial.println("[WEB] Hotspot password updated");
    request->send(200, "text/plain", "Hotspot password saved. It applies the next time the hotspot starts.");
  });

  // Current settings for the Config tab (no passwords)
  server->on(AsyncURIMatcher::exact("/api/config"), HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getConfigAsJson());
  });

  // Save Config tab; all fields validated before anything is changed
  server->on(AsyncURIMatcher::exact("/api/config"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
    String error = apply_config_form(request);
    if (error.length() > 0) {
      request->send(400, "text/plain", error);
      return;
    }
    if (!saveConfig()) {
      request->send(500, "text/plain", "Failed to write config to SPIFFS");
      return;
    }
    applyDisplaySettings();
    request->send(200, "text/plain", "Configuration saved.");
  });

  // Factory reset (includes WiFi credentials), then reboot
  server->on(AsyncURIMatcher::exact("/api/config/reset"), HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!require_login(request)) return;
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
