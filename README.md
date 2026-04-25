# Hidroponia IoT - ESP32

Sistema de control IoT para cultivo hidropónico basado en ESP32. Monitorea el pH del agua en tiempo real mediante un sensor PH-4502C con ADC externo ADS1115, temperatura y humedad ambiental con sensor AM2305B, y nivel de agua con interruptor de flotador DPL-1-BK. Controla una bomba de agua principal junto con dos bombas peristálticas para regulación de pH, todo desde un dashboard web accesible por WiFi con alertas y registro histórico.

## Funcionalidades

- **Sensor de pH con ADS1115** — Lectura de alta precisión (16-bit) vía I2C con promedio de 10 muestras y validación de datos
- **Sensor de temperatura y humedad (AM2305B)** — Lectura ambiental cada 5 segundos vía protocolo DHT21 (GPIO17)
- **Sensor de nivel de agua (DPL-1-BK)** — Interruptor de flotador para detectar nivel bajo (GPIO16)
- **Alerta de nivel bajo** — Banner visual con sonido cuando el nivel de agua baja, silenciable por 5 minutos
- **Historial de humedad** — Registro diario (mín/máx/promedio) almacenado localmente hasta 30 días
- **WiFi auto-configurable** — Portal cautivo para configurar la red desde el celular, sin credenciales hardcodeadas
- **Dashboard web responsive** — Interfaz moderna con gauge de pH, ambiente, nivel de agua, controles y tabla histórica
- **Bomba de agua principal** — Control ON/OFF por relé (GPIO27)
- **Bombas peristálticas de pH** — Control independiente de pH+ (subir) y pH- (bajar)
- **Reset WiFi** — Desde la interfaz web o manteniendo el botón BOOT del ESP32 por 3 segundos
- **Credenciales persistentes** — Se guardan en NVS (flash), sobreviven reinicios y cortes de luz
- **Diagnóstico I2C al arranque** — Escaneo completo del bus I2C y lectura de los 4 canales del ADS1115
- **Protección anti-bloqueo** — Timeout de 50ms en I2C para evitar que un cable suelto congele el ESP32

## Hardware

| Componente | Especificación | Cantidad |
|---|---|---|
| ESP32 DevKit | Con placa de borneras (screw terminals) | 1 |
| Fuente de alimentación | 12V 10A | 1 |
| Convertidor DC-DC | 12V → 5V (para ESP32 y módulo pH) | 1 |
| Módulo relé | 5V, 1 canal | 1 |
| Bomba de agua | 12V | 1 |
| Bomba peristáltica | Para regulación pH | 2 |
| Módulo sensor pH | PH-4502C con electrodo E201-BNC | 1 |
| ADC 16-bit | ADS1115 (I2C, dirección 0x48) | 1 |
| Sensor temp/humedad | AM2305B (protocolo DHT21) | 1 |
| Sensor de nivel | DPL-1-BK (interruptor de flotador) | 1 |
| Soluciones buffer | pH 4.0 y pH 7.0 (para calibración) | 1 kit |

## Pines GPIO

| Pin | Función |
|---|---|
| GPIO27 | Relé → Bomba de agua principal |
| GPIO26 | Bomba peristáltica pH+ (subir) |
| GPIO25 | Bomba peristáltica pH- (bajar) |
| GPIO21 | I2C SDA → ADS1115 |
| GPIO22 | I2C SCL → ADS1115 |
| GPIO17 | Datos → Sensor AM2305B (Temp/Humedad) |
| GPIO16 | Señal → Flotador DPL-1-BK (Nivel de agua) |
| GPIO0  | Botón BOOT → Reset WiFi (mantener 3s) |

## Esquema de conexión

### Sensor de pH (PH-4502C → ADS1115 → ESP32)

```
SONDA pH ──BNC──► MÓDULO PH-4502C ──Po──► ADS1115 (A0) ──I2C──► ESP32
                   (5V, GND)              (3.3V, GND)           (P21, P22)
```

**Módulo PH-4502C:**

```
PH-4502C             Alimentación
─────────            ──────────────
V+           ────    5V (del convertidor DC-DC)
GND          ────    GND común
Po (señal)   ────    A0 del ADS1115
Do           ────    (sin conectar)
To           ────    (sin conectar)
```

**ADS1115 → ESP32:**

```
ADS1115              ESP32
─────────            ──────
VDD          ────    3.3V (pin 3V3)     ⚠️ NO conectar a 5V
GND          ────    GND
SCL          ────    GPIO22 (P22)
SDA          ────    GPIO21 (P21)
ADDR         ────    GND (obligatorio, fija dirección 0x48)
A0           ────    Po del PH-4502C
A1-A3        ────    (sin conectar, libres para otros sensores)
ALRT         ────    (sin conectar, no necesario para polling)
```

