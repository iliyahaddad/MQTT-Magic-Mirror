// MQTT Magic Mirror - ESP32 sensor node
// Publishes temperature/humidity, presence (PIR) and IR remote presses.
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <IRrecv.h>
#include <IRsend.h>
#include <IRutils.h>
#include "config.h"

WiFiClient net;
PubSubClient mqtt(net);
DHT dht(PIN_DHT, DHT22);
IRrecv irrecv(PIN_IR);
IRsend irsend(PIN_IR_TX);
decode_results irResult;

String base;
unsigned long lastSensor = 0, lastMotion = 0, lastTry = 0;
int8_t lastPresence = -1;  // -1 = nothing published yet
unsigned long lastDiscovery = 0;

void publishJson(const char *kind, JsonDocument &doc) {
  char buf[128];
  serializeJson(doc, buf, sizeof(buf));
  mqtt.publish((base + "/" + kind).c_str(), buf);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  if (length > 384) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  String t(topic);
  if (t != "mirror/command") return;
  const char* type = doc["type"] | "";
  const char* command = doc["command"] | "";
  if (strcmp(type, "ir") != 0) return;
  // Explicit allow-list: do not accept arbitrary raw IR codes from MQTT.
  if (!strcmp(command, "tv_power")) irsend.sendNEC(IR_TV_POWER, 32);
  else if (!strcmp(command, "tv_volume_up")) irsend.sendNEC(IR_TV_VOL_UP, 32);
  else if (!strcmp(command, "tv_volume_down")) irsend.sendNEC(IR_TV_VOL_DOWN, 32);
  else if (!strcmp(command, "ac_on")) mqtt.publish((base + "/ir/unsupported").c_str(), "AC code not configured", false);
  else if (!strcmp(command, "ac_off")) mqtt.publish((base + "/ir/unsupported").c_str(), "AC code not configured", false);
}

void publishDiscovery() {
  if (lastDiscovery != 0 && millis() - lastDiscovery < 60000) return;
  lastDiscovery = millis();
  String id = String(DEVICE_ID);
  String dev = "{\"identifiers\":[\"" + id + "\"],\"name\":\"Magic Mirror " + id + "\",\"manufacturer\":\"MQTT Magic Mirror\",\"model\":\"ESP32 sensor node\"}";
  String root = "homeassistant/sensor/" + id + "/";
  String t = "{\"name\":\"" + id + " Temperature\",\"unique_id\":\"" + id + "_temperature\",\"state_topic\":\"" + base + "/sensors\",\"value_template\":\"{{ value_json.temp }}\",\"unit_of_measurement\":\"°C\",\"device_class\":\"temperature\",\"state_class\":\"measurement\",\"device\":" + dev + "}";
  String h = "{\"name\":\"" + id + " Humidity\",\"unique_id\":\"" + id + "_humidity\",\"state_topic\":\"" + base + "/sensors\",\"value_template\":\"{{ value_json.hum }}\",\"unit_of_measurement\":\"%\",\"device_class\":\"humidity\",\"state_class\":\"measurement\",\"device\":" + dev + "}";
  mqtt.publish((root + "temperature/config").c_str(), t.c_str(), true);
  mqtt.publish((root + "humidity/config").c_str(), h.c_str(), true);
}

bool connectMqtt() {
  String will = base + "/status";
  bool hasAuth = strlen(MQTT_USER) > 0;
  bool ok = mqtt.connect(DEVICE_ID, hasAuth ? MQTT_USER : nullptr, hasAuth ? MQTT_PASS : nullptr,
                         will.c_str(), 1, true, "offline");  // last will: offline
  if (ok) {
    mqtt.publish(will.c_str(), "online", true);
    mqtt.subscribe("mirror/command");
    publishDiscovery();
    lastPresence = -1;  // republish presence after (re)connect
    Serial.println("MQTT connected");
  }
  return ok;
}

void ensureConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastTry > 5000) { lastTry = millis(); WiFi.reconnect(); }
    return;
  }
  if (!mqtt.connected() && millis() - lastTry > 5000) {
    lastTry = millis();
    connectMqtt();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PIR, INPUT);
  dht.begin();
  irrecv.enableIRIn();
  base = String("mirror/") + DEVICE_ID;
  lastMotion = millis();  // assume someone is around at boot

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setBufferSize(768); // Home Assistant discovery JSON exceeds PubSubClient default buffer
  mqtt.setCallback(mqttCallback);
  irsend.begin();
  WiFi.setHostname(DEVICE_ID);
  WiFi.setAutoReconnect(true);
  mqtt.setKeepAlive(30);
  ArduinoOTA.setHostname(DEVICE_ID);
  ArduinoOTA.setPassword(OTA_PASSWORD); // use a unique, strong per-device password
  ArduinoOTA.onStart([](){ Serial.println("OTA update starting"); });
  ArduinoOTA.onEnd([](){ Serial.println("OTA update complete"); });
  ArduinoOTA.onError([](ota_error_t error){ Serial.printf("OTA error: %u\n", error); });
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  ensureConnected();
  if (!mqtt.connected()) return;
  mqtt.loop();
  publishDiscovery();

  // Presence: PIR motion keeps presence on until PRESENCE_TIMEOUT_MS of stillness.
  if (digitalRead(PIN_PIR)) lastMotion = millis();
  int8_t present = (millis() - lastMotion < PRESENCE_TIMEOUT_MS) ? 1 : 0;
  if (present != lastPresence) {
    lastPresence = present;
    mqtt.publish((base + "/presence").c_str(), present ? "1" : "0", true);
  }

  // Climate
  if (millis() - lastSensor >= SENSOR_INTERVAL_MS) {
    lastSensor = millis();
    float t = dht.readTemperature(), h = dht.readHumidity();
    if (!isnan(t) && !isnan(h)) {
      JsonDocument doc;
      doc["temp"] = round(t * 10) / 10.0;
      doc["hum"] = round(h * 10) / 10.0;
      publishJson("sensors", doc);
    } else {
      Serial.println("DHT read failed");
    }
  }

  // IR remote
  if (irrecv.decode(&irResult)) {
    if (!irResult.repeat) {
      JsonDocument doc;
      doc["protocol"] = typeToString(irResult.decode_type);
      doc["code"] = resultToHexidecimal(&irResult);
      publishJson("ir", doc);
    }
    irrecv.resume();
  }
}
