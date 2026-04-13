#include "app_state.h"

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;
Adafruit_ADS1115 ads;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

bool pumpState = false;
bool peri1State = false;
bool peri2State = false;
bool apMode = false;
bool adsReady = false;
bool mqttConfigured = false;

float currentPH = 0.0f;
float currentVoltage = 0.0f;
unsigned long lastPHRead = 0;
unsigned long lastMQTTPublish = 0;

String mqttServer = "";
int mqttPort = 1883;
String mqttUser = "";
String mqttPass = "";
String deviceToken = "esp32_hidro";

String savedSSID;
String savedPass;
