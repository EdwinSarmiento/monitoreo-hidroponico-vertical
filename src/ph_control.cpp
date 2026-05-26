#include "ph_control.h"
#include "app_state.h"
#include "actuators.h"
#include "config.h"

bool phAutoEnabled = false;
float phTarget = 6.0f;
float phTolerance = 0.3f;
uint16_t phDoseSeconds = 1;
uint16_t phSettleSeconds = 90;

PhCycleState phCycleState = PH_IDLE;
String phCycleDirection = "";

static unsigned long cycleStartMs = 0;
static unsigned long cycleDurationMs = 0;

void loadPhControlConfig() {
    prefs.begin("phctrl", true);
    phTarget = prefs.getFloat("target", 6.0f);
    phTolerance = prefs.getFloat("tol", 0.3f);
    phDoseSeconds = prefs.getUShort("dose", 1);
    phSettleSeconds = prefs.getUShort("settle", 90);
    prefs.end();
    Serial.printf("[pH Ctrl] Config: target=%.1f tol=±%.1f dosis=%ds espera=%ds\n",
                  phTarget, phTolerance, phDoseSeconds, phSettleSeconds);
}

void savePhControlConfig() {
    prefs.begin("phctrl", false);
    prefs.putFloat("target", phTarget);
    prefs.putFloat("tol", phTolerance);
    prefs.putUShort("dose", phDoseSeconds);
    prefs.putUShort("settle", phSettleSeconds);
    prefs.end();
    Serial.println("[pH Ctrl] Config guardada en NVS.");
}

static void startDose(const char* dir) {
    phCycleState = PH_DOSING;
    phCycleDirection = dir;
    cycleStartMs = millis();
    cycleDurationMs = (unsigned long)phDoseSeconds * 1000UL;

    if (strcmp(dir, "up") == 0) {
        setPeristaltic(1, true);
        setPeristaltic(2, false);
        Serial.printf("[pH Ctrl] pH %.2f < %.1f → Bomba pH+ ON (%ds)\n",
                      currentPH, phTarget - phTolerance, phDoseSeconds);
    } else {
        setPeristaltic(1, false);
        setPeristaltic(2, true);
        Serial.printf("[pH Ctrl] pH %.2f > %.1f → Bomba pH- ON (%ds)\n",
                      currentPH, phTarget + phTolerance, phDoseSeconds);
    }
}

static void startSettle() {
    setPeristaltic(1, false);
    setPeristaltic(2, false);

    phCycleState = PH_SETTLING;
    cycleStartMs = millis();
    cycleDurationMs = (unsigned long)phSettleSeconds * 1000UL;
    Serial.printf("[pH Ctrl] Dosis completada. Esperando %ds para estabilización...\n", phSettleSeconds);
}

void cancelPhCycle() {
    if (phCycleState == PH_DOSING) {
        setPeristaltic(1, false);
        setPeristaltic(2, false);
    }
    phCycleState = PH_IDLE;
    phCycleDirection = "";
    Serial.println("[pH Ctrl] Ciclo cancelado.");
}

void updatePhControl() {
    if (!phAutoEnabled) return;

    switch (phCycleState) {
        case PH_IDLE: {
            if (currentPH < 0.01f) return;  // sensor not ready

            float lo = phTarget - phTolerance;
            float hi = phTarget + phTolerance;

            if (currentPH < lo) {
                startDose("up");
            } else if (currentPH > hi) {
                startDose("down");
            }
            break;
        }

        case PH_DOSING: {
            if (millis() - cycleStartMs >= cycleDurationMs) {
                startSettle();
            }
            break;
        }

        case PH_SETTLING: {
            if (millis() - cycleStartMs >= cycleDurationMs) {
                phCycleState = PH_IDLE;
                phCycleDirection = "";
                Serial.printf("[pH Ctrl] Estabilización completa. pH actual: %.2f\n", currentPH);
            }
            break;
        }
    }
}
