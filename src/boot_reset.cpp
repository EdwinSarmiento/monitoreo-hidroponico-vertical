#include <Arduino.h>
#include "boot_reset.h"
#include "config.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WIFI_RESET_HOLD_MS 3000

static void bootResetTask(void *arg) {
  (void)arg;
  for (;;) {
    pollBootReset();
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}

void startBootResetMonitor() {
  static bool started = false;
  if (started) return;
  started = true;
  // Core 1 = loop() habitual; Core 0 = solo esto => BOOT funciona durante bloqueos en Core 1
  xTaskCreatePinnedToCore(bootResetTask, "bootrst", 3072, nullptr, 2, nullptr, 0);
}

void pollBootReset() {
  static bool pinWasHigh = true;
  static unsigned long holdStart = 0;
  static uint8_t holdSecPrinted = 0;

  bool pressed = (digitalRead(WIFI_RESET_PIN) == LOW);
  if (pressed) {
    if (pinWasHigh) {
      holdStart = millis();
      pinWasHigh = false;
      holdSecPrinted = 0;
      Serial.println("BOOT: GPIO0 en LOW — mantener 3s para borrar WiFi/MQTT...");
    }
    unsigned long held = millis() - holdStart;
    uint8_t sec = (uint8_t)(held / 1000u);
    if (sec > holdSecPrinted && sec >= 1u && sec <= 3u) {
      holdSecPrinted = sec;
      Serial.printf("BOOT: %us / 3\n", (unsigned)sec);
    }
    if (held >= (unsigned long)WIFI_RESET_HOLD_MS) {
      Serial.println("Reset WiFi (BOOT 3s)!");
      clearAllConfig();
      delay(500);
      ESP.restart();
    }
  } else {
    pinWasHigh = true;
    holdSecPrinted = 0;
  }
}

void delayWithBootPoll(unsigned long ms) {
  vTaskDelay(pdMS_TO_TICKS(ms));
}
