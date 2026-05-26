# Evidencias de Operación y Comunicación - Hidroponia IoT

Este documento recopila de manera formal y estructurada las evidencias técnicas de funcionamiento del sistema de control y monitoreo de cultivo hidropónico basado en el microcontrolador **ESP32**. Sirve como soporte experimental y de validación para la tesis del proyecto.

---

## Índice de Evidencias

1. [Arquitectura de Comunicación y Red](#1-arquitectura-de-comunicación-y-red)
2. [Registros del Broker Mosquitto](#2-registros-del-broker-mosquitto)
3. [Logs de Conexión del ESP32 (Monitor Serial)](#3-logs-de-conexión-del-esp32-monitor-serial)
4. [Estructura y Ejemplos de Mensajes MQTT](#4-estructura-y-ejemplos-de-mensajes-mqtt)
5. [Algoritmos de Validación de Sensores](#5-algoritmos-de-validación-de-sensores)
6. [Pruebas de Actuación y Control](#6-pruebas-de-actuación-y-control)
7. [Capturas del Dashboard e Implementación Física](#7-capturas-del-dashboard-e-implementación-física)

---

## 1. Arquitectura de Comunicación y Red

El sistema utiliza una arquitectura desacoplada basada en el protocolo **MQTT** para la transmisión de telemetría y el envío de comandos en tiempo real. 

```mermaid
graph TD
    ESP32[ESP32 Controller] -- Publica Estado /state (port 1883) --> Broker[Mosquitto Broker (Docker)]
    ESP32 -- Suscribe Comandos /cmd (port 1883) --> Broker
    Dashboard[Dashboard Web (Nginx/JS)] -- Suscribe Estado /state (port 9001 WS) --> Broker
    Dashboard -- Publica Comandos /cmd (port 9001 WS) --> Broker
```

- **ESP32**: Se conecta a la red local configurada por el usuario. Implementa el cliente MQTT usando la librería `PubSubClient` sobre el puerto TCP estándar **1883**.
- **Servidor de Monitoreo (Docker)**: Corre en un entorno contenedorizado con dos servicios principales expuestos:
  - **Mosquitto (Broker MQTT)**: Puerto `1883` para comunicación M2M (ESP32) y puerto `9001` (WebSockets) para comunicación con clientes web.
  - **Nginx (Servidor Web)**: Puerto `8080` (o dinámico según configuración `.env`) que sirve los archivos estáticos de la interfaz web de usuario (`web-ui/`).
- **Dashboard Web**: Desarrollado en JavaScript vainilla. Se conecta directamente al broker por WebSockets y actualiza la UI de forma reactiva al recibir payloads JSON.

---

## 2. Registros del Broker Mosquitto

A continuación, se presenta un extracto real del log del contenedor Mosquitto (`docker/mosquitto/log/mosquitto.log`), que evidencia la inicialización del broker, los puertos de escucha y la conexión concurrente de la placa ESP32 y del Dashboard Web:

```log
mosquitto version 2.1.2 starting
Config loaded from /mosquitto/config/mosquitto.conf.
Bridge support available.
Persistence support available.
TLS support available.
TLS-PSK support available.
Websockets support available.
Opening ipv4 listen socket on port 1883.
Opening ipv4 listen socket on port 9001.
mosquitto version 2.1.2 running

# Conexión del Dashboard Web vía WebSockets (Puerto 9001)
New connection from 192.168.65.1:21592 on port 9001.
New client connected from 192.168.65.1:21592 as web_ui_c45d4b (p4, c1, k60).

# Conexión del ESP32 vía TCP estándar (Puerto 1883)
New connection from 192.168.65.1:49647 on port 1883.
New client connected from 192.168.65.1:49647 as ESP32-B0:A7:32:2B:2B:A4 (p4, c1, k15).

# Desconexión y reconexión automática del ESP32 por control de timeout
Client ESP32-B0:A7:32:2B:2B:A4 [192.168.65.1:49647] disconnected.
New connection from 192.168.65.1:61882 on port 1883.
New client connected from 192.168.65.1:61882 as ESP32-B0:A7:32:2B:2B:A4 (p4, c1, k15).
```

### Análisis Técnico:
- El log confirma que Mosquitto levanta el socket TCP en el puerto `1883` para microcontroladores y el socket WebSocket en el puerto `9001` para navegadores.
- Identificador de cliente web: Generado dinámicamente como `web_ui_<hash>` para evitar colisiones.
- Identificador de cliente ESP32: Compuesto por el prefijo `ESP32-` seguido de su dirección MAC física (`ESP32-B0:A7:32:2B:2B:A4`), garantizando unicidad en la red.

---

## 3. Logs de Conexión del ESP32 (Monitor Serial)

La consola de depuración del ESP32 (115200 baudios) registra los eventos críticos de inicio, escaneo de hardware y conexión de red.

### Log de Inicialización de Red y Sensores:
```
=== Hidroponia IoT - ESP32 ===
Escaneando dispositivos I2C...
  Dispositivo I2C encontrado en 0x48
ADS1115 OK en direccion 0x48

Leyendo todos los canales del ADS1115 (Autodiagnóstico):
  Canal A0: raw=4762  volts=0.5953V
  Canal A1: raw=4748  volts=0.5935V
  Canal A2: raw=4752  volts=0.5940V
  Canal A3: raw=4752  volts=0.5940V

[DHT] Sensor iniciado.
[NIVEL] Sensor de nivel iniciado.
[MQTT Config] Server: 192.168.0.19:1883 Token: esp32_hidro Configurado: SI
Conectando a "FAMILIA RODRIGUEZ"......
WiFi conectado!
Red: FAMILIA RODRIGUEZ
IP: 192.168.0.26
Portal admin: http://192.168.0.26

[MQTT] Conectando a 192.168.0.19:1883...
[MQTT] Conectado!
[MQTT] Suscrito a: hidroponia/esp32_hidro/cmd
```

### Log de Operación en Bucle (Loop):
```
pH: 6.50 (1.381V) [ADS1115]
[DHT] Temp: 25.3C | Hum: 62.1%
[MQTT] Publicado -> pH: 6.50 V: 1.381 auto:OFF state:idle
```

### Análisis Técnico:
1. **Escaneo I2C**: Se realiza un barrido en el bus I2C al arrancar. La dirección `0x48` corresponde al conversor analógico-digital de 16 bits ADS1115.
2. **Lectura Multicanal**: Sirve para autodiagnóstico. Si las líneas de señal están flotando, marcan ~0.59V. Si hay una lectura estable en `A0`, es el sensor de pH (`PH-4502C`).
3. **Portal Admin**: En la IP local `192.168.0.26:80`, el ESP32 despliega un servidor HTTP asíncrono que permite reconfigurar las credenciales MQTT guardadas en la memoria flash NVS.
4. **Subscripción MQTT**: Al conectarse, el cliente se suscribe al tópico de comandos exclusivo (`hidroponia/esp32_hidro/cmd`).

---

## 4. Estructura y Ejemplos de Mensajes MQTT

La mensajería utiliza JSON para encapsular datos. Esto permite que el canal sea flexible para futuros sensores o mandos.

### A. Tópico de Estado (ESP32 $\rightarrow$ Dashboard)
* **Tópico**: `hidroponia/<deviceToken>/state` (Publicación periódica cada 2-5 segundos)
* **Ejemplo de Payload**:
```json
{
  "ph": 6.50,
  "phV": 1.381,
  "pump": false,
  "peri1": false,
  "peri2": false,
  "temp": 25.3,
  "hum": 62.1,
  "level": "OK",
  "ip": "192.168.0.26",
  "rssi": -45,
  "adsOk": true,
  "phAuto": true,
  "phTarget": 6.0,
  "phTol": 0.3,
  "phDose": 1,
  "phSettle": 90,
  "phState": "settling",
  "phDir": "down"
}
```

### B. Tópico de Comandos (Dashboard $\rightarrow$ ESP32)
* **Tópico**: `hidroponia/<deviceToken>/cmd` (Publicación ante acciones de usuario)
* **Ejemplos de Payload**:
  - Activar bomba principal de agua:
    ```json
    {"pump": true}
    ```
  - Configurar y activar auto-regulación de pH:
    ```json
    {
      "phAuto": true,
      "phTarget": 6.2,
      "phTol": 0.2,
      "phDose": 2,
      "phSettle": 60
    }
    ```
  - Activar bomba peristáltica pH- manualmente (solo si `phAuto` está apagado):
    ```json
    {"peri2": true}
    ```

---

## 5. Algoritmos de Validación de Sensores

El firmware incluye algoritmos específicos para filtrar ruido electromagnético, descartar lecturas erróneas y estabilizar la telemetría antes de ser publicada.

### A. Validación y Filtrado de pH (Conversor ADS1115 de 16-bits)
Ubicación del código: [ph_sensor.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/ph_sensor.cpp)

El sensor de pH toma $N$ muestras consecutivas y descarta lecturas anómalas (por ejemplo, valores negativos generados por un transitorio eléctrico o desconexión física).

```cpp
float readPHVoltage() {
  if (!adsReady) return 0.0f;

  long total = 0;
  int validSamples = 0;
  for (int i = 0; i < PH_SAMPLES; i++) { // PH_SAMPLES = 10
    int16_t raw = ads.readADC_SingleEnded(PH_ADS_CHANNEL);
    // Filtrado: el conversor entrega valores de 16-bits con signo (0-32767 positivos)
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
```

Posteriormente, el voltaje se mapea a pH usando una interpolación lineal de dos puntos de calibración (calibrados en laboratorio a pH 7.0 y pH 4.0):

```cpp
float voltageToPH(float voltage) {
  // Pendiente de respuesta del electrodo
  float slope = (7.0f - 4.0f) / (PH_VOLTAGE_AT_7 - PH_VOLTAGE_AT_4);
  float ph = 7.0f + slope * (voltage - PH_VOLTAGE_AT_7);
  
  // Acotamiento de rango físico
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}
```

### B. Validación de Temperatura y Humedad (Sensor AM2305B)
Ubicación del código: [dht_sensor.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/dht_sensor.cpp)

El protocolo DHT22 puede fallar debido a interferencia por cables largos. El firmware valida que la lectura sea un número real (`isnan`) antes de actualizar el estado global:

```cpp
void updateDHT() {
    if (millis() - lastEnvRead < ENV_READ_INTERVAL) return;
    lastEnvRead = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Validación de integridad
    if (isnan(h) || isnan(t)) {
        Serial.println("[DHT] Error leyendo sensor! Descartando muestra.");
        return;
    }

    currentHum = h;
    currentTemp = t;
}
```

### C. Validación de Nivel de Agua (Sensor de Flotador DPL-1-BK)
Ubicación del código: [level_sensor.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/level_sensor.cpp)

Usa un pin digital con resistencia de pull-up interna (`INPUT_PULLUP`). La lógica se lee cada segundo de manera filtrada:
- **LOW (0V)**: Flotador arriba (contacto cerrado $\rightarrow$ Nivel OK).
- **HIGH (3.3V)**: Flotador abajo (contacto abierto $\rightarrow$ Nivel BAJO).

---

## 6. Pruebas de Actuación y Control

El firmware controla los pines de potencia mediante relés y transistores. Se han validado las pruebas de encendido y apagado manual, así como el ciclo automático de pH.

### A. Control de Salidas de Potencia
Ubicación del código: [actuators.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/actuators.cpp)

```cpp
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
```

### B. Ciclos de la Auto-regulación de pH (Dosificación + Estabilización)
Ubicación del código: [ph_control.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/ph_control.cpp)

El sistema de lazo cerrado funciona según la siguiente máquina de estados del firmware:

1. **PH_IDLE**: Si el pH cae por debajo del umbral mínimo (`phTarget - phTolerance`), se activa el ciclo `up` (bomba peristáltica 1). Si supera el umbral máximo (`phTarget + phTolerance`), se activa el ciclo `down` (bomba peristáltica 2).
2. **PH_DOSING**: Se activa el actuador por un tiempo muy corto configurado por el usuario (`phDoseSeconds` de 1 a 30s) para evitar sobre-dosificación en el estanque.
3. **PH_SETTLING**: Se apaga el actuador y se espera a que la bomba principal recircule el agua homogéneamente durante el tiempo configurado (`phSettleSeconds` de 10 a 300s). Durante este periodo no se dosifica más.
4. **Retorno a PH_IDLE**: Una vez terminado el tiempo de espera, se evalúa nuevamente el valor de pH.

#### Captura de Consola en Prueba de Regulación Ácida (pH = 6.9, Objetivo = 6.0, Tolerancia = ±0.3):
```
[pH Ctrl] Estado: pH 6.9 > 6.3 → Iniciando regulación
[pH Ctrl] pH 6.90 > 6.30 → Bomba pH- ON (2s)
Peristaltica pH-: ON
[pH Ctrl] Dosis completada. Esperando 90s para estabilización...
Peristaltica pH-: OFF
... (90 segundos después) ...
[pH Ctrl] Estabilización completa. pH actual: 6.10
[pH Ctrl] Estado: pH en rango óptimo.
```

---

## 7. Capturas del Dashboard e Implementación Física

### A. Interfaz del Portal de Monitoreo (Dashboard Web)
Las capturas y respuestas visuales del dashboard de control y telemetría están contenidas en el repositorio como archivos HTML autodeclarados e interactivos, simulando los estados del sistema:

* **Lectura Óptima (pH 6.5)**: Simulación del panel cuando el cultivo se encuentra en perfectas condiciones.
  - Ver archivo: [evidencias/portal/ph-6.5.html](file:///Users/edwin/Documents/Tesis/esp32/evidencias/portal/ph-6.5.html)
* **Lectura Ácida Crítica (pH 4.3)**: Simulación del panel con alerta de estado crítico (fuera de rango óptimo) con barra destellante y aviso visual.
  - Ver archivo: [evidencias/portal/ph-4.3.html](file:///Users/edwin/Documents/Tesis/esp32/evidencias/portal/ph-4.3.html)

### B. Fotos de la Implementación Física del Cultivo Hidropónico
El montaje experimental del hardware del cultivo hidropónico IoT, incluyendo el cableado, las borneras del ESP32, los sensores en el estanque y las bombas peristálticas de pH, se encuentra registrado en el directorio [evidencias/implementacion/](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/). 

A continuación, se listan los archivos fotográficos de evidencia ordenados para consulta:

- **Placa Controladora y Borneras de Conexión**:
  - [Vista general del módulo de control](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT.jpeg)
  - [Conexiones y alimentación del ESP32 y sensores (Detalle)](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(2).jpeg)
  - [Esquema de distribución y cableado interno](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(3).jpeg)
- **Instalación de Bombas Peristálticas y de Recirculación**:
  - [Módulos de bombas peristálticas para pH+ y pH-](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(4).jpeg)
  - [Montaje y tubos de dosificación de reactivos químicos](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(5).jpeg)
  - [Bomba de recirculación de agua sumergible instalada](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(6).jpeg)
- **Instalación de Sensores en el Tanque de Nutrientes**:
  - [Sonda industrial de pH (E201-BNC) y sensor de nivel (DPL-1-BK)](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(7).jpeg)
  - [Detalle del sensor de nivel y flotador mecánico](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(8).jpeg)
  - [Sensor de temperatura y humedad AM2305B (Montaje ambiental)](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/Cultivo%20Hidroponico%20IOT%20(9).jpeg)
- **Fotografías Secuenciales del Montaje y Ensayos**:
  - [Registro fotográfico de montaje experimental (Fotos 10 a 27)](file:///Users/edwin/Documents/Tesis/esp32/evidencias/implementacion/)

---
*Fin del Reporte de Evidencias.*
