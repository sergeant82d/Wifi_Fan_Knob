#include "fan_profiles.h"
#include "config.h"
#include "fan_control.h"
#include <ArduinoJson.h>
#include <SPIFFS.h>

static const char *PROFILES_PATH = "/fans.json";

static FanProfile profiles[FAN_PROFILE_MAX];
static volatile int active = -1;

static bool save_profiles() {
  DynamicJsonDocument doc(6144);
  doc["active"] = active;
  JsonArray list = doc.createNestedArray("profiles");
  for (int i = 0; i < FAN_PROFILE_MAX; i++) {
    const FanProfile &p = profiles[i];
    if (!p.used) {
      list.add(nullptr);
      continue;
    }
    JsonObject o = list.createNestedObject();
    o["name"] = p.name;
    o["maxRpm"] = p.maxRpm;
    o["stall"] = p.stall;
    JsonArray presets = o.createNestedArray("presets");
    for (uint16_t v : p.presets) presets.add(v);
    JsonArray rpm = o.createNestedArray("rpm");
    for (uint16_t v : p.rpm) rpm.add(v);
  }
  File file = SPIFFS.open(PROFILES_PATH, "w");
  if (!file) {
    Serial.println("[FANS] Failed to open profiles for writing");
    return false;
  }
  serializeJson(doc, file);
  file.close();
  return true;
}

void fan_profiles_init() {
  memset(profiles, 0, sizeof(profiles));
  active = -1;
  File file = SPIFFS.open(PROFILES_PATH, "r");
  if (!file) return;  // No profiles yet
  DynamicJsonDocument doc(6144);
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.printf("[FANS] Profiles unreadable (%s); starting empty\n", err.c_str());
    return;
  }
  JsonArray list = doc["profiles"];
  for (int i = 0; i < FAN_PROFILE_MAX && i < (int)list.size(); i++) {
    JsonObject o = list[i];
    if (o.isNull()) continue;
    FanProfile &p = profiles[i];
    p.used = true;
    strlcpy(p.name, o["name"] | "Fan", sizeof(p.name));
    p.maxRpm = o["maxRpm"] | 0;
    p.stall = o["stall"] | 0;
    for (int k = 0; k < 4; k++) p.presets[k] = o["presets"][k] | 0;
    for (int s = 0; s < FAN_TABLE_SIZE; s++) p.rpm[s] = o["rpm"][s] | 0;
  }
  int a = doc["active"] | -1;
  active = (a >= 0 && a < FAN_PROFILE_MAX && profiles[a].used) ? a : -1;
  Serial.printf("[FANS] Active profile: %s\n", active >= 0 ? profiles[active].name : "none");
}

int fan_profiles_active() {
  return active;
}

const FanProfile *fan_profile(int slot) {
  return (slot >= 0 && slot < FAN_PROFILE_MAX && profiles[slot].used) ? &profiles[slot] : nullptr;
}

// Profile values become the live limit and presets
static void apply_to_config(const FanProfile &p) {
  config.fan.maxRpm = p.maxRpm;
  config.fan.presets.low = p.presets[0];
  config.fan.presets.medium = p.presets[1];
  config.fan.presets.high = p.presets[2];
  config.fan.presets.max = p.presets[3];
  saveConfig();
  fan_set_target(fan_get_target());  // Re-clamp a running speed to the new limit
}

bool fan_profiles_activate(int slot) {
  if (slot != -1 && !fan_profile(slot)) return false;
  active = slot;
  if (slot >= 0) apply_to_config(profiles[slot]);
  return save_profiles();
}

bool fan_profiles_rename(int slot, const char *name) {
  if (!fan_profile(slot)) return false;
  strlcpy(profiles[slot].name, name, sizeof(profiles[slot].name));
  return save_profiles();
}

bool fan_profiles_delete(int slot) {
  if (!fan_profile(slot)) return false;
  if (active == slot) active = -1;  // Before clearing, so the fan never maps through an empty table
  profiles[slot].used = false;
  return save_profiles();
}

