#ifndef CONFIG_H
#define CONFIG_H

// =============================================
//  Pines GPIO
// =============================================
#define RELAY_PUMP_PIN    27  // GPIO27 -> Relé -> Bomba de agua principal
#define PERISTALTIC_1_PIN 26  // GPIO26 -> Bomba peristáltica pH (subir)
#define PERISTALTIC_2_PIN 25  // GPIO25 -> Bomba peristáltica pH (bajar)
#define WIFI_RESET_PIN     0  // GPIO0 (botón BOOT) -> Mantener 3s para resetear WiFi
#define DHT_PIN           17  // GPIO17 -> Sensor AM2305B (Temp/Hum)
#define LEVEL_SENSOR_PIN  16  // GPIO16 -> Interruptor de flotador (Nivel)

// =============================================
//  I2C - ADS1115 (sensor de pH)
// =============================================
#define I2C_SDA  21  // GPIO21 -> SDA del ADS1115
#define I2C_SCL  22  // GPIO22 -> SCL del ADS1115
#define PH_ADS_CHANNEL  0  // Canal A0 del ADS1115 -> Señal del sensor pH

// Calibración del sensor de pH (ajustar con soluciones buffer)
// Voltaje a pH 7.0 (solución neutra) y pH 4.0 (solución ácida)
#define PH_VOLTAGE_AT_7  1.293f  // Voltaje en V cuando el pH es 7.0 (calibrado)
#define PH_VOLTAGE_AT_4  1.818f  // Voltaje en V cuando el pH es 4.0 (calibrado)

#define PH_READ_INTERVAL  2000  // Leer pH cada 2 segundos (ms)
#define PH_SAMPLES         10   // Promedio de N lecturas para estabilizar

// =============================================
//  Temperatura y Humedad (AM2305B)
// =============================================
#define DHT_TYPE          DHT21 // AM2305B usa el protocolo DHT21/AM2301
#define ENV_READ_INTERVAL 5000  // Leer cada 5 segundos

// =============================================
//  Configuración WiFiManager
// =============================================
#define AP_NAME     "Hidroponia-Setup"
#define AP_PASSWORD "hidroponia123"

// =============================================
//  Configuración MQTT
// =============================================
// La IP/puerto del broker se configuran desde el portal web de la ESP32.
// DEVICE_TOKEN es el valor por defecto; el portal puede reemplazarlo.
#define DEVICE_TOKEN "esp32_hidro"

// =============================================
//  Credenciales Admin (Portal Interno)
// =============================================
#define ADMIN_USER "admin"
#define ADMIN_PASS "admin"

#endif
