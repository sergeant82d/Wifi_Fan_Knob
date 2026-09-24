#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include "lv_conf.h"

#include "config.h"  // Add near top with other includes
#include "webserver.h"
#include "ui.h"
#include "fan_control.h"
#include "power.h"
#include "mqtt.h"

// ============================================================================
// PIN DEFINITIONS (Elecrow 1.28" Rotary Display)
// ============================================================================

// Display (GC9A01 SPI)
#define DISPLAY_SCLK_PIN 10
#define DISPLAY_MOSI_PIN 11
#define DISPLAY_DC_PIN 3
#define DISPLAY_CS_PIN 9
#define DISPLAY_RST_PIN 14
#define SCREEN_BACKLIGHT_PIN 46

// Touchscreen (CST816D I2C)
#define TOUCH_SDA_PIN 6
#define TOUCH_SCL_PIN 7
#define TOUCH_INT_PIN 5
#define TOUCH_RST_PIN 13

// Main I2C (OLED & EMC2101)
#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39

// RGB LED (WS2812)
#define LED_PIN 48
#define LED_NUM 5

// Rotary Encoder
#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define ENCODER_SW_PIN 41

// Power Control
// Elecrow example: GPIO 1 and 2 are rails that "must remain enabled while
// the display is operating".
#define DISPLAY_RAIL_PIN 1
#define KEEP_ALIVE_PIN 2
#define POWER_LIGHT_PIN 40

// Peripheral power switch: transistor cutting external power (fan, lights, sensors,
// EMC2101). Undriven until config loads; hardware pull must hold it OFF.
#define PERIPH_POWER_PIN 4

// Test I/O (available for future use)
#define TEST_PIN_2 12

// ============================================================================
// DISPLAY SETUP (LGFX + LVGL)
// ============================================================================

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;

// GC9A01 panel config, taken from Elecrow's RotaryScreen_1_28 example
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel_instance;
  lgfx::Bus_SPI _bus_instance;

 public:
  LGFX(void) {
    {
      auto cfg = _bus_instance.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 80000000;
      cfg.freq_read = 20000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = DISPLAY_SCLK_PIN;
      cfg.pin_mosi = DISPLAY_MOSI_PIN;
      cfg.pin_miso = -1;
      cfg.pin_dc = DISPLAY_DC_PIN;
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }
    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = DISPLAY_CS_PIN;
      cfg.pin_rst = DISPLAY_RST_PIN;
      cfg.pin_busy = -1;
      cfg.memory_width = screenWidth;
      cfg.memory_height = screenHeight;
      cfg.panel_width = screenWidth;
      cfg.panel_height = screenHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      _panel_instance.config(cfg);
    }
    setPanel(&_panel_instance);
  }
};

LGFX gfx;

#include "dragon_eye.h"  // Standby/screensaver eye, draws straight to gfx
#include "eye_styles.h"
#include <driver/pulse_cnt.h>

// ============================================================================
// LVGL BUFFER & DISPLAY CALLBACK
// ============================================================================

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

void display_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  gfx.startWrite();
  gfx.setAddrWindow(area->x1, area->y1, w, h);
  gfx.writePixels((lgfx::rgb565_t *)&color_p->full, w * h);
  gfx.endWrite();

  lv_disp_flush_ready(disp);
}

// ============================================================================
// TOUCH INPUT CALLBACK
// ============================================================================

// CST816D on its own I2C bus (Wire1). Init sequence follows Elecrow's
// driver; reads are single-attempt so a missing chip can't hang the loop.
#define TOUCH_I2C_ADDR 0x15
static bool touch_ok = false;

