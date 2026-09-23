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

// Starts the background SNTP client; it re-syncs every NTP_SYNC_INTERVAL
// on its own, so nothing here blocks.
void start_ntp() {
  esp_sntp_set_sync_interval(NTP_SYNC_INTERVAL);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
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

  // GPIO Setup
  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  pinMode(POWER_LIGHT_PIN, OUTPUT);
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  digitalWrite(POWER_LIGHT_PIN, HIGH);
  digitalWrite(SCREEN_BACKLIGHT_PIN, HIGH);

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

  init_wifi();
  if (WiFi.status() == WL_CONNECTED) {
    start_ntp();
  }
  init_webserver();


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

