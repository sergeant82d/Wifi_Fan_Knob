#include "config.h"
#include "eye_styles.h"
#include <SPIFFS.h>
#include <ArduinoJson.h>

// ============================================================================
// GLOBAL CONFIG INSTANCE
// ============================================================================

Config config;

// ============================================================================
// SPIFFS INITIALIZATION
// ============================================================================

bool initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("[CONFIG] SPIFFS mount failed!");
    return false;
  }
  Serial.println("[CONFIG] SPIFFS mounted successfully");
  return true;
}

// ============================================================================
// CONFIG INITIALIZATION (Called once on boot)
// ============================================================================

void initConfig() {
  Serial.println("[CONFIG] Initializing configuration system...");

  // Initialize SPIFFS
  if (!initSPIFFS()) {
    Serial.println("[CONFIG] WARNING: SPIFFS init failed, using defaults");
    setDefaultConfig();
    return;
  }

  // Try to load config from SPIFFS
  if (!loadConfig()) {
    Serial.println("[CONFIG] Config file not found, creating defaults...");
    setDefaultConfig();
    saveConfig();  // Write defaults to SPIFFS
  }

  // Validate loaded config
  if (!validateConfig()) {
    Serial.println("[CONFIG] WARNING: Loaded config has invalid values, using safe defaults");
    setDefaultConfig();
  }

  Serial.println("[CONFIG] Configuration ready");
  if (config.advanced.debugMode) {
    printConfig();
  }
}

// ============================================================================
// LOAD CONFIG FROM SPIFFS
// ============================================================================

// Configs saved before worldwide zones stored a US code ("CST") and no POSIX rule
static void migrateLegacyTimezone() {
  static const struct { const char *code, *name, *posix; } legacy[] = {
    {"EST", "America/New_York", "EST5EDT,M3.2.0,M11.1.0"},
    {"CST", "America/Chicago", "CST6CDT,M3.2.0,M11.1.0"},
    {"MST", "America/Denver", "MST7MDT,M3.2.0,M11.1.0"},
    {"PST", "America/Los_Angeles", "PST8PDT,M3.2.0,M11.1.0"},
    {"UTC", "Etc/UTC", "UTC0"},
  };
  for (auto &z : legacy) {
    if (strcmp(config.display.timezone, z.code) == 0 || strcmp(config.display.timezone, z.name) == 0) {
      strlcpy(config.display.timezone, z.name, sizeof(config.display.timezone));
      strlcpy(config.display.posixTz, z.posix, sizeof(config.display.posixTz));
      Serial.printf("[CONFIG] Time zone migrated to %s\n", z.name);
      return;
    }
  }
  strlcpy(config.display.timezone, "Etc/UTC", sizeof(config.display.timezone));
  strlcpy(config.display.posixTz, "UTC0", sizeof(config.display.posixTz));
}

