#include "ph_sensor.h"
#include "app_state.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

float readPHVoltage() {
  if (!adsReady) return 0.0f;

  long total = 0;
  int validSamples = 0;
  for (int i = 0; i < PH_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(PH_ADS_CHANNEL);
    if (raw >= 0 && raw < 32767) {
      total += raw;
      validSamples++;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (validSamples == 0) return 0.0f;
  float avgRaw = (float)total / validSamples;
  return ads.computeVolts(avgRaw);
}

float voltageToPH(float voltage) {
  float slope = (7.0f - 4.0f) / (PH_VOLTAGE_AT_7 - PH_VOLTAGE_AT_4);
  float ph = 7.0f + slope * (voltage - PH_VOLTAGE_AT_7);
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}

void updatePH() {
  if (millis() - lastPHRead < PH_READ_INTERVAL) return;
  lastPHRead = millis();
  currentVoltage = readPHVoltage();
  currentPH = voltageToPH(currentVoltage);
  Serial.printf("pH: %.2f (%.3fV) [ADS1115]\n", currentPH, currentVoltage);
}
