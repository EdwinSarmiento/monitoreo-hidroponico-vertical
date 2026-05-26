#ifndef APP_STATE_H
#define APP_STATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Adafruit_ADS1X15.h>
#include <PubSubClient.h>

// Objetos globales compartidos entre modulos
extern AsyncWebServer server;
extern DNSServer dnsServer;
extern Preferences prefs;
extern Adafruit_ADS1115 ads;
extern WiFiClient espClient;
extern PubSubClient mqttClient;

// Estado en tiempo de ejecucion
extern bool pumpState;
extern bool peri1State;
extern bool peri2State;
extern bool apMode;
extern bool adsReady;
extern bool mqttConfigured;

extern float currentPH;
extern float currentVoltage;
extern float currentTemp;
extern float currentHum;
extern bool isLevelLow;
extern unsigned long lastPHRead;
extern unsigned long lastEnvRead;
extern unsigned long lastLevelCheck;
extern unsigned long lastMQTTPublish;

extern String mqttServer;
extern int mqttPort;
extern String mqttUser;
extern String mqttPass;
extern String deviceToken;

extern String savedSSID;
extern String savedPass;

#endif