> **Importante:** El ADS1115 se alimenta con **3.3V** del ESP32 (no 5V). Si se alimenta con 5V, las líneas I2C envían 5V a los GPIO del ESP32 que solo toleran 3.3V, lo que puede dañar el microcontrolador. El módulo PH-4502C sí necesita 5V para funcionar correctamente.

### Sensor de temperatura y humedad (AM2305B)

```
AM2305B              ESP32
─────────            ──────
VCC (rojo)   ────    3.3V (pin 3V3)
GND (negro)  ────    GND
DATA (amarillo) ──   GPIO17
```

> **Nota:** El AM2305B utiliza el protocolo DHT21. Algunos modelos incluyen resistencia pull-up interna; si la lectura es inestable, agregar una resistencia de 4.7kΩ entre DATA y VCC.

### Sensor de nivel de agua (DPL-1-BK)

```
DPL-1-BK             ESP32
─────────            ──────
Cable 1      ────    GPIO16
Cable 2      ────    GND
```

> **Funcionamiento:** El flotador es un interruptor reed (ON/OFF). Cuando el nivel baja, el flotador cierra el circuito a GND → el ESP32 lee `LOW` en GPIO16 (configurado como `INPUT_PULLUP`). Solo reporta dos estados: nivel OK o nivel BAJO.

### Bombas y relé

```
FUENTE 12V 10A
    │
    ├───────── [RELÉ COM/NO] ──── Bomba de agua 12V
    │
    ├── Convertidor 12V → 5V ──── ESP32 VIN (5V)
    │                              PH-4502C V+
    │
    └── GND ───────────────────── GND común (ESP32, ADS1115, PH-4502C, relé, AM2305B, DPL-1-BK)
```

## Mapeo de Pines GPIO (Hardware)

Esta tabla detalla el uso actual de los pines del ESP32 DevKit v1:

| Categoría | Pin GPIO | Función / Dispositivo | Estado |
| :--- | :---: | :--- | :---: |
| **Sistema** | `GND` | Tierra común (Bus GND) | Ocupado |
| **Sistema** | `VIN (5V)` | Alimentación desde convertidor | Ocupado |
| **Comunicación** | `GPIO 21` | I2C SDA (Datos ADS1115) | Ocupado |
| **Comunicación** | `GPIO 22` | I2C SCL (Reloj ADS1115) | Ocupado |
| **Control** | `GPIO 27` | Relé - Bomba de Agua Principal | Ocupado |
| **Control** | `GPIO 26` | Bomba Peristáltica 1 (pH+) | Ocupado |
| **Control** | `GPIO 25` | Bomba Peristáltica 2 (pH-) | Ocupado |
| **Sensor** | `GPIO 17` | AM2305B - Temperatura/Humedad | Ocupado |
| **Sensor** | `GPIO 16` | DPL-1-BK - Nivel de Agua | Ocupado |
| **Entrada** | `GPIO 0` | Botón BOOT (Reset WiFi) | Ocupado |
| **Disponible** | `GPIO 4, 13, 14` | Pines digitales libres | **LIBRE** |
| **Disponible** | `GPIO 32, 33` | Pines analógicos/digitales libres | **LIBRE** |
| **Disponible** | `GPIO 34, 35` | Pines **solo entrada** | **LIBRE** |
| **Disponible** | `GPIO 2` | LED interno (a veces usado) | **LIBRE** |

**Conexión del relé (lado de potencia):**
- COM → Cable positivo (+) de la bomba
- NO (Normally Open) → Positivo (+) de la fuente 12V
- El negativo (-) de la bomba va directo al negativo (-) de la fuente

## Infraestructura y Puertos (Docker)

Para el funcionamiento del sistema desacoplado, se utilizan los siguientes puertos:

| Servicio | Puerto Externo | Puerto Interno | Uso |
| :--- | :---: | :---: | :--- |
| **MQTT Broker** | `1883` | `1883` | Conexión del hardware (ESP32) |
| **MQTT WebSockets** | `9001` | `9001` | Conexión del Dashboard Web |
| **Web UI (Nginx)** | `8080` | `80` | Acceso al panel de control |
| **Config Portal** | `80` | `80` | Modo configuración del ESP32 |

### Puertos Disponibles para Expansión
- `3000`: Reservado para Grafana (Visualización).
- `8086`: Reservado para InfluxDB (Historial).
- `5432`: Reservado para PostgreSQL.

## Calibración del sensor de pH

