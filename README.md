# MQTT Magic Mirror

> **Review update (v1.4):** Fixed MQTT QoS 1 receive handling (PUBACK), made number-key navigation respect custom view lists, and added left-arrow/page-up navigation. Removed accidentally bundled Python bytecode artifacts. Smoke tests and JavaScript syntax checks pass; hardware integration still needs physical validation.

A black-background, MQTT-driven smart mirror for an ESP32 sensor node. Includes temperature/humidity monitoring, presence, Home Assistant MQTT Discovery, deduplicated threshold alerts, local weather bridge, Persian-calendar display, MQTT IR command controls, authenticated MQTT examples, and authenticated ArduinoOTA updates.

> **Status:** development release. Browser-side behavior and Python syntax are smoke-tested; hardware, OTA on a physical ESP32, Home Assistant discovery and the target IR codes still require validation on your setup. OTA password authentication and ArduinoOTA's transfer integrity checks are not a cryptographic firmware-signing system. Keep OTA on a trusted LAN/VPN and use signed-update verification for high-assurance deployments.

## Architecture

```text
ESP32 (DHT22 + PIR + IR receiver/transmitter) --MQTT--> Mosquitto <--MQTT/WebSocket-- Mirror UI
             | ArduinoOTA (password protected)              ^              |
             +----------------------------------------------+              +-- IR command publish
Local weather bridge -- MQTT mirror/weather -------------------------------+
```

## Run the demo

```bash
docker compose up -d
python -m venv .venv
# Linux/macOS: source .venv/bin/activate
# Windows PowerShell: .venv\Scripts\Activate.ps1
python -m pip install -r tools/requirements.txt
python tools/simulator.py
```

Open <http://localhost:8080>. Swipe left/right on touchscreens, or navigate using `1` clock, `2` devices, `3` sensors, `4` IR control, `5` weather, `H` wake. The UI ships with `mirror/mqtt-ws.js`, a small MQTT 3.1.1 WebSocket client, so it does not load runtime JavaScript from a CDN. It supports QoS 0 publishing and subscriptions; do not treat it as a general-purpose replacement for a fully featured MQTT library.

## Features and topics

| Topic | Payload | Direction |
|---|---|---|
| `mirror/<id>/status` | `online` / `offline` (retained, LWT) | ESP32 → broker |
| `mirror/<id>/sensors` | `{"temp":22.4,"hum":45.1}` | ESP32 → broker |
| `mirror/<id>/presence` | `1` / `0` | ESP32 → broker |
| `mirror/<id>/ir` | `{"protocol":"NEC","code":"0x20DF10EF"}` | receiver → mirror |
| `mirror/command` | `{"type":"ir","command":"tv_power"}` | mirror → ESP32 |
| `mirror/weather` | `{"city":"Baku","temp":18.2,"hum":64,"condition":"Cloudy","wind":"12 km/h"}` | local bridge → mirror |
| `mirror/message` | text or `{"text":"...","ttl":30}` | publisher → mirror |
| `homeassistant/sensor/<id>/temperature/config` | retained HA discovery JSON | ESP32 → Home Assistant |
| `homeassistant/sensor/<id>/humidity/config` | retained HA discovery JSON | ESP32 → Home Assistant |

### Threshold alerts

Defaults: low/high temperature 10/30 °C, low/high humidity 20/70%, repeat cooldown 15 minutes. Override via `window.MIRROR_CONFIG` in `mirror/index.html`:

```js
window.MIRROR_CONFIG = {
  tempLow: 16, tempHigh: 28,
  humLow: 30, humHigh: 65,
  alertCooldownMs: 900000
};
```

Alerts are evaluated in the browser when a sensor message arrives and are deduplicated per device/threshold during the cooldown. They are not server-side alarms and won't fire while the browser is closed or disconnected.

### Home Assistant

Enable MQTT integration in Home Assistant and point it at the same broker. The ESP32 publishes retained discovery configs on connect and periodically. Make sure the broker ACL permits the ESP32 user to write `homeassistant/sensor/<device-id>/+/config`.

### IR hardware and commands

Use a transistor driver for the IR LED; do **not** drive an IR LED directly from an ESP32 GPIO. Receiver is GPIO 14, transmitter output is GPIO 13 by default. Replace `IR_TV_POWER`, `IR_TV_VOL_UP`, and `IR_TV_VOL_DOWN` in `firmware/include/config.h` with codes learned from your own remote. The included NEC codes are examples only. AC protocols often need long state frames; `ac_on`/`ac_off` intentionally report “not configured” until you implement the correct model-specific frame. Never send arbitrary raw codes from an unauthenticated MQTT topic.

### Weather and Persian calendar