bool init_touch() {
  Wire1.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);

  // Wake pulse on INT, then hardware reset
  pinMode(TOUCH_INT_PIN, OUTPUT);
  digitalWrite(TOUCH_INT_PIN, HIGH);
  delay(1);
  digitalWrite(TOUCH_INT_PIN, LOW);
  delay(1);
  pinMode(TOUCH_INT_PIN, INPUT);
  pinMode(TOUCH_RST_PIN, OUTPUT);
  digitalWrite(TOUCH_RST_PIN, LOW);
  delay(10);
  digitalWrite(TOUCH_RST_PIN, HIGH);
  delay(300);

  // Register 0xFE = 0xFF disables auto-sleep (as in Elecrow's driver)
  Wire1.beginTransmission(TOUCH_I2C_ADDR);
  Wire1.write(0xFE);
  Wire1.write(0xFF);
  return Wire1.endTransmission() == 0;
}

// Reads finger count + coordinates (regs 0x02-0x06); false if no touch or I2C error
static bool read_touch(uint16_t &x, uint16_t &y) {
  Wire1.beginTransmission(TOUCH_I2C_ADDR);
  Wire1.write(0x02);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom(TOUCH_I2C_ADDR, 5) != 5) return false;
  uint8_t d[5];
  for (uint8_t &b : d) b = Wire1.read();
  if (d[0] == 0) return false;
  x = ((d[1] & 0x0F) << 8) | d[2];
  y = ((d[3] & 0x0F) << 8) | d[4];
  return true;
}

static void note_activity();

void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  static bool was_pressed = false;
  uint16_t x, y;
  bool pressed = touch_ok && read_touch(x, y);
  if (pressed) {
    note_activity();
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    if (!was_pressed) {
      Serial.printf("Touch: %u,%u\n", x, y);
      if (power_is_standby()) power_request_standby(false);  // Touch wakes
    }
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
  was_pressed = pressed;
}

// ============================================================================
// ENCODER & BUTTON HANDLING
// ============================================================================

// Knob: quadrature decoded by the PCNT hardware counter, not GPIO interrupts. Every A/B
// edge counts (4 per detent); direction as in Elecrow's example (A leading = +1).
// Why no interrupts: attachInterrupt() installs the SDK's GPIO ISR service from the ipc1
// task, whose 1 KB stack (fixed in the prebuilt SDK) overflowed on ~1 in 10 boots when
// another interrupt landed mid-install. PCNT needs no interrupt (no event callbacks).
// The button is polled in loop() with a debounce.
static pcnt_unit_handle_t encoder_unit = nullptr;
static const int ENCODER_PCNT_LIMIT = 30000;  // Counter returns to 0 here; unwrapped below
static bool encoder_button_pressed = false;   // Debounced press latched by poll_button()

const unsigned long BUTTON_DEBOUNCE_MS = 20;

static bool pcnt_ok(esp_err_t err, const char *what) {
  if (err != ESP_OK) Serial.printf("ERROR: encoder %s: %s\n", what, esp_err_to_name(err));
  return err == ESP_OK;
}

