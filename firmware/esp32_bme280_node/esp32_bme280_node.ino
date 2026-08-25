#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <ESPmDNS.h>
#include "secrets.h"
// Copy secrets.h.example to secrets.h and fill in your own values:
// WIFI_SSID, WIFI_PASSWORD, HUB_ADDRESS, DEVICE_ID, DEVICE_NAME

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
unsigned long lastIngestMs = 0;
const unsigned long measurementIntervalMs = 60UL * 60UL * 1000UL;

void readBme280() {
  lastTemperature = bme.readTemperature();
  lastHumidity = bme.readHumidity();
  lastPressure = bme.readPressure();
  lastMeasurementMs = millis();
  Serial.printf("[%s] Sensor read -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n",
                DEVICE_ID, lastTemperature, lastHumidity, lastPressure);
}

// Sends the latest reading to the hub's /ingest endpoint. Non-blocking to
// the rest of the loop logic - failures are logged and simply retried on
// the next measurement cycle, never block the node's own operation.
bool sendToHub(float t, float h, float p) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping hub upload");
    return false;
  }

  HTTPClient http;
  String url = String("http://") + HUB_ADDRESS + "/ingest";
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(4000);

  String json = "{";
  json += "\"id\":\"" + String(DEVICE_ID) + "\",";
  json += "\"name\":\"" + String(DEVICE_NAME) + "\",";
  json += "\"temp\":" + String(t, 2) + ",";
  json += "\"humid\":" + String(h, 2) + ",";
  json += "\"press\":" + String(p, 2);
  json += "}";

  int code = http.POST(json);
  String payload = http.getString();
  Serial.printf("Hub ingest HTTP %d, response: %s\n", code, payload.c_str());
  http.end();

  if (code == 200) {
    lastIngestMs = millis();
    return true;
  }
  Serial.println("Hub ingest failed, will retry on next measurement cycle");
  return false;
}

void handleMeasure() {
  readBme280();
  // Note: deliberately NOT calling sendToHub() here. The hub already gets
  // this exact reading in the HTTP response below (it calls this endpoint
  // and parses the body directly). If we also tried to POST back to the
  // hub's /ingest from inside this handler, both sides would be doing a
  // blocking HTTP call to each other at the same time (single-threaded
  // WebServer on both ends) - a deadlock that only resolves via timeout
  // (seen as "HTTP -11" on both hub and node). The periodic /ingest push
  // in loop() is enough to keep the hub updated between Measure Now calls.

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
  json += "\"lastMeasurementMs\":" + String(lastMeasurementMs) + ",";
  json += "\"lastIngestMs\":" + String(lastIngestMs);
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

  readBme280();
  sendToHub(lastTemperature, lastHumidity, lastPressure);

  // No web UI here - this node only exposes a small API for the hub.
  server.on("/measure", handleMeasure);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Node HTTP server started");
}

void loop() {
  server.handleClient();

  if (millis() - lastMeasurementMs >= measurementIntervalMs) {
    readBme280();
    sendToHub(lastTemperature, lastHumidity, lastPressure);
  }
}