bool loadConfig() {
  const char* CONFIG_PATH = "/config.json";

  if (!SPIFFS.exists(CONFIG_PATH)) {
    Serial.println("[CONFIG] File not found: " + String(CONFIG_PATH));
    return false;
  }

  File file = SPIFFS.open(CONFIG_PATH, "r");
  if (!file) {
    Serial.println("[CONFIG] Failed to open config file");
    return false;
  }

  // Deserialize JSON (heap; full config is ~1.5 KB of JSON with copied strings)
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);

  file.close();

  if (error) {
    Serial.print("[CONFIG] JSON parse error: ");
    Serial.println(error.c_str());
    return false;
  }

  // Device Info (read-only, but load for reference)
  strlcpy(config.name, doc["device"]["name"] | "WiFi Fan Knob", sizeof(config.name));
  strlcpy(config.chipId, doc["device"]["chipId"] | "000000", sizeof(config.chipId));
  strlcpy(config.firmwareVersion, doc["device"]["firmwareVersion"] | "v1.0.0", sizeof(config.firmwareVersion));
  strlcpy(config.buildDate, doc["device"]["buildDate"] | "2026-09-22", sizeof(config.buildDate));

  // WiFi
  strlcpy(config.wifi.ssid, doc["network"]["wifi"]["ssid"] | "", sizeof(config.wifi.ssid));
  strlcpy(config.wifi.password, doc["network"]["wifi"]["password"] | "", sizeof(config.wifi.password));
  config.wifi.saveCredentials = doc["network"]["wifi"]["saveCredentials"] | true;
  strlcpy(config.wifi.apPassword, doc["network"]["wifi"]["apPassword"] | "12345678", sizeof(config.wifi.apPassword));
  if (strlen(config.wifi.apPassword) < 8) {  // WPA2 minimum; softAP rejects shorter
    strlcpy(config.wifi.apPassword, "12345678", sizeof(config.wifi.apPassword));
  }
  // Static IP configuration
  config.wifi.useStaticIp = doc["network"]["wifi"]["useStaticIp"] | false;
  strlcpy(config.wifi.staticIp, doc["network"]["wifi"]["staticIp"] | "", sizeof(config.wifi.staticIp));
  strlcpy(config.wifi.staticGateway, doc["network"]["wifi"]["staticGateway"] | "", sizeof(config.wifi.staticGateway));
  strlcpy(config.wifi.staticSubnet, doc["network"]["wifi"]["staticSubnet"] | "", sizeof(config.wifi.staticSubnet));
  strlcpy(config.wifi.staticDns, doc["network"]["wifi"]["staticDns"] | "", sizeof(config.wifi.staticDns));

  // Webserver
  config.webserver.port = doc["network"]["webserver"]["port"] | 8080;
  strlcpy(config.webserver.username, doc["network"]["webserver"]["username"] | "", sizeof(config.webserver.username));
  strlcpy(config.webserver.password, doc["network"]["webserver"]["password"] | "", sizeof(config.webserver.password));

  // MQTT
  config.mqtt.enabled = doc["network"]["mqtt"]["enabled"] | true;
  strlcpy(config.mqtt.broker, doc["network"]["mqtt"]["broker"] | "192.168.10.50", sizeof(config.mqtt.broker));
  config.mqtt.port = doc["network"]["mqtt"]["port"] | 1883;
  strlcpy(config.mqtt.username, doc["network"]["mqtt"]["username"] | "", sizeof(config.mqtt.username));
  strlcpy(config.mqtt.password, doc["network"]["mqtt"]["password"] | "", sizeof(config.mqtt.password));
  config.mqtt.discoveryEnabled = doc["network"]["mqtt"]["discoveryEnabled"] | true;
  strlcpy(config.mqtt.topicPrefix, doc["network"]["mqtt"]["topicPrefix"] | "wifi_fan_knob", sizeof(config.mqtt.topicPrefix));

  // NTP
  strlcpy(config.ntp.server, doc["network"]["ntp"]["server"] | "pool.ntp.org", sizeof(config.ntp.server));
  config.ntp.syncInterval = doc["network"]["ntp"]["syncInterval"] | 3600000;  // 60 minutes

  // Display
  strlcpy(config.display.timezone, doc["display"]["timezone"] | "America/Chicago", sizeof(config.display.timezone));
  strlcpy(config.display.posixTz, doc["display"]["posixTz"] | "", sizeof(config.display.posixTz));
  if (config.display.posixTz[0] == '\0') migrateLegacyTimezone();
  strlcpy(config.display.timeFormat, doc["display"]["timeFormat"] | "12h", sizeof(config.display.timeFormat));
  strlcpy(config.display.eyeStyle, doc["display"]["eyeStyle"] | "dragon", sizeof(config.display.eyeStyle));
  config.display.brightness = doc["display"]["brightness"] | 80;
  config.display.screenTimeout = doc["display"]["screenTimeout"] | 0;
  config.display.screensaverSec = doc["display"]["screensaverSec"] | 30;

  // Fan
  config.fan.minRpm = doc["fan"]["minRpm"] | 0;
  config.fan.maxRpm = doc["fan"]["maxRpm"] | 2500;
  config.fan.rpmStep = doc["fan"]["rpmStep"] | 100;
  config.fan.calibration.minPwm = doc["fan"]["calibration"]["minPwm"] | 50;
  config.fan.calibration.maxPwm = doc["fan"]["calibration"]["maxPwm"] | 200;
  config.fan.calibration.enableFeedback = doc["fan"]["calibration"]["enableFeedback"] | true;
  config.fan.presets.low = doc["fan"]["presets"]["low"] | 800;
  config.fan.presets.medium = doc["fan"]["presets"]["medium"] | 1200;
  config.fan.presets.high = doc["fan"]["presets"]["high"] | 1600;
  config.fan.presets.max = doc["fan"]["presets"]["max"] | 2000;

  // System
  config.system.deepSleepEnabled = doc["system"]["deepSleepEnabled"] | true;
  config.system.standbyTimeout = doc["system"]["standbyTimeout"] | 0;
  config.system.audioEnabled = doc["system"]["audioEnabled"] | true;
  config.system.autoUpdate = doc["system"]["autoUpdate"] | false;

  // Peripheral power switch
  config.power.activeHigh = doc["power"]["activeHigh"] | true;

  // Advanced
  config.advanced.debugMode = doc["advanced"]["debugMode"] | false;
  strlcpy(config.advanced.logLevel, doc["advanced"]["logLevel"] | "info", sizeof(config.advanced.logLevel));

  Serial.println("[CONFIG] Loaded from SPIFFS");
  return true;
}

