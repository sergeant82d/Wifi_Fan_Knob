#ifndef MQTT_H
#define MQTT_H

// MQTT client + Home Assistant discovery, running in its own FreeRTOS task
// (broker connects block, so they must not stall loop()/LVGL).
void mqtt_init();          // After config + chip ID are loaded
bool mqtt_connected();
void mqtt_reconfigure();   // Broker settings changed: reconnect with new settings

#endif
