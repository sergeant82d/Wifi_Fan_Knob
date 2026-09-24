> **Superseded (2026-09-23):** this was the pre-hardware design. The implemented schema is in
> `src/mqtt.cpp` and summarised in `docs/CLAUDE.md` (MQTT / Home Assistant). Main differences:
> chip ID in unique IDs and topics, availability via Last Will instead of an "MQTT Connected"
> sensor, a Standby switch instead of the Power Mode sensor, and no RPM sensor until the EMC2101
> is fitted.

# MQTT Topic Structure & Home Assistant Auto-Discovery

## Device Identification

```
Device Name: wifi_fan_knob
Hostname: wifi-fan-knob
MQTT Topic Prefix: homeassistant/
Device ID: wifi_fan_knob_<CHIPID>
```

---

## MQTT Topics - Home Assistant Auto-Discovery

### 1. FAN RPM Sensor (Read-Only)

**Discovery Topic** (publish once on boot):
```
homeassistant/sensor/wifi_fan_knob_rpm/config
```

**Payload**:
```json
{
  "name": "Fan RPM",
  "unique_id": "wifi_fan_knob_rpm",
  "unit_of_measurement": "RPM",
  "state_topic": "wifi_fan_knob/fan/rpm",
  "value_template": "{{ value }}",
  "icon": "mdi:fan",
  "device": {
    "identifiers": ["wifi_fan_knob"],
    "name": "WiFi Fan Knob",
    "manufacturer": "DIY",
    "model": "PWM Fan Controller",
    "sw_version": "v1.0.0"
  }
}
```

**State Topic** (publish current RPM):
```
wifi_fan_knob/fan/rpm
Payload: 1500    (numeric RPM value)
Frequency: Every 5 seconds (or on change)
```

---

### 2. Fan Speed Command (Write/Control)

**Discovery Topic**:
```
homeassistant/number/wifi_fan_knob_speed/config
```

