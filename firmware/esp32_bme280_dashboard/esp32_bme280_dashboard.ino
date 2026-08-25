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
// #define NODE_ADDRESS "esp32-bedroom.local"   // hostname/IP of the "bedroom" node

Adafruit_BME280 bme;
WebServer server(80);

// --- Multi-device state -----------------------------------------------
// devices[DEVICE_LIVING] is the hub's own local BME280 sensor.
// devices[DEVICE_BEDROOM] is populated remotely by the second ESP32 node,
// either via its own periodic POST /ingest, or refreshed on-demand when
// the hub asks it directly during a Measure Now (see fetchNodeMeasure()).
struct DeviceState {
  const char* id;
  const char* name;
  float temp = NAN;
  float humid = NAN;
  float press = NAN;
  float tempMin = NAN;
  float tempMax = NAN;
  float humidMin = NAN;
  float humidMax = NAN;
  float pressMin = NAN;
  float pressMax = NAN;
  unsigned long lastSeenMs = 0;
  bool everSeen = false;
};

const int DEVICE_LIVING = 0;
const int DEVICE_BEDROOM = 1;
const int DEVICE_COUNT = 2;

DeviceState devices[DEVICE_COUNT] = {
  { "living", "Хол" },
  { "bedroom", "Спалня" }
};

// A remote device is considered "offline" if we haven't heard from it in
// more than this many ms (2x its expected hourly reporting interval).
const unsigned long remoteOfflineTimeoutMs = 2UL * 60UL * 60UL * 1000UL;

unsigned long lastMeasurementMs = 0;
unsigned long lastUploadMs = 0;
const unsigned long measurementIntervalMs = 60UL * 60UL * 1000UL;

void updateDeviceReading(DeviceState& d, float t, float h, float p) {
  d.temp = t;
  d.humid = h;
  d.press = p;
  d.lastSeenMs = millis();
  d.everSeen = true;
  if (isnan(d.tempMin) || t < d.tempMin) d.tempMin = t;
  if (isnan(d.tempMax) || t > d.tempMax) d.tempMax = t;
  if (isnan(d.humidMin) || h < d.humidMin) d.humidMin = h;
  if (isnan(d.humidMax) || h > d.humidMax) d.humidMax = h;
  if (isnan(d.pressMin) || p < d.pressMin) d.pressMin = p;
  if (isnan(d.pressMax) || p > d.pressMax) d.pressMax = p;
}

void readBme280() {
  float t = bme.readTemperature();
  float h = bme.readHumidity();
  float p = bme.readPressure();
  lastMeasurementMs = millis();
  updateDeviceReading(devices[DEVICE_LIVING], t, h, p);
  Serial.printf("Sensor read -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n", t, h, p);
}

// Minimal hand-rolled JSON value extraction - good enough since we fully
// control the payload shape on both ends (no external ArduinoJson dep).
float extractJsonFloat(const String& json, const char* key) {
  String pattern = String("\"") + key + "\":";
  int idx = json.indexOf(pattern);
  if (idx < 0) return NAN;
  idx += pattern.length();
  return json.substring(idx).toFloat();
}

String extractJsonString(const String& json, const char* key) {
  String pattern = String("\"") + key + "\":\"";
  int idx = json.indexOf(pattern);
  if (idx < 0) return "";
  idx += pattern.length();
  int end = json.indexOf("\"", idx);
  if (end < 0) return "";
  return json.substring(idx, end);
}

DeviceState* findDeviceById(const String& id) {
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (id == devices[i].id) return &devices[i];
  }
  return nullptr;
}

// Best-effort: ask the bedroom node to measure right now and update our
// cached copy of its reading from the response. Used by Measure Now so
// both rooms refresh together. If the node is unreachable/offline, we
// simply keep whatever we already know about it - never blocks the UI.
bool fetchNodeMeasure() {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String url = String("http://") + NODE_ADDRESS + "/measure";
  http.begin(url);
  http.setTimeout(4000);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("Node measure request failed, HTTP %d\n", code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  float t = extractJsonFloat(payload, "temp");
  float h = extractJsonFloat(payload, "humid");
  float p = extractJsonFloat(payload, "press") * 1000.0f; // node sends kPa, we store Pa
  if (isnan(t) || isnan(h) || isnan(p)) {
    Serial.println("Node measure response could not be parsed");
    return false;
  }

  updateDeviceReading(devices[DEVICE_BEDROOM], t, h, p);
  Serial.printf("Node measure -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n", t, h, p);
  return true;
}

void sendToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected, skipping ThingSpeak upload");
    return;
  }

  DeviceState& living = devices[DEVICE_LIVING];
  DeviceState& bedroom = devices[DEVICE_BEDROOM];

  HTTPClient http;
  String url = String("http://api.thingspeak.com/update?api_key=")
             + THINGSPEAK_API_KEY
             + "&field1=" + String(living.temp, 2)
             + "&field2=" + String(living.press, 2)
             + "&field3=" + String(living.humid, 2);
  if (bedroom.everSeen) {
    url += "&field4=" + String(bedroom.temp, 2)
         + "&field5=" + String(bedroom.press, 2)
         + "&field6=" + String(bedroom.humid, 2);
  }

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
  fetchNodeMeasure();
  Serial.println("Measure now pressed -> refreshed both rooms (local refresh only)");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Measured");
}

void handleRefresh() {
  readBme280();
  Serial.println("Manual refresh pressed (hub room only)");
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "Refreshed");
}

