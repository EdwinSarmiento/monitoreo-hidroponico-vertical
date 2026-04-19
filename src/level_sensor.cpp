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

    // El sensor cierra el circuito a GND cuando el nivel es BAJO (depende del modelo, ajustable)
    // Usamos digitalRead. LOW = Activado (Nivel bajo detectado)
    bool rawState = digitalRead(LEVEL_SENSOR_PIN);
    
    // Si el flotador está configurado para abrirse cuando el nivel baja:
    // Aquí asumimos lógica inversa por el Pull-up (LOW = contacto cerrado)
    isLevelLow = (rawState == LOW);

    if (isLevelLow) {
        // Podríamos apagar la bomba aquí por seguridad si quisiéramos
        // pumpState = false; 
    }
}
