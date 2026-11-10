#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include "secrets.h"

#define DHTPIN 4
#define DHTTYPE DHT22

const char* DEVICE_ID       = "esp32-01";
const char* TOPIC_TELEMETRY = "p1/esp32-01/telemetry";
const char* TOPIC_STATUS    = "p1/esp32-01/status";
const unsigned long PUBLISH_MS = 5000;

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastPublish = 0;

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print(" connected, IP: ");
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqtt.connected()) {
    Serial.print("MQTT...");
    // Last Will: the broker publishes "offline" if this board drops off
    if (mqtt.connect(DEVICE_ID, nullptr, nullptr, TOPIC_STATUS, 1, true, "offline")) {
      mqtt.publish(TOPIC_STATUS, "online", true);
      Serial.println(" connected");
    } else {
      Serial.print(" failed, rc=");
      Serial.println(mqtt.state());
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  connectWiFi();
  mqtt.setServer(MQTT_HOST, 1883);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();

  if (millis() - lastPublish >= PUBLISH_MS) {
    lastPublish = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    if (isnan(t) || isnan(h)) {
      Serial.println("DHT read failed");
      return;
    }
    JsonDocument doc;
    doc["temperature"] = round(t * 10) / 10.0;
    doc["humidity"]    = round(h * 10) / 10.0;
    char buf[128];
    serializeJson(doc, buf);
    mqtt.publish(TOPIC_TELEMETRY, buf);
    Serial.println(buf);
  }
}