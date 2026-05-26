# Manual de Recomendaciones de Seguridad y Operación - Hidroponia IoT

Este documento reúne las directrices técnicas, de red, eléctricas y de mantenimiento físico para garantizar una operación segura y robusta del sistema de control hidropónico basado en el microcontrolador **ESP32**. Estas recomendaciones complementan el diseño experimental y están dirigidas a evitar fallas de hardware, accesos no autorizados y daños en los componentes biológicos o mecánicos del cultivo.

---

## 1. Control de Acceso y Seguridad MQTT

El protocolo MQTT carece de cifrado o autenticación por defecto en redes locales abiertas. Para entornos de desarrollo o producción expuestos, deben aplicarse las siguientes directrices:

### A. Deshabilitar Conexiones Anónimas
En el archivo de configuración del broker (`docker/mosquitto/config/mosquitto.conf`), se debe cambiar la directiva:
```ini
# Deshabilitar accesos anónimos
allow_anonymous false
```

### B. Creación de Usuarios y Contraseñas
Se debe generar un archivo de contraseñas cifradas en el contenedor Mosquitto mediante la herramienta `mosquitto_passwd`.
* **Comando para crear el archivo y agregar un usuario administrador**:
  ```bash
  docker compose exec mosquitto mosquitto_passwd -c /mosquitto/config/passwd usuario_esp32
  ```
  *(El sistema solicitará ingresar y confirmar una contraseña segura)*.
* **Configuración en `mosquitto.conf`**:
  ```ini
  password_file /mosquitto/config/passwd
  ```

### C. Uso de Listas de Control de Acceso (ACL)
Para evitar que un cliente web o dispositivo comprometido manipule otros nodos, se debe restringir el acceso a nivel de tópico.
* **Archivo `/mosquitto/config/acl`**:
  ```acl
  # El microcontrolador solo puede publicar estado y recibir comandos en sus propios tópicos
  user usuario_esp32
  topic write hidroponia/esp32_hidro/state
  topic read hidroponia/esp32_hidro/cmd

  # La interfaz web de monitoreo solo puede leer estado y mandar comandos
  user usuario_web
  topic read hidroponia/esp32_hidro/state
  topic write hidroponia/esp32_hidro/cmd
  ```
* **Cargar en `mosquitto.conf`**:
  ```ini
  acl_file /mosquitto/config/acl
  ```

---

## 2. Protección de Credenciales y Criptografía

El firmware básico contiene credenciales en texto plano que representan un riesgo si el código se almacena en repositorios públicos.

