#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <esp_sntp.h>
#include <Adafruit_EMC2101.h>
#include "lv_conf.h"

#include "config.h"  // Add near top with other includes
#include "webserver.h"

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

// Test I/O (available for future use)
#define TEST_PIN_1 4
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

void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  // TODO: Implement CST816D touch reading
  // For now, return no touch
  data->state = LV_INDEV_STATE_REL;
}

// ============================================================================
// ENCODER & BUTTON HANDLING
// ============================================================================

// Quadrature decoding adapted from Elecrow's RotaryScreen_1_28 example.
// encoder_count holds whole detents (4 valid transitions each).
volatile int32_t encoder_count = 0;
volatile int8_t encoder_quarter_steps = 0;
volatile uint8_t encoder_last_state = 0;
volatile bool encoder_button_pressed = false;
portMUX_TYPE encoder_mux = portMUX_INITIALIZER_UNLOCKED;

// Index = (last_state << 2) | current_state, with A = bit1, B = bit0.
// Invalid (skipped) transitions and no-change map to 0.
DRAM_ATTR static const int8_t encoder_transition_table[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

const unsigned long BUTTON_DEBOUNCE_MS = 20;

void IRAM_ATTR encoder_isr() {
  uint8_t current_state = ((uint8_t)digitalRead(ENCODER_A_PIN) << 1) |
                          (uint8_t)digitalRead(ENCODER_B_PIN);
  int8_t movement = encoder_transition_table[(encoder_last_state << 2) | current_state];

  portENTER_CRITICAL_ISR(&encoder_mux);
  encoder_last_state = current_state;
  if (movement != 0) {
    encoder_quarter_steps += movement;
    if (encoder_quarter_steps >= 4) {
      encoder_count++;
      encoder_quarter_steps -= 4;
    } else if (encoder_quarter_steps <= -4) {
      encoder_count--;
      encoder_quarter_steps += 4;
    }
  }
  portEXIT_CRITICAL_ISR(&encoder_mux);
}

// Latches a press on a debounced falling edge; loop() clears it
void IRAM_ATTR button_isr() {
  static unsigned long last_interrupt_time = 0;
  unsigned long now = millis();
  if (now - last_interrupt_time > BUTTON_DEBOUNCE_MS && !digitalRead(ENCODER_SW_PIN)) {
    encoder_button_pressed = true;
  }
  last_interrupt_time = now;
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
// EMC2101 FAN CONTROLLER
// ============================================================================

Adafruit_EMC2101 emc2101;

bool init_fan_controller() {
  if (!emc2101.begin(0x4C, &Wire)) {
    Serial.println("EMC2101 not found!");
    return false;
  }
  Serial.println("EMC2101 initialized");
  return true;
}

// ============================================================================
// WIFI SETUP
// ============================================================================

void startAPMode() {
  Serial.println("Starting AP mode...");
  WiFi.mode(WIFI_AP_STA);  // STA side needed for network scans from the web UI
  
  String apName = "WiFi-Fan-Knob-" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);
  String apPass = "12345678";  // User can change in webserver
  
  WiFi.softAP(apName.c_str(), apPass.c_str());
  IPAddress apIP = WiFi.softAPIP();
  
  Serial.print("AP started: ");
  Serial.println(apName);
  Serial.print("IP: ");
  Serial.println(apIP);
  Serial.print("Password: ");
  Serial.println(apPass);
  
  // Display AP info on screen (TODO: show on LVGL)
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
// POSIX TZ string for the web UI's time zone choices (US zones observe DST)
static const char *posix_tz(const char *tz) {
  if (!strcmp(tz, "EST")) return "EST5EDT,M3.2.0,M11.1.0";
  if (!strcmp(tz, "CST")) return "CST6CDT,M3.2.0,M11.1.0";
  if (!strcmp(tz, "MST")) return "MST7MDT,M3.2.0,M11.1.0";
  if (!strcmp(tz, "PST")) return "PST8PDT,M3.2.0,M11.1.0";
  return "UTC0";
}

// Apply saved brightness and time zone (boot, and after Config tab save)
void applyDisplaySettings() {
  ledcWrite(SCREEN_BACKLIGHT_PIN, config.display.brightness * 255 / 100);
  setenv("TZ", posix_tz(config.display.timezone), 1);
  tzset();
}

void start_ntp() {
  esp_sntp_set_sync_interval(NTP_SYNC_INTERVAL);
  // configTzTime, not configTime: configTime would reset TZ to UTC
  configTzTime(posix_tz(config.display.timezone), "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP started");
}

// ============================================================================
// STATUS BOX (center of LCD: IP address, flashes when attention needed)
// ============================================================================

static lv_obj_t *status_box = nullptr;
static lv_obj_t *status_label = nullptr;
static lv_timer_t *status_flash_timer = nullptr;
static bool status_flash_red = false;

static void status_set_colors(lv_color_t bg, lv_color_t fg) {
  lv_obj_set_style_bg_color(status_box, bg, 0);
  lv_obj_set_style_text_color(status_label, fg, 0);
}

static void status_flash_cb(lv_timer_t *) {
  status_flash_red = !status_flash_red;
  if (status_flash_red) {
    status_set_colors(lv_palette_main(LV_PALETTE_RED), lv_color_white());
  } else {
    status_set_colors(lv_color_white(), lv_color_black());
  }
}

void create_status_box() {
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

  status_box = lv_obj_create(lv_scr_act());
  lv_obj_set_size(status_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_center(status_box);
  lv_obj_clear_flag(status_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_border_width(status_box, 0, 0);
  lv_obj_set_style_radius(status_box, 8, 0);
  lv_obj_set_style_pad_all(status_box, 10, 0);

  status_label = lv_label_create(status_box);
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_text(status_label, "Starting...");

  status_set_colors(lv_color_white(), lv_color_black());
  status_flash_timer = lv_timer_create(status_flash_cb, 500, nullptr);
  lv_timer_pause(status_flash_timer);
}

// Steady white box normally; flashes red/white while attention is needed
void status_set_attention(bool attention) {
  bool flashing = !status_flash_timer->paused;
  if (attention == flashing) return;
  if (attention) {
    lv_timer_resume(status_flash_timer);
  } else {
    lv_timer_pause(status_flash_timer);
    status_flash_red = false;
    status_set_colors(lv_color_white(), lv_color_black());
  }
}

// Refresh IP/mode text and attention state from current WiFi status
void update_status_box() {
  String ip, mode;
  if (WiFi.status() == WL_CONNECTED) {
    ip = WiFi.localIP().toString();
    mode = "WiFi";
  } else if (WiFi.getMode() & WIFI_AP) {
    ip = WiFi.softAPIP().toString();
    mode = "AP mode";
  } else {
    ip = "--";
    mode = "WiFi lost";
  }
  String text = ip + ":" + String(config.webserver.port) + "\n" + mode;
  if (text != lv_label_get_text(status_label)) {
    lv_label_set_text(status_label, text.c_str());
  }

  // Attention: saved network configured but not connected
  status_set_attention(config.wifi.ssid[0] != '\0' && WiFi.status() != WL_CONNECTED);
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
  ledcAttach(SCREEN_BACKLIGHT_PIN, 5000, 8);  // PWM backlight, full until config loads
  ledcWrite(SCREEN_BACKLIGHT_PIN, 255);

  // Display & LVGL
  Serial.println("Initializing display...");
  gfx.init();
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

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  create_status_box();
  lv_task_handler();  // Show "Starting..." while WiFi connects

  // I2C & EMC2101
  Serial.println("Initializing I2C and fan controller...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!init_fan_controller()) {
    Serial.println("WARNING: Fan controller not responding");
  }

  // Encoder Input
  Serial.println("Initializing encoder...");
  encoder_last_state = ((uint8_t)digitalRead(ENCODER_A_PIN) << 1) |
                       (uint8_t)digitalRead(ENCODER_B_PIN);
  attachInterrupt(ENCODER_A_PIN, encoder_isr, CHANGE);
  attachInterrupt(ENCODER_B_PIN, encoder_isr, CHANGE);
  attachInterrupt(ENCODER_SW_PIN, button_isr, CHANGE);

// ============================================================================
// CONFIGURATION & SPIFFS
// ============================================================================

Serial.println("Initializing configuration system...");
initConfig();  // Load config from SPIFFS (or set defaults)
applyDisplaySettings();

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
  update_status_box();


  // UI Setup
  // TODO: Create main screen UI (LVGL screens)

  Serial.println("=== Boot Complete ===\n");
  current_state = STATE_ACTIVE;
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
  portENTER_CRITICAL(&encoder_mux);
  int32_t delta = encoder_count;
  encoder_count = 0;
  portEXIT_CRITICAL(&encoder_mux);
  if (delta != 0) {
    // TODO: Update fan speed by delta * 100 RPM
    Serial.print("Encoder: ");
    Serial.println(delta);
  }

  // Handle encoder button
  if (encoder_button_pressed) {
    encoder_button_pressed = false;
    // TODO: Trigger standby/shutdown menu
    Serial.println("Button pressed");  }

  // Refresh status box once a second
  static unsigned long last_status_update = 0;
  if (millis() - last_status_update >= 1000) {
    last_status_update = millis();
    update_status_box();
  }

  // LVGL tick
  lv_tick_inc(5);
  lv_task_handler();

  delay(5);
}

