// =============================================
//  Hidroponia IoT - Firmware ESP32
//  main: orquestacion (portales, sensores, MQTT en modulos aparte)
// =============================================
#include <Arduino.h>
#include <Wire.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config.h"
#include "app_state.h"
#include "storage.h"
#include "ph_sensor.h"
#include "hidro_mqtt.h"
#include "portal_wifi.h"
#include "portal_admin.h"
#include "boot_reset.h"
#include "wifi_connect.h"

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(PERISTALTIC_1_PIN, OUTPUT);
  pinMode(PERISTALTIC_2_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  digitalWrite(PERISTALTIC_1_PIN, LOW);
  digitalWrite(PERISTALTIC_2_PIN, LOW);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  startBootResetMonitor();

  delay(1000);
  Serial.println("\n=== Hidroponia IoT - ESP32 ===");
  Serial.printf(
      "Pin BOOT (GPIO%d): sin pulsar debe leer HIGH. Ahora: %s\n",
      WIFI_RESET_PIN,
      digitalRead(WIFI_RESET_PIN) == HIGH ? "HIGH" : "LOW (revisar boton/USB)");
  Serial.println("[BOOT] Sondeo del boton en Core 0 (independiente del loop principal).");

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);

  Serial.println("Escaneando dispositivos I2C...");
  int devicesFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Dispositivo I2C encontrado en 0x%02X\n", addr);
      devicesFound++;
    }
  }
  if (devicesFound == 0) {
    Serial.println("  Ningun dispositivo I2C detectado! Verificar cableado SDA(21)/SCL(22).");
  }

  ads.setGain(GAIN_ONE);
  if (ads.begin()) {
    adsReady = true;
    Serial.println("ADS1115 OK en direccion 0x48");
    Serial.println("Leyendo todos los canales del ADS1115:");
    for (int ch = 0; ch < 4; ch++) {
      int16_t raw = ads.readADC_SingleEnded(ch);
      float volts = ads.computeVolts(raw);
      Serial.printf("  Canal A%d: raw=%d  volts=%.4fV\n", ch, raw, volts);
    }
  } else {
    Serial.println("ADS1115 no detectado! Verificar conexiones I2C y pin ADDR a GND.");
  }

  loadWiFiCredentials();
  loadMQTTConfig();

  if (savedSSID.length() > 0) {
    if (connectToWiFi()) {
      Serial.println("WiFi conectado!");
      Serial.print("Red: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      setupAppRoutes();
      server.begin();

      Serial.print("Portal admin: http://");
      Serial.println(WiFi.localIP());
      Serial.println("Usuario: admin / Password: admin");

      if (mqttConfigured) {
        connectMQTT();
      }
      Serial.println("[BOOT] Prueba: pulsa BOOT 3s; deben aparecer lineas \"BOOT: ...\" por serial.");
      return;
    }
    Serial.println("No se pudo conectar al WiFi guardado.");
  }

  Serial.println("Sin credenciales WiFi. Iniciando portal de configuracion...");
  startAPMode();
}

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
    return;
  }

  updatePH();

  if (mqttConfigured) {
    if (!mqttClient.connected()) {
      connectMQTT();
      delayWithBootPoll(2000);
    } else {
      mqttClient.loop();
      publishMQTT();
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reiniciando...");
    delayWithBootPoll(3000);
    ESP.restart();
  }
  vTaskDelay(pdMS_TO_TICKS(100));
}
