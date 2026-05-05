#include "level_sensor.h"
#include "config.h"
#include "app_state.h"
#include <Arduino.h>

void setupLevelSensor() {
    pinMode(LEVEL_SENSOR_PIN, INPUT_PULLUP);
    Serial.println("[NIVEL] Sensor de nivel iniciado.");
}

void updateLevelSensor() {
    if (millis() - lastLevelCheck < 1000) return;
    lastLevelCheck = millis();

    // INPUT_PULLUP: LOW = contacto cerrado (nivel OK), HIGH = contacto abierto (nivel bajo)
    isLevelLow = (digitalRead(LEVEL_SENSOR_PIN) == HIGH);
}
