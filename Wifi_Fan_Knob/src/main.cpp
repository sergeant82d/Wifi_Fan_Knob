#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>
#include <Adafruit_EMC2101.h>
#include "lv_conf.h"

#include "config.h"  // Add near top with other includes

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
#define KEEP_ALIVE_PIN 2
#define POWER_LIGHT_PIN 40

// Test I/O (available for future use)
#define TEST_PIN_1 4
#define TEST_PIN_2 12

// ============================================================================
// DISPLAY SETUP (LGFX + LVGL)
// ============================================================================

#define LGFX_USE_V1
#include <LGFX_AUTODETECT.hpp>

// LovyanGFX auto-detects and creates 'gfx' instance
// (defined by LGFX_AUTODETECT.hpp, no need to redefine)


LGFX gfx;

// ============================================================================
// LVGL BUFFER & DISPLAY CALLBACK
// ============================================================================

static const uint32_t screenWidth = 240;
static const uint32_t screenHeight = 240;
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

      int32_t encoder_count = 0;
volatile bool encoder_button_pressed = false;

void IRAM_ATTR encoder_isr() {
  static uint8_t last_state = 0;
  uint8_t a = digitalRead(ENCODER_A_PIN);
  uint8_t b = digitalRead(ENCODER_B_PIN);
  uint8_t current_state = (a << 1) | b;

  // Simple Gray code decoding
  if ((last_state == 0 && current_state == 1) || 
      (last_state == 2 && current_state == 3) ||
      (last_state == 3 && current_state == 2) ||
      (last_state == 1 && current_state == 0)) {
    encoder_count++;  // Remove volatile cast
  } else {
    encoder_count--;  // Remove volatile cast
  }
  last_state = current_state;
}

void IRAM_ATTR button_isr() {
  encoder_button_pressed = !digitalRead(ENCODER_SW_PIN);
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
unsigned long last_ntp_sync = 0;
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
  WiFi.mode(WIFI_AP);
  
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

void sync_time_ntp() {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 24 * 3600 && attempts < 20) {
    delay(500);
    now = time(nullptr);
    attempts++;
  }
  
  if (now > 24 * 3600) {
    Serial.print("Time synced: ");
    Serial.println(ctime(&now));
    last_ntp_sync = millis();
  } else {
    Serial.println("NTP sync failed");
  }
}

// ============================================================================
// SOFT POWER LATCH (KEEP_ALIVE)
// ============================================================================

void init_power_latch() {
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
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== PWM Fan Controller Startup ===\n");

  // GPIO Setup
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(POWER_LIGHT_PIN, OUTPUT);
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(POWER_LIGHT_PIN, HIGH);
  digitalWrite(SCREEN_BACKLIGHT_PIN, HIGH);

  // Soft Power Latch
  init_power_latch();

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

  // I2C & EMC2101
  Serial.println("Initializing I2C and fan controller...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!init_fan_controller()) {
    Serial.println("WARNING: Fan controller not responding");
  }

  // Encoder Input
  Serial.println("Initializing encoder...");
  attachInterrupt(ENCODER_A_PIN, encoder_isr, CHANGE);
  attachInterrupt(ENCODER_SW_PIN, button_isr, CHANGE);

// ============================================================================
// CONFIGURATION & SPIFFS
// ============================================================================

Serial.println("Initializing configuration system...");
initConfig();  // Load config from SPIFFS (or set defaults)

// Update device info with actual chip ID (optional, for logging)
snprintf(config.chipId, sizeof(config.chipId), "%06X", (uint32_t)(ESP.getEfuseMac() >> 24));

if (config.advanced.debugMode) {
  Serial.println("[MAIN] Debug mode enabled");
}


  // UI Setup
  // TODO: Create main screen UI (LVGL screens)

  Serial.println("=== Boot Complete ===\n");
  current_state = STATE_ACTIVE;
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Handle periodic NTP sync
  if (millis() - last_ntp_sync > NTP_SYNC_INTERVAL) {
    sync_time_ntp();
  }

  // Handle encoder rotation
  if (encoder_count != 0) {
    int32_t delta = encoder_count;
    encoder_count = 0;
    // TODO: Update fan speed by delta * 100 RPM
    Serial.print("Encoder: ");
    Serial.println(delta);
  }

  // Handle encoder button
  if (encoder_button_pressed) {
    encoder_button_pressed = false;
    // TODO: Trigger standby/shutdown menu
    Serial.println("Button pressed");
  }

  // LVGL tick
  lv_tick_inc(5);
  lv_task_handler();

  delay(5);
}

