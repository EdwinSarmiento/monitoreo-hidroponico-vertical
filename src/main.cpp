#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <DNSServer.h>
#include <Adafruit_ADS1X15.h>
#include "config.h"

AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;
Adafruit_ADS1115 ads;

bool pumpState = false;
bool peri1State = false;
bool peri2State = false;
bool apMode = false;
bool adsReady = false;

float currentPH = 0.0;
float currentVoltage = 0.0;
unsigned long lastPHRead = 0;

// =============================================
//  Lectura de pH (ADS1115 por I2C)
// =============================================
float readPHVoltage() {
  if (!adsReady) return 0.0f;

  long total = 0;
  int validSamples = 0;
  for (int i = 0; i < PH_SAMPLES; i++) {
    int16_t raw = ads.readADC_SingleEnded(PH_ADS_CHANNEL);
    if (raw >= 0 && raw < 32767) {
      total += raw;
      validSamples++;
    }
    delay(10);
  }
  if (validSamples == 0) return 0.0f;
  float avgRaw = (float)total / validSamples;
  return ads.computeVolts(avgRaw);
}

float voltageToPH(float voltage) {
  float slope = (7.0f - 4.0f) / (PH_VOLTAGE_AT_7 - PH_VOLTAGE_AT_4);
  float ph = 7.0f + slope * (voltage - PH_VOLTAGE_AT_7);
  if (ph < 0.0f) ph = 0.0f;
  if (ph > 14.0f) ph = 14.0f;
  return ph;
}

void updatePH() {
  if (millis() - lastPHRead < PH_READ_INTERVAL) return;
  lastPHRead = millis();
  currentVoltage = readPHVoltage();
  currentPH = voltageToPH(currentVoltage);
  Serial.printf("pH: %.2f (%.3fV) [ADS1115]\n", currentPH, currentVoltage);
}

// =============================================
//  Portal de configuración WiFi (modo AP)
// =============================================
const char SETUP_HTML[] PROGMEM = R"rawliteral(
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

