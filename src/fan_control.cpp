#include "fan_control.h"
#include "fan_profiles.h"
#include "config.h"
#include "power.h"
#include <Adafruit_EMC2101.h>

// EMC2101 PWM (datasheet rev 2.54, Appendix A): frequency = 360 kHz / (2 x PWM_F), and there
// are 2 x PWM_F usable Fan Settings (anything higher is 100 %). PWM_F 15 = 12.0 kHz, 30 steps
// (~100 RPM each on a 3000 RPM fan). Adafruit's setDutyCycle() assumes 64 steps and its
// begin() picks the 1.4 kHz base clock (~23 Hz PWM), so both are overridden here.
// Starting from stopped is handled by the chip: on a change from setting 0 it drives 100 %
// for up to 3.2 s (Fan Spin Up register, default), ending early once the tach sees the fan.
static const uint8_t EMC2101_ADDR = 0x4C;
static const uint8_t FAN_PWM_F = 15;
static const uint8_t FAN_STEPS = 2 * FAN_PWM_F;
static_assert(FAN_STEPS + 1 == FAN_TABLE_SIZE, "fan_profiles.h table size must match the Fan Settings");

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

// ============================================================================
// AUTO CONFIGURE: run the fan at full speed, then step down one Fan Setting at a time,
// recording the settled RPM at each until it stops. The table becomes a fan profile.
// Runs from fan_update() (loop) a step at a time, so the screen and web page keep working.
// ============================================================================

// Each step waits at least the minimum, then reads the tach every AC_READ_MS until two
// readings agree within 1 % (10 RPM for slow fans) or the timeout passes. Big fans take
// much longer to change speed than small ones.
static const uint32_t AC_TOP_MIN_MS = 3000;     // Full speed: spin up from wherever it was
static const uint32_t AC_TOP_MAX_MS = 20000;
static const uint32_t AC_STEP_MIN_MS = 2000;    // Each step down
static const uint32_t AC_STEP_MAX_MS = 10000;
static const uint32_t AC_READ_MS = 500;

static volatile int8_t ac_request_slot = -2;    // -2 none; else slot to save into (-1 = first free)
static volatile bool ac_cancel_request = false;
static char ac_request_name[FAN_PROFILE_NAME_LEN];
static bool ac_running = false;
static int ac_setting = 0;                      // Fan Setting being measured
static int32_t ac_prev = -1;                    // Previous tach reading this step (-1 = none)
static unsigned long ac_step_start = 0;         // When this setting was written
static unsigned long ac_until = 0;              // Next tach read (millis)
static int8_t ac_slot = -1;
static char ac_name[FAN_PROFILE_NAME_LEN];
static uint16_t ac_rpm[FAN_TABLE_SIZE];
static char ac_result[96] = "";

static void ac_finish(const char *result) {
  ac_running = false;
  strlcpy(ac_result, result, sizeof(ac_result));
  applied_setting = -1;  // fan_update() puts the fan back to the (re-clamped) target
  Serial.printf("[FAN] Auto configure: %s\n", result);
}

static void ac_start() {
  int8_t slot = ac_request_slot;
  ac_request_slot = -2;
  if (!controller_present) {
    strlcpy(ac_result, "No fan controller", sizeof(ac_result));
    return;
  }
  ac_slot = slot;
  strlcpy(ac_name, ac_request_name, sizeof(ac_name));
  memset(ac_rpm, 0, sizeof(ac_rpm));
  ac_setting = FAN_STEPS;
  ac_prev = -1;
  ac_running = true;
  ac_result[0] = '\0';
  write_fan_setting(FAN_STEPS);
  ac_step_start = millis();
  ac_until = ac_step_start + AC_TOP_MIN_MS;
  Serial.printf("[FAN] Auto configure '%s' started\n", ac_name);
}

