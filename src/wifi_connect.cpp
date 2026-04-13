#include "wifi_connect.h"
#include "app_state.h"
#include <WiFi.h>

bool connectToWiFi() {
  Serial.printf("Conectando a \"%s\"", savedSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    for (int i = 0; i < 10; i++) {
      delay(50);
    }
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}
