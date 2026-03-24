# Hidroponia IoT - ESP32

Sistema de control IoT para cultivo hidropónico basado en ESP32. Controla una bomba de agua principal, dos bombas peristálticas para regulación de pH, y monitorea el pH del agua en tiempo real desde una interfaz web accesible por WiFi.

## Funcionalidades

- **Sensor de pH** — Lectura en tiempo real con módulo PH-4502C (vía ADS1115 o GPIO34 directo)
- **WiFi auto-configurable** — Sin credenciales hardcodeadas. El ESP32 crea un portal WiFi para configurar la red desde el celular
- **Panel web responsive** — Interfaz moderna con gauge de pH, control de bombas, accesible desde cualquier navegador
- **Bomba de agua principal** — Control ON/OFF por relé (GPIO27)
- **Bombas peristálticas de pH** — Control independiente de pH+ y pH- (GPIO26, GPIO25)
- **Reset WiFi** — Desde la web o manteniendo el botón BOOT 3 segundos
- **Credenciales persistentes** — Se guardan en la memoria flash del ESP32, sobreviven reinicios y cortes de luz
- **Fallback automático** — Si no detecta el ADS1115, lee el pH directo por GPIO34

## Hardware

| Componente | Especificación | Cantidad |
|---|---|---|
| ESP32 DevKit | Cualquier variante | 1 |
| Fuente de alimentación | 12V 10A | 1 |
| Convertidor DC-DC | 12V → 5V (para ESP32 y módulo pH) | 1 |
| Módulo relé | 5V, 1 canal | 1 |
| Bomba de agua | 12V | 1 |
| Bomba peristáltica | Para regulación pH | 2 |
| Módulo sensor pH | PH-4502C con electrodo E201-BNC | 1 |
| ADC 16-bit | ADS1115 (opcional, mejora precisión) | 1 |
| Soluciones buffer | pH 4.0 y pH 7.0 (para calibración) | 1 kit |

## Pines GPIO

| Pin | Función |
|---|---|
| GPIO27 | Relé → Bomba de agua principal |
| GPIO26 | Bomba peristáltica pH+ (subir) |
| GPIO25 | Bomba peristáltica pH- (bajar) |
| GPIO21 | I2C SDA → ADS1115 |
| GPIO22 | I2C SCL → ADS1115 |
| GPIO34 | Lectura directa pH (fallback sin ADS1115) |
| GPIO0  | Botón BOOT → Reset WiFi (mantener 3s) |

## Esquema de conexión

### Sensor de pH

```
SONDA pH ──BNC──► MÓDULO PH-4502C ──Po──► ADS1115 (A0) ──I2C──► ESP32
                   (5V, GND)              (3.3V, GND)           (P21, P22)
```

**Módulo PH-4502C → ADS1115:**

```
PH-4502C             Alimentación
─────────            ──────────────
V+           ────    5V (del convertidor o VIN del ESP32)
GND          ────    GND común
Po (señal)   ────    A0 del ADS1115
Do           ────    (sin conectar)
To           ────    (sin conectar)
```

**ADS1115 → ESP32:**

```
ADS1115              ESP32
─────────            ──────
VDD          ────    3.3V (pin 3V3)
GND          ────    GND
SCL          ────    GPIO22 (P22)
SDA          ────    GPIO21 (P21)
ADDR         ────    GND (puente obligatorio)
A0           ────    Po del PH-4502C
ALRT         ────    (sin conectar)
```

**Sin ADS1115 (conexión directa):**

```
PH-4502C             ESP32
─────────            ──────
V+           ────    5V (VIN)
GND          ────    GND
Po (señal)   ────    GPIO34 (P34)
```

> Nota: La lectura directa por GPIO34 funciona pero tiene menor precisión (12-bit vs 16-bit del ADS1115).

### Bombas y relé

```
FUENTE 12V 10A
    │
    ├───────── [RELÉ COM/NO] ──── Bomba de agua 12V
    │
    ├── Convertidor 12V → 5V ──── ESP32 VIN (5V)
    │                              ESP32 GND
    │
    └── GND ───────────────────── GND común

ESP32 GPIO27 ──── IN  Relé (bomba principal)
ESP32 GPIO26 ──── IN  Driver bomba peristáltica pH+
ESP32 GPIO25 ──── IN  Driver bomba peristáltica pH-
ESP32 5V     ──── VCC módulos
ESP32 GND    ──── GND módulos
```

**Conexión del relé (lado de potencia):**
- COM → Cable positivo (+) de la bomba
- NO (Normally Open) → Positivo (+) de la fuente 12V
- El negativo (-) de la bomba va directo al negativo (-) de la fuente

## Calibración del sensor de pH

Se requieren las soluciones buffer pH 4.0 y pH 7.0 incluidas en el kit.

1. Sumergir la sonda en **buffer pH 7.0** y esperar 1-2 minutos
2. Anotar el voltaje estable del monitor serie
3. Enjuagar la sonda, sumergir en **buffer pH 4.0** y esperar 1-2 minutos
4. Anotar el voltaje estable
5. Actualizar los valores en `include/config.h`:

```c
#define PH_VOLTAGE_AT_7  1.293f  // Voltaje medido en buffer pH 7.0
#define PH_VOLTAGE_AT_4  1.818f  // Voltaje medido en buffer pH 4.0
```

> Los valores de ejemplo corresponden a la calibración realizada con lectura por GPIO34. Si se usa ADS1115, recalibrar.

## Configuración WiFi

No se necesita editar ningún archivo. El ESP32 se configura solo:

1. **Primera vez:** El ESP32 crea la red WiFi `Hidroponia-Setup` (password: `hidroponia123`)
2. Conectate a esa red desde tu celular
3. Abrí el navegador e ingresá a `http://192.168.4.1`
4. Seleccioná tu red WiFi e ingresá la contraseña
5. El ESP32 se reinicia y se conecta automáticamente

Para cambiar la red WiFi después:
- **Desde la web:** botón "Cambiar red WiFi" en el panel
- **Físicamente:** mantener el botón BOOT del ESP32 por 3 segundos

## Compilar y subir

Requiere [PlatformIO](https://platformio.org/).

```bash
# Compilar y subir al ESP32
pio run --target upload --upload-port /dev/cu.usbserial-0001

# Monitor serie (ver IP, pH, estados)
pio device monitor --port /dev/cu.usbserial-0001
```

## Uso

1. Subir el firmware al ESP32 por USB
2. Configurar el WiFi desde el portal (primera vez)
3. Abrir el monitor serie para ver la IP asignada
4. En el navegador, ir a `http://<IP_DEL_ESP32>`
5. Monitorear el pH y controlar las bombas desde el panel web

## Estructura del proyecto

```
esp32/
├── platformio.ini        # Configuración PlatformIO y dependencias
├── include/
│   └── config.h          # Pines GPIO, calibración pH, configuración AP
├── src/
│   └── main.cpp          # Código principal (WiFi, web server, pH, bombas)
└── README.md
```

## Próximos pasos

- [x] Sensor de pH (PH-4502C + ADS1115/GPIO34)
- [x] Bombas peristálticas para regulación de pH
- [x] Portal de configuración WiFi
- [ ] Sensor de temperatura del agua
- [ ] Sensor de nivel de agua
- [ ] Sensor de EC (conductividad eléctrica)
- [ ] Temporizador automático para la bomba
- [ ] OTA (actualización remota por WiFi)
- [ ] Dashboard con gráficos históricos
- [ ] Alertas por MQTT / Telegram