static void ac_step() {
  if (ac_cancel_request) {
    ac_cancel_request = false;
    ac_finish("Cancelled");
    return;
  }
  if ((long)(millis() - ac_until) < 0) return;
  int32_t r = emc2101.getFanRPM();
  measured_rpm = r;
  bool steady = ac_prev >= 0 && abs(r - ac_prev) <= max<int32_t>(ac_prev / 100, 10);
  uint32_t max_ms = ac_setting == FAN_STEPS ? AC_TOP_MAX_MS : AC_STEP_MAX_MS;
  if (!steady && millis() - ac_step_start < max_ms) {
    ac_prev = r;
    ac_until = millis() + AC_READ_MS;
    return;
  }
  uint16_t rpm = ac_prev >= 0 ? (r + ac_prev) / 2 : r;
  ac_rpm[ac_setting] = rpm;
  Serial.printf("[FAN] Auto configure: setting %d/%d = %u RPM (%s after %lu ms)\n", ac_setting, FAN_STEPS, rpm,
                steady ? "steady" : "timeout", millis() - ac_step_start);
  ac_prev = -1;

  if (ac_setting == FAN_STEPS && rpm == 0) {
    ac_finish("Failed: no RPM at full speed (check the fan's power and tach wire)");
    return;
  }
  // Stopped (or reached setting 0): the slowest running setting is the one above
  if (rpm == 0 || ac_setting == 0) {
    uint8_t stall = rpm == 0 ? ac_setting + 1 : 0;
    for (int s = 0; s < stall; s++) ac_rpm[s] = 0;
    int slot = fan_profiles_store(ac_slot, ac_name, ac_rpm, stall);
    char msg[96];
    if (slot < 0) {
      snprintf(msg, sizeof(msg), "Failed: no free profile slot (delete one first)");
    } else if (stall == 0) {
      snprintf(msg, sizeof(msg), "Saved '%s': top %u RPM; at 0 %% it still runs at %u RPM (PWM can't stop it)",
               ac_name, ac_rpm[FAN_STEPS], ac_rpm[0]);
    } else {
      snprintf(msg, sizeof(msg), "Saved '%s': top %u RPM, slowest %u RPM", ac_name, ac_rpm[FAN_STEPS], ac_rpm[stall]);
    }
    ac_finish(msg);
    return;
  }
  ac_setting--;
  write_fan_setting(ac_setting);
  ac_step_start = millis();
  ac_until = ac_step_start + AC_STEP_MIN_MS;
}

bool fan_autoconfig_start(int slot, const char *name) {
  if (ac_running || !controller_present || power_is_standby()) return false;
  if (slot < -1 || slot >= FAN_PROFILE_MAX) return false;
  strlcpy(ac_request_name, name, sizeof(ac_request_name));
  ac_cancel_request = false;
  ac_request_slot = slot;  // Last: loop() starts it
  return true;
}

void fan_autoconfig_cancel() {
  if (ac_running) ac_cancel_request = true;
}

int fan_autoconfig_progress() {
  if (!ac_running && ac_request_slot == -2) return -1;
  if (!ac_running) return 0;
  return (FAN_STEPS - ac_setting) * 100 / (FAN_STEPS + 1);
}

const char *fan_autoconfig_result() {
  return ac_result;
}

int fan_autoconfig_step() {
  return ac_running ? ac_setting : FAN_STEPS;
}

int fan_autoconfig_steps() {
  return FAN_STEPS;
}

const char *fan_autoconfig_name() {
  return ac_running ? ac_name : ac_request_name;
}

// ============================================================================
// NORMAL RUNNING
// ============================================================================

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

// Standby calls this before cutting the external power: fan_update() would only write the
// new setting on its next pass, by which time the chip is already unpowered.
void fan_stop_now() {
  target_rpm = 0;
  if (ac_running) ac_finish("Cancelled (standby)");
  if (!controller_present) return;
  bool ok = write_fan_setting(0);
  applied_setting = ok ? 0 : -1;
  Serial.printf("[FAN] Stopped (setting 0)%s\n", ok ? "" : " WRITE FAILED");
}

void fan_power_lost() {
  controller_present = false;
  measured_rpm = 0;
  if (ac_running) ac_finish("Cancelled (standby)");
}

// Target RPM -> Fan Setting (0-FAN_STEPS). 0 RPM = off.
// With a fan profile active: the running setting whose measured RPM is closest.
// Without: linear between the web Config's Min/Max PWM (0-255 duty, so they stay valid
// if FAN_PWM_F changes).
static uint8_t setting_for(uint16_t rpm) {
  if (rpm == 0 || config.fan.maxRpm == 0) return 0;
  const FanProfile *p = fan_profile(fan_profiles_active());
  if (p) {
    uint8_t best = FAN_STEPS;
    uint32_t best_diff = UINT32_MAX;
    for (int s = max<int>(p->stall, 1); s <= FAN_STEPS; s++) {
      uint32_t diff = abs((int32_t)p->rpm[s] - (int32_t)rpm);
      if (p->rpm[s] > 0 && diff < best_diff) {
        best = s;
        best_diff = diff;
      }
    }
    return best;
  }
  uint32_t lo = config.fan.calibration.minPwm, hi = config.fan.calibration.maxPwm;
  uint32_t duty = lo + (hi - lo) * min<uint32_t>(rpm, config.fan.maxRpm) / config.fan.maxRpm;
  uint8_t s = (duty * FAN_STEPS + 127) / 255;
  return s == 0 ? 1 : s;  // A non-zero target always gets at least one step
}

void fan_update() {
  if (!controller_present) return;
  if (ac_request_slot != -2 && !ac_running) ac_start();
  if (ac_running) {
    ac_step();
    return;
  }
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
