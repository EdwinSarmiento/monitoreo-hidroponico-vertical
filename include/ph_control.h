#ifndef PH_CONTROL_H
#define PH_CONTROL_H

#include <Arduino.h>

// Estados del ciclo de regulación
enum PhCycleState { PH_IDLE, PH_DOSING, PH_SETTLING };

// Configuración de auto-regulación (modificable via MQTT)
extern bool phAutoEnabled;
extern float phTarget;
extern float phTolerance;
extern uint16_t phDoseSeconds;
extern uint16_t phSettleSeconds;

// Estado actual del ciclo (publicado via MQTT)
extern PhCycleState phCycleState;
extern String phCycleDirection;  // "up", "down", ""

void loadPhControlConfig();
void savePhControlConfig();
void updatePhControl();
void cancelPhCycle();

#endif