static void init_encoder() {
  pcnt_unit_config_t unit_cfg = {};
  unit_cfg.low_limit = -ENCODER_PCNT_LIMIT;
  unit_cfg.high_limit = ENCODER_PCNT_LIMIT;
  if (!pcnt_ok(pcnt_new_unit(&unit_cfg, &encoder_unit), "unit")) {
    encoder_unit = nullptr;
    return;
  }
  pcnt_glitch_filter_config_t filter = {};
  filter.max_glitch_ns = 1000;  // Ignore contact bounce shorter than 1 us
  pcnt_ok(pcnt_unit_set_glitch_filter(encoder_unit, &filter), "filter");

  // Standard x4 quadrature (as ESP-IDF's rotary encoder example): each channel counts
  // one pin's edges, direction set by the other pin's level
  pcnt_chan_config_t a_cfg = {};
  a_cfg.edge_gpio_num = ENCODER_A_PIN;
  a_cfg.level_gpio_num = ENCODER_B_PIN;
  pcnt_chan_config_t b_cfg = {};
  b_cfg.edge_gpio_num = ENCODER_B_PIN;
  b_cfg.level_gpio_num = ENCODER_A_PIN;
  pcnt_channel_handle_t chan_a = nullptr, chan_b = nullptr;
  pcnt_ok(pcnt_new_channel(encoder_unit, &a_cfg, &chan_a), "channel A");
  pcnt_ok(pcnt_new_channel(encoder_unit, &b_cfg, &chan_b), "channel B");
  pcnt_channel_set_edge_action(chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
  pcnt_channel_set_level_action(chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
  pcnt_channel_set_edge_action(chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
  pcnt_channel_set_level_action(chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

  pcnt_ok(pcnt_unit_enable(encoder_unit), "enable");
  pcnt_ok(pcnt_unit_clear_count(encoder_unit), "clear");
  pcnt_ok(pcnt_unit_start(encoder_unit), "start");
}

// Whole detents turned since the last call (part-turns carry over)
static int32_t encoder_read_detents() {
  static int last = 0, quarters = 0;
  int count = 0;
  if (!encoder_unit || pcnt_unit_get_count(encoder_unit, &count) != ESP_OK) return 0;
  int diff = count - last;
  last = count;
  if (diff > ENCODER_PCNT_LIMIT / 2) diff -= ENCODER_PCNT_LIMIT;       // Wrapped at a limit
  else if (diff < -ENCODER_PCNT_LIMIT / 2) diff += ENCODER_PCNT_LIMIT;
  quarters += diff;
  int32_t detents = quarters / 4;
  quarters -= detents * 4;
  return detents;
}

// Latches a press once the button has read low for BUTTON_DEBOUNCE_MS; loop() clears it
static void poll_button() {
  static bool raw_last = false, stable = false;
  static unsigned long changed_at = 0;
  bool raw = digitalRead(ENCODER_SW_PIN) == LOW;
  if (raw != raw_last) {
    raw_last = raw;
    changed_at = millis();
  } else if (raw != stable && millis() - changed_at >= BUTTON_DEBOUNCE_MS) {
    stable = raw;
    if (stable) encoder_button_pressed = true;
  }
}

// ============================================================================
// DEVICE STATE MACHINE
// ============================================================================

enum SystemState {
  STATE_SHUTDOWN = 0,
  STATE_STANDBY = 1,
  STATE_ACTIVE = 2
};

volatile SystemState current_state = STATE_ACTIVE;
bool time_synced = false;
const unsigned long NTP_SYNC_INTERVAL = 60 * 60 * 1000; // 60 minutes

// ============================================================================
// WIFI SETUP
// ============================================================================

void startAPMode() {
  Serial.println("Starting AP mode...");
  WiFi.mode(WIFI_AP_STA);  // STA side needed for network scans from the web UI
  
  String apName = "WiFi-Fan-Knob-" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
  // Password set on the web UI WiFi tab (default 12345678); not logged
  WiFi.softAP(apName.c_str(), config.wifi.apPassword);
  IPAddress apIP = WiFi.softAPIP();

  Serial.print("AP started: ");
  Serial.println(apName);
  Serial.print("IP: ");
  Serial.println(apIP);
}

// Static IP from the WiFi tab, or DHCP; call before WiFi.begin().
// Addresses were validated when saved. DNS defaults to the gateway.
static void apply_ip_config() {
  IPAddress ip, gw, mask, dns;
  if (config.wifi.useStaticIp && ip.fromString(config.wifi.staticIp) &&
      gw.fromString(config.wifi.staticGateway) && mask.fromString(config.wifi.staticSubnet)) {
    if (!dns.fromString(config.wifi.staticDns)) dns = gw;
    WiFi.config(ip, gw, mask, dns);
  } else {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);  // DHCP
  }
}

// Called once a second: hotspot off once the saved network is connected;
// back on if that network has been lost for 60 s (so the board stays reachable)
void maintain_wifi() {
  static unsigned long sta_lost_since = 0;
  bool connected = WiFi.status() == WL_CONNECTED;
  bool ap_on = WiFi.getMode() & WIFI_AP;

  if (connected) {
    sta_lost_since = 0;
    if (ap_on) {
      WiFi.softAPdisconnect(true);  // true = disable AP interface (mode → STA)
      Serial.println("WiFi connected, hotspot off");
    }
  } else if (config.wifi.ssid[0] != '\0' && ap_on) {
    // Hotspot up but saved network not joined: switching to AP_STA stops the
    // pending STA attempt, so retry every 20 s
    static unsigned long last_retry = 0;
    if (millis() - last_retry > 20000) {
      last_retry = millis();
      Serial.println("Retrying saved WiFi...");
      apply_ip_config();
      WiFi.begin(config.wifi.ssid, config.wifi.password);
    }
  } else if (config.wifi.ssid[0] != '\0' && !ap_on) {
    if (sta_lost_since == 0) {
      sta_lost_since = millis();
    } else if (millis() - sta_lost_since > 60000) {
      Serial.println("WiFi lost for 60 s, starting hotspot");
      startAPMode();  // STA keeps retrying; hotspot goes off again on reconnect
      sta_lost_since = 0;
    }
  }
}

void init_wifi() {
// ============================================================================
// WIFI INITIALIZATION
// ============================================================================
  Serial.println("Initializing WiFi...");

  if (config.wifi.saveCredentials && config.wifi.ssid[0] != '\0') {
    // Try to connect with saved credentials
    Serial.print("Connecting to: ");
    Serial.println(config.wifi.ssid);
    
    WiFi.mode(WIFI_STA);
    apply_ip_config();
    WiFi.begin(config.wifi.ssid, config.wifi.password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("WiFi connected: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("WiFi connection failed, starting AP mode");
      startAPMode();
    }
  } else {
    // No saved credentials, start AP mode
    Serial.println("No saved WiFi credentials, starting AP mode");
    startAPMode();
}
}

// ============================================================================
// NTP TIME SYNC
// ============================================================================

// Starts the background SNTP client; it re-syncs every NTP_SYNC_INTERVAL
// on its own, so nothing here blocks.
// Apply saved brightness and time zone (boot, and after Config tab save)
const uint8_t STANDBY_BRIGHTNESS = 10;  // % backlight in standby
const int STANDBY_EYE_FPS = 15;         // Eye frame rate in standby (screensaver: as fast as it draws)

static void set_backlight(uint8_t percent) {
  uint32_t duty = percent * 255 / 100;
  bool ok = ledcWrite(SCREEN_BACKLIGHT_PIN, duty);
  Serial.printf("[DISPLAY] Brightness %u%% (duty %u/255) %s\n", percent, duty, ok ? "OK" : "FAILED");
}

void applyDisplaySettings() {
  set_backlight(current_state == STATE_STANDBY ? STANDBY_BRIGHTNESS : config.display.brightness);
  setenv("TZ", config.display.posixTz, 1);  // POSIX rule chosen on the web UI
  tzset();
}

// ============================================================================
// PERIPHERAL POWER (GPIO 4 transistor switch)
// ============================================================================

static bool periph_power_on = false;

static void set_peripheral_power(bool on) {
  periph_power_on = on;
  bool level = (on == config.power.activeHigh);
  digitalWrite(PERIPH_POWER_PIN, level ? HIGH : LOW);
  Serial.printf("[POWER] Peripherals %s (GPIO %d %s)\n", on ? "ON" : "OFF", PERIPH_POWER_PIN, level ? "HIGH" : "LOW");
}

// Power up external devices, let them settle, then probe the EMC2101 (on the switched rail)
static void power_up_peripherals() {
  set_peripheral_power(true);
  delay(50);
  if (!fan_init()) {
    Serial.println("WARNING: Fan controller not responding");
  }
}

bool power_peripherals_on() {
  return periph_power_on;
}

void applyPowerSettings() {
  set_peripheral_power(periph_power_on);
}

// ============================================================================
// STANDBY (knob long-press, web Standby button; any input wakes)
// ============================================================================

static volatile int8_t standby_request = -1;  // -1 none, 0 wake, 1 standby
static bool standby_touch_armed = false;      // Set once no touch is seen in standby

void power_request_standby(bool standby) {
  standby_request = standby ? 1 : 0;
}

bool power_is_standby() {
  return current_state == STATE_STANDBY;
}

// ============================================================================
// SCREENSAVER: dragon eye after config.display.screensaverSec idle seconds (0 = off).
// Unlike standby, the fan and peripherals keep running and brightness is unchanged.
// A touch, knob turn or short press only dismisses it; a long press still goes to
// standby. A fan speed change (web, MQTT) also dismisses it so the new speed shows.
// ============================================================================

static bool saver_on = false;
static unsigned long last_activity = 0;

static void note_activity() {
  last_activity = millis();
}

bool power_screensaver_on() {
  return saver_on;
}

static bool eye_showing() {
  return saver_on || power_is_standby();
}

// loop() only (LVGL)
static void set_saver(bool on) {
  if (on == saver_on) return;
  saver_on = on;
  note_activity();
  if (on) standby_touch_armed = false;  // Wake needs a new touch
  ui_set_standby(on);                   // LVGL steps aside for the eye; back to Main after
  Serial.println(on ? "Screensaver on" : "Screensaver off");
}

// loop() only: switches screen (LVGL) and backlight
static void set_standby(bool standby) {
  if (standby == power_is_standby()) return;
  current_state = standby ? STATE_STANDBY : STATE_ACTIVE;
  saver_on = false;   // Standby shows the eye itself
  note_activity();    // Idle timer restarts after standby or wake
  if (standby) {
    standby_touch_armed = false;  // Ignore the touch that came with the knob press
    fan_set_target(0);  // Standby stops the fan; waking leaves it at 0
    set_peripheral_power(false);
    fan_power_lost();
  } else {
    power_up_peripherals();
  }
  ui_set_standby(standby);
  set_backlight(standby ? STANDBY_BRIGHTNESS : config.display.brightness);
  Serial.println(standby ? "Standby" : "Wake");
}

void start_ntp() {
  esp_sntp_set_sync_interval(NTP_SYNC_INTERVAL);
  // configTzTime, not configTime: configTime would reset TZ to UTC
  configTzTime(config.display.posixTz, "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP started");
}

// ============================================================================
// SOFT POWER LATCH (KEEP_ALIVE)
// ============================================================================

void init_power_latch() {
  pinMode(DISPLAY_RAIL_PIN, OUTPUT);
  digitalWrite(DISPLAY_RAIL_PIN, HIGH);
  pinMode(KEEP_ALIVE_PIN, OUTPUT);
  digitalWrite(KEEP_ALIVE_PIN, HIGH); // Keep system powered on boot
  Serial.println("Power latch engaged");
}

void shutdown_system() {
  Serial.println("Initiating system shutdown...");
  current_state = STATE_SHUTDOWN;
  
  // TODO: Save any pending state, disable peripherals
  
  digitalWrite(KEEP_ALIVE_PIN, LOW); // Cut power to system
  // Will not reach here; system powers off
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void setup() {
  // Soft Power Latch — must be first, before any delay
  init_power_latch();

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== PWM Fan Controller Startup ===\n");
  Serial.printf("Flash: %u MB, PSRAM: %u bytes\n", ESP.getFlashChipSize() / (1024 * 1024), ESP.getPsramSize());

  // GPIO Setup
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(POWER_LIGHT_PIN, OUTPUT);
  digitalWrite(POWER_LIGHT_PIN, HIGH);
  // PWM backlight, full until config loads
  if (!ledcAttach(SCREEN_BACKLIGHT_PIN, 5000, 8)) {
    Serial.println("WARNING: Backlight PWM attach failed");
  }
  ledcWrite(SCREEN_BACKLIGHT_PIN, 255);

  // Display & LVGL
  Serial.println("Initializing display...");
  gfx.init();
  eye_begin(&gfx);
  gfx.setColorDepth(16);
  gfx.fillScreen(TFT_BLACK);
  gfx.setTextColor(TFT_WHITE);
  gfx.setTextSize(1);
  gfx.setCursor(10, 10);
  gfx.println("Booting...");

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = display_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  touch_ok = init_touch();
  Serial.println(touch_ok ? "Touch controller found" : "WARNING: Touch controller not responding");

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  // I2C (EMC2101 is probed after peripheral power comes on, below)
  Serial.println("Initializing I2C...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Encoder Input
  Serial.println("Initializing encoder...");
  init_encoder();  // PCNT hardware counter; button is polled (no GPIO interrupts)

// ============================================================================
// CONFIGURATION & SPIFFS
// ============================================================================

Serial.println("Initializing configuration system...");
initConfig();  // Load config from SPIFFS (or set defaults)
applyDisplaySettings();
if (!eye_set_style(config.display.eyeStyle)) eye_set_style(EYE_STYLES[0]->id);  // Unknown: first style

// Peripheral power needs config (active level). Any glitch from pinMode is toward ON,
// which is where it is going anyway.
pinMode(PERIPH_POWER_PIN, OUTPUT);
power_up_peripherals();

ui_init();          // Needs config (RPM range, time format)
lv_task_handler();  // Show screen while WiFi connects

// Update device info with actual chip ID (optional, for logging)
snprintf(config.chipId, sizeof(config.chipId), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));

if (config.advanced.debugMode) {
  Serial.println("[MAIN] Debug mode enabled");
}

  init_wifi();
  if (WiFi.status() == WL_CONNECTED) {
    start_ntp();
  }
  init_webserver();
  mqtt_init();  // Own task; connects once WiFi (STA) is up
  ui_update();


  // UI Setup
  // TODO: Create main screen UI (LVGL screens)

  Serial.println("=== Boot Complete ===\n");
  current_state = STATE_ACTIVE;
  note_activity();  // Screensaver idle timer starts at boot complete
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Report first NTP sync (re-syncs happen in the background)
  if (!time_synced && time(nullptr) > 24 * 3600) {
    time_synced = true;
    time_t now = time(nullptr);
    Serial.print("Time synced: ");
    Serial.println(ctime(&now));
  }

  // Handle encoder rotation
  int32_t delta = encoder_read_detents();
  poll_button();
  if (delta != 0) note_activity();
  if (delta != 0 && power_is_standby()) {
    Serial.printf("Wake: knob (%ld)\n", (long)delta);
    power_request_standby(false);  // Turning the knob wakes; this turn is discarded
  } else if (delta != 0 && saver_on) {
    set_saver(false);  // Dismiss only; this turn is discarded
  } else if (delta != 0 && ui_menu_open()) {
    ui_menu_turn(delta);  // Menu open: knob chooses a page, RPM unchanged
  } else if (delta != 0) {
    // Knob sets target RPM (fan_control clamps to config range)
    fan_set_target((int32_t)fan_get_target() + delta * config.fan.rpmStep);
    ui_show_main();  // Show the RPM being changed
    Serial.printf("Encoder: %ld -> target %u RPM\n", (long)delta, fan_get_target());
  }

  // Redraw target RPM when changed by knob or web UI (LVGL only touched here)
  static uint16_t shown_rpm = 0;
  if (fan_get_target() != shown_rpm) {
    shown_rpm = fan_get_target();
    note_activity();
    if (saver_on) set_saver(false);  // Show the new speed (web/MQTT change)
    ui_set_target_rpm(shown_rpm);
  }

  // Encoder button: ISR latches the (debounced) press; duration measured here.
  // Held LONG_PRESS_MS → standby (fires while still held). Any press in standby wakes.
  const unsigned long LONG_PRESS_MS = 1000;
  static unsigned long press_start = 0;
  static bool press_handled = false;
  static bool press_in_saver = false;  // Short press only dismisses the screensaver
  if (encoder_button_pressed) {
    encoder_button_pressed = false;
    note_activity();
    press_start = millis();
    press_handled = false;
    press_in_saver = saver_on;
    if (power_is_standby()) {
      Serial.println("Wake: button");
      power_request_standby(false);
      press_handled = true;  // Waking consumes this press
    }
  }
  if (press_start != 0) {
    bool held = digitalRead(ENCODER_SW_PIN) == LOW;
    unsigned long held_ms = millis() - press_start;
    if (held && !press_handled && held_ms >= LONG_PRESS_MS) {
      press_handled = true;
      Serial.println("Button long press");
      power_request_standby(true);
    } else if (!held && held_ms > 50) {  // Released (ignore bounce right after press)
      if (!press_handled && press_in_saver) {
        set_saver(false);
      } else if (!press_handled) {
        Serial.println("Button short press");
        ui_menu_button();  // Open the page menu, or go to the chosen page
      }
      press_start = 0;
    }
  }

  // Apply standby/wake requested by button, knob, touch or web
  if (standby_request >= 0) {
    set_standby(standby_request == 1);
    standby_request = -1;
  }

  // Refresh clock + status box once a second
  static unsigned long last_status_update = 0;
  if (millis() - last_status_update >= 1000) {
    last_status_update = millis();
    maintain_wifi();
    ui_update();
    // Eye style chosen on the web page (built here: PSRAM tables, ~0.3 s)
    static char tried_style[sizeof(config.display.eyeStyle)] = "";
    if (strcmp(config.display.eyeStyle, eye_style_id()) != 0 && strcmp(config.display.eyeStyle, tried_style) != 0) {
      strlcpy(tried_style, config.display.eyeStyle, sizeof(tried_style));  // Don't retry a failure every second
      eye_set_style(config.display.eyeStyle);
    }
  }

  // Screensaver after the idle delay (a held button counts as activity)
  if (press_start != 0) note_activity();
  if (!eye_showing() && config.display.screensaverSec > 0 &&
      millis() - last_activity >= config.display.screensaverSec * 1000UL) {
    set_saver(true);
  }

  if (eye_showing()) {
    // Standby or screensaver: dragon eye owns the display; LVGL (and its touch read)
    // is paused, so poll touch directly. LVGL redraws Main when the screen reloads.
    // Standby saves power by drawing fewer frames; the screensaver runs flat out.
    static unsigned long last_eye_frame = 0;
    bool draw = !power_is_standby() || millis() - last_eye_frame >= 1000 / STANDBY_EYE_FPS;
    if (draw) {
      last_eye_frame = millis();
      eye_frame();
    } else {
      delay(5);  // Keep polling touch, knob and button between frames
    }
    // Wake only on a NEW touch: pressing the knob puts a finger on the glass,
    // so the touch that accompanied the long press must lift first.
    uint16_t tx, ty;
    bool touching = touch_ok && read_touch(tx, ty);
    if (!touching) {
      standby_touch_armed = true;
    } else if (standby_touch_armed && saver_on) {
      set_saver(false);  // Dismiss only
    } else if (standby_touch_armed) {
      Serial.printf("Wake: touch %u,%u\n", tx, ty);
      power_request_standby(false);
    }

    static uint32_t eye_frames = 0, eye_fps_start = 0;
    if (draw) eye_frames++;
    if (millis() - eye_fps_start >= 10000) {
      if (eye_fps_start) Serial.printf("[EYE] %lu fps\n", eye_frames * 1000UL / (millis() - eye_fps_start));
      eye_frames = 0;
      eye_fps_start = millis();
    }
  } else {
    // LVGL tick
    lv_tick_inc(5);
    lv_task_handler();

    delay(5);
  }
}