// ============================================================================
// SAVE CONFIG TO SPIFFS
// ============================================================================

bool saveConfig() {
  const char* CONFIG_PATH = "/config.json";

  // Create JSON document (heap; see loadConfig)
  DynamicJsonDocument doc(4096);

  // Device Info
  doc["device"]["name"] = config.name;
  doc["device"]["chipId"] = config.chipId;
  doc["device"]["firmwareVersion"] = config.firmwareVersion;
  doc["device"]["buildDate"] = config.buildDate;

  // WiFi
  doc["network"]["wifi"]["ssid"] = config.wifi.ssid;
  doc["network"]["wifi"]["password"] = config.wifi.password;
  doc["network"]["wifi"]["saveCredentials"] = config.wifi.saveCredentials;
  doc["network"]["wifi"]["apPassword"] = config.wifi.apPassword;
  doc["network"]["wifi"]["useStaticIp"] = config.wifi.useStaticIp;
  doc["network"]["wifi"]["staticIp"] = config.wifi.staticIp;
  doc["network"]["wifi"]["staticGateway"] = config.wifi.staticGateway;
  doc["network"]["wifi"]["staticSubnet"] = config.wifi.staticSubnet;
  doc["network"]["wifi"]["staticDns"] = config.wifi.staticDns;

  // Webserver
  doc["network"]["webserver"]["port"] = config.webserver.port;
  doc["network"]["webserver"]["username"] = config.webserver.username;
  doc["network"]["webserver"]["password"] = config.webserver.password;

  // MQTT
  doc["network"]["mqtt"]["enabled"] = config.mqtt.enabled;
  doc["network"]["mqtt"]["broker"] = config.mqtt.broker;
  doc["network"]["mqtt"]["port"] = config.mqtt.port;
  doc["network"]["mqtt"]["username"] = config.mqtt.username;
  doc["network"]["mqtt"]["password"] = config.mqtt.password;
  doc["network"]["mqtt"]["discoveryEnabled"] = config.mqtt.discoveryEnabled;
  doc["network"]["mqtt"]["topicPrefix"] = config.mqtt.topicPrefix;

  // NTP
  doc["network"]["ntp"]["server"] = config.ntp.server;
  doc["network"]["ntp"]["syncInterval"] = config.ntp.syncInterval;

  // Display
  doc["display"]["timezone"] = config.display.timezone;
  doc["display"]["posixTz"] = config.display.posixTz;
  doc["display"]["timeFormat"] = config.display.timeFormat;
  doc["display"]["eyeStyle"] = config.display.eyeStyle;
  doc["display"]["brightness"] = config.display.brightness;
  doc["display"]["screenTimeout"] = config.display.screenTimeout;
  doc["display"]["screensaverSec"] = config.display.screensaverSec;

  // Fan
  doc["fan"]["minRpm"] = config.fan.minRpm;
  doc["fan"]["maxRpm"] = config.fan.maxRpm;
  doc["fan"]["rpmStep"] = config.fan.rpmStep;
  doc["fan"]["calibration"]["minPwm"] = config.fan.calibration.minPwm;
  doc["fan"]["calibration"]["maxPwm"] = config.fan.calibration.maxPwm;
  doc["fan"]["calibration"]["enableFeedback"] = config.fan.calibration.enableFeedback;
  doc["fan"]["presets"]["low"] = config.fan.presets.low;
  doc["fan"]["presets"]["medium"] = config.fan.presets.medium;
  doc["fan"]["presets"]["high"] = config.fan.presets.high;
  doc["fan"]["presets"]["max"] = config.fan.presets.max;

  // System
  doc["system"]["deepSleepEnabled"] = config.system.deepSleepEnabled;
  doc["system"]["standbyTimeout"] = config.system.standbyTimeout;
  doc["system"]["audioEnabled"] = config.system.audioEnabled;
  doc["system"]["autoUpdate"] = config.system.autoUpdate;

  // Peripheral power switch
  doc["power"]["activeHigh"] = config.power.activeHigh;

  // Advanced
  doc["advanced"]["debugMode"] = config.advanced.debugMode;
  doc["advanced"]["logLevel"] = config.advanced.logLevel;

  // Refuse to write a truncated config
  if (doc.overflowed()) {
    Serial.println("[CONFIG] JSON document overflowed; not saving");
    return false;
  }

  // Write to SPIFFS
  File file = SPIFFS.open(CONFIG_PATH, "w");
  if (!file) {
    Serial.println("[CONFIG] Failed to open config for writing");
    return false;
  }

  if (serializeJson(doc, file) == 0) {
    Serial.println("[CONFIG] Failed to serialize JSON");
    file.close();
    return false;
  }

  file.close();
  Serial.println("[CONFIG] Saved to SPIFFS");
  return true;
}

