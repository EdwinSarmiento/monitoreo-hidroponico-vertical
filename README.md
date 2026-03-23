# Hidroponia IoT - ESP32

Sistema de control IoT para cultivo hidropónico basado en ESP32. Controla una bomba de agua principal y dos bombas peristálticas para regulación de pH, todo desde una interfaz web accesible por WiFi.

## Funcionalidades

- **WiFi auto-configurable** — Sin credenciales hardcodeadas. El ESP32 crea un portal WiFi para configurar la red desde el celular
- **Panel web responsive** — Interfaz moderna accesible desde cualquier navegador en la misma red
- **Bomba de agua principal** — Control ON/OFF por relé (GPIO27)
- **Bombas peristálticas de pH** — Control independiente de pH+ y pH- (GPIO26, GPIO25)
- **Reset WiFi** — Desde la web o manteniendo el botón BOOT 3 segundos
- **Credenciales persistentes** — Se guardan en la memoria flash del ESP32, sobreviven reinicios y cortes de luz

## Hardware necesario

| Componente | Especificación | Cantidad |
|---|---|---|
| ESP32 DevKit | Cualquier variante | 1 |
| Fuente de alimentación | 12V 10A | 1 |
| Convertidor DC-DC | 12V → 5V (para ESP32) | 1 |
| Módulo relé | 5V, 1 canal | 1 |
| Bomba de agua | 12V | 1 |
| Bomba peristáltica | Para regulación pH | 2 |

## Pines GPIO

| Pin | Función |
|---|---|
| GPIO27 | Relé → Bomba de agua principal |
| GPIO26 | Bomba peristáltica pH+ (subir) |
| GPIO25 | Bomba peristáltica pH- (bajar) |
| GPIO0  | Botón BOOT → Reset WiFi (mantener 3s) |

## Esquema de conexión

```
FUENTE 12V 10A
    │
    ├───────── [RELÉ 1 COM/NO] ──── Bomba de agua 12V
    │
    ├── Convertidor 12V → 5V ────── ESP32 VIN (5V)
    │                                ESP32 GND
    │
    └── GND ─────────────────────── GND común

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
pio run --target upload

# Monitor serie (ver IP asignada)
pio device monitor
```

## Uso

1. Subir el firmware al ESP32 por USB
2. Configurar el WiFi desde el portal (primera vez)
3. Abrir el monitor serie para ver la IP asignada
4. En el navegador, ir a `http://<IP_DEL_ESP32>`
5. Controlar las bombas desde el panel web

## Estructura del proyecto

```
esp32/
├── platformio.ini        # Configuración PlatformIO y dependencias
├── include/
│   └── config.h          # Pines GPIO y configuración del AP
├── src/
│   └── main.cpp          # Código principal (WiFi, web server, control)
└── README.md
```

## Próximos pasos

- [ ] Sensor de pH
- [ ] Sensor de temperatura del agua
- [ ] Sensor de nivel de agua
- [ ] Sensor de EC (conductividad eléctrica)
- [ ] Temporizador automático para la bomba
- [ ] OTA (actualización remota por WiFi)
- [ ] Dashboard con gráficos históricos
- [ ] Alertas por MQTT / Telegram
