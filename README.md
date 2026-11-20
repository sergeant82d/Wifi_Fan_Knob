# WiFi Fan Knob

https://github.com/user-attachments/assets/addfebd7-e481-4c5c-bfac-5fc20ed92344


ESP32-S3 fan controller for a solder fume extractor, built on the Elecrow CrowPanel 1.28" rotary display. It has WiFi setup, a web page, OTA updates and Home Assistant integration (MQTT).

## Documentation

- [Project history](docs/PROJECT_HISTORY.md): features, pins, build notes, decisions and plans
- [To-do list](docs/TODO.md)
- [Display guide](docs/DISPLAY_GUIDE.md): how to change the LCD's colours, fonts, buttons and pages
- [Status reports](docs/Status_Reports/): end-of-day reports
- [Configuration](docs/SPIFFS_CONFIG_SCHEMA.md): settings structure
- [MQTT schema](docs/MQTT_SCHEMA.md): original Home Assistant design (the project guide has the current entities)

## Quick Start

1. Clone the repo.
2. Open `Wifi_Fan_Knob.code-workspace` in VS Code with PlatformIO.
3. Connect the board by USB.
4. Run `pio run --target upload`.

## License

The code is MIT - see [LICENSE](LICENSE). Not covered by it:

- `lib/Adafruit_EMC2101` keeps Adafruit's license (`lib/Adafruit_EMC2101/license.txt`); libraries pulled in by PlatformIO keep theirs.
- The eye artwork: the Uncanny Eyes styles in `include/eyes` come from Adafruit's Uncanny Eyes project, and the photo eyes (Dragon 2 to 11, `assets/`) are made from purchased dragon-eye sheets by an artist. Neither is licensed for reuse here.