// ============================================================================
// RESET TO FACTORY DEFAULTS
// ============================================================================

void setDefaultConfig() {
  Serial.println("[CONFIG] Setting factory defaults...");

  // Device Info
  strlcpy(config.name, "WiFi Fan Knob", sizeof(config.name));
  strlcpy(config.chipId, "000000", sizeof(config.chipId));
  strlcpy(config.firmwareVersion, "v1.0.0", sizeof(config.firmwareVersion));
  strlcpy(config.buildDate, "2026-09-22", sizeof(config.buildDate));

  // WiFi (empty by default = AP mode on boot)
  config.wifi.ssid[0] = '\0';
  config.wifi.password[0] = '\0';
  config.wifi.saveCredentials = true;
  strlcpy(config.wifi.apPassword, "12345678", sizeof(config.wifi.apPassword));
  config.wifi.useStaticIp = false;
  config.wifi.staticIp[0] = '\0';
  config.wifi.staticGateway[0] = '\0';
  config.wifi.staticSubnet[0] = '\0';
  config.wifi.staticDns[0] = '\0';
  config.power.activeHigh = true;

  // Webserver
  config.webserver.port = 8080;
  config.webserver.username[0] = '\0';
  config.webserver.password[0] = '\0';

  // MQTT
  config.mqtt.enabled = true;
  strlcpy(config.mqtt.broker, "192.168.10.50", sizeof(config.mqtt.broker));
  config.mqtt.port = 1883;
  config.mqtt.username[0] = '\0';
  config.mqtt.password[0] = '\0';
  config.mqtt.discoveryEnabled = true;
  strlcpy(config.mqtt.topicPrefix, "wifi_fan_knob", sizeof(config.mqtt.topicPrefix));

  // NTP
  strlcpy(config.ntp.server, "pool.ntp.org", sizeof(config.ntp.server));
  config.ntp.syncInterval = 3600000;  // 60 minutes

  // Display
  strlcpy(config.display.timezone, "America/Chicago", sizeof(config.display.timezone));
  strlcpy(config.display.posixTz, "CST6CDT,M3.2.0,M11.1.0", sizeof(config.display.posixTz));
  strlcpy(config.display.timeFormat, "12h", sizeof(config.display.timeFormat));
  strlcpy(config.display.eyeStyle, "dragon", sizeof(config.display.eyeStyle));
  config.display.brightness = 80;
  config.display.screenTimeout = 0;
  config.display.screensaverSec = 30;

  // Fan
  config.fan.minRpm = 0;
  config.fan.maxRpm = 2500;
  config.fan.rpmStep = 100;
  config.fan.calibration.minPwm = 50;
  config.fan.calibration.maxPwm = 200;
  config.fan.calibration.enableFeedback = true;
  config.fan.presets.low = 800;
  config.fan.presets.medium = 1200;
  config.fan.presets.high = 1600;
  config.fan.presets.max = 2000;

  // System
  config.system.deepSleepEnabled = true;
  config.system.standbyTimeout = 0;
  config.system.audioEnabled = true;
  config.system.autoUpdate = false;

  // Advanced
  config.advanced.debugMode = false;
  strlcpy(config.advanced.logLevel, "info", sizeof(config.advanced.logLevel));
}

