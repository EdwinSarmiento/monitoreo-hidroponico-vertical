#include "hidro_mqtt.h"
#include "actuators.h"
#include "app_state.h"
#include "ph_control.h"
#include <ArduinoJson.h>
#include <WiFi.h>

static void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.printf("[MQTT] Recibido [%s]: %s\n", topic, msg.c_str());

  StaticJsonDocument<300> doc;
  if (deserializeJson(doc, msg)) return;

  // Control manual de bombas (solo si auto-regulación está OFF)
  if (doc.containsKey("pump")) setPump(doc["pump"].as<bool>());
  if (!phAutoEnabled) {
    if (doc.containsKey("peri1")) setPeristaltic(1, doc["peri1"].as<bool>());
    if (doc.containsKey("peri2")) setPeristaltic(2, doc["peri2"].as<bool>());
  }

  // Configuración de auto-regulación de pH
  bool configChanged = false;
  if (doc.containsKey("phAuto")) {
    bool newState = doc["phAuto"].as<bool>();
    if (newState != phAutoEnabled) {
      phAutoEnabled = newState;
      if (!phAutoEnabled) cancelPhCycle();
      Serial.printf("[pH Ctrl] Auto-regulación: %s\n", phAutoEnabled ? "ON" : "OFF");
    }
  }
  if (doc.containsKey("phTarget")) {
    phTarget = constrain(doc["phTarget"].as<float>(), 2.0f, 10.0f);
    configChanged = true;
  }
  if (doc.containsKey("phTol")) {
    phTolerance = constrain(doc["phTol"].as<float>(), 0.1f, 2.0f);
    configChanged = true;
  }
  if (doc.containsKey("phDose")) {
    phDoseSeconds = constrain(doc["phDose"].as<int>(), 1, 30);
    configChanged = true;
  }
  if (doc.containsKey("phSettle")) {
    phSettleSeconds = constrain(doc["phSettle"].as<int>(), 10, 300);
    configChanged = true;
  }

  if (configChanged) {
    savePhControlConfig();
    Serial.printf("[pH Ctrl] Nueva config: target=%.1f tol=±%.1f dosis=%ds espera=%ds\n",
                  phTarget, phTolerance, phDoseSeconds, phSettleSeconds);
  }

  lastMQTTPublish = 0;
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
  if (millis() - lastMQTTPublish < 2000) return;
  lastMQTTPublish = millis();

  StaticJsonDocument<384> doc;
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

  // Estado de auto-regulación de pH
  doc["phAuto"] = phAutoEnabled;
  doc["phTarget"] = phTarget;
  doc["phTol"] = phTolerance;
  doc["phDose"] = phDoseSeconds;
  doc["phSettle"] = phSettleSeconds;

  const char* stateStr = "idle";
  if (phCycleState == PH_DOSING) stateStr = "dosing";
  else if (phCycleState == PH_SETTLING) stateStr = "settling";
  doc["phState"] = stateStr;
  doc["phDir"] = phCycleDirection;

  char buf[384];
  serializeJson(doc, buf);

  String topic = "hidroponia/" + deviceToken + "/state";
  mqttClient.publish(topic.c_str(), buf);
  Serial.printf("[MQTT] Publicado -> pH: %.2f V: %.3f auto:%s state:%s\n",
                currentPH, currentVoltage,
                phAutoEnabled ? "ON" : "OFF", stateStr);
}