// Receives a pushed reading from a node (e.g. the bedroom ESP32).
// Expected body: {"id":"bedroom","name":"Спалня","temp":..,"humid":..,"press":..}
void handleIngest() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing body");
    return;
  }
  String body = server.arg("plain");
  String id = extractJsonString(body, "id");
  DeviceState* d = findDeviceById(id);
  if (!d) {
    Serial.println("Ingest from unknown device id: " + id);
    server.send(404, "text/plain", "Unknown device id");
    return;
  }

  float t = extractJsonFloat(body, "temp");
  float h = extractJsonFloat(body, "humid");
  float p = extractJsonFloat(body, "press");
  if (isnan(t) || isnan(h) || isnan(p)) {
    server.send(400, "text/plain", "Invalid payload");
    return;
  }

  updateDeviceReading(*d, t, h, p);
  Serial.printf("Ingest from '%s' -> T: %.2f C, H: %.2f %%, P: %.0f Pa\n", d->id, t, h, p);
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

// Builds the JSON object for a single device, matching the shape the
// website/desktop app expect (one entry per room in the /data array).
String deviceJson(DeviceState& d, bool isLocal) {
  String trend = "steady";
  String trendText = "\u2192 steady";
  String trendClass = "flat";
  if (!isnan(d.temp) && !isnan(d.tempMin) && !isnan(d.tempMax)) {
    if (d.temp >= d.tempMax) {
      trend = "warming";
      trendText = "\u2191 warming";
      trendClass = "up";
    } else if (d.temp <= d.tempMin) {
      trend = "cooling";
      trendText = "\u2193 cooling";
      trendClass = "down";
    }
  }

  String comfort = "N/A";
  if (!isnan(d.temp) && !isnan(d.humid)) {
    if (d.temp >= 20.0f && d.temp <= 26.0f && d.humid >= 40.0f && d.humid <= 60.0f) comfort = "Good";
    else if (d.temp > 26.0f) comfort = "Warm";
    else if (d.humid < 35.0f) comfort = "Dry";
    else comfort = "OK";
  }

  unsigned long now = millis();
  bool online = isLocal ? true : (d.everSeen && (now - d.lastSeenMs) < remoteOfflineTimeoutMs);
  String statusText = online ? "Connected" : "Disconnected";

  String lastMeasurement = d.everSeen ? ageText(now - d.lastSeenMs) : String("never");
  String lastUpload = lastUploadMs ? ageText(now - lastUploadMs) : String("never");
  unsigned long remaining = lastUploadMs ? ((now - lastUploadMs) >= measurementIntervalMs ? 0 : (measurementIntervalMs - (now - lastUploadMs))) : measurementIntervalMs;
  String nextUpload = countdownText(remaining);

  String json = "{";
  json += "\"id\":\"" + String(d.id) + "\",";
  json += "\"name\":\"" + String(d.name) + "\",";
  json += "\"online\":" + String(online ? "true" : "false") + ",";
  json += "\"temp\":" + String(d.temp, 1) + ",";
  json += "\"humid\":" + String(d.humid, 1) + ",";
  json += "\"press\":" + String(d.press / 1000.0f, 1) + ",";
  json += "\"tempMin\":" + String(d.tempMin, 1) + ",";
  json += "\"tempMax\":" + String(d.tempMax, 1) + ",";
  json += "\"humidMin\":" + String(d.humidMin, 1) + ",";
  json += "\"humidMax\":" + String(d.humidMax, 1) + ",";
  json += "\"pressMin\":" + String(d.pressMin / 1000.0f, 1) + ",";
  json += "\"pressMax\":" + String(d.pressMax / 1000.0f, 1) + ",";
  json += "\"tempAngle\":" + String((int)(-90 + constrain((d.temp + 10) / 60.0f, 0.0f, 1.0f) * 180)) + ",";
  json += "\"humidAngle\":" + String((int)(-90 + constrain(d.humid / 100.0f, 0.0f, 1.0f) * 180)) + ",";
  json += "\"pressAngle\":" + String((int)(-75 + constrain((d.press / 1000.0f - 90.0f) / 14.0f, 0.0f, 1.0f) * 150)) + ",";
  json += "\"lastMeasurement\":\"" + lastMeasurement + "\",";
  json += "\"lastUpload\":\"" + lastUpload + "\",";
  json += "\"nextUpload\":\"" + nextUpload + "\",";
  json += "\"trend\":\"" + trend + "\",";
  json += "\"trendText\":\"" + trendText + "\",";
  json += "\"trendClass\":\"" + trendClass + "\",";
  json += "\"wifi\":\"" + statusText + "\",";
  json += "\"comfort\":\"" + comfort + "\"";
  json += "}";
  return json;
}

void handleData() {
  String json = "[";
  for (int i = 0; i < DEVICE_COUNT; i++) {
    if (i > 0) json += ",";
    json += deviceJson(devices[i], i == DEVICE_LIVING);
  }
  json += "]";
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
  sendToThingSpeak();

  // Serve the dashboard UI straight from LittleFS (data/ folder contents).
  server.serveStatic("/", LittleFS, "/index.html");
  server.serveStatic("/index.html", LittleFS, "/index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");
  server.serveStatic("/app.js", LittleFS, "/app.js");

  server.on("/measure", handleMeasure);
  server.on("/refresh", handleRefresh);
  server.on("/data", handleData);
  server.on("/ingest", HTTP_POST, handleIngest);
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
    sendToThingSpeak();
  }
}
