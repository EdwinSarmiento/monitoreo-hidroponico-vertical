#include "hidro_mqtt.h"
#include "actuators.h"
#include "app_state.h"
#include <ArduinoJson.h>
#include <WiFi.h>

static void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] Recibido [%s]: %s\n", topic, msg.c_str());

  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, msg)) return;

  if (doc.containsKey("pump")) setPump(doc["pump"].as<bool>());
  if (doc.containsKey("peri1")) setPeristaltic(1, doc["peri1"].as<bool>());
  if (doc.containsKey("peri2")) setPeristaltic(2, doc["peri2"].as<bool>());
}

void connectMQTT() {
  if (!mqttConfigured || mqttClient.connected()) return;

  Serial.printf("[MQTT] Conectando a %s:%d...\n", mqttServer.c_str(), mqttPort);
  mqttClient.setServer(mqttServer.c_str(), mqttPort);
  mqttClient.setCallback(mqttCallback);

  String clientId = "ESP32-" + WiFi.macAddress();
  bool ok = (mqttUser.length() > 0)
                ? mqttClient.connect(clientId.c_str(), mqttUser.c_str(), mqttPass.c_str())
                : mqttClient.connect(clientId.c_str());

  if (ok) {
    Serial.println("[MQTT] Conectado!");
    String topic = "hidroponia/" + deviceToken + "/cmd";
    mqttClient.subscribe(topic.c_str());
    Serial.printf("[MQTT] Suscrito a: %s\n", topic.c_str());
  } else {
    Serial.printf("[MQTT] Fallo rc=%d\n", mqttClient.state());
  }
}

void publishMQTT() {
  if (!mqttClient.connected()) return;
  if (millis() - lastMQTTPublish < 5000) return;
  lastMQTTPublish = millis();

  StaticJsonDocument<256> doc;
  doc["ph"] = currentPH;
  doc["phV"] = currentVoltage;
  doc["pump"] = pumpState;
  doc["peri1"] = peri1State;
  doc["peri2"] = peri2State;
  doc["temp"] = currentTemp;
  doc["hum"] = currentHum;
  doc["level"] = isLevelLow ? "LOW" : "OK";
  doc["ip"] = WiFi.localIP().toString();
  doc["rssi"] = WiFi.RSSI();
  doc["adsOk"] = adsReady;

  char buf[256];
  serializeJson(doc, buf);

  String topic = "hidroponia/" + deviceToken + "/state";
  mqttClient.publish(topic.c_str(), buf);
  Serial.printf("[MQTT] Publicado -> pH: %.2f V: %.3f\n", currentPH, currentVoltage);
}