// =============================================
//  Interfaz principal (modo conectado)
// =============================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Hidroponia IoT</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
      background: linear-gradient(135deg, #0f2027, #203a43, #2c5364);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      color: #e0e0e0;
      padding: 20px;
    }
    .card {
      background: rgba(255,255,255,0.05);
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.1);
      border-radius: 20px;
      padding: 32px 28px;
      width: 380px;
      text-align: center;
      box-shadow: 0 8px 32px rgba(0,0,0,0.3);
    }
    .title { font-size: 1.5rem; font-weight: 700; margin-bottom: 4px; }
    .subtitle { font-size: 0.85rem; color: #8ab4c4; margin-bottom: 24px; }

    .section {
      background: rgba(255,255,255,0.03);
      border: 1px solid rgba(255,255,255,0.06);
      border-radius: 14px;
      padding: 18px 16px;
      margin-bottom: 14px;
    }
    .section-title {
      font-size: 0.75rem; color: #8ab4c4; text-transform: uppercase;
      letter-spacing: 1px; margin-bottom: 12px;
    }
    .status-row {
      display: flex; align-items: center; justify-content: space-between;
      margin-bottom: 12px;
    }
    .status-label { font-size: 0.95rem; }
    .status-dot {
      width: 10px; height: 10px; border-radius: 50%;
      display: inline-block; margin-right: 8px;
    }
    .dot-on  { background: #4ade80; box-shadow: 0 0 8px rgba(74,222,128,0.5); }
    .dot-off { background: #6b7280; }

    /* pH gauge */
    .ph-gauge {
      position: relative;
      height: 100px;
      display: flex;
      align-items: center;
      justify-content: center;
      margin-bottom: 10px;
    }
    .ph-value {
      font-size: 3rem;
      font-weight: 700;
      line-height: 1;
    }
    .ph-unit { font-size: 1rem; color: #8ab4c4; margin-left: 4px; }
    .ph-voltage {
      font-size: 0.7rem; color: #4b6b7a; margin-top: 4px;
    }
    .ph-bar {
      width: 100%; height: 8px; border-radius: 4px;
      background: linear-gradient(to right,
        #ef4444 0%, #f97316 14%, #eab308 28%,
        #22c55e 42%, #22c55e 57%,
        #3b82f6 71%, #6366f1 85%, #8b5cf6 100%);
      position: relative; margin-top: 10px;
    }
    .ph-marker {
      position: absolute; top: -4px;
      width: 4px; height: 16px; border-radius: 2px;
      background: #fff; box-shadow: 0 0 6px rgba(255,255,255,0.8);
      transition: left 0.5s ease;
    }
    .ph-labels {
      display: flex; justify-content: space-between;
      font-size: 0.6rem; color: #4b6b7a; margin-top: 4px;
    }
    .ph-status {
      font-size: 0.85rem; font-weight: 600; margin-top: 8px;
    }

    .btn {
      width: 100%; padding: 13px; border: none; border-radius: 10px;
      font-size: 0.95rem; font-weight: 600; cursor: pointer;
      transition: all 0.3s ease; text-transform: uppercase;
    }
    .btn:active { transform: scale(0.97); }
    .btn:disabled { opacity: 0.5; cursor: wait; }
    .btn-on {
      background: linear-gradient(135deg, #22c55e, #16a34a); color: #fff;
      box-shadow: 0 4px 12px rgba(34,197,94,0.3);
    }
    .btn-on:hover { box-shadow: 0 6px 16px rgba(34,197,94,0.5); }
    .btn-off {
      background: linear-gradient(135deg, #ef4444, #dc2626); color: #fff;
      box-shadow: 0 4px 12px rgba(239,68,68,0.3);
    }
    .btn-off:hover { box-shadow: 0 6px 16px rgba(239,68,68,0.5); }

    .btn-purple {
      background: linear-gradient(135deg, #a855f7, #7c3aed); color: #fff;
      box-shadow: 0 4px 12px rgba(168,85,247,0.3);
    }
    .btn-purple:hover { box-shadow: 0 6px 16px rgba(168,85,247,0.5); }
    .btn-purple-off {
      background: linear-gradient(135deg, #6b21a8, #581c87); color: #fff;
      box-shadow: 0 4px 12px rgba(107,33,168,0.3);
    }

    .ph-buttons { display: flex; gap: 10px; }
    .ph-buttons .btn { flex: 1; }

    .btn-reset {
      margin-top: 14px; background: transparent;
      border: 1px solid rgba(255,255,255,0.12); color: #6b8a97;
      padding: 9px; font-size: 0.75rem;
    }
    .btn-reset:hover { border-color: #f87171; color: #f87171; }

    .wifi-info {
      margin-top: 14px; font-size: 0.7rem; color: #4b6b7a; line-height: 1.5;
    }
    .wifi-name { color: #4ade80; font-weight: 600; }
  </style>
</head>
<body>
  <div class="card">
    <div class="title">Hidroponia IoT</div>
    <div class="subtitle">Panel de Control</div>

    <!-- Sensor de pH -->
    <div class="section">
      <div class="section-title">Sensor de pH</div>
      <div class="ph-gauge">
        <span class="ph-value" id="phValue">--</span>
        <span class="ph-unit">pH</span>
      </div>
      <div class="ph-voltage" id="phVoltage">Voltaje: -- V</div>
      <div class="ph-bar"><div class="ph-marker" id="phMarker" style="left:50%"></div></div>
      <div class="ph-labels">
        <span>&Aacute;cido (0)</span>
        <span>Neutro (7)</span>
        <span>Alcalino (14)</span>
      </div>
      <div class="ph-status" id="phStatus" style="color:#8ab4c4">Leyendo...</div>
    </div>

    <!-- Bomba principal -->
    <div class="section">
      <div class="section-title">Bomba de Agua Principal</div>
      <div class="status-row">
        <span class="status-label">
          <span class="status-dot dot-off" id="pumpDot"></span>
          <span id="pumpText">Apagada</span>
        </span>
      </div>
      <button class="btn btn-on" id="pumpBtn" onclick="togglePump()">ENCENDER</button>
    </div>

    <!-- Bombas peristálticas -->
    <div class="section">
      <div class="section-title">Bombas Perist&aacute;lticas - pH</div>
      <div class="status-row">
        <span class="status-label">
          <span class="status-dot dot-off" id="ph1Dot"></span>
          pH+ (Subir): <span id="ph1Text">Off</span>
        </span>
      </div>
      <div class="status-row">
        <span class="status-label">
          <span class="status-dot dot-off" id="ph2Dot"></span>
          pH- (Bajar): <span id="ph2Text">Off</span>
        </span>
      </div>
      <div class="ph-buttons">
        <button class="btn btn-purple" id="ph1Btn" onclick="togglePeri(1)">pH+ ON</button>
        <button class="btn btn-purple" id="ph2Btn" onclick="togglePeri(2)">pH- ON</button>
      </div>
    </div>

    <button class="btn btn-reset" onclick="resetWiFi()">Cambiar red WiFi</button>
    <div class="wifi-info">
      WiFi: <span class="wifi-name" id="wifiName">---</span> &bull;
      IP: <span id="ipAddr">---</span>
    </div>
  </div>

  <script>
    let state = { pump: false, peri1: false, peri2: false, ph: 0, phV: 0 };

    async function fetchState() {
      try {
        const r = await fetch('/state');
        state = await r.json();
        document.getElementById('wifiName').textContent = state.wifi || '---';
        document.getElementById('ipAddr').textContent = state.ip || '---';
        updateUI();
      } catch (e) { console.error(e); }
    }

    async function togglePump() {
      document.getElementById('pumpBtn').disabled = true;
      try {
        const action = state.pump ? 'off' : 'on';
        const r = await fetch('/pump?action=' + action, { method: 'POST' });
        state = await r.json();
        updateUI();
      } catch (e) { console.error(e); }
      document.getElementById('pumpBtn').disabled = false;
    }

    async function togglePeri(n) {
      const btnId = 'ph' + n + 'Btn';
      document.getElementById(btnId).disabled = true;
      try {
        const isOn = n === 1 ? state.peri1 : state.peri2;
        const action = isOn ? 'off' : 'on';
        const r = await fetch('/peristaltic?id=' + n + '&action=' + action, { method: 'POST' });
        state = await r.json();
        updateUI();
      } catch (e) { console.error(e); }
      document.getElementById(btnId).disabled = false;
    }

    async function resetWiFi() {
      if (!confirm('Se va a reiniciar el ESP32 y abrir el portal WiFi. Continuar?')) return;
      try { await fetch('/reset-wifi', { method: 'POST' }); } catch (e) {}
    }

    function getPhColor(ph) {
      if (ph < 5.5) return '#ef4444';
      if (ph < 6.0) return '#f97316';
      if (ph <= 6.5) return '#4ade80';
      if (ph <= 7.0) return '#22c55e';
      if (ph <= 7.5) return '#3b82f6';
      return '#8b5cf6';
    }

    function getPhLabel(ph) {
      if (ph < 5.5) return 'Muy acido';
      if (ph < 6.0) return 'Acido';
      if (ph <= 7.0) return 'Optimo para hidroponia';
      if (ph <= 7.5) return 'Ligeramente alcalino';
      return 'Alcalino';
    }

    function updateUI() {
      // pH
      const phVal = document.getElementById('phValue');
      const phVolt = document.getElementById('phVoltage');
      const phMarker = document.getElementById('phMarker');
      const phStatus = document.getElementById('phStatus');
      const ph = state.ph;
      phVal.textContent = ph > 0 ? ph.toFixed(1) : '--';
      phVal.style.color = ph > 0 ? getPhColor(ph) : '#8ab4c4';
      phVolt.textContent = 'Voltaje: ' + (state.phV > 0 ? state.phV.toFixed(3) : '--') + ' V';
      phMarker.style.left = (Math.min(Math.max(ph, 0), 14) / 14 * 100) + '%';
      phStatus.textContent = ph > 0 ? getPhLabel(ph) : (state.adsOk ? 'Leyendo...' : 'Sensor no detectado');
      phStatus.style.color = ph > 0 ? getPhColor(ph) : '#f87171';

      // Bomba principal
      const pDot = document.getElementById('pumpDot');
      const pText = document.getElementById('pumpText');
      const pBtn = document.getElementById('pumpBtn');
      pDot.className = 'status-dot ' + (state.pump ? 'dot-on' : 'dot-off');
      pText.textContent = state.pump ? 'Encendida' : 'Apagada';
      pBtn.textContent = state.pump ? 'APAGAR' : 'ENCENDER';
      pBtn.className = 'btn ' + (state.pump ? 'btn-off' : 'btn-on');

      // Peristáltica 1 (pH+)
      const d1 = document.getElementById('ph1Dot');
      const t1 = document.getElementById('ph1Text');
      const b1 = document.getElementById('ph1Btn');
      d1.className = 'status-dot ' + (state.peri1 ? 'dot-on' : 'dot-off');
      t1.textContent = state.peri1 ? 'On' : 'Off';
      b1.textContent = state.peri1 ? 'pH+ OFF' : 'pH+ ON';
      b1.className = 'btn ' + (state.peri1 ? 'btn-purple-off' : 'btn-purple');

      // Peristáltica 2 (pH-)
      const d2 = document.getElementById('ph2Dot');
      const t2 = document.getElementById('ph2Text');
      const b2 = document.getElementById('ph2Btn');
      d2.className = 'status-dot ' + (state.peri2 ? 'dot-on' : 'dot-off');
      t2.textContent = state.peri2 ? 'On' : 'Off';
      b2.textContent = state.peri2 ? 'pH- OFF' : 'pH- ON';
      b2.className = 'btn ' + (state.peri2 ? 'btn-purple-off' : 'btn-purple');
    }

    fetchState();
    setInterval(fetchState, 3000);
  </script>
</body>
</html>
)rawliteral";

// =============================================
//  WiFi: guardar/cargar credenciales en flash
// =============================================
String savedSSID, savedPass;

void loadWiFiCredentials() {
  prefs.begin("wifi", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();
}

void saveWiFiCredentials(const String &ssid, const String &pass) {
  prefs.begin("wifi", false);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", pass);
  prefs.end();
}

void clearWiFiCredentials() {
  prefs.begin("wifi", false);
  prefs.clear();
  prefs.end();
}

// =============================================
//  Modo AP: portal de configuración
// =============================================
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

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  WiFi.scanNetworks(true);
  server.begin();
  Serial.println("Portal de configuracion iniciado.");
}

// =============================================
//  Modo STA: conexión normal
// =============================================
bool connectToWiFi() {
  Serial.printf("Conectando a \"%s\"", savedSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(savedSSID.c_str(), savedPass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  return WiFi.status() == WL_CONNECTED;
}

// =============================================
//  Control de bombas
// =============================================
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

String buildStateJson() {
  return "{\"pump\":" + String(pumpState ? "true" : "false") +
         ",\"peri1\":" + String(peri1State ? "true" : "false") +
         ",\"peri2\":" + String(peri2State ? "true" : "false") +
         ",\"ph\":" + String(currentPH, 2) +
         ",\"phV\":" + String(currentVoltage, 3) +
         ",\"adsOk\":" + String(adsReady ? "true" : "false") +
         ",\"wifi\":\"" + WiFi.SSID() + "\"" +
         ",\"ip\":\"" + WiFi.localIP().toString() + "\"}";
}

void setupAppRoutes() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", INDEX_HTML);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildStateJson());
  });

  server.on("/pump", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("action")) {
      String action = request->getParam("action")->value();
      if (action == "on")  setPump(true);
      if (action == "off") setPump(false);
    }
    request->send(200, "application/json", buildStateJson());
  });

  server.on("/peristaltic", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("id") && request->hasParam("action")) {
      int id = request->getParam("id")->value().toInt();
      String action = request->getParam("action")->value();
      if (id >= 1 && id <= 2) {
        setPeristaltic(id, action == "on");
      }
    }
    request->send(200, "application/json", buildStateJson());
  });

  server.on("/reset-wifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Reiniciando...");
    clearWiFiCredentials();
    delay(1000);
    ESP.restart();
  });
}

// =============================================
//  Botón BOOT para resetear WiFi (mantener 3s)
// =============================================
void checkResetButton() {
  if (digitalRead(WIFI_RESET_PIN) == LOW) {
    Serial.println("Boton BOOT presionado...");
    unsigned long pressStart = millis();
    while (digitalRead(WIFI_RESET_PIN) == LOW) {
      delay(100);
      if (millis() - pressStart > 3000) {
        Serial.println("Reset WiFi!");
        clearWiFiCredentials();
        delay(500);
        ESP.restart();
      }
    }
  }
}

// =============================================
//  Setup & Loop
// =============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== Hidroponia IoT - ESP32 ===");

  pinMode(RELAY_PUMP_PIN, OUTPUT);
  pinMode(PERISTALTIC_1_PIN, OUTPUT);
  pinMode(PERISTALTIC_2_PIN, OUTPUT);
  digitalWrite(RELAY_PUMP_PIN, LOW);
  digitalWrite(PERISTALTIC_1_PIN, LOW);
  digitalWrite(PERISTALTIC_2_PIN, LOW);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setTimeOut(50);

  Serial.println("Escaneando dispositivos I2C...");
  int devicesFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  Dispositivo I2C encontrado en 0x%02X\n", addr);
      devicesFound++;
    }
  }
  if (devicesFound == 0) {
    Serial.println("  Ningun dispositivo I2C detectado! Verificar cableado SDA(21)/SCL(22).");
  }

  ads.setGain(GAIN_ONE);
  if (ads.begin()) {
    adsReady = true;
    Serial.println("ADS1115 OK en direccion 0x48");
    Serial.println("Leyendo todos los canales del ADS1115:");
    for (int ch = 0; ch < 4; ch++) {
      int16_t raw = ads.readADC_SingleEnded(ch);
      float volts = ads.computeVolts(raw);
      Serial.printf("  Canal A%d: raw=%d  volts=%.4fV\n", ch, raw, volts);
    }
  } else {
    Serial.println("ADS1115 no detectado! Verificar conexiones I2C y pin ADDR a GND.");
  }

  loadWiFiCredentials();

  if (savedSSID.length() > 0) {
    if (connectToWiFi()) {
      Serial.println("WiFi conectado!");
      Serial.print("Red: ");
      Serial.println(WiFi.SSID());
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());

      setupAppRoutes();
      server.begin();
      Serial.print("Abrir en navegador: http://");
      Serial.println(WiFi.localIP());
      return;
    }
    Serial.println("No se pudo conectar al WiFi guardado.");
  }

  Serial.println("Sin credenciales WiFi. Iniciando portal de configuracion...");
  startAPMode();
}

void loop() {
  if (apMode) {
    dnsServer.processNextRequest();
    return;
  }

  updatePH();
  checkResetButton();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reiniciando...");
    delay(3000);
    ESP.restart();
  }
  delay(100);
}
