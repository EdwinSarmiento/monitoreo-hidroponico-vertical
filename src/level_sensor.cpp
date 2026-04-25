#include "level_sensor.h"
#include "config.h"
#include "app_state.h"
#include <Arduino.h>

void setupLevelSensor() {
    pinMode(LEVEL_SENSOR_PIN, INPUT_PULLUP);
    Serial.println("[NIVEL] Sensor de nivel iniciado.");
}

void updateLevelSensor() {
    // Revisar cada 1 segundo
    if (millis() - lastLevelCheck < 1000) return;
    lastLevelCheck = millis();

    // El sensor cierra el circuito a GND cuando el nivel es ALTO (tanque lleno)
    // Usamos digitalRead con INPUT_PULLUP:
    //   LOW  = contacto cerrado = tanque lleno  (nivel OK)
    //   HIGH = contacto abierto = tanque vacío  (nivel bajo)
    bool rawState = digitalRead(LEVEL_SENSOR_PIN);
    
    isLevelLow = (rawState == HIGH);

    if (isLevelLow) {
        // Podríamos apagar la bomba aquí por seguridad si quisiéramos
        // pumpState = false; 
    }
}
