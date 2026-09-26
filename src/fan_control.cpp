#include "fan_control.h"
#include "config.h"
#include "power.h"
#include <Adafruit_EMC2101.h>

// EMC2101 PWM (datasheet rev 2.54, Appendix A): frequency = 360 kHz / (2 x PWM_F), and there
// are 2 x PWM_F usable Fan Settings (anything higher is 100 %). PWM_F 15 = 12.0 kHz, 30 steps
// (~100 RPM each on a 3000 RPM fan). Adafruit's setDutyCycle() assumes 64 steps and its
// begin() picks the 1.4 kHz base clock (~23 Hz PWM), so both are overridden here.
static const uint8_t EMC2101_ADDR = 0x4C;
static const uint8_t FAN_PWM_F = 15;
static const uint8_t FAN_STEPS = 2 * FAN_PWM_F;

static Adafruit_EMC2101 emc2101;
static bool controller_present = false;
static volatile uint16_t target_rpm = 0;  // Starts at 0 (off); 16-bit write is atomic
static volatile uint16_t measured_rpm = 0;
static int applied_setting = -1;          // Fan Setting last written (-1 = none since init)

static bool write_fan_setting(uint8_t setting) {
  Wire.beginTransmission(EMC2101_ADDR);
  Wire.write(EMC2101_REG_FAN_SETTING);
  Wire.write(setting);
  return Wire.endTransmission() == 0;
}

bool fan_init() {
  controller_present = emc2101.begin(EMC2101_ADDR, &Wire);
  applied_setting = -1;
  measured_rpm = 0;
  if (controller_present) {
    emc2101.configPWMClock(false, false);  // 360 kHz base clock, divider not used
    emc2101.setPWMFrequency(FAN_PWM_F);
    write_fan_setting(0);                  // begin() left it at 100 %; fan_update() sets the target
  }
  Serial.println(controller_present ? "EMC2101 initialized" : "EMC2101 not found!");
  return controller_present;
}

bool fan_controller_present() {
  return controller_present;
}

void fan_power_lost() {
  controller_present = false;
  measured_rpm = 0;
}

// Target RPM -> Fan Setting (0-FAN_STEPS). Linear between the web Config's Min/Max PWM
// (0-255 duty, so they stay valid if FAN_PWM_F changes). 0 RPM = off.
static uint8_t setting_for(uint16_t rpm) {
  if (rpm == 0 || config.fan.maxRpm == 0) return 0;
  uint32_t lo = config.fan.calibration.minPwm, hi = config.fan.calibration.maxPwm;
  uint32_t duty = lo + (hi - lo) * min<uint32_t>(rpm, config.fan.maxRpm) / config.fan.maxRpm;
  uint8_t s = (duty * FAN_STEPS + 127) / 255;
  return s == 0 ? 1 : s;  // A non-zero target always gets at least one step
}

void fan_update() {
  if (!controller_present) return;
  uint8_t s = setting_for(target_rpm);
  if (s != applied_setting) {
    bool ok = write_fan_setting(s);
    applied_setting = ok ? s : -1;  // Retry next loop if the write failed
    Serial.printf("[FAN] Target %u RPM -> setting %u/%u (%u%%)%s\n", target_rpm, s, FAN_STEPS,
                  s * 100 / FAN_STEPS, ok ? "" : " WRITE FAILED");
  }
  static unsigned long last_tach = 0, last_log = 0;
  if (millis() - last_tach >= 1000) {
    last_tach = millis();
    measured_rpm = emc2101.getFanRPM();
  }
  if (millis() - last_log >= 5000 && (measured_rpm > 0 || s > 0)) {
    last_log = millis();
    Serial.printf("[FAN] %u RPM measured (setting %u/%u)\n", measured_rpm, s, FAN_STEPS);
  }
}

uint16_t fan_get_rpm() {
  return measured_rpm;
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
