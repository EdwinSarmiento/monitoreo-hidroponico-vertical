#include "portal_wifi.h"
#include "app_state.h"
#include "config.h"
#include "storage.h"

static const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hidroponia - Config WiFi</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #e0e0e0;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 20px;
      padding: 40px 36px;
      width: 360px;
      text-align: center;
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
    .btn:hover { box-shadow: 0 6px 20px rgba(34,197,94,0.5); }
    .scan-list { margin: 16px 0; max-height: 200px; overflow-y: auto; text-align: left; }
    .scan-item {
      padding: 10px 14px; margin: 4px 0; border-radius: 8px; cursor: pointer;
      background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.08);
      display: flex; justify-content: space-between; font-size: 0.9rem;
    }
    .scan-item:hover { border-color: #4ade80; background: rgba(74,222,128,0.1); }
    .signal { color: #8ab4c4; font-size: 0.75rem; }
    .msg { margin-top: 16px; font-size: 0.85rem; color: #f87171; }
    .msg.ok { color: #4ade80; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">Hidroponia IoT</div>
    <div class="subtitle">Configuraci&oacute;n de WiFi</div>
    <div id="scanResult" class="scan-list"></div>
    <form id="wifiForm" onsubmit="return saveWiFi(event)">
      <div class="field">
        <label>Nombre de la red (SSID)</label>
        <input type="text" id="ssid" name="ssid" required placeholder="Selecciona o escribe tu red">
      </div>
      <div class="field">
        <label>Contrase&ntilde;a</label>
        <input type="password" id="pass" name="pass" placeholder="Contrase&ntilde;a del WiFi">
      </div>
      <button class="btn" type="submit">Conectar</button>
    </form>
    <div id="msg" class="msg"></div>
  </div>
  <script>
    async function scan() {
      try {
        const r = await fetch('/scan');
        const nets = await r.json();
        const el = document.getElementById('scanResult');
        if (!nets.length) { el.innerHTML = '<div style="color:#8ab4c4;font-size:0.85rem">No se encontraron redes</div>'; return; }
        el.innerHTML = nets.map(n =>
          '<div class="scan-item" onclick="pickNet(\'' + n.ssid.replace(/'/g,"\\'") + '\')">' +
          '<span>' + n.ssid + '</span><span class="signal">' + n.rssi + ' dBm</span></div>'
        ).join('');
      } catch(e) { console.error(e); }
    }
    function pickNet(ssid) {
      document.getElementById('ssid').value = ssid;
      document.getElementById('pass').focus();
    }
    async function saveWiFi(e) {
      e.preventDefault();
      const ssid = document.getElementById('ssid').value;
      const pass = document.getElementById('pass').value;
      const msg = document.getElementById('msg');
      msg.className = 'msg'; msg.textContent = 'Guardando...';
      try {
        const r = await fetch('/save-wifi', {
          method: 'POST',
          headers: {'Content-Type':'application/x-www-form-urlencoded'},
          body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
        });
        const data = await r.json();
        if (data.ok) {
          msg.className = 'msg ok';
          msg.textContent = 'Guardado! El ESP32 se va a reiniciar y conectar a "' + ssid + '"...';
        } else { msg.textContent = 'Error al guardar'; }
      } catch(e) { msg.textContent = 'Error de conexion'; }
      return false;
    }
    scan();
  </script>
</body>
</html>
)rawliteral";

void startAPMode() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME, AP_PASSWORD);
  delay(500);

  Serial.println("Modo AP activado");
  Serial.print("Red: ");
  Serial.println(AP_NAME);
  Serial.print("Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("Portal: http://");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", SETUP_HTML);
  });

  server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_FAILED) {
      WiFi.scanNetworks(true);
      request->send(200, "application/json", "[]");
      return;
    }
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i > 0) json += ",";
      json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    json += "]";
    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    request->send(200, "application/json", json);
  });

  server.on("/save-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true)) {
      String ssid = request->getParam("ssid", true)->value();
      String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
      saveWiFiCredentials(ssid, pass);
      request->send(200, "application/json", "{\"ok\":true}");
      Serial.printf("WiFi guardado: %s\n", ssid.c_str());
      delay(2000);
      ESP.restart();
    } else {
      request->send(400, "application/json", "{\"ok\":false}");
    }
  });

  server.onNotFound([](AsyncWebServerRequest *request) { request->redirect("/"); });

  WiFi.scanNetworks(true);
  server.begin();
  Serial.println("Portal de configuracion iniciado.");
}
