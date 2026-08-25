# ESP32 Home Dashboard

ESP32 web dashboard for BME280 sensors with ThingSpeak logging. Supports
one "hub" device (with its own local sensor) plus an optional second
"node" device in another room, reporting to the hub over local Wi-Fi.

## What it does

- Shows live temperature, humidity, and pressure for one or more rooms
  in a browser
- A "Measure now" button measures all rooms on demand
- Sends periodic readings to ThingSpeak over Wi-Fi
- Serves the dashboard directly from the hub ESP32

## Project structure

- `firmware/esp32_bme280_dashboard/` - hub firmware (own sensor + web
  dashboard + ThingSpeak upload + receives readings from node(s))
- `firmware/esp32_bme280_node/` - node firmware (a second ESP32+BME280
  in another room, pushes its readings to the hub)

## Setup - hub (first ESP32, e.g. "Хол"/Living room)

1. Open `firmware/esp32_bme280_dashboard/esp32_bme280_dashboard.ino` in
   Arduino IDE or PlatformIO.
2. Install these libraries:
   - `Adafruit BME280 Library`
   - `Adafruit Unified Sensor`
3. Create `secrets.h` next to the sketch and fill in:
   - `WIFI_SSID`
   - `WIFI_PASSWORD`
   - `THINGSPEAK_API_KEY`
   - `NODE_ADDRESS` - hostname/IP of the bedroom node (default
     `esp32-bedroom.local`, only needed once you add a node - see below)
4. Flash the ESP32.
5. Open the IP address (or `http://esp32-home.local`) printed in Serial
   Monitor.

## Setup - node (second ESP32, e.g. "Спалня"/Bedroom, optional)

1. Open `firmware/esp32_bme280_node/esp32_bme280_node.ino`.
2. Install the same BME280 libraries as the hub.
3. Copy `secrets.h.example` to `secrets.h` and fill in:
   - `WIFI_SSID` / `WIFI_PASSWORD` (same network as the hub)
   - `HUB_ADDRESS` - hostname/IP of the hub (default
     `esp32-home.local`)
   - `DEVICE_ID` / `DEVICE_NAME` - must match an entry the hub knows
     about (`bedroom` / "Спалня" by default)
4. Flash the ESP32 and place it in the new room.
5. It measures every hour, pushes its reading to the hub's `/ingest`
   endpoint, and exposes its own `/measure` + `/data` for the hub to
   call directly (used by "Measure now").

## ThingSpeak field mapping

The hub uploads all rooms to the **same** ThingSpeak channel:

| Field  | Room             | Value        |
|--------|------------------|--------------|
| field1 | Хол (hub)        | Temperature  |
| field2 | Хол (hub)        | Pressure     |
| field3 | Хол (hub)        | Humidity     |
| field4 | Спалня (node)    | Temperature  |
| field5 | Спалня (node)    | Pressure     |
| field6 | Спалня (node)    | Humidity     |

Fields 4-6 are only sent once the hub has heard from the node at least
once (they're simply omitted before that).

## Notes

- The dashboard refreshes automatically every 15 seconds.
- The `Measure now` button triggers a local measurement on the hub and
  (if configured) asks the node to measure too - no ThingSpeak upload.
- The hourly upload cycle is controlled by `measurementIntervalMs` in
  the hub sketch.
- A node is considered "offline" on the dashboard if the hub hasn't
  heard from it in over 2x its expected reporting interval.
- Communication between node and hub is plain HTTP over the local
  Wi-Fi network (no cloud/internet round-trip needed between them).

