#pragma once

#include "wled.h"

#include <WiFiClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*
  HomeWizard P1 Color usermod for WLED

  HomeWizard API v1 endpoint:
    http://<P1_IP>/api/v1/data

  Uses:
    active_power_w

  Convention:
    active_power_w > 0  = importing / consuming from grid
    active_power_w < 0  = exporting / feeding into grid

  Colors:
    import  = purple
    near 0  = white
    export  = green
*/

class HomeWizardP1Color : public Usermod {
private:
  static const char _name[];

  bool enabled = true;

  // Set this in WLED Usermod Settings after flashing
  String host = "192.168.1.100";

  // Polling interval in ms. 2000 ms is a good balance.
  uint32_t updateIntervalMs = 2000;

  // Anything between -deadbandW and +deadbandW becomes white.
  int deadbandW = 75;

  // At this absolute wattage or above, the color is fully purple/green.
  int fullScaleW = 2500;

  // Brightness used when the usermod updates the color.
  // Set to 0 if you do not want the usermod to touch brightness.
  uint8_t targetBrightness = 160;

  // Optional: force static effect so the color is clearly visible.
  bool forceStaticEffect = true;

  uint32_t lastUpdate = 0;
  float lastPowerW = 0;
  bool lastFetchOk = false;
  uint16_t lastHttpCode = 0;

  uint8_t lastR = 255;
  uint8_t lastG = 255;
  uint8_t lastB = 255;

  uint8_t blendChannel(uint8_t from, uint8_t to, float t) {
    t = constrain(t, 0.0f, 1.0f);
    return (uint8_t)((float)from + ((float)to - (float)from) * t);
  }

  void powerToColor(float powerW, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (abs(powerW) <= deadbandW) {
      r = 255;
      g = 255;
      b = 255;
      return;
    }

    float t = (abs(powerW) - deadbandW) / (float)max(1, fullScaleW - deadbandW);
    t = constrain(t, 0.0f, 1.0f);

    if (powerW > 0) {
      // Import: white -> purple
      // Purple target: RGB(160, 0, 255)
      r = blendChannel(255, 160, t);
      g = blendChannel(255, 0,   t);
      b = blendChannel(255, 255, t);
    } else {
      // Export: white -> green
      // Green target: RGB(0, 255, 0)
      r = blendChannel(255, 0,   t);
      g = blendChannel(255, 255, t);
      b = blendChannel(255, 0,   t);
    }
  }

  void applyColor(uint8_t r, uint8_t g, uint8_t b) {
    bool changed = (r != lastR || g != lastG || b != lastB);

    if (!changed) return;

    lastR = r;
    lastG = g;
    lastB = b;

    // Set primary color
    col[0] = r;
    col[1] = g;
    col[2] = b;
    col[3] = 0;

    if (targetBrightness > 0) {
      bri = targetBrightness;
    }

    if (forceStaticEffect) {
      effectCurrent = FX_MODE_STATIC;
    }

    colorUpdated(CALL_MODE_DIRECT_CHANGE);
  }

  bool fetchPower(float &powerW) {
    if (host.length() == 0) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClient client;
    HTTPClient http;

    String url = "http://" + host + "/api/v1/data";

    if (!http.begin(client, url)) {
      lastHttpCode = 0;
      return false;
    }

    http.setTimeout(1500);

    int httpCode = http.GET();
    lastHttpCode = httpCode;

    if (httpCode != HTTP_CODE_OK) {
      http.end();
      return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      return false;
    }

    if (!doc.containsKey("active_power_w")) {
      return false;
    }

    powerW = doc["active_power_w"].as<float>();
    return true;
  }

public:
  void setup() override {
    lastUpdate = millis();
  }

  void loop() override {
    if (!enabled) return;
    if (millis() - lastUpdate < updateIntervalMs) return;

    lastUpdate = millis();

    float powerW = 0;
    bool ok = fetchPower(powerW);

    lastFetchOk = ok;

    if (!ok) {
      return;
    }

    lastPowerW = powerW;

    uint8_t r, g, b;
    powerToColor(powerW, r, g, b);
    applyColor(r, g, b);
  }

  void addToJsonInfo(JsonObject &root) override {
    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray info = user.createNestedArray(FPSTR(_name));

    if (!enabled) {
      info.add("disabled");
      return;
    }

    if (lastFetchOk) {
      String s = String(lastPowerW, 0) + " W";
      if (lastPowerW > deadbandW) s += " import";
      else if (lastPowerW < -deadbandW) s += " export";
      else s += " near zero";
      info.add(s);
    } else {
      String s = "fetch failed";
      if (lastHttpCode > 0) {
        s += ", HTTP ";
        s += String(lastHttpCode);
      }
      info.add(s);
    }
  }

  void addToConfig(JsonObject &root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));

    top["enabled"] = enabled;
    top["host"] = host;
    top["updateIntervalMs"] = updateIntervalMs;
    top["deadbandW"] = deadbandW;
    top["fullScaleW"] = fullScaleW;
    top["targetBrightness"] = targetBrightness;
    top["forceStaticEffect"] = forceStaticEffect;
  }

  bool readFromConfig(JsonObject &root) override {
    JsonObject top = root[FPSTR(_name)];

    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top["enabled"], enabled, true);
    configComplete &= getJsonValue(top["host"], host, "192.168.1.100");
    configComplete &= getJsonValue(top["updateIntervalMs"], updateIntervalMs, 2000);
    configComplete &= getJsonValue(top["deadbandW"], deadbandW, 75);
    configComplete &= getJsonValue(top["fullScaleW"], fullScaleW, 2500);
    configComplete &= getJsonValue(top["targetBrightness"], targetBrightness, 160);
    configComplete &= getJsonValue(top["forceStaticEffect"], forceStaticEffect, true);

    updateIntervalMs = max<uint32_t>(500, updateIntervalMs);
    deadbandW = max(0, deadbandW);
    fullScaleW = max(deadbandW + 1, fullScaleW);

    return configComplete;
  }
};

const char HomeWizardP1Color::_name[] PROGMEM = "HomeWizardP1Color";