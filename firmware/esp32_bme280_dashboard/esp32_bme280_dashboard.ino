#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include "secrets.h"
//include your WiFi and ThingSpeak credentials in this file with this format:

// #pragma once
// #define WIFI_SSID "your_wifi_ssid"
// #define WIFI_PASSWORD "your_wifi_password"
// #define THINGSPEAK_API_KEY "your_thingspeak_api_key"

Adafruit_BME280 bme;
WebServer server(80);

float lastTemperature = NAN;
float lastHumidity = NAN;
float lastPressure = NAN;
float minTemperature = NAN;
float maxTemperature = NAN;
float minHumidity = NAN;
float maxHumidity = NAN;
float minPressure = NAN;
float maxPressure = NAN;
unsigned long lastMeasurementMs = 0;
unsigned long lastUploadMs = 0;
const unsigned long measurementIntervalMs = 60UL * 60UL * 1000UL;

void readBme280() {
  lastTemperature = bme.readTemperature();
  lastHumidity = bme.readHumidity();
  lastPressure = bme.readPressure();
  lastMeasurementMs = millis();
  if (isnan(minTemperature) || lastTemperature < minTemperature) minTemperature = lastTemperature;
  if (isnan(maxTemperature) || lastTemperature > maxTemperature) maxTemperature = lastTemperature;
  if (isnan(minHumidity) || lastHumidity < minHumidity) minHumidity = lastHumidity;
  if (isnan(maxHumidity) || lastHumidity > maxHumidity) maxHumidity = lastHumidity;
  if (isnan(minPressure) || lastPressure < minPressure) minPressure = lastPressure;
  if (isnan(maxPressure) || lastPressure > maxPressure) maxPressure = lastPressure;
  Serial.printf("Sensor read -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n", lastTemperature, lastHumidity, lastPressure);
}

void sendToThingSpeak(float t, float h, float p) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping ThingSpeak upload");
    return;
  }

  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=")
             + THINGSPEAK_API_KEY
             + "&field1=" + String(t, 2)
             + "&field2=" + String(p, 2)
             + "&field3=" + String(h, 2);

  http.begin(url);
  Serial.println("ThingSpeak upload started");
  int code = http.GET();
  String payload = http.getString();
  Serial.printf("ThingSpeak HTTP %d, response: %s\n", code, payload.c_str());
  http.end();
  if (code > 0) {
    lastUploadMs = millis();
  }
}

String ageText(unsigned long msAgo) {
  unsigned long seconds = msAgo / 1000UL;
  if (seconds < 60) return String(seconds) + " s ago";
  unsigned long minutes = seconds / 60UL;
  if (minutes < 60) return String(minutes) + " min ago";
  unsigned long hours = minutes / 60UL;
  return String(hours) + " h ago";
}

String countdownText(unsigned long msUntil) {
  unsigned long seconds = msUntil / 1000UL;
  if (seconds < 60) return String(seconds) + " s";
  unsigned long minutes = seconds / 60UL;
  if (minutes < 60) return String(minutes) + " min";
  unsigned long hours = minutes / 60UL;
  return String(hours) + " h";
}

void handleMeasure() {
  readBme280();
  Serial.println("Measure now pressed -> local refresh only");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Measured");
}

void handleRefresh() {
  readBme280();
  Serial.println("Manual refresh pressed");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Refreshed");
}

void handleData() {
  String trend = "steady";
  String trendText = "\u2192 steady";
  String trendClass = "flat";
  if (!isnan(lastTemperature) && !isnan(minTemperature) && !isnan(maxTemperature)) {
    if (lastTemperature >= maxTemperature) {
      trend = "warming";
      trendText = "\u2191 warming";
      trendClass = "up";
    } else if (lastTemperature <= minTemperature) {
      trend = "cooling";
      trendText = "\u2193 cooling";
      trendClass = "down";
    }
  }

  String comfort = "N/A";
  if (!isnan(lastTemperature) && !isnan(lastHumidity)) {
    if (lastTemperature >= 20.0f && lastTemperature <= 26.0f && lastHumidity >= 40.0f && lastHumidity <= 60.0f) comfort = "Good";
    else if (lastTemperature > 26.0f) comfort = "Warm";
    else if (lastHumidity < 35.0f) comfort = "Dry";
    else comfort = "OK";
  }

  unsigned long now = millis();
  unsigned long lastMeasurementAgo = lastMeasurementMs ? (now - lastMeasurementMs) : 0;
  String lastMeasurement = ageText(lastMeasurementAgo);
  String lastUpload = lastUploadMs ? ageText(now - lastUploadMs) : String("never");
  unsigned long remaining = lastUploadMs ? ((now - lastUploadMs) >= measurementIntervalMs ? 0 : (measurementIntervalMs - (now - lastUploadMs))) : measurementIntervalMs;
  String nextUpload = countdownText(remaining);

  String json = "{";
  json += "\"temp\":" + String(lastTemperature, 1) + ",";
  json += "\"humid\":" + String(lastHumidity, 1) + ",";
  json += "\"press\":" + String(lastPressure / 1000.0f, 1) + ",";
  json += "\"tempMin\":" + String(minTemperature, 1) + ",";
  json += "\"tempMax\":" + String(maxTemperature, 1) + ",";
  json += "\"humidMin\":" + String(minHumidity, 1) + ",";
  json += "\"humidMax\":" + String(maxHumidity, 1) + ",";
  json += "\"pressMin\":" + String(minPressure / 1000.0f, 1) + ",";
  json += "\"pressMax\":" + String(maxPressure / 1000.0f, 1) + ",";
  json += "\"tempAngle\":" + String((int)(-90 + constrain((lastTemperature + 10) / 60.0f, 0.0f, 1.0f) * 180)) + ",";
  json += "\"humidAngle\":" + String((int)(-90 + constrain(lastHumidity / 100.0f, 0.0f, 1.0f) * 180)) + ",";
  json += "\"pressAngle\":" + String((int)(-75 + constrain((lastPressure / 1000.0f - 90.0f) / 14.0f, 0.0f, 1.0f) * 150)) + ",";
  json += "\"lastMeasurement\":\"" + lastMeasurement + "\",";
  json += "\"lastUpload\":\"" + lastUpload + "\",";
  json += "\"nextUpload\":\"" + nextUpload + "\",";
  json += "\"trend\":\"" + trend + "\",";
  json += "\"trendText\":\"" + trendText + "\",";
  json += "\"trendClass\":\"" + trendClass + "\",";
  json += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\",";
  json += "\"comfort\":\"" + comfort + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  if (!bme.begin(0x76)) {
    Serial.println("BME280 not found");
    while (true) delay(1000);
  }

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  if (MDNS.begin("esp32-home")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS started: http://esp32-home.local");
  } else {
    Serial.println("mDNS failed to start");
  }

  readBme280();
  sendToThingSpeak(lastTemperature, lastHumidity, lastPressure);

  // Serve the dashboard UI straight from LittleFS (data/ folder contents).
  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/app.js", LittleFS, "/app.js");

  server.on("/measure", handleMeasure);
  server.on("/refresh", handleRefresh);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started");

  Serial.println();
  Serial.print("Open: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();

  if (millis() - lastMeasurementMs >= measurementIntervalMs) {
    readBme280();
    sendToThingSpeak(lastTemperature, lastHumidity, lastPressure);
  }
}
