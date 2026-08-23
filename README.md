# ESP32 Home Dashboard

ESP32 web dashboard for a BME280 sensor with ThingSpeak logging.

## What it does

- Shows live temperature, humidity, and pressure in a browser
- Uses a button to measure now on demand
- Sends periodic readings to ThingSpeak over Wi-Fi
- Serves the dashboard directly from the ESP32

## Project structure

- `firmware/` - ESP32 firmware
- `firmware/esp32_bme280_dashboard/esp32_bme280_dashboard.ino` - main firmware sketch

## Setup

1. Open the sketch in Arduino IDE or PlatformIO.
2. Install these libraries:
   - `Adafruit BME280 Library`
   - `Adafruit Unified Sensor`
3. Edit the sketch and fill in:
   - `WIFI_SSID`
   - `WIFI_PASSWORD`
   - `THINGSPEAK_API_KEY`
4. Flash the ESP32.
5. Open the IP address printed in Serial Monitor.

## Notes

- The dashboard refreshes automatically every 15 seconds.
- The `Measure now` button triggers a local measurement and ThingSpeak upload.
- The hourly cycle is controlled by `measurementIntervalMs`.
