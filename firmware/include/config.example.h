// Copy to config.h and fill in your values. config.h is git-ignored.
#pragma once

#define WIFI_SSID  "your-wifi"
#define WIFI_PASS  "your-password"

#define MQTT_HOST  "192.168.1.10"   // machine running docker compose
#define MQTT_PORT  1883
#define MQTT_USER  "mirror-esp32"
#define MQTT_PASS  "change-this-to-a-unique-password"
#define OTA_PASSWORD "change-this-to-a-unique-strong-ota-password"

#define DEVICE_ID  "esp32-livingroom"

#define PIN_DHT    4     // DHT22 data
#define PIN_PIR    27    // PIR (HC-SR501) output
#define PIN_IR     14    // IR receiver (TSOP/VS1838) output

#define SENSOR_INTERVAL_MS   10000
#define PRESENCE_TIMEOUT_MS  30000   // no motion for this long -> presence = 0

#define PIN_IR_TX 13
#define IR_TV_POWER 0x20DF10EF
#define IR_TV_VOL_UP 0x20DF40BF
#define IR_TV_VOL_DOWN 0x20DFC03F
