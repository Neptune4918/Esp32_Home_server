#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <ESPmDNS.h>
#include "secrets.h"
// Copy secrets.h.example to secrets.h and fill in your own values:
// WIFI_SSID, WIFI_PASSWORD, DEVICE_ID, DEVICE_NAME
// (HUB_ADDRESS is currently unused - the node never contacts the hub on
// its own; the hub polls this node's /measure endpoint instead.)

// I2C pins for the BME280. Many ESP32 boards (classic Wroom) are fine with
// Wire.begin() and its default pins (SDA=21, SCL=22), but some boards (e.g.
// ESP32-C3 "Super Mini" style modules) need explicit pins. Override these by
// #define-ing NODE_SDA_PIN / NODE_SCL_PIN in your secrets.h before this file
// picks its defaults.
#ifndef NODE_SDA_PIN
#define NODE_SDA_PIN 6
#endif
#ifndef NODE_SCL_PIN
#define NODE_SCL_PIN 7
#endif

Adafruit_BME280 bme;
WebServer server(80);

float lastTemperature = NAN;
float lastHumidity = NAN;
float lastPressure = NAN;
unsigned long lastMeasurementMs = 0;

void readBme280() {
  lastTemperature = bme.readTemperature();
  lastHumidity = bme.readHumidity();
  lastPressure = bme.readPressure();
  lastMeasurementMs = millis();
  Serial.printf("[%s] Sensor read -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n",
                DEVICE_ID, lastTemperature, lastHumidity, lastPressure);
}

// The node has no timer of its own and never contacts the hub on its own
// initiative - it only measures and responds when the hub asks (GET
// /measure), whether that's the hub's own hourly ThingSpeak cycle or a
// user pressing "Measure now". This keeps timing fully controlled by the
// hub and avoids any node-side push/deadlock concerns entirely.
void handleMeasure() {
  readBme280();

  String json = "{";
  json += "\"id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"temp\":" + String(lastTemperature, 1) + ",";
  json += "\"humid\":" + String(lastHumidity, 1) + ",";
  json += "\"press\":" + String(lastPressure / 1000.0f, 1);
  json += "}";
  server.send(200, "application/json", json);
}

void handleData() {
  String json = "{";
  json += "\"id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"name\":\"" + String(DEVICE_NAME) + "\",";
  json += "\"temp\":" + String(lastTemperature, 1) + ",";
  json += "\"humid\":" + String(lastHumidity, 1) + ",";
  json += "\"press\":" + String(lastPressure / 1000.0f, 1) + ",";
  json += "\"lastMeasurementMs\":" + String(lastMeasurementMs);
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Wire.begin(NODE_SDA_PIN, NODE_SCL_PIN);

  if (!bme.begin(0x76) && !bme.begin(0x77)) {
    Serial.println("BME280 not found");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }
  Serial.println();
  Serial.print("Connected, IP: ");
  Serial.println(WiFi.localIP());

  String hostname = String("esp32-") + DEVICE_ID;
  if (MDNS.begin(hostname.c_str())) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS started: http://" + hostname + ".local");
  } else {
    Serial.println("mDNS failed to start");
  }

  // Take one reading at boot so /data has something valid even before the
  // hub's first poll.
  readBme280();

  // No web UI here - this node only exposes a small API for the hub.
  server.on("/measure", handleMeasure);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Node HTTP server started - waiting for hub requests");
}

void loop() {
  server.handleClient();
}