The mirror uses the browser's `fa-IR-u-ca-persian` locale for the Solar Hijri date (browser locale support required). Weather is not fetched from a cloud API by the mirror. A local service can publish a retained JSON payload to `mirror/weather`. Example:

```bash
python -m pip install -r tools/requirements.txt
python bridge/publish_weather.py --host 127.0.0.1 --file bridge/weather.example.json
```

The bridge script republishes data collected by your own local weather integration; it does not itself retrieve forecast data.

## ESP32 setup

1. Copy `firmware/include/config.example.h` to `firmware/include/config.h` and set Wi-Fi, broker credentials, a unique OTA password and device ID.
2. Update the sample IR codes for your equipment.
3. Install PlatformIO and run `pio run -d firmware`; upload using `pio run -d firmware -t upload`.
4. After the ESP32 joins Wi-Fi, ArduinoOTA is enabled with `OTA_PASSWORD`. Keep OTA on a trusted LAN/VPN; don't forward OTA ports to the internet. Use unique credentials per device.

The MQTT client currently uses `WiFiClient` (plain TCP). For networks that require TLS, change the firmware transport to `WiFiClientSecure`, validate the broker CA certificate, and use a broker listener configured for TLS. Do not merely disable certificate validation.

## Secure Mosquitto configuration

The default `mosquitto/mosquitto.conf` is an **insecure local demo profile** (`allow_anonymous true`). Do not expose it to an untrusted network.

For a protected LAN deployment:

1. Create password files with the Mosquitto utilities, e.g. `mosquitto_passwd -c mosquitto/password_file mirror-esp32` and add separate `mirror-browser` and `weather-bridge` users with `mosquitto_passwd` (omit `-c` when adding users).
2. Copy `mosquitto/mosquitto-secure.conf.example` to `mosquitto/mosquitto.conf`, and `mosquitto/acl.example` to `mosquitto/acl`. Mount both `password_file` and `acl` read-only in `docker-compose.yml` (example mounts are commented in the compose file).
3. Configure the mirror browser credentials through `window.MIRROR_CONFIG.mqttUsername` and `.mqttPassword`. These are readable by anyone who can inspect the browser, so use a dedicated **read-only** account and never an admin/device credential. Prefer a reverse proxy with TLS/WSS and LAN/VPN access control.
4. Give the weather bridge its own account with write permission only for `mirror/weather`.
5. Restrict ports 1883, 9001 and 8080 using host/network firewall rules. Never port-forward them directly to the internet.

`mosquitto/acl.example` is a starting template: review and adapt the ACL syntax and permissions to your installed Mosquitto version and exact client behavior before deployment. The ESP32's command subscription and discovery publishing need the permissions listed in that file.

## Configuration options

| Option | Default | Description |
|---|---|---|
| `broker` | `ws://<page host>:9001` | MQTT WebSocket endpoint; supports `ws://` / `wss://` |
| `sleepAfterMs` | `15000` | Blank after no presence |
| `messageTtlMs` | `30000` | Default notification lifetime |
| `tempLow` / `tempHigh` | `10` / `30` | Temperature alert limits in °C |
| `humLow` / `humHigh` | `20` / `70` | Humidity alert limits in percent |
| `alertCooldownMs` | `900000` | Per-alert repeat suppression interval |
| `rotateMs` | `0` (off) | Auto-rotate views every N ms (min 3000); also `?rotateMs=10000` |
| `views` | all | Ordered list, e.g. `["home","sensors","weather"]` |
| `mqttUsername` / `mqttPassword` | empty | Dedicated browser MQTT account; use only on trusted HTTPS/WSS deployment |

## Tests and CI

GitHub Actions compiles the PlatformIO firmware, checks the Python simulator and validates JavaScript syntax. CI compilation does not prove physical hardware behavior. Before release, test HA discovery, IR output with the actual appliance, broker ACLs, OTA update/recovery, and the display kiosk on your target device.

## Troubleshooting

- **Broker offline:** check `docker compose ps` and `docker compose logs mosquitto`; verify WebSocket port 9001 and correct `ws://`/`wss://` URL.
- **Browser MQTT auth fails:** make sure the WebSocket listener supports the configured user, and that the ACL permits `read mirror/#` for the mirror account.
- **ESP32 missing:** `MQTT_HOST` must be the LAN IP of the Docker host, not `localhost`.
- **No sensor values:** check DHT22 wiring and serial output at 115200 baud.
- **IR command doesn't operate device:** learn the real protocol/code; example NEC codes won't match every TV. AC commands require model-specific frame implementation.
- **No weather:** publish valid JSON to `mirror/weather` and open view 5.

## License

MIT. See [LICENSE](LICENSE).
