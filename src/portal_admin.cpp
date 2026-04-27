#include "portal_admin.h"
#include "app_state.h"
#include "config.h"
#include "hidro_mqtt.h"
#include "storage.h"

static const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hidroponia IoT - Login</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
      min-height: 100vh;
      display: flex; align-items: center; justify-content: center;
      color: #e0e0e0;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 20px;
      padding: 40px 36px; width: 360px; text-align: center;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
    }
    .title { font-size: 1.4rem; font-weight: 700; margin-bottom: 6px; }
    .subtitle { font-size: 0.85rem; color: #8ab4c4; margin-bottom: 28px; }
    .field { margin-bottom: 16px; text-align: left; }
    label { font-size: 0.8rem; color: #8ab4c4; display: block; margin-bottom: 6px; }
    input {
      width: 100%; padding: 12px 14px; border: 1px solid rgba(255,255,255,0.15);
      border-radius: 10px; background: rgba(255,255,255,0.08); color: #fff;
      font-size: 1rem; outline: none; transition: border 0.3s;
    }
    input:focus { border-color: #4ade80; }
    .btn {
      width: 100%; padding: 14px; border: none; border-radius: 12px;
      font-size: 1rem; font-weight: 600; cursor: pointer;
      background: linear-gradient(135deg, #22c55e, #16a34a);
      color: #fff; margin-top: 10px; text-transform: uppercase;
      box-shadow: 0 4px 15px rgba(34,197,94,0.3);
    }
    .msg { margin-top: 16px; font-size: 0.85rem; color: #f87171; }
    .info { margin-top: 20px; font-size: 0.7rem; color: #4b6b7a; line-height: 1.5; }
    .wifi-name { color: #4ade80; font-weight: 600; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">Hidroponia IoT</div>
    <div class="subtitle">Acceso de Administraci&oacute;n</div>
    <form action="/login" method="POST">
      <div class="field">
        <label>Usuario</label>
        <input type="text" name="user" required placeholder="admin">
      </div>
      <div class="field">
        <label>Contrase&ntilde;a</label>
        <input type="password" name="pass" required placeholder="admin">
      </div>
      <button class="btn" type="submit">Entrar</button>
    </form>
    <div id="msg" class="msg">{msg}</div>
    <div class="info">
      WiFi: <span class="wifi-name">{wifi}</span> &bull; IP: {ip}
    </div>
  </div>
</body>
</html>
)rawliteral";

static const char MQTT_CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hidroponia IoT - Configurar MQTT</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
      min-height: 100vh;
      display: flex; align-items: center; justify-content: center;
      color: #e0e0e0; padding: 20px;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 20px;
      padding: 32px 28px; width: 380px;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
    }
    .title { font-size: 1.4rem; font-weight: 700; margin-bottom: 6px; text-align: center; }
    .subtitle { font-size: 0.85rem; color: #8ab4c4; margin-bottom: 24px; text-align: center; }
    .section {
      background: rgba(255,255,255,0.03);
      border: 1px solid rgba(255,255,255,0.06);
      border-radius: 14px; padding: 18px 16px; margin-bottom: 14px;
    }
    .section-title {
      font-size: 0.75rem; color: #8ab4c4; text-transform: uppercase;
      letter-spacing: 1px; margin-bottom: 12px;
    }
    .field { margin-bottom: 14px; }
    label { font-size: 0.8rem; color: #8ab4c4; display: block; margin-bottom: 6px; }
    input {
      width: 100%; padding: 12px 14px; border: 1px solid rgba(255,255,255,0.15);
      border-radius: 10px; background: rgba(255,255,255,0.08); color: #fff;
      font-size: 1rem; outline: none; transition: border 0.3s;
    }
    input:focus { border-color: #4ade80; }
    .hint { font-size: 0.7rem; color: #4b6b7a; margin-top: 4px; }
    .btn {
      width: 100%; padding: 14px; border: none; border-radius: 12px;
      font-size: 1rem; font-weight: 600; cursor: pointer;
      background: linear-gradient(135deg, #22c55e, #16a34a);
      color: #fff; margin-top: 10px; text-transform: uppercase;
      box-shadow: 0 4px 15px rgba(34,197,94,0.3);
    }
    .btn:hover { box-shadow: 0 6px 20px rgba(34,197,94,0.5); }
    .btn-reset {
      margin-top: 10px; background: transparent;
      border: 1px solid rgba(255,255,255,0.12); color: #6b8a97;
      padding: 10px; font-size: 0.8rem; width: 100%;
      border-radius: 10px; cursor: pointer;
    }
    .btn-reset:hover { border-color: #f87171; color: #f87171; }
    .status {
      text-align: center; margin-top: 14px; padding: 10px; border-radius: 10px;
      font-size: 0.85rem;
    }
    .status.ok { background: rgba(74,222,128,0.1); color: #4ade80; border: 1px solid rgba(74,222,128,0.3); }
    .status.err { background: rgba(248,113,113,0.1); color: #f87171; border: 1px solid rgba(248,113,113,0.3); }
    .info { text-align: center; margin-top: 14px; font-size: 0.7rem; color: #4b6b7a; line-height: 1.5; }
    .wifi-name { color: #4ade80; font-weight: 600; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">Hidroponia IoT</div>
    <div class="subtitle">Configuraci&oacute;n del Broker MQTT</div>

    <div class="section">
      <div class="section-title">Conexi&oacute;n al Broker</div>
      <form action="/save-mqtt" method="POST">
        <div class="field">
          <label>IP o Dominio del Servidor</label>
          <input type="text" name="mqtt_server" value="{mqtt_server}" required placeholder="ej: 192.168.0.19 o mi-servidor.com">
          <div class="hint">IP privada (red local) o IP p&uacute;blica / dominio para servidor en la nube.</div>
        </div>
        <div class="field">
          <label>Token del Dispositivo</label>
          <input type="text" name="device_token" value="{device_token}" placeholder="esp32_hidro">
          <div class="hint">Debe coincidir con el del Dashboard web (t&oacute;picos hidroponia/&lt;token&gt;/...).</div>
        </div>
        <button class="btn" type="submit">Guardar y Conectar</button>
      </form>
    </div>

    <div class="status {mqtt_status_class}">{mqtt_status_msg}</div>

    <button class="btn-reset" onclick="if(confirm('Se borran las credenciales WiFi y se reinicia.'))fetch('/reset-wifi',{method:'POST'})">
      Cambiar red WiFi
    </button>

    <div class="info">
      WiFi: <span class="wifi-name">{wifi}</span> &bull; IP: {ip}
    </div>
  </div>
</body>
</html>
)rawliteral";

static String buildLoginPage(const String &msg) {
  String html = String(LOGIN_HTML);
  html.replace("{msg}", msg);
  html.replace("{wifi}", WiFi.SSID());
  html.replace("{ip}", WiFi.localIP().toString());
  return html;
}

static String buildMQTTPage() {
  String html = String(MQTT_CONFIG_HTML);
  html.replace("{mqtt_server}", mqttServer);
  html.replace("{device_token}", deviceToken);
  html.replace("{wifi}", WiFi.SSID());
  html.replace("{ip}", WiFi.localIP().toString());

  if (mqttConfigured && mqttClient.connected()) {
    html.replace("{mqtt_status_class}", "ok");
    html.replace("{mqtt_status_msg}", "&#10003; Conectado al broker MQTT en " + mqttServer);
  } else if (mqttConfigured) {
    html.replace("{mqtt_status_class}", "err");
    html.replace("{mqtt_status_msg}", "&#10007; No se pudo conectar a " + mqttServer + ". Verifica IP y puerto.");
  } else {
    html.replace("{mqtt_status_class}", "");
    html.replace("{mqtt_status_msg}", "Sin configurar. Ingresa los datos del broker.");
  }
  return html;
}

void setupAppRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", buildLoginPage(""));
  });

  server.on("/login", HTTP_POST, [](AsyncWebServerRequest *request) {
    String user = request->hasParam("user", true) ? request->getParam("user", true)->value() : "";
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";

    if (user == ADMIN_USER && pass == ADMIN_PASS) {
      request->send(200, "text/html", buildMQTTPage());
    } else {
      request->send(200, "text/html", buildLoginPage("Usuario o contrase&ntilde;a incorrectos."));
    }
  });

  server.on("/save-mqtt", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("mqtt_server", true)) {
      mqttServer = request->getParam("mqtt_server", true)->value();
    }
    if (request->hasParam("device_token", true)) {
      deviceToken = request->getParam("device_token", true)->value();
    }
    mqttPort = 1883;
    mqttUser = "";
    mqttPass = "";

    saveMQTTConfig();
    mqttClient.disconnect();
    connectMQTT();
    request->send(200, "text/html", buildMQTTPage());
  });

  server.on("/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Reiniciando...");
    clearAllConfig();
    delay(1000);
    ESP.restart();
  });
}