**Payload**:
```json
{
  "name": "Fan Speed",
  "unique_id": "wifi_fan_knob_speed",
  "unit_of_measurement": "RPM",
  "min": 0,
  "max": 2500,
  "step": 100,
  "command_topic": "wifi_fan_knob/fan/speed/set",
  "state_topic": "wifi_fan_knob/fan/speed",
  "value_template": "{{ value }}",
  "icon": "mdi:speedometer",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic** (publish current speed setting):
```
wifi_fan_knob/fan/speed
Payload: 1500    (current target RPM)
Frequency: Every 5 seconds or on change
```

**Command Topic** (subscribe to receive commands from HA):
```
wifi_fan_knob/fan/speed/set
Payload: 1200    (desired RPM, 0-2500, steps of 100)
Action: Update EMC2101 PWM to match requested RPM
```

---

### 3. Fan Status

**Discovery Topic**:
```
homeassistant/binary_sensor/wifi_fan_knob_status/config
```

**Payload**:
```json
{
  "name": "Fan Running",
  "unique_id": "wifi_fan_knob_status",
  "state_topic": "wifi_fan_knob/fan/running",
  "value_template": "{{ value }}",
  "payload_on": "true",
  "payload_off": "false",
  "icon": "mdi:fan-check",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic**:
```
wifi_fan_knob/fan/running
Payload: "true" or "false"
Frequency: Every 5 seconds or on change
```

---

### 4. Power Mode

**Discovery Topic**:
```
homeassistant/sensor/wifi_fan_knob_power_mode/config
```

**Payload**:
```json
{
  "name": "Power Mode",
  "unique_id": "wifi_fan_knob_power_mode",
  "state_topic": "wifi_fan_knob/system/power_mode",
  "value_template": "{{ value }}",
  "icon": "mdi:power-settings",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic**:
```
wifi_fan_knob/system/power_mode
Payload: "active" | "standby" | "shutdown"
Frequency: On change
```

---

### 5. WiFi Signal Strength

**Discovery Topic**:
```
homeassistant/sensor/wifi_fan_knob_signal/config
```

**Payload**:
```json
{
  "name": "WiFi Signal",
  "unique_id": "wifi_fan_knob_signal",
  "unit_of_measurement": "dBm",
  "state_topic": "wifi_fan_knob/wifi/signal",
  "value_template": "{{ value }}",
  "icon": "mdi:wifi",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic**:
```
wifi_fan_knob/wifi/signal
Payload: -55    (negative dBm value)
Frequency: Every 30 seconds
```

---

### 6. MQTT Connection Status

**Discovery Topic**:
```
homeassistant/binary_sensor/wifi_fan_knob_mqtt/config
```

**Payload**:
```json
{
  "name": "MQTT Connected",
  "unique_id": "wifi_fan_knob_mqtt",
  "state_topic": "wifi_fan_knob/system/mqtt_connected",
  "value_template": "{{ value }}",
  "payload_on": "true",
  "payload_off": "false",
  "icon": "mdi:connection",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic**:
```
wifi_fan_knob/system/mqtt_connected
Payload: "true" | "false"
Frequency: On change (or keep-alive every 60 seconds)
```

---

### 7. Last Reboot Time

**Discovery Topic**:
```
homeassistant/sensor/wifi_fan_knob_uptime/config
```

**Payload**:
```json
{
  "name": "Uptime",
  "unique_id": "wifi_fan_knob_uptime",
  "state_topic": "wifi_fan_knob/system/uptime",
  "value_template": "{{ value }}",
  "icon": "mdi:clock-outline",
  "device": {
    "identifiers": ["wifi_fan_knob"]
  }
}
```

**State Topic**:
```
wifi_fan_knob/system/uptime
Payload: "2h 34m"    (human-readable uptime)
Frequency: Every 60 seconds
```

---

## MQTT Publish Cycle (Device → HA)

On boot, publish ALL discovery configs immediately. Then:

| Topic | Payload Type | Frequency | Priority |
|-------|--------------|-----------|----------|
| `.../fan/rpm` | Numeric (0-2500) | Every 5s | High |
| `.../fan/speed` | Numeric (0-2500) | Every 5s | High |
| `.../fan/running` | Boolean ("true"/"false") | Every 5s | High |
| `.../system/power_mode` | String ("active"/"standby"/"shutdown") | On change | Medium |
| `.../wifi/signal` | Numeric dBm | Every 30s | Low |
| `.../system/mqtt_connected` | Boolean | Every 60s (keep-alive) | Low |
| `.../system/uptime` | String ("Xh Ym") | Every 60s | Low |

---

## MQTT Subscribe Cycle (HA → Device)

Device subscribes to:
```
wifi_fan_knob/fan/speed/set
```

When message arrives:
1. Parse RPM value (0-2500, must be multiple of 100)
2. Validate range
3. Convert RPM to EMC2101 PWM value using calibration
4. Update EMC2101 register
5. Publish new state to `wifi_fan_knob/fan/speed`

---

## Home Assistant Automations (Examples)

### Example 1: Auto-discover device on startup
```yaml
# Automatic - HA will see discovery messages and create entities
# No YAML needed; device appears in Home Assistant automatically
```

### Example 2: Turn on fan when air quality drops
```yaml
automation:
  - alias: "Fan Auto-On (High Fumes)"
    trigger:
      platform: numeric_state
      entity_id: sensor.air_quality
      below: 300
    action:
      service: number.set_value
      target:
        entity_id: number.fan_speed
      data:
        value: 1800
```

### Example 3: Turn off at night
```yaml
automation:
  - alias: "Fan Off at Night"
    trigger:
      platform: time
      at: "22:00:00"
    action:
      service: number.set_value
      target:
        entity_id: number.fan_speed
      data:
        value: 0
```

---

## Notes

1. **Device ID**: Use ESP32 MAC address or chip ID for unique_id
2. **Keep-Alive**: Publish `mqtt_connected: true` every 60 seconds so HA knows device is online
3. **QoS**: Use QoS 1 (at-least-once delivery) for all messages
4. **Retained Messages**: Mark discovery configs as retained (broker keeps them)
5. **Offline Detection**: HA will mark device offline if no keep-alive after ~5 minutes
6. **RPM Rounding**: Device always publishes in 100-RPM steps (as specified in requirements)

---

## C++ Code Structure (Pseudocode)

```cpp
// On boot, publish all discovery payloads
void publishDiscoveryConfigs() {
  mqtt.publish("homeassistant/sensor/wifi_fan_knob_rpm/config", rpmConfig, true);
  mqtt.publish("homeassistant/number/wifi_fan_knob_speed/config", speedConfig, true);
  mqtt.publish("homeassistant/binary_sensor/wifi_fan_knob_status/config", statusConfig, true);
  // ... etc
}

// Main loop: publish state topics
void mqttPublishStates() {
  static unsigned long lastRpmPublish = 0;
  static unsigned long lastSignalPublish = 0;
  
  // High-frequency: every 5 seconds
  if (millis() - lastRpmPublish > 5000) {
    mqtt.publish("wifi_fan_knob/fan/rpm", currentRpm);
    mqtt.publish("wifi_fan_knob/fan/speed", targetSpeed);
    mqtt.publish("wifi_fan_knob/fan/running", fanRunning ? "true" : "false");
    lastRpmPublish = millis();
  }
  
  // Low-frequency: every 30-60 seconds
  if (millis() - lastSignalPublish > 30000) {
    mqtt.publish("wifi_fan_knob/wifi/signal", WiFi.RSSI());
    mqtt.publish("wifi_fan_knob/system/mqtt_connected", "true");
    lastSignalPublish = millis();
  }
}

// Callback when command received
void onMqttMessage(topic, payload) {
  if (topic == "wifi_fan_knob/fan/speed/set") {
    int requestedRpm = atoi(payload);
    setFanSpeed(requestedRpm);
  }
}
```
