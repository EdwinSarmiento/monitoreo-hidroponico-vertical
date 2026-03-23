#ifndef CONFIG_H
#define CONFIG_H

// =============================================
//  Pines GPIO
// =============================================
#define RELAY_PUMP_PIN    27  // GPIO27 -> Relé -> Bomba de agua principal
#define PERISTALTIC_1_PIN 26  // GPIO26 -> Bomba peristáltica pH (subir)
#define PERISTALTIC_2_PIN 25  // GPIO25 -> Bomba peristáltica pH (bajar)
#define WIFI_RESET_PIN     0  // GPIO0 (botón BOOT) -> Mantener 3s para resetear WiFi

// =============================================
//  Configuración WiFiManager
// =============================================
#define AP_NAME     "Hidroponia-Setup"
#define AP_PASSWORD "hidroponia123"

#endif
