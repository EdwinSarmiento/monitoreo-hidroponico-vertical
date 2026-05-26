#include "storage.h"
#include "app_state.h"
#include <WiFi.h>
#include <esp_wifi.h>

void loadWiFiCredentials() {
  prefs.begin("wifi", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();
}

void saveWiFiCredentials(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void clearAllConfig() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
  prefs.begin("mqtt", false);
  prefs.clear();
  prefs.end();
  prefs.begin("hidro", false);
  prefs.clear();
  prefs.end();
  WiFi.disconnect(true, true);
  esp_wifi_restore();
  Serial.println("Toda la configuracion borrada (WiFi + MQTT + legacy).");
}

void loadMQTTConfig() {
  prefs.begin("mqtt", true);
  mqttServer = prefs.getString("server", "");
  mqttPort = prefs.getInt("port", 1883);
  mqttUser = prefs.getString("user", "");
  mqttPass = prefs.getString("pass", "");
  deviceToken = prefs.getString("token", "esp32_hidro");
  prefs.end();
  mqttConfigured = (mqttServer.length() > 0);
  Serial.printf("[MQTT Config] Server: %s:%d Token: %s Configurado: %s\n",
                mqttServer.c_str(), mqttPort, deviceToken.c_str(),
                mqttConfigured ? "SI" : "NO");
}

void saveMQTTConfig() {
  prefs.begin("mqtt", false);
  prefs.putString("server", mqttServer);
  prefs.putInt("port", mqttPort);
  prefs.putString("user", mqttUser);
  prefs.putString("pass", mqttPass);
  prefs.putString("token", deviceToken);
  prefs.end();
  mqttConfigured = true;
  Serial.println("[MQTT Config] Guardado en flash.");
}