// ============================================================================
// VALIDATION
// ============================================================================

bool validateConfig() {
  // Basic range checks
  if (config.webserver.port < 1 || config.webserver.port > 65535) {
    Serial.println("[CONFIG] Invalid webserver port");
    return false;
  }

  if (config.mqtt.port < 1 || config.mqtt.port > 65535) {
    Serial.println("[CONFIG] Invalid MQTT port");
    return false;
  }

  if (config.fan.minRpm > config.fan.maxRpm) {
    Serial.println("[CONFIG] Fan minRpm > maxRpm");
    return false;
  }

  if (config.fan.calibration.minPwm > config.fan.calibration.maxPwm) {
    Serial.println("[CONFIG] Calibration minPwm > maxPwm");
    return false;
  }

  if (config.display.brightness > 100) {
    Serial.println("[CONFIG] Brightness > 100%");
    return false;
  }

  return true;
}

// ============================================================================
// DEBUG & UTILITIES
// ============================================================================

void printConfig() {
  Serial.println("\n========== CONFIG DUMP ==========");
  Serial.print("Device: "); Serial.println(config.name);
  Serial.print("Firmware: "); Serial.println(config.firmwareVersion);
  Serial.print("WiFi SSID: "); Serial.println(config.wifi.ssid[0] ? config.wifi.ssid : "(empty - AP mode)");
  Serial.print("Webserver Port: "); Serial.println(config.webserver.port);
  Serial.print("MQTT Broker: "); Serial.print(config.mqtt.broker); Serial.print(":"); Serial.println(config.mqtt.port);
  Serial.print("MQTT Discovery: "); Serial.println(config.mqtt.discoveryEnabled ? "YES" : "NO");
  Serial.print("Timezone: "); Serial.println(config.display.timezone);
  Serial.print("Time Format: "); Serial.println(config.display.timeFormat);
  Serial.print("Fan Cal Min/Max PWM: "); Serial.print(config.fan.calibration.minPwm); Serial.print("/"); Serial.println(config.fan.calibration.maxPwm);
  Serial.print("Fan Presets: "); Serial.print(config.fan.presets.low); Serial.print(", ");
  Serial.print(config.fan.presets.medium); Serial.print(", ");
  Serial.print(config.fan.presets.high); Serial.print(", ");
  Serial.println(config.fan.presets.max);
  Serial.println("================================\n");
}