Se requieren las soluciones buffer pH 4.0 y pH 7.0 incluidas en el kit. Cada vez que se cambie de método de lectura (GPIO directo vs ADS1115) o se cambie el módulo, se debe recalibrar.

### Procedimiento

1. Conectar el monitor serial para ver las lecturas en tiempo real
2. Sumergir la sonda en **buffer pH 7.0** y esperar ~2 minutos hasta que el voltaje se estabilice
3. Anotar el voltaje estable
4. Enjuagar la sonda con agua limpia, secar suavemente
5. Sumergir en **buffer pH 4.0** y esperar ~2 minutos
6. Anotar el voltaje estable
7. Actualizar los valores en `include/config.h`:

```c
#define PH_VOLTAGE_AT_7  1.293f  // Voltaje medido en buffer pH 7.0
#define PH_VOLTAGE_AT_4  1.818f  // Voltaje medido en buffer pH 4.0
```

8. Compilar y subir el firmware actualizado

> **Nota:** Los valores de ejemplo corresponden a la calibración realizada con lectura directa por GPIO34. Al usar el ADS1115 los voltajes serán diferentes y se debe recalibrar.

## Configuración WiFi

No se necesita editar ningún archivo. El ESP32 se configura solo:

1. **Primera vez:** El ESP32 crea la red WiFi `Hidroponia-Setup` (password: `hidroponia123`)
2. Conectarse a esa red desde el celular
3. Abrir el navegador e ingresar a `http://192.168.4.1`
4. Seleccionar la red WiFi de destino e ingresar la contraseña
5. El ESP32 se reinicia y se conecta automáticamente

Para cambiar la red WiFi después:
- **Desde la web:** botón "Cambiar red WiFi" en el panel
- **Físicamente:** mantener el botón BOOT del ESP32 por 3 segundos

## Requisitos

