# ESP32 Home Dashboard

ESP32 web dashboard for BME280 sensors with ThingSpeak logging. Supports
one "hub" device (with its own local sensor) plus an optional second
"node" device in another room. The hub is fully in control of timing: it
polls the node over local Wi-Fi whenever it needs a fresh reading (its
own hourly ThingSpeak cycle, or a user pressing "Measure now"). The node
has no timer of its own and never contacts the hub on its own initiative
- it only measures and responds when asked.

## What it does

- Shows live temperature, humidity, and pressure for one or more rooms
  in a browser
- A "Measure now" button measures all rooms on demand
- Sends periodic readings to ThingSpeak over Wi-Fi
- Serves the dashboard directly from the hub ESP32
- Optional Windows desktop app (tray widget) that shows the same data
  right on your desktop, with auto-update

## Project structure

- `firmware/esp32_bme280_dashboard/` - hub firmware (own sensor + web
  dashboard + ThingSpeak upload + polls the node for its reading)
- `firmware/esp32_bme280_node/` - node firmware (a second ESP32+BME280
  in another room; only responds to the hub's requests, no timer)
- `desktop/` - Windows tray-widget desktop app (Electron) that connects
  to the hub's website and shows the current readings on the desktop

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
5. **Upload the filesystem (`data/` folder)** - this is a separate step
   from flashing the sketch, and is required whenever `index.html`,
   `app.js`, or `style.css` change (like after this multi-room update).
   Otherwise the ESP32 keeps serving the old cached website even though
   the sketch itself is up to date.
   - Arduino IDE 2.x: install the "Upload LittleFS to Pico/ESP32/ESP8266"
     extra tool if you haven't already (Tools menu → search "littlefs"),
     then use it to upload the `data/` folder.
   - Arduino IDE 1.x: Tools → "ESP32 Sketch Data Upload".
   - PlatformIO: `pio run --target uploadfs`.
6. Open the IP address (or `http://esp32-home.local`) printed in Serial
   Monitor.

## Setup - node (second ESP32, e.g. "Спалня"/Bedroom, optional)

1. Open `firmware/esp32_bme280_node/esp32_bme280_node.ino`.
2. Install the same BME280 libraries as the hub.
3. Copy `secrets.h.example` to `secrets.h` and fill in:
   - `WIFI_SSID` / `WIFI_PASSWORD` (same network as the hub)
   - `DEVICE_ID` / `DEVICE_NAME` - must match an entry the hub knows
     about (`bedroom` / "Спалня" by default)
4. Flash the ESP32 and place it in the new room.
5. It has no timer of its own: it just runs a small web server exposing
   `/measure` (take a fresh reading and return it) and `/data` (return
   the last reading). The hub calls `/measure` on its own hourly cycle
   right before uploading to ThingSpeak, and also when "Measure now" is
   pressed - so the node's readings are always as fresh as the hub asked
   for, with no separate schedule to keep in sync.

## Setup - desktop app (optional, Windows)

A small Electron tray-widget app that shows the same live readings
right on your desktop, no browser needed.

- **Just want to use it:** download the latest installer from
  [Releases](https://github.com/Neptune4918/Esp32_Home_server/releases),
  run it, and it'll sit in your system tray. It auto-updates itself.
- **Developing/building it yourself:** see [`desktop/README.md`](desktop/README.md)
  for dev mode, building the `.exe`, and how to publish new versions.

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

Fields 4-6 are only sent once the hub has successfully polled the node at
least once (they're simply omitted before that).

## Notes

- The dashboard refreshes automatically every 15 seconds.
- The `Measure now` button triggers a local measurement on the hub and
  (if configured) asks the node to measure too - no ThingSpeak upload.
- The hourly upload cycle is controlled by `measurementIntervalMs` in
  the hub sketch. Right before each scheduled upload, the hub also polls
  the node for a fresh reading (same as "Measure now" does), so ThingSpeak
  always gets the node's latest value, not a stale one.
- A node is considered "offline" on the dashboard if the hub hasn't
  successfully polled it in over 2x its expected reporting interval.
- Communication between node and hub is plain HTTP over the local
  Wi-Fi network (no cloud/internet round-trip needed between them). The
  hub always initiates the request; the node never contacts the hub.

