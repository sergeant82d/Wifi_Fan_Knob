# WiFi Fan Knob

ESP32-S3 fan controller for a solder fume extractor, built on the Elecrow CrowPanel 1.28" rotary display. It has WiFi setup, a web page, OTA updates and Home Assistant integration (MQTT).

## Documentation

- [Project guide](docs/CLAUDE.md): features, pins, build notes and the to-do list
- [Display guide](docs/DISPLAY_GUIDE.md): how to change the LCD's colours, fonts, buttons and pages
- [Status reports](docs/Status_Reports/): end-of-day reports
- [Configuration](docs/SPIFFS_CONFIG_SCHEMA.md): settings structure
- [MQTT schema](docs/MQTT_SCHEMA.md): original Home Assistant design (the project guide has the current entities)

## Quick Start

1. Clone the repo.
2. Open `Wifi_Fan_Knob.code-workspace` in VS Code with PlatformIO.
3. Connect the board by USB.
4. Run `pio run --target upload`.
