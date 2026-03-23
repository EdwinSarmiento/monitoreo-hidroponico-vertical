# Hidroponia IoT - ESP32

Sistema de control IoT para cultivo hidropónico basado en ESP32.

## Funcionalidad actual

- Conexión WiFi automática con reconexión
- Servidor web con interfaz responsive
- Control ON/OFF de bomba de agua mediante relé

## Hardware necesario

| Componente | Especificación |
|---|---|
| ESP32 DevKit | Cualquier variante |
| Fuente de alimentación | 12V 10A |
| Convertidor DC-DC | 12V → 5V (para ESP32) |
| Módulo relé | 5V, 1 canal (soportar 10A @ 12V) |
| Bomba de agua | 12V |

## Esquema de conexión

```
FUENTE 12V 10A
    │
    ├──────────────────────────── Bomba 12V (+)
    │                                │
    │                          [RELÉ COM/NO]
    │                                │
    │                            Bomba 12V (-)
    │
    ├── Convertidor 12V → 5V ──── ESP32 VIN (5V)
    │                              ESP32 GND
    │
    └── GND ──────────────────── GND común


ESP32 GPIO26 ──── IN del módulo relé
ESP32 5V     ──── VCC del módulo relé
ESP32 GND    ──── GND del módulo relé
```

**Conexión del relé (lado de potencia):**
- COM → Cable positivo (+) de la bomba
- NO (Normally Open) → Positivo (+) de la fuente 12V
- El negativo (-) de la bomba va directo al negativo (-) de la fuente

## Configuración

1. Editar `include/config.h` con las credenciales de tu red WiFi:

```c
#define WIFI_SSID     "TU_RED_WIFI"
#define WIFI_PASSWORD "TU_CONTRASEÑA"
```

2. Si usás un pin diferente para el relé, modificar:

```c
#define RELAY_PIN 26
```

## Compilar y subir

Requiere [PlatformIO](https://platformio.org/).

```bash
# Compilar
pio run

# Subir al ESP32
pio run --target upload

# Monitor serie (ver IP asignada)
pio device monitor
```

## Uso

1. Subir el firmware al ESP32
2. Abrir el monitor serie para ver la IP asignada
3. En el navegador, ir a `http://<IP_DEL_ESP32>`
4. Usar el botón para encender/apagar la bomba

## Próximos pasos (IoT Hidroponia)

- [ ] Sensor de pH
- [ ] Sensor de temperatura del agua
- [ ] Sensor de nivel de agua
- [ ] Sensor de EC (conductividad eléctrica)
- [ ] Temporizador automático para la bomba
- [ ] Dashboard con gráficos históricos
- [ ] Alertas por MQTT / Telegram
