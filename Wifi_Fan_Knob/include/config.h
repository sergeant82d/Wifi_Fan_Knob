#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <ArduinoJson.h>

// ============================================================================
// CONFIGURATION STRUCT
// ============================================================================

typedef struct {
  // Device Info (read-only, auto-set)
  char name[32];                    // "WiFi Fan Knob"
  char chipId[13];                  // ESP32 MAC address (hex string)
  char firmwareVersion[16];         // "v1.0.0"
  char buildDate[16];               // "2026-09-22"

  // WiFi Settings
  struct {
    char ssid[33];                  // 0-32 chars
    char password[64];              // 0-63 chars
    bool saveCredentials;           // Auto-reconnect on boot
  } wifi;

  // Webserver Settings
  struct {
    uint16_t port;                  // Default 8080
    char username[32];              // Empty = no auth
    char password[32];              // Empty = no auth
  } webserver;

  // MQTT Settings
  struct {
    bool enabled;                   // Enable MQTT
    char broker[64];                // IP or hostname
    uint16_t port;                  // Default 1883
    char username[32];              // Empty = no auth
    char password[32];              // Empty = no auth
    bool discoveryEnabled;          // Home Assistant auto-discovery
    char topicPrefix[32];           // Default "wifi_fan_knob"
  } mqtt;

  // NTP Settings
  struct {
    char server[64];                // pool.ntp.org
    uint32_t syncInterval;          // Milliseconds (3600000 = 60 min)
  } ntp;

  // Display Settings
  struct {
    char timezone[8];               // "EST", "CST", "MST", "PST", "UTC"
    char timeFormat[4];             // "12h" or "24h"
    uint8_t brightness;             // 0-100%
    uint16_t screenTimeout;         // Minutes (0 = never)
  } display;

  // Fan Settings
  struct {
    uint16_t minRpm;                // 0
    uint16_t maxRpm;                // 2500
    uint16_t rpmStep;               // 100 (per knob click)
    
    struct {
      uint8_t minPwm;               // 0-255 (fan start threshold)
      uint8_t maxPwm;               // 0-255 (full speed)
      bool enableFeedback;          // PID tachometer feedback
    } calibration;

    struct {
      uint16_t low;                 // Quick preset: low
      uint16_t medium;              // Quick preset: medium
      uint16_t high;                // Quick preset: high
      uint16_t max;                 // Quick preset: max
    } presets;
  } fan;

  // System Settings
  struct {
    bool deepSleepEnabled;          // Allow deep sleep in standby
    uint16_t standbyTimeout;        // Minutes (0 = disabled)
    bool audioEnabled;              // Speaker audio (sounds, chimes)
    bool autoUpdate;                // Auto check for OTA updates
  } system;

  // Advanced Settings
  struct {
    bool debugMode;                 // Serial debug output
    char logLevel[8];               // "error", "warn", "info", "debug"
  } advanced;

} Config;

// ============================================================================
// GLOBAL CONFIG INSTANCE
// ============================================================================

extern Config config;

// ============================================================================
// FUNCTION DECLARATIONS
// ============================================================================

// Core config management
void initConfig();                  // Initialize on boot (load or set defaults)
bool loadConfig();                  // Load from SPIFFS
bool saveConfig();                  // Save to SPIFFS
void setDefaultConfig();            // Reset to factory defaults

// Utilities
bool validateConfig();              // Validate config values
void printConfig();                 // Serial debug dump
String getConfigAsJson();           // Return JSON string of current config

// Config getters/setters (optional, for cleaner access)
void setWiFiCredentials(const char* ssid, const char* password);
void setMqttBroker(const char* broker, uint16_t port);
void setFanCalibration(uint8_t minPwm, uint8_t maxPwm);
void setTimezone(const char* tz);

#endif