// Nearest multiple of step
static uint16_t round_to(uint32_t v, uint16_t step) {
  return step ? (v + step / 2) / step * step : v;
}

int fan_profiles_store(int slot, const char *name, const uint16_t rpm[FAN_TABLE_SIZE], uint8_t stall) {
  if (slot < 0) {
    for (int i = 0; i < FAN_PROFILE_MAX && slot < 0; i++) {
      if (!profiles[i].used) slot = i;
    }
    if (slot < 0) return -1;
  }
  if (slot >= FAN_PROFILE_MAX) return -1;
  if (active == slot) active = -1;  // Don't map through the table while it is rewritten

  FanProfile &p = profiles[slot];
  p.used = true;
  strlcpy(p.name, name, sizeof(p.name));
  memcpy(p.rpm, rpm, sizeof(p.rpm));
  p.stall = stall;

  // Max = top speed, rounded down to the knob step so it is reachable; the others 25/50/75 %.
  // If 25 % is below the slowest speed the fan holds (e.g. a fan that never stops), the
  // presets are spread evenly from that slowest speed (rounded up to the step) to the top.
  uint16_t step = config.fan.rpmStep ? config.fan.rpmStep : 1;
  uint16_t top = rpm[FAN_TABLE_SIZE - 1] / step * step;
  if (top < config.fan.minRpm + step) top = config.fan.minRpm + step;  // Config tab's lower limit
  uint16_t slowest = min<uint16_t>((rpm[stall] + step - 1) / step * step, top);
  p.maxRpm = top;
  p.presets[3] = top;
  uint16_t quarter = round_to((uint32_t)top / 4, step);
  for (int k = 0; k < 3; k++) {
    p.presets[k] = quarter >= slowest ? round_to((uint32_t)top * (k + 1) / 4, step)
                                      : round_to(slowest + (uint32_t)(top - slowest) * k / 3, step);
    p.presets[k] = constrain(p.presets[k], slowest, top);
  }
  for (int k = 1; k < 4; k++) p.presets[k] = max(p.presets[k], p.presets[k - 1]);  // Keep them in order

  active = slot;
  apply_to_config(p);
  save_profiles();
  Serial.printf("[FANS] Saved '%s' in slot %d: top %u RPM, stall at setting %u, presets %u/%u/%u/%u\n",
                p.name, slot, rpm[FAN_TABLE_SIZE - 1], stall, p.presets[0], p.presets[1], p.presets[2], p.presets[3]);
  return slot;
}

void fan_profiles_sync_from_config() {
  if (active < 0) return;
  FanProfile &p = profiles[active];
  p.maxRpm = config.fan.maxRpm;
  p.presets[0] = config.fan.presets.low;
  p.presets[1] = config.fan.presets.medium;
  p.presets[2] = config.fan.presets.high;
  p.presets[3] = config.fan.presets.max;
  save_profiles();
}

String fan_profiles_json() {
  DynamicJsonDocument doc(6144);
  doc["active"] = active;
  JsonArray list = doc.createNestedArray("profiles");
  for (int i = 0; i < FAN_PROFILE_MAX; i++) {
    const FanProfile &p = profiles[i];
    if (!p.used) {
      list.add(nullptr);
      continue;
    }
    JsonObject o = list.createNestedObject();
    o["name"] = p.name;
    o["maxRpm"] = p.maxRpm;
    o["stall"] = p.stall;
    o["top"] = p.rpm[FAN_TABLE_SIZE - 1];
    o["slowest"] = p.rpm[p.stall];
    o["stops"] = p.stall > 0;  // False: still turning at 0 % PWM
    JsonArray rpm = o.createNestedArray("rpm");
    for (uint16_t v : p.rpm) rpm.add(v);
  }
  JsonObject ac = doc.createNestedObject("autoconfig");
  ac["progress"] = fan_autoconfig_progress();
  ac["result"] = fan_autoconfig_result();
  String out;
  serializeJson(doc, out);
  return out;
}