String getConfigAsJson() {
  DynamicJsonDocument doc(3072);

  doc["device"]["name"] = config.name;
  doc["device"]["chipId"] = config.chipId;
  doc["device"]["firmwareVersion"] = config.firmwareVersion;
  doc["device"]["buildDate"] = config.buildDate;

  doc["network"]["wifi"]["ssid"] = config.wifi.ssid;
  doc["network"]["wifi"]["useStaticIp"] = config.wifi.useStaticIp;
  doc["network"]["wifi"]["staticIp"] = config.wifi.staticIp;
  doc["network"]["wifi"]["staticGateway"] = config.wifi.staticGateway;
  doc["network"]["wifi"]["staticSubnet"] = config.wifi.staticSubnet;
  doc["network"]["wifi"]["staticDns"] = config.wifi.staticDns;
  doc["network"]["webserver"]["port"] = config.webserver.port;
  doc["network"]["mqtt"]["broker"] = config.mqtt.broker;
  doc["network"]["mqtt"]["port"] = config.mqtt.port;
  doc["network"]["mqtt"]["username"] = config.mqtt.username;  // password deliberately omitted
  doc["network"]["mqtt"]["discoveryEnabled"] = config.mqtt.discoveryEnabled;

  doc["display"]["timezone"] = config.display.timezone;
  doc["display"]["timeFormat"] = config.display.timeFormat;
  doc["display"]["brightness"] = config.display.brightness;
  doc["display"]["screensaverSec"] = config.display.screensaverSec;
  doc["display"]["eyeStyle"] = config.display.eyeStyle;
  JsonArray styles = doc.createNestedArray("eyeStyles");
  for (int i = 0; i < EYE_STYLE_COUNT; i++) {
    JsonObject o = styles.createNestedObject();
    o["id"] = EYE_STYLES[i]->id;
    o["name"] = EYE_STYLES[i]->name;
  }

  doc["fan"]["calibration"]["minPwm"] = config.fan.calibration.minPwm;
  doc["fan"]["calibration"]["maxPwm"] = config.fan.calibration.maxPwm;
  doc["fan"]["presets"]["low"] = config.fan.presets.low;
  doc["fan"]["presets"]["medium"] = config.fan.presets.medium;
  doc["fan"]["presets"]["high"] = config.fan.presets.high;
  doc["fan"]["presets"]["max"] = config.fan.presets.max;
  doc["fan"]["maxRpm"] = config.fan.maxRpm;

  doc["power"]["activeHigh"] = config.power.activeHigh;

  String jsonString;
  serializeJson(doc, jsonString);
  return jsonString;
}

// ============================================================================
// CONVENIENCE SETTERS
// ============================================================================

void setWiFiCredentials(const char* ssid, const char* password) {
  strlcpy(config.wifi.ssid, ssid, sizeof(config.wifi.ssid));
  strlcpy(config.wifi.password, password, sizeof(config.wifi.password));
  saveConfig();
  Serial.print("[CONFIG] WiFi updated: ");
  Serial.println(config.wifi.ssid);
}

void setMqttBroker(const char* broker, uint16_t port) {
  strlcpy(config.mqtt.broker, broker, sizeof(config.mqtt.broker));
  config.mqtt.port = port;
  saveConfig();
  Serial.print("[CONFIG] MQTT broker updated: ");
  Serial.print(config.mqtt.broker);
  Serial.print(":");
  Serial.println(config.mqtt.port);
}

void setFanCalibration(uint8_t minPwm, uint8_t maxPwm) {
  if (minPwm >= maxPwm) {
    Serial.println("[CONFIG] ERROR: minPwm must be < maxPwm");
    return;
  }
  config.fan.calibration.minPwm = minPwm;
  config.fan.calibration.maxPwm = maxPwm;
  saveConfig();
  Serial.print("[CONFIG] Fan calibration: ");
  Serial.print(minPwm);
  Serial.print("-");
  Serial.println(maxPwm);
}

void setTimezone(const char* name, const char* posix) {
  strlcpy(config.display.timezone, name, sizeof(config.display.timezone));
  strlcpy(config.display.posixTz, posix, sizeof(config.display.posixTz));
  saveConfig();
  Serial.print("[CONFIG] Timezone: ");
  Serial.println(config.display.timezone);
}
