#include "fan_control.h"
#include "config.h"
#include "power.h"
#include <Adafruit_EMC2101.h>

static Adafruit_EMC2101 emc2101;
static bool controller_present = false;
static volatile uint16_t target_rpm = 0;  // Starts at 0 (off); 16-bit write is atomic

bool fan_init() {
  controller_present = emc2101.begin(0x4C, &Wire);
  Serial.println(controller_present ? "EMC2101 initialized" : "EMC2101 not found!");
  return controller_present;
}

bool fan_controller_present() {
  return controller_present;
}

void fan_power_lost() {
  controller_present = false;
}

void fan_set_target(int32_t rpm) {
  target_rpm = constrain(rpm, (int32_t)config.fan.minRpm, (int32_t)config.fan.maxRpm);
  // Fan only runs when awake: a non-zero target (web, MQTT) wakes from standby
  if (target_rpm > 0 && power_is_standby()) {
    Serial.println("Wake: fan target set (web/MQTT)");
    power_request_standby(false);
  }
}

uint16_t fan_get_target() {
  return target_rpm;
}
