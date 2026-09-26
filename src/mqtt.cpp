#include "mqtt.h"
#include "config.h"
#include "fan_control.h"
#include "power.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// Topics: <topicPrefix>/<chipId>/...  e.g. wifi_fan_knob/24C55D/speed/set
// Availability via Last Will: <base>/status = online | offline (retained)
// All client calls happen in mqtt_task; callbacks only use thread-safe setters.

static WiFiClient net;
static PubSubClient client(net);
static volatile bool connected_flag = false;
static volatile bool reconnect_requested = false;
static String base;    // <topicPrefix>/<chipId>
static String dev_id;  // <topicPrefix>_<chipId>, unique per board

static String topic(const char *suffix) {
  return base + "/" + suffix;
}

// ============================================================================
// HOME ASSISTANT DISCOVERY
// ============================================================================

static void publish_config(const char *component, const char *object, JsonDocument &doc) {
  doc["unique_id"] = dev_id + "_" + object;
  doc["availability_topic"] = topic("status");
  JsonObject dev = doc.createNestedObject("device");
  dev.createNestedArray("identifiers").add(dev_id);
  dev["name"] = config.name;
  dev["manufacturer"] = "DIY";
  dev["model"] = "WiFi Fan Knob (ESP32-S3)";
  dev["sw_version"] = config.firmwareVersion;

  String t = String("homeassistant/") + component + "/" + dev_id + "/" + object + "/config";
  String payload;
  serializeJson(doc, payload);
  if (!client.publish(t.c_str(), payload.c_str(), true)) {
    Serial.printf("[MQTT] Discovery publish failed: %s\n", t.c_str());
  }
}

