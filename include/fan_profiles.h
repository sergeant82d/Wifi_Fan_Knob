#ifndef FAN_PROFILES_H
#define FAN_PROFILES_H

#include <Arduino.h>

// Fan profiles: one per fan model, made by Auto Configure (fan_control). Each holds the
// RPM measured at every EMC2101 Fan Setting, plus that fan's max RPM and presets.
// The active profile's max RPM and presets are copied into config.fan, which everything
// else (knob, LCD, web, Home Assistant) already uses. Stored in /fans.json on SPIFFS.

#define FAN_PROFILE_MAX 5
#define FAN_TABLE_SIZE 31  // Fan Settings 0-30 (fan_control FAN_STEPS + 1)
#define FAN_PROFILE_NAME_LEN 24

struct FanProfile {
  bool used;
  char name[FAN_PROFILE_NAME_LEN];
  uint16_t maxRpm;
  uint16_t presets[4];            // Low, Med, High, Max
  uint8_t stall;                  // Lowest Fan Setting that keeps the fan turning
  uint16_t rpm[FAN_TABLE_SIZE];   // Measured RPM at each Fan Setting (0 = stopped)
};

void fan_profiles_init();                        // Load /fans.json (after initConfig)
int fan_profiles_active();                       // Active slot, or -1 (none: linear Min/Max PWM)
const FanProfile *fan_profile(int slot);         // nullptr if the slot is empty
bool fan_profiles_activate(int slot);            // -1 = none. Copies max RPM + presets into config, saves
bool fan_profiles_rename(int slot, const char *name);
bool fan_profiles_delete(int slot);              // Deleting the active one leaves none active
// Save a measured table into slot (-1 = first free). Sets max RPM to the top speed and the
// presets to 25/50/75/100 % of it, then activates it. Returns the slot, or -1 (no free slot).
int fan_profiles_store(int slot, const char *name, const uint16_t rpm[FAN_TABLE_SIZE], uint8_t stall);
void fan_profiles_sync_from_config();            // Config tab saved: copy max RPM + presets into the active profile
String fan_profiles_json();                      // For the web page

#endif
