#include "webserver.h"
#include "config.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>

// Port comes from config, so the server is created after config loads
static AsyncWebServer *server = nullptr;

void init_webserver() {
  server = new AsyncWebServer(config.webserver.port);

  // Serve only the UI page — /config.json in SPIFFS holds credentials
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!SPIFFS.exists("/index.html")) {
      request->send(500, "text/plain", "index.html missing from SPIFFS (run: pio run -t uploadfs)");
      return;
    }
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server->onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
  });

  server->begin();
  Serial.printf("[WEB] Serving on port %u\n", config.webserver.port);
}