static void publish_discovery() {
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Fan Speed";
    doc["command_topic"] = topic("speed/set");
    doc["state_topic"] = topic("speed");
    doc["min"] = config.fan.minRpm;
    doc["max"] = config.fan.maxRpm;
    doc["step"] = config.fan.rpmStep;
    doc["unit_of_measurement"] = "RPM";
    doc["mode"] = "slider";
    doc["icon"] = "mdi:fan";
    publish_config("number", "speed", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Standby";
    doc["command_topic"] = topic("standby/set");
    doc["state_topic"] = topic("standby");
    doc["icon"] = "mdi:power-sleep";
    publish_config("switch", "standby", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Fan Running";
    doc["state_topic"] = topic("running");
    doc["device_class"] = "running";
    publish_config("binary_sensor", "running", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Fan RPM";
    doc["state_topic"] = topic("rpm");
    doc["unit_of_measurement"] = "RPM";
    doc["state_class"] = "measurement";
    doc["icon"] = "mdi:fan";
    publish_config("sensor", "rpm", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "WiFi Signal";
    doc["state_topic"] = topic("rssi");
    doc["unit_of_measurement"] = "dBm";
    doc["device_class"] = "signal_strength";
    doc["entity_category"] = "diagnostic";
    publish_config("sensor", "rssi", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Uptime";
    doc["state_topic"] = topic("uptime");
    doc["unit_of_measurement"] = "s";
    doc["device_class"] = "duration";
    doc["entity_category"] = "diagnostic";
    publish_config("sensor", "uptime", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "IP Address";
    doc["state_topic"] = topic("ip");
    doc["icon"] = "mdi:ip-network";
    doc["entity_category"] = "diagnostic";
    publish_config("sensor", "ip", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "LCD Brightness";
    doc["command_topic"] = topic("brightness/set");
    doc["state_topic"] = topic("brightness");
    doc["min"] = 10;
    doc["max"] = 100;
    doc["step"] = 1;
    doc["unit_of_measurement"] = "%";
    doc["mode"] = "slider";
    doc["icon"] = "mdi:brightness-6";
    publish_config("number", "brightness", doc);
  }
  {
    StaticJsonDocument<768> doc;
    doc["name"] = "Screensaver";
    doc["command_topic"] = topic("screensaver/set");
    doc["state_topic"] = topic("screensaver");
    doc["icon"] = "mdi:eye";
    publish_config("switch", "screensaver", doc);
  }
  // Screensaver was briefly a binary_sensor: an empty retained config removes that entity
  client.publish((String("homeassistant/binary_sensor/") + dev_id + "/screensaver/config").c_str(), "", true);
  Serial.println("[MQTT] Home Assistant discovery published");
}

// ============================================================================
// STATE (retained; on change, diagnostics every 60 s)
// ============================================================================

static void publish_state(bool force) {
  static int last_rpm = -1;
  static int last_standby = -1;
  static unsigned long last_diag = 0;

  int rpm = fan_get_target();
  if (force || rpm != last_rpm) {
    client.publish(topic("speed").c_str(), String(rpm).c_str(), true);
    client.publish(topic("running").c_str(), rpm > 0 ? "ON" : "OFF", true);
    last_rpm = rpm;
  }
  // Measured RPM: on a change of 30+ RPM, to/from stopped, or at most every 10 s while it moves
  static int last_measured = -1;
  static unsigned long last_measured_ms = 0;
  int measured = fan_get_rpm();
  if (force || abs(measured - last_measured) >= 30 || ((measured == 0) != (last_measured == 0)) ||
      (measured != last_measured && millis() - last_measured_ms >= 10000)) {
    client.publish(topic("rpm").c_str(), String(measured).c_str(), true);
    last_measured = measured;
    last_measured_ms = millis();
  }
  int standby = power_is_standby();
  if (force || standby != last_standby) {
    client.publish(topic("standby").c_str(), standby ? "ON" : "OFF", true);
    last_standby = standby;
  }
  static int last_saver = -1;
  int saver = power_screensaver_on();
  if (force || saver != last_saver) {
    client.publish(topic("screensaver").c_str(), saver ? "ON" : "OFF", true);
    last_saver = saver;
  }
  static int last_brightness = -1;
  int brightness = config.display.brightness;  // Changed by LCD slider, web or HA
  if (force || brightness != last_brightness) {
    client.publish(topic("brightness").c_str(), String(brightness).c_str(), true);
    last_brightness = brightness;
  }
  if (force || millis() - last_diag > 60000) {
    client.publish(topic("rssi").c_str(), String(WiFi.RSSI()).c_str(), true);
    client.publish(topic("uptime").c_str(), String(millis() / 1000).c_str(), true);
    client.publish(topic("ip").c_str(), WiFi.localIP().toString().c_str(), true);
    last_diag = millis();
  }
}

// ============================================================================
// COMMANDS FROM HOME ASSISTANT
// ============================================================================

static void on_message(char *t, byte *payload, unsigned int len) {
  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];
  String tp(t);
  Serial.printf("[MQTT] %s = %s\n", t, msg.c_str());

  if (tp == topic("speed/set")) {
    // HA may send "1200" or "1200.0"; fan_set_target clamps to config range
    fan_set_target(lroundf(msg.toFloat()));
  } else if (tp == topic("standby/set")) {
    if (msg == "ON" || msg == "OFF") power_request_standby(msg == "ON");
  } else if (tp == topic("screensaver/set")) {
    if (msg == "ON" || msg == "OFF") power_request_screensaver(msg == "ON");  // Ignored in standby
  } else if (tp == topic("brightness/set")) {
    // Same as the web slider: apply and save (HA sends once, when the slider is released)
    long b = lroundf(msg.toFloat());
    if (b >= 10 && b <= 100) {
      config.display.brightness = b;
      applyDisplaySettings();
      saveConfig();
    }
  }
}

// ============================================================================
// CONNECTION TASK
// ============================================================================

static void try_connect() {
  client.setServer(config.mqtt.broker, config.mqtt.port);
  const char *user = config.mqtt.username[0] ? config.mqtt.username : nullptr;
  const char *pass = config.mqtt.password[0] ? config.mqtt.password : nullptr;
  String will = topic("status");
  if (!client.connect(dev_id.c_str(), user, pass, will.c_str(), 1, true, "offline")) {
    Serial.printf("[MQTT] Connect to %s:%u failed (state %d)\n", config.mqtt.broker, config.mqtt.port, client.state());
    return;
  }
  Serial.printf("[MQTT] Connected to %s:%u as %s\n", config.mqtt.broker, config.mqtt.port, dev_id.c_str());
  client.publish(will.c_str(), "online", true);
  if (config.mqtt.discoveryEnabled) publish_discovery();
  client.subscribe(topic("speed/set").c_str());
  client.subscribe(topic("standby/set").c_str());
  client.subscribe(topic("brightness/set").c_str());
  client.subscribe(topic("screensaver/set").c_str());
  publish_state(true);
}

static void mqtt_task(void *) {
  unsigned long last_attempt = 0;
  bool retry_now = true;
  for (;;) {
    if (reconnect_requested) {
      reconnect_requested = false;
      if (client.connected()) {
        client.publish(topic("status").c_str(), "offline", true);
        client.disconnect();
      }
      retry_now = true;
    }

    bool wanted = config.mqtt.enabled && config.mqtt.broker[0] != '\0' && WiFi.status() == WL_CONNECTED;
    if (!wanted) {
      if (client.connected()) client.disconnect();
    } else if (!client.connected()) {
      if (retry_now || millis() - last_attempt > 15000) {  // Connect attempts block; space them out
        retry_now = false;
        last_attempt = millis();
        try_connect();
      }
    } else {
      client.loop();
      publish_state(false);
    }
    connected_flag = client.connected();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void mqtt_init() {
  dev_id = String(config.mqtt.topicPrefix) + "_" + config.chipId;
  base = String(config.mqtt.topicPrefix) + "/" + config.chipId;
  client.setBufferSize(1024);  // Discovery payloads exceed the 256-byte default
  client.setSocketTimeout(5);
  client.setCallback(on_message);
  xTaskCreatePinnedToCore(mqtt_task, "mqtt", 6144, nullptr, 1, nullptr, 0);
}

bool mqtt_connected() {
  return connected_flag;
}

void mqtt_reconfigure() {
  reconnect_requested = true;
}
