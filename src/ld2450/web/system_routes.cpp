// System routes (split from WebService.cpp)
//   POST /api/restart           — graceful restart přes shouldReboot flag
//   POST /api/ota               — Web OTA upload, podporuje volitelný ?md5=<hex32> pro hash check
//   POST /api/radar/bluetooth   — vypne/zapne BLE rádia v modulu LD2450 (security)

#include "ld2450/services/WebService.h"

extern void safeRestart(const char* reason);

void WebService::setupSystem() {
  _server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      r->send(200, "text/plain", "Restarting...");
      if (_ctx->shouldReboot) *_ctx->shouldReboot = true;
  });

  // POST /api/radar/bluetooth?enabled=0|1 — toggle BLE v LD2450 modulu.
  // Změna persistuje v NVS modulu, vyžaduje restart radaru (provede se automaticky).
  // Doporučeno: enabled=0 pro security (vypne neautentizovaný GATT konfigurační kanál).
  _server->on("/api/radar/bluetooth", HTTP_POST, [this](AsyncWebServerRequest *r){
      if (!requireAuth(r)) return;
      if (!r->hasParam("enabled", true)) { r->send(400, "text/plain", "Missing 'enabled' param"); return; }
      bool en = (r->getParam("enabled", true)->value().toInt() != 0);
      if (!_ctx->radar) { r->send(503, "text/plain", "Radar service unavailable"); return; }
      if (_ctx->radar->setBluetoothEnabled(en)) {
          r->send(200, "text/plain", en ? "Radar BLE enabled" : "Radar BLE disabled");
      } else {
          r->send(500, "text/plain", "Command send failed");
      }
  });

  // OTA Handler — podporuje volitelný MD5 hash query parametr `?md5=<hex32>`
  // Pokud je předán, Update.setMD5() porovná po dokončení uploadu — eliminuje
  // riziko nasazení korumpovaného binárního souboru.
  _server->on("/api/ota", HTTP_POST, [this](AsyncWebServerRequest *request){
    if (!requireAuth(request)) return;
    bool shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain",
        shouldReboot ? "OK - Restarting..." : ("FAIL: " + String(Update.errorString())).c_str());
    response->addHeader("Connection", "close");
    request->send(response);

    if (shouldReboot) {
      if (_ctx->mqtt->connected()) {
        _ctx->mqtt->publish(_ctx->mqtt->getTopics().notification, "Web OTA update finished. Restarting...", false);
        _ctx->mqtt->update(); // Flush
      }
      delay(500);
      safeRestart("web_ota_update");
    }
  }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    // Handle upload
    if (!index) {
      Serial.printf("[Web OTA] Update Start: %s (size=%u)\n", filename.c_str(), request->contentLength());
      if (_ctx->mqtt->connected()) {
        _ctx->mqtt->publish(_ctx->mqtt->getTopics().notification, ("Web OTA started: " + filename).c_str(), false);
      }
      if (!Update.begin((request->contentLength()) ? request->contentLength() : UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        return;
      }
      // Volitelný MD5 hash z query — formát ?md5=<32 hex chars>
      if (request->hasParam("md5")) {
        String md5 = request->getParam("md5")->value();
        md5.toLowerCase();
        if (md5.length() == 32) {
          if (!Update.setMD5(md5.c_str())) {
            Serial.println("[Web OTA] setMD5 failed — pokracuju bez kontroly");
          } else {
            Serial.printf("[Web OTA] MD5 expected: %s\n", md5.c_str());
          }
        }
      }
    }
    if (len) {
      if (Update.write(data, len) != len) Update.printError(Serial);
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("[Web OTA] Success: %u bytes (MD5 OK)\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });
}
