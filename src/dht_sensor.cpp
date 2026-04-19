#include "dht_sensor.h"
#include "config.h"
#include "app_state.h"

DHT dht(DHT_PIN, DHT_TYPE);

void setupDHT() {
    dht.begin();
    Serial.println("[DHT] Sensor iniciado.");
}

void updateDHT() {
    if (millis() - lastEnvRead < ENV_READ_INTERVAL) return;
    lastEnvRead = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println("[DHT] Error leyendo sensor!");
        return;
    }

    currentHum = h;
    currentTemp = t;

    Serial.printf("[DHT] Temp: %.1fC | Hum: %.1f%%\n", currentTemp, currentHum);
}