- [PlatformIO](https://platformio.org/) (CLI o extensión de VS Code / Cursor)
- Cable USB para conexión al ESP32
- Si PlatformIO no está en el PATH:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

## Comandos

### Compilar y subir al ESP32

```bash
pio run --target upload --upload-port /dev/cu.usbserial-0001
```

### Monitor serial (ver IP, pH, estados, diagnóstico I2C)

```bash
pio device monitor --port /dev/cu.usbserial-0001 --baud 115200
```

Para salir del monitor: `Ctrl+C`

### Compilar y subir + abrir monitor (todo junto)

```bash
pio run --target upload --upload-port /dev/cu.usbserial-0001 && pio device monitor --port /dev/cu.usbserial-0001 --baud 115200
```

### Borrar flash completa (soluciona NVS corrupta o error "Set status to INIT")

```bash
pio run --target erase --upload-port /dev/cu.usbserial-0001
```

> Después de borrar la flash hay que volver a subir el firmware y reconfigurar el WiFi.

### Solo compilar (sin subir)

```bash
pio run
```

## Uso

1. Subir el firmware al ESP32 por USB
2. Abrir el monitor serial para ver la IP asignada
3. Configurar el WiFi desde el portal cautivo (primera vez)
4. En el navegador, ir a `http://<IP_DEL_ESP32>` (ejemplo: `http://192.168.0.26`)
5. Monitorear el pH y controlar las bombas desde el panel web

### Salida esperada del monitor serial

```
=== Hidroponia IoT - ESP32 ===
Escaneando dispositivos I2C...
  Dispositivo I2C encontrado en 0x48
ADS1115 OK en direccion 0x48
Leyendo todos los canales del ADS1115:
  Canal A0: raw=4762  volts=0.5953V
  Canal A1: raw=4748  volts=0.5935V
  Canal A2: raw=4752  volts=0.5940V
  Canal A3: raw=4752  volts=0.5940V
Conectando a "FAMILIA RODRIGUEZ"......
WiFi conectado!
Red: FAMILIA RODRIGUEZ
IP: 192.168.0.26
Abrir en navegador: http://192.168.0.26
pH: 6.50 (1.380V) [ADS1115]
pH: 6.51 (1.378V) [ADS1115]
```

## Solución de problemas

| Problema | Causa probable | Solución |
|---|---|---|
| `Ningun dispositivo I2C detectado` | Cableado SDA/SCL suelto o ADS1115 mal soldado | Verificar conexiones P21 (SDA), P22 (SCL). Resoldar el módulo si es necesario |
| `ADS1115 no detectado` | Pin ADDR no conectado a GND | Conectar ADDR a GND para fijar dirección 0x48 |
| Voltaje fijo (~0.597V) que no cambia | Falso contacto en cable Po → A0 | Verificar/reemplazar el cable entre Po del PH-4502C y A0 del ADS1115 |
| ESP32 se bloquea/congela | Cable I2C suelto durante operación | Ya implementado Wire.setTimeOut(50ms). Verificar cableado |
| `wifi:Set status to INIT` en loop | NVS corrupta | `pio run --target erase` y volver a subir firmware |
| Puerto no encontrado | Monitor serial abierto o puerto incorrecto | Cerrar el monitor antes de subir. Verificar puerto con `ls /dev/cu.*` |
| pH muestra "Sensor no detectado" | ADS1115 no inicializó correctamente | Reiniciar ESP32. Verificar alimentación 3.3V y conexiones I2C |

## Estructura del proyecto

```
hydroponics-iot-ph-controller/
├── platformio.ini              # Configuración PlatformIO y dependencias
├── include/
│   ├── config.h                # Pines GPIO, calibración pH, config AP
│   ├── app_state.h             # Variables de estado compartidas
│   ├── dht_sensor.h            # Sensor AM2305B (temp/humedad)
│   ├── level_sensor.h          # Sensor DPL-1-BK (nivel de agua)
│   ├── ph_sensor.h             # Sensor de pH (ADS1115)
│   ├── hidro_mqtt.h            # Cliente MQTT
│   ├── actuators.h             # Bombas y relés
│   ├── portal_wifi.h           # Portal cautivo WiFi
│   ├── portal_admin.h          # Panel admin
│   ├── storage.h               # Persistencia NVS
│   ├── boot_reset.h            # Reset por botón BOOT
│   └── wifi_connect.h          # Conexión WiFi
├── src/
│   ├── main.cpp                # Orquestación principal
│   ├── dht_sensor.cpp          # Lectura AM2305B
│   ├── level_sensor.cpp        # Lectura flotador DPL-1-BK
│   ├── ph_sensor.cpp           # Lectura pH vía ADS1115
│   ├── hidro_mqtt.cpp          # Publicación/suscripción MQTT
│   ├── actuators.cpp           # Control de bombas
│   ├── app_state.cpp           # Variables globales
│   ├── portal_admin.cpp        # Servidor web admin
│   ├── portal_wifi.cpp         # Portal cautivo
│   ├── storage.cpp             # Lectura/escritura NVS
│   ├── boot_reset.cpp          # Monitor botón BOOT (Core 0)
│   └── wifi_connect.cpp        # Conexión WiFi
├── web-ui/
│   ├── index.html              # Dashboard principal
│   ├── app.js                  # Lógica MQTT, alertas, historial
│   └── style.css               # Estilos del dashboard
├── docker-iot/                 # Mosquitto + Web UI (desarrollo)
├── docker-prod/                # Configuración producción
└── README.md
```

## Librerías

| Librería | Versión | Uso |
|---|---|---|
| AsyncTCP | ^1.1.1 | TCP asíncrono para el servidor web |
| ESPAsyncWebServer | ^1.2.4 | Servidor web HTTP asíncrono |
| Adafruit ADS1X15 | ^2.5.0 | Driver del ADC ADS1115 (I2C) |
| DHT sensor library | ^1.4.6 | Lectura del sensor AM2305B (DHT21) |
| ArduinoJson | ^6.21.0 | Serialización JSON para MQTT |
| PubSubClient | ^2.8 | Cliente MQTT |

## API REST (endpoints del ESP32)

| Método | Ruta | Descripción |
|---|---|---|
| GET | `/` | Interfaz web principal |
| GET | `/state` | Estado JSON: pH, voltaje, bombas, WiFi, IP |
| POST | `/pump?action=on\|off` | Encender/apagar bomba principal |
| POST | `/peristaltic?id=1\|2&action=on\|off` | Control bombas peristálticas |
| POST | `/reset-wifi` | Borra credenciales y reinicia en modo AP |

## Próximos pasos

- [x] Sensor de pH (PH-4502C + ADS1115)
- [x] Bombas peristálticas para regulación de pH
- [x] Portal de configuración WiFi
- [x] Diagnóstico I2C y lectura multi-canal al arranque
- [x] Protección anti-bloqueo I2C (timeout)
- [x] Sensor de temperatura y humedad ambiental (AM2305B)
- [x] Sensor de nivel de agua (DPL-1-BK)
- [x] Alerta visual y sonora de nivel bajo
- [x] Historial de humedad con tabla diaria (30 días, localStorage)
- [ ] Recalibrar pH con voltajes del ADS1115
- [ ] Sensor de EC (conductividad eléctrica)
- [ ] Temporizador automático para la bomba
- [ ] OTA (actualización remota por WiFi)
- [ ] Dashboard con gráficos históricos (Grafana + InfluxDB)
- [ ] Alertas por Telegram