### A. Eliminación de Credenciales en Código (`config.h`)
En el archivo [config.h](file:///Users/edwin/Documents/Tesis/esp32/include/config.h), las líneas:
```cpp
#define ADMIN_USER "admin"
#define ADMIN_PASS "admin"
```
deben ser migradas. En su lugar, el primer arranque del sistema debe forzar al usuario a establecer credenciales personalizadas a través de un portal web cautivo de inicialización (`portal_wifi.h`), guardando el hash (por ejemplo, SHA-256) en la memoria no volátil (NVS) del ESP32 usando la librería `Preferences`.

### B. Cifrado de Tránsito (HTTPS/WSS)
Cuando el dashboard web (`web-ui/`) se comunica con el broker Mosquitto desde fuera de la red local, el tráfico (incluidas las credenciales de usuario de WebSockets) viaja en texto plano.
- **Proxy Inverso con SSL**: Se debe utilizar el servidor Nginx (provisto en `docker/nginx/default.conf`) configurado con un certificado SSL de Let's Encrypt o autofirmado.
- El puerto `9001` de WebSockets debe redirigirse al puerto seguro `443` HTTPS mediante Nginx.
- En el código JavaScript ([app.js](file:///Users/edwin/Documents/Tesis/esp32/web-ui/app.js)), la URL de conexión debe cambiar de `ws://` a `wss://` (WebSockets Secure).

---

## 3. Restricción de Puertos y Cortafuegos (Firewall)

El servidor que ejecuta Docker no debe exponer todos sus puertos al exterior (Internet).

### A. Limitar Exposición en Docker Compose
En el archivo [docker-compose.yml](file:///Users/edwin/Documents/Tesis/esp32/docker/docker-compose.yml), los puertos de Mosquitto no deben mapearse a todas las interfaces de red (`0.0.0.0`), sino limitarse al host local (`127.0.0.1`) si Nginx es quien maneja la salida pública:
```yaml
services:
  mosquitto:
    image: eclipse-mosquitto:2
    ports:
      - "127.0.0.1:1883:1883"  # Solo accesible localmente o por VPN
      - "127.0.0.1:9001:9001"  # Nginx actuará como proxy reverso para este puerto
```

### B. Configuración de Reglas de Firewall (UFW en Linux)
Se deben denegar todos los puertos de administración por defecto y solo permitir HTTP/HTTPS:
```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 80/tcp     # Acceso HTTP redirección
sudo ufw allow 443/tcp    # Acceso Web seguro (Nginx + WSS)
sudo ufw enable
```

---

## 4. Seguridad y Segmentación de Red (Red Local e IoT)

Los dispositivos IoT son puntos de entrada vulnerables en redes domésticas o industriales.

### A. Aislamiento por VLAN (Virtual Local Area Network)
- Se recomienda configurar el router para conectar el ESP32 a una **red Wi-Fi de invitados** o a una **VLAN IoT** dedicada.
- Esto impide que si el ESP32 es vulnerado, un atacante pueda realizar movimientos laterales hacia computadoras personales, servidores locales o almacenamiento NAS de la misma red.

### B. Monitoreo Remoto Seguro vía VPN
- **Evitar la apertura de puertos (Port Forwarding)** en el router doméstico hacia la IP del ESP32 o del broker MQTT.
- Para acceder al dashboard desde el exterior, el host del servidor y el dispositivo móvil/PC del usuario deben pertenecer a una red privada virtual como **Tailscale** o **WireGuard**. Así, la comunicación se realiza a través de un túnel cifrado extremo a extremo sin exponer puertos en la IP pública WAN.

---

## 5. Buenas Prácticas Eléctricas

El control de actuadores inductivos (motores de las bombas) introduce ruido electromagnético y picos de voltaje que pueden congelar o destruir los circuitos digitales del ESP32.

```
       [ FUENTE DE 12V ] ──────────────► [ CONVERTIDOR DC-DC ] ──► [ ESP32 (5V) ]
               │                                                      │
               │ (Alimentación Potencia)                              │ (Señal digital)
               ▼                                                      ▼
    ┌──────────────────────┐                                 ┌─────────────────┐
    │ BOMBA / ACTUADOR DC  │ ◄─────── [ RELÉ / OPTO ] ◄──────│  PINES CONTROL  │
    │ (Con Diodo Flyback)  │                                 │  (GND Común)    │
    └──────────────────────┘                                 └─────────────────┘
```

### A. Aislamiento de Circuitos y Fuentes Separadas
- **Alimentación dividida**: Utilizar una línea independiente de potencia (ej: 12V 10A) para las bombas y derivar una línea limpia mediante un convertidor reductor (Buck Converter DC-DC) de alta calidad para alimentar el ESP32 en su pin `VIN` con 5V.
- **Relés optoacoplados**: La placa de relés utilizada para controlar la bomba principal debe contar con aislamiento por optoacoplador. El jumper `JD-VCC` debe removerse para alimentar los LEDs del optoacoplador con los 3.3V del ESP32, y la bobina del relé con los 5V de la fuente externa de potencia, evitando que el retorno de corriente de la bobina afecte al ESP32.

### B. Supresión de Picos Inductivos (Diodos de Retorno Libre / Flyback)
- Los motores de las bombas peristálticas de corriente continua (DC) son cargas altamente inductivas. Al apagarse, el campo magnético del motor colapsa y genera un pico inverso de alta tensión (fuerza contraelectromotriz) que daña los transistores de control o el relé.
- **Solución**: Soldar un **diodo rectificador en antiparalelo** (ej. 1N4007) directamente en los terminales de cada motor DC de las bombas. El cátodo del diodo (línea gris) debe ir conectado al terminal positivo (+), y el ánodo al terminal negativo (-).

### C. Configuración de Tierra Común (GND)
- Todos los componentes del sistema (convertidor DC-DC, ESP32, ADS1115 y módulo sensor de pH PH-4502C) deben compartir la misma línea de referencia de tierra (**GND**). 
- Si no hay un GND común, las lecturas analógicas del sensor de pH flotarán erráticamente y las señales I2C de comunicación perderán la referencia de nivel lógico, causando fallas intermitentes.

---

## 6. Operación Segura de Bombas (Actuadores)

Las bombas eléctricas requieren precauciones lógicas y físicas para evitar fallas mecánicas catastróficas.

### A. Protección Contra Funcionamiento en Seco (*Dry Run*)
La bomba de recirculación principal de 12V puede quemarse en pocos minutos si funciona sin agua.
- **Lógica en el Firmware**: Se debe implementar en [ph_control.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/ph_control.cpp) o [actuators.cpp](file:///Users/edwin/Documents/Tesis/esp32/src/actuators.cpp) un bloqueo de hardware basado en el sensor de nivel `DPL-1-BK`:
  ```cpp
  void updateLevelSensorProtection() {
      // Si el sensor detecta nivel bajo, apagar la bomba de inmediato por seguridad
      if (isLevelLow && pumpState) {
          setPump(false);
          Serial.println("[ALERTA CRÍTICA] Bomba principal apagada preventivamente por bajo nivel de agua.");
          // Enviar alerta MQTT adicional
          mqttClient.publish("hidroponia/esp32_hidro/alerta", "{\"error\":\"dry_run_prevented\"}");
      }
  }
  ```

### B. Limitación del Ciclo de Trabajo en Bombas Peristálticas
Las bombas peristálticas dosifican reactivos altamente concentrados (ácido o base). Funcionan mediante la compresión de una manguera de silicona flexible.
- **Lógica de Dosis y Espera**: Mantener activa una bomba peristáltica por más de 30 segundos continuos puede dosificar un volumen excesivo, alterando violentamente el equilibrio del agua y quemando la raíz de las plantas.
- **Control**: Limitar en el firmware el parámetro `phDoseSeconds` a un máximo estricto de 5 segundos por ciclo, y exigir un tiempo de mezcla/estabilización (`phSettleSeconds`) de al menos 90 a 180 segundos antes de permitir una nueva dosificación. Esto da tiempo a que el agua se homogenice y el sensor de pH registre la variación real.
- **Mantenimiento de la Manguera**: La manguera de silicona interna de las bombas peristálticas debe inspeccionarse visualmente cada 30 días. El desgaste por fricción puede romper la manguera y provocar derrames de soluciones ácidas concentradas sobre la electrónica.

---

## 7. Mantenimiento y Manejo de Sensores

Los sensores químicos son transductores delicados que requieren calibración y almacenamiento específicos.

### A. Mantenimiento del Electrodo de pH (E201-BNC)
El electrodo de pH consiste en una membrana de vidrio extremadamente delgada que contiene una solución de referencia interna.
- **Hidratación**: La sonda **nunca** debe almacenarse en agua destilada ni dejarse secar al aire libre. Cuando el sistema no esté operativo, el bulbo sensor debe permanecer sumergido en su capuchón protector lleno de **solución de almacenamiento (KCl 3M)**.
- Si la sonda se seca por accidente, perderá sensibilidad o quedará inoperable. Se puede intentar recuperar sumergiéndola en KCl 3M durante 24 horas.
- **Limpieza**: El bulbo de vidrio acumula algas y sales minerales. Se debe limpiar suavemente con agua destilada y un paño de microfibra sin frotar (para no rayar el vidrio ni generar cargas estáticas).

### B. Prevención de Bucles de Tierra (*Ground Loops*) en el pH
- **Interferencia eléctrica**: El agua del estanque hidropónico está en contacto con bombas y calentadores eléctricos que pueden fugar microcorrientes. Estas corrientes viajan por el agua y distorsionan la lectura analógica de microvoltios de la sonda de pH.
- **Diagnóstico**: Si la lectura de pH es estable con la sonda en un vaso de agua, pero oscila bruscamente al sumergirla en el estanque principal con la bomba encendida, existe interferencia eléctrica.
- **Soluciones**:
  1. Utilizar un **aislador galvánico analógico** para la señal del módulo `PH-4502C`.
  2. Aislar eléctricamente el chasis y la alimentación de la bomba principal.
  3. Asegurar una conexión a tierra física del agua del depósito introduciendo una varilla de acero inoxidable conectada al polo de tierra eléctrica del edificio.

### C. Historial y Calibración Periódica del Sensor de pH
El desgaste del electrodo desplaza los voltajes de referencia de calibración de forma natural a lo largo del tiempo.
- Se recomienda realizar una **recalibración de 2 puntos** (con soluciones buffer pH 7.0 y pH 4.0) cada 15 a 30 días de uso continuo del sistema.
- Los nuevos voltajes resultantes (`PH_VOLTAGE_AT_7` y `PH_VOLTAGE_AT_4`) deben actualizarse en la memoria flash o reprogramarse en el firmware para contrarrestar la deriva del sensor.

---
*Fin de las Recomendaciones de Seguridad.*
