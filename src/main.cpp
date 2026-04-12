#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h> // Se mantiene solo para el portal WiFi inicial
#include <Adafruit_ADS1X15.h>
#include "config.h"

// Objetos globales
WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer setupServer(80);
DNSServer dnsServer;
Preferences prefs;
Adafruit_ADS1115 ads;

// Estado del sistema
bool pumpState = false;
bool peri1State = false;
bool peri2State = false;
bool apMode = false;
bool adsReady = false;

float currentPH = 0.0;
float currentVoltage = 0.0;
unsigned long lastPHRead = 0;
unsigned long lastMQTTMsg = 0;

// =============================================
//  Lectura de pH (ADS1115 por I2C)
// =============================================
float readPHVoltage() {
  if (!adsReady) return 0.0f;
  long total = 0;
  int validSamples = 0;
  for (int i = 0; i < PH_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(PH_ADS_CHANNEL);
    if (raw >= 0 && raw < 32767) {
      total += raw;
      validSamples++;
    }
    delay(10);
  }
  if (validSamples == 0) return 0.0f;
  float avgRaw = (float)total / validSamples;
  return ads.computeVolts(avgRaw);
}

float voltageToPH(float voltage) {
  float slope = (7.0f - 4.0f) / (PH_VOLTAGE_AT_7 - PH_VOLTAGE_AT_4);
  float ph = 7.0f + slope * (voltage - PH_VOLTAGE_AT_7);
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}

// =============================================
//  Control de Actuadores
// =============================================
void updateActuators() {
  digitalWrite(RELAY_PUMP_PIN, pumpState ? HIGH : LOW);
  digitalWrite(PERISTALTIC_1_PIN, peri1State ? HIGH : LOW);
  digitalWrite(PERISTALTIC_2_PIN, peri2State ? HIGH : LOW);
}

// =============================================
//  MQTT: Recepción de mensajes (comandos)
// =============================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String msg;
  for (int i = 0; i < length; i++) msg += (char)payload[i];
  Serial.println(msg);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, msg);
  if (error) return;

  if (doc.containsKey("pump")) pumpState = doc["pump"];
  if (doc.containsKey("peri1")) peri1State = doc["peri1"];
  if (doc.containsKey("peri2")) peri2State = doc["peri2"];
  
  updateActuators();
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Intentando conexión MQTT a ");
    Serial.println(MQTT_SERVER);
    
    String clientId = "ESP32-Hidro-";
    clientId += String(WiFi.macAddress());

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT Conectado!");
      String cmdTopic = "hidroponia/" + String(DEVICE_TOKEN) + "/cmd";
      mqttClient.subscribe(cmdTopic.c_str());
    } else {
      Serial.print("Fallo, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" reintentando en 5s...");
      delay(5000);
      if (WiFi.status() != WL_CONNECTED) return;
    }
  }
}

// =============================================
//  WiFi Setup Portal (Simplificado)
// =============================================
const char SETUP_HTML[] PROGMEM = "HTML_DEL_PORTAL_SIMPLIFICADO"; // Ver README para contenido completo

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  dnsServer.start(53, "*", WiFi.softAPIP());
  
  setupServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", "Explora el manual para configurar el WiFi");
  });
  // ... (aquí irían las rutas de scan/save simplificadas)
  setupServer.begin();
}

bool connectToWiFi() {
  Preferences wifiPrefs;
  wifiPrefs.begin("wifi", true);
  String ssid = wifiPrefs.getString("ssid", "");
  String pass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();

  if (ssid == "") return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  return WiFi.status() == WL_CONNECTED;
}

// =============================================
//  Setup & Loop
// =============================================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(PERISTALTIC_1_PIN, OUTPUT);
  pinMode(PERISTALTIC_2_PIN, OUTPUT);
  updateActuators();

  Wire.begin(I2C_SDA, I2C_SCL);
  if (ads.begin()) adsReady = true;

  if (!connectToWiFi()) {
    Serial.println("Iniciando modo AP...");
    startAPMode();
  } else {
    Serial.println("\nWiFi Conectado!");
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
  }
}

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) ESP.restart();

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Publicar estado cada 5 segundos
  if (millis() - lastPHRead > 5000) {
    lastPHRead = millis();
    currentVoltage = readPHVoltage();
    currentPH = voltageToPH(currentVoltage);

    StaticJsonDocument<200> doc;
    doc["ph"] = currentPH;
    doc["phV"] = currentVoltage;
    doc["pump"] = pumpState;
    doc["peri1"] = peri1State;
    doc["peri2"] = peri2State;
    doc["ip"] = WiFi.localIP().toString();

    char buffer[256];
    serializeJson(doc, buffer);
    
    String stateTopic = "hidroponia/" + String(DEVICE_TOKEN) + "/state";
    mqttClient.publish(stateTopic.c_str(), buffer);
    Serial.println("Datos publicados.");
  }
}
