#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
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

String htmlEscape(const String& s) {
  String out;
  out.reserve(s.length());
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

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

String gaugeHtml(const char* label, float value, float minValue, float maxValue, const char* unit, const char* color) {
  float ratio = (value - minValue) / (maxValue - minValue);
  if (ratio < 0) ratio = 0;
  if (ratio > 1) ratio = 1;
  int angle = (int)(-120 + ratio * 240);

  String s;
  s += "<div class='card'>";
  s += "<div class='label'>⬤ " + htmlEscape(label) + "</div>";
  s += "<div class='gauge'>";
  s += "<div class='needle' style='transform:rotate(" + String(angle) + "deg) translateX(-50%);'></div>";
  s += "<div class='center'></div>";
  s += "</div>";
  s += "<div class='value' style='color:" + String(color) + "'>" + String(value, 1) + " " + htmlEscape(unit) + "</div>";
  s += "</div>";
  return s;
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

String statLine(const char* label, float minValue, float maxValue, const char* unit) {
  return String(label) + ": " + String(minValue, 1) + " / " + String(maxValue, 1) + " " + unit;
}

const char* tempColor(float t) {
  if (isnan(t)) return "#cbd5e1";
  if (t < 18.0f) return "#38bdf8";
  if (t < 26.0f) return "#22c55e";
  return "#f59e0b";
}

const char* humidityColor(float h) {
  if (isnan(h)) return "#cbd5e1";
  if (h < 35.0f) return "#f59e0b";
  if (h > 70.0f) return "#38bdf8";
  return "#22c55e";
}

const char* pressureColor(float p) {
  if (isnan(p)) return "#cbd5e1";
  if (p < 99000.0f) return "#f59e0b";
  if (p > 103000.0f) return "#38bdf8";
  return "#22c55e";
}

String buildPage() {
  String page;
  page += F("<!doctype html><html><head><meta charset='utf-8'>");
  page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<meta http-equiv='Cache-Control' content='no-cache, no-store, must-revalidate'>");
  page += F("<meta http-equiv='Pragma' content='no-cache'>");
  page += F("<meta http-equiv='Expires' content='0'>");
  page += F("<title>ESP32 Home Dashboard</title>");
  page += F("<style>");
  page += F(":root{color-scheme:dark}");
  page += F("body{margin:0;font-family:system-ui,Segoe UI,sans-serif;background:radial-gradient(circle at top,#1e293b 0,#0f172a 50%,#020617 100%);color:#e5e7eb}");
  page += F(".wrap{max-width:1000px;margin:0 auto;padding:24px}");
  page += F(".top{display:flex;justify-content:space-between;align-items:center;gap:16px;flex-wrap:wrap;background:rgba(15,23,42,.55);border:1px solid rgba(148,163,184,.16);border-radius:24px;padding:20px 22px;backdrop-filter:blur(10px)}");
  page += F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:18px;margin-top:24px}");
  page += F(".wifi{display:inline-flex;align-items:center;gap:8px;padding:8px 12px;border-radius:999px;background:rgba(59,130,246,.14);border:1px solid rgba(59,130,246,.25);font-size:13px;font-weight:700}");
  page += F(".trend{font-weight:800}");
  page += F(".card{background:linear-gradient(180deg,rgba(15,23,42,.95),rgba(15,23,42,.72));border:1px solid rgba(148,163,184,.18);border-radius:24px;padding:20px;box-shadow:0 16px 50px rgba(0,0,0,.35)}");
  page += F(".label{font-size:13px;opacity:.8;margin-bottom:14px;letter-spacing:.08em;text-transform:uppercase}");
  page += F(".value{font-size:30px;font-weight:800;margin-top:14px;text-align:center;letter-spacing:.01em}");
  page += F(".sub{margin-top:8px;text-align:center;opacity:.7;font-size:13px}");
  page += F(".gauge{position:relative;width:180px;height:90px;margin:0 auto;border-radius:180px 180px 0 0;background:conic-gradient(from 180deg,#22c55e 0 33%,#eab308 33% 66%,#ef4444 66% 100%);overflow:hidden;box-shadow:inset 0 0 0 1px rgba(255,255,255,.06)}");
  page += F(".gauge:after{content:'';position:absolute;left:18px;right:18px;bottom:0;height:72px;background:radial-gradient(circle at center,#0f172a,#030712);border-radius:180px 180px 0 0}");
  page += F(".needle{position:absolute;left:50%;bottom:0;width:4px;height:78px;background:linear-gradient(180deg,#f8fafc,#38bdf8);transform-origin:bottom center;border-radius:99px;z-index:2;box-shadow:0 0 12px rgba(56,189,248,.55);transition:transform 1.2s cubic-bezier(.22,1,.36,1);margin-left:-2px}");
  page += F(".card{animation:fadeUp .45s ease both}");
  page += F("@keyframes fadeUp{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:translateY(0)}}");
  page += F(".center{position:absolute;left:50%;bottom:-6px;width:18px;height:18px;background:#f8fafc;border-radius:50%;transform:translateX(-50%);z-index:3}");
  page += F(".actions{margin-top:22px;display:flex;gap:12px;flex-wrap:wrap}");
  page += F("button,a.btn{border:0;border-radius:14px;padding:14px 18px;font-weight:700;text-decoration:none;cursor:pointer;transition:transform .15s ease,opacity .15s ease}");
  page += F("button:hover,a.btn:hover{transform:translateY(-1px);opacity:.95}");
  page += F(".primary{background:linear-gradient(135deg,#38bdf8,#2563eb);color:#eff6ff;box-shadow:0 8px 24px rgba(37,99,235,.32)}");
  page += F(".ghost{background:rgba(31,41,55,.9);color:#e5e7eb;border:1px solid rgba(148,163,184,.15)}");
  page += F(".meta{margin-top:14px;opacity:.72;font-size:14px}");
  page += F(".summary{margin-top:14px;padding:14px 16px;border-radius:16px;background:rgba(34,197,94,.12);border:1px solid rgba(34,197,94,.22);font-weight:700}");
  page += F(".status{margin-top:10px;display:grid;gap:8px}");
  page += F(".stats{margin-top:14px;display:grid;gap:8px;font-size:13px;opacity:.78}");
  page += F(".pill{display:inline-block;padding:6px 10px;border-radius:999px;background:rgba(56,189,248,.14);border:1px solid rgba(56,189,248,.25);font-size:12px;letter-spacing:.02em}");
  page += F(".live{opacity:.9}");
  page += F(".updated{animation:pulse .5s ease}");
  page += F("@keyframes pulse{0%{opacity:.6;transform:scale(.98)}100%{opacity:1;transform:scale(1)}}");
  page += F(".card,.top,.summary,.status,.stats{will-change:transform,opacity}");
  page += F("</style><script>");
  page += F("async function refreshData(){try{const r=await fetch('/data',{cache:'no-store'});const d=await r.json();const set=(sel,val)=>{const e=document.querySelector(sel);if(e)e.textContent=val;};set('#temp',d.temp.toFixed(1)+' °C');set('#humid',d.humid.toFixed(1)+' %');set('#press',d.press.toFixed(0)+' Pa');set('#lastMeasurement',d.lastMeasurement);set('#lastUpload',d.lastUpload);set('#nextUpload',d.nextUpload);set('#trend',d.trend);set('#wifi',d.wifi);set('#comfort',d.comfort);document.querySelectorAll('.gauge').forEach((g,i)=>{const a=[d.tempAngle,d.humidAngle,d.pressAngle][i];const n=g.querySelector('.needle');if(n)n.style.transform='rotate('+a+'deg) translateX(-50%)';});const m=document.querySelector('.meta');if(m)m.classList.add('updated');}catch(e){}}");
  page += F("window.addEventListener('load',()=>{refreshData();setInterval(refreshData,15000);});");
  page += F("</script></head><body><div class='wrap'>");
  page += F("<div class='top'><div><h1>ESP32 Home Dashboard</h1><div class='meta'>Live BME280 data from your sensor</div></div>");
  page += F("<div style='display:grid;gap:10px;justify-items:end'><div class='wifi' id='wifi'>Wi-Fi: ...</div><div class='actions'><a class='btn primary' href='/measure'>Measure now</a><a class='btn ghost' href='/refresh'>Refresh</a></div></div></div>");
  page += F("<div class='grid'>");
  page += gaugeHtml("Temperature", lastTemperature, -10, 50, "°C", tempColor(lastTemperature));
  page += gaugeHtml("Humidity", lastHumidity, 0, 100, "%", humidityColor(lastHumidity));
  page += gaugeHtml("Pressure", lastPressure, 95000, 105000, "Pa", pressureColor(lastPressure));
  page += F("</div>");
  page += F("<div class='summary'>Comfort: <span id='comfort'>N/A</span></div>");
  page += F("<div class='meta'>Trend: <span id='trend'>N/A</span></div>");
  page += F("<div class='meta'>Last measurement: <span id='lastMeasurement'>N/A</span></div>");
  page += F("<div class='status'>");
  page += F("<div class='pill'>Last upload: ");
  page += F("<span id='lastUpload'>never</span>");
  page += F("</div>");
  page += F("<div class='pill'>Next upload in: ");
  page += F("<span id='nextUpload'>...</span>");
  page += F("</div>");
  page += F("</div>");
  page += F("<div class='stats'>");
  page += F("<div class='pill'>");
  page += statLine("Temp min/max", minTemperature, maxTemperature, "°C");
  page += F("</div><div class='pill'>");
  page += statLine("Humidity min/max", minHumidity, maxHumidity, "%");
  page += F("</div><div class='pill'>");
  page += statLine("Pressure min/max", minPressure, maxPressure, "Pa");
  page += F("</div>");
  page += F("</div>");
  page += F("</div></body></html>");
  return page;
}

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", buildPage());
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
  if (!isnan(lastTemperature) && !isnan(minTemperature) && !isnan(maxTemperature)) {
    if (lastTemperature >= maxTemperature) trend = "warming";
    else if (lastTemperature <= minTemperature) trend = "cooling";
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
  json += "\"press\":" + String(lastPressure, 0) + ",";
  json += "\"tempAngle\":" + String((int)(-120 + constrain((lastTemperature + 10) / 60.0f, 0.0f, 1.0f) * 240)) + ",";
  json += "\"humidAngle\":" + String((int)(-120 + constrain(lastHumidity / 100.0f, 0.0f, 1.0f) * 240)) + ",";
  json += "\"pressAngle\":" + String((int)(-120 + constrain((lastPressure - 95000.0f) / 10000.0f, 0.0f, 1.0f) * 240) + 18) + ",";
  json += "\"lastMeasurement\":\"" + lastMeasurement + "\",";
  json += "\"lastUpload\":\"" + lastUpload + "\",";
  json += "\"nextUpload\":\"" + nextUpload + "\",";
  json += "\"trend\":\"" + trend + "\",";
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

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  readBme280();
  sendToThingSpeak(lastTemperature, lastHumidity, lastPressure);

  server.on("/", handleRoot);
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
