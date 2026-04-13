#include "actuators.h"
#include "app_state.h"
#include "config.h"

void setPump(bool on) {
  pumpState = on;
  digitalWrite(RELAY_PUMP_PIN, on ? HIGH : LOW);
  Serial.printf("Bomba principal: %s\n", on ? "ON" : "OFF");
}

void setPeristaltic(int id, bool on) {
  if (id == 1) {
    peri1State = on;
    digitalWrite(PERISTALTIC_1_PIN, on ? HIGH : LOW);
    Serial.printf("Peristaltica pH+: %s\n", on ? "ON" : "OFF");
  } else if (id == 2) {
    peri2State = on;
    digitalWrite(PERISTALTIC_2_PIN, on ? HIGH : LOW);
    Serial.printf("Peristaltica pH-: %s\n", on ? "ON" : "OFF");
  }
}
