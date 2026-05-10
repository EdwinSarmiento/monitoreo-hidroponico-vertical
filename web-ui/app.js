let client = null;
let currentDeviceToken = '';

// =============================================
//  Alert System State
// =============================================
let alertSnoozed = false;
let snoozeTimeout = null;
let lastLevelState = 'OK';
const SNOOZE_DURATION = 5 * 60 * 1000;

// =============================================
//  pH Auto-regulation State (controlada por firmware)
// =============================================
let phAutoEnabled = false;
let phTarget = parseFloat(localStorage.getItem('ph_target') || '6.0');
let phTolerance = parseFloat(localStorage.getItem('ph_tolerance') || '0.3');
let lastPH = null;
let pumpIsOn = false;

// Estado del ciclo reportado por el firmware
let phCycleState = 'idle';
let phCycleDirection = '';
let phProgressInterval = null;

// =============================================
//  UI Elements
// =============================================
const statusBadge = document.getElementById('connection-status');
const connectBtn = document.getElementById('connect-btn');
const brokerInfo = document.getElementById('broker-info');
const logContent = document.getElementById('log-content');
const phValue = document.getElementById('ph-value');
const phVoltage = document.getElementById('ph-voltage');
const phMarker = document.getElementById('ph-marker');
const pumpBtn = document.getElementById('pump-btn');
const pumpStatus = document.getElementById('pump-status');
const peri1Btn = document.getElementById('peri1-btn');
const peri2Btn = document.getElementById('peri2-btn');
const tempValue = document.getElementById('temp-value');
const humValue = document.getElementById('hum-value');
const levelBadge = document.getElementById('level-badge');
const levelDesc = document.getElementById('level-desc');
const alertBanner = document.getElementById('alert-banner');
const alertDismiss = document.getElementById('alert-dismiss');
const historyBody = document.getElementById('history-body');
const daysBadge = document.getElementById('days-badge');
const phAutoToggle = document.getElementById('ph-auto-toggle');
const phAutoStatus = document.getElementById('ph-auto-status');
const phTargetValue = document.getElementById('ph-target-value');
const phRangeText = document.getElementById('ph-range-text');
const phTargetUp = document.getElementById('ph-target-up');
const phTargetDown = document.getElementById('ph-target-down');
const phCycleStatusEl = document.getElementById('ph-cycle-status');
const phCycleProgress = document.getElementById('ph-cycle-progress');
const phCycleText = document.getElementById('ph-cycle-text');

document.getElementById('device-token').value = localStorage.getItem('mqtt_token') || 'esp32_hidro';

// Load saved timing & tolerance
document.getElementById('dose-seconds').value = localStorage.getItem('ph_dose_s') || '1';
document.getElementById('settle-seconds').value = localStorage.getItem('ph_settle_s') || '90';
document.getElementById('ph-tolerance').value = localStorage.getItem('ph_tolerance') || '0.3';

function sendPhConfig() {
    if (!client || !client.connected) return;
    const topic = `hidroponia/${currentDeviceToken}/cmd`;
    const payload = JSON.stringify({
        phTarget: phTarget,
        phTol: phTolerance,
        phDose: parseInt(document.getElementById('dose-seconds').value) || 1,
        phSettle: parseInt(document.getElementById('settle-seconds').value) || 90
    });
    client.publish(topic, payload);
    addLog(`Config pH enviada: ${payload}`, 'out');
}

document.getElementById('dose-seconds').addEventListener('change', (e) => {
    localStorage.setItem('ph_dose_s', e.target.value);
    sendPhConfig();
});
document.getElementById('settle-seconds').addEventListener('change', (e) => {
    localStorage.setItem('ph_settle_s', e.target.value);
    sendPhConfig();
});
document.getElementById('ph-tolerance').addEventListener('change', (e) => {
    phTolerance = parseFloat(e.target.value) || 0.3;
    phTolerance = Math.max(0.1, Math.min(2.0, phTolerance));
    localStorage.setItem('ph_tolerance', phTolerance.toFixed(1));
    updatePhTargetUI();
    sendPhConfig();
});

// =============================================
//  Broker: same host that served this page
// =============================================
function getBrokerHost() {
    return window.location.hostname || 'localhost';
}

// =============================================
//  Logging
// =============================================
function addLog(msg, type = 'system') {
    const entry = document.createElement('div');
    const time = new Date().toLocaleTimeString();
    entry.className = `log-entry ${type}`;
    entry.textContent = `[${time}] ${msg}`;
    logContent.appendChild(entry);
    logContent.scrollTop = logContent.scrollHeight;
}

// =============================================
//  MQTT Connection
// =============================================
function connect() {
    const token = document.getElementById('device-token').value.trim() || 'esp32_hidro';
    const host = getBrokerHost();
    const port = 9001;

    currentDeviceToken = token;
    localStorage.setItem('mqtt_token', token);

    if (client) client.end();

    const wsUrl = `ws://${host}:${port}`;
    addLog(`Conectando a ${wsUrl}...`);
    brokerInfo.textContent = `Broker: ${host}:${port}`;

    client = mqtt.connect(wsUrl, {
        clientId: 'web_ui_' + Math.random().toString(16).substring(2, 8),
        keepalive: 60,
    });

    client.on('connect', () => {
        addLog('Conectado al Broker MQTT!', 'in');
        statusBadge.textContent = 'Conectado';
        statusBadge.className = 'status-badge connected';
        connectBtn.textContent = 'Reconectar';
        const stateTopic = `hidroponia/${token}/state`;
        client.subscribe(stateTopic, (err) => {
            if (!err) addLog(`Suscrito a: ${stateTopic}`, 'system');
        });
    });

    client.on('message', (topic, message) => {
        const payload = JSON.parse(message.toString());
        addLog(`Recibido: ${message.toString()}`, 'in');
        updateUI(payload);
    });

    client.on('error', (err) => {
        addLog(`Error: ${err.message}`, 'error');
        statusBadge.textContent = 'Error';
        statusBadge.className = 'status-badge disconnected';
    });

    client.on('close', () => {
        addLog('Conexión cerrada', 'system');
        statusBadge.textContent = 'Desconectado';
        statusBadge.className = 'status-badge disconnected';
    });
}

// =============================================
//  UI Update
// =============================================
function updateUI(data) {
    if (data.ph !== undefined) {
        const ph = data.ph;
        lastPH = ph;
        phValue.textContent = ph.toFixed(2);
        phVoltage.textContent = `Voltaje: ${data.phV.toFixed(3)} V`;
        const pos = (Math.min(Math.max(ph, 0), 14) / 14) * 100;
        phMarker.style.left = `${pos}%`;
    }

    if (data.pump !== undefined) {
        pumpIsOn = data.pump;
        updatePumpUI();
    }

    if (data.peri1 !== undefined) {
        peri1Btn.className = `btn-circular ${data.peri1 ? 'active' : ''}`;
        peri1Btn.textContent = data.peri1 ? 'OFF' : 'ON';
    }

    if (data.peri2 !== undefined) {
        peri2Btn.className = `btn-circular btn-red ${data.peri2 ? 'active' : ''}`;
        peri2Btn.textContent = data.peri2 ? 'OFF' : 'ON';
    }

    if (data.temp !== undefined) tempValue.textContent = data.temp.toFixed(1);
    if (data.hum !== undefined) humValue.textContent = data.hum.toFixed(1);

    if (data.level !== undefined) {
        const isLow = data.level === 'LOW';
        levelBadge.textContent = isLow ? 'BAJO' : 'OPTIMO';
        levelBadge.className = `badge ${isLow ? 'low' : 'ok'}`;
        levelDesc.textContent = isLow ? '¡ALERTA! Depósito de agua casi vacío.' : 'Depósito con agua suficiente.';
        handleLevelAlert(isLow);
    }

    if (data.hum !== undefined && data.temp !== undefined) {
        recordHistory(data.hum, data.temp);
    }

    // Sincronizar estado de auto-regulación desde el firmware
    if (data.phAuto !== undefined) {
        phAutoEnabled = data.phAuto;
        phAutoToggle.checked = phAutoEnabled;
        phAutoStatus.textContent = phAutoEnabled ? 'AUTO' : 'MANUAL';
        phAutoStatus.className = `status-text ${phAutoEnabled ? 'on' : 'off'}`;
    }
    if (data.phTarget !== undefined) {
        phTarget = data.phTarget;
        phTargetValue.textContent = phTarget.toFixed(1);
    }
    if (data.phTol !== undefined) {
        phTolerance = data.phTol;
        document.getElementById('ph-tolerance').value = phTolerance.toFixed(1);
    }
    if (data.phDose !== undefined) {
        document.getElementById('dose-seconds').value = data.phDose;
    }
    if (data.phSettle !== undefined) {
        document.getElementById('settle-seconds').value = data.phSettle;
    }
    if (data.phTarget !== undefined || data.phTol !== undefined) {
        const lo = (phTarget - phTolerance).toFixed(1);
        const hi = (phTarget + phTolerance).toFixed(1);
        phRangeText.textContent = `${lo} – ${hi}`;
    }

    // Mostrar estado del ciclo de dosificación del firmware
    if (data.phState !== undefined) {
        phCycleState = data.phState;
        phCycleDirection = data.phDir || '';
        updateCycleStatusUI();
    }
}

function updateCycleStatusUI() {
    if (phCycleState === 'dosing') {
        phCycleStatusEl.classList.remove('hidden');
        phCycleProgress.className = 'ph-cycle-progress dosing';
        phCycleProgress.style.width = '100%';
        const label = phCycleDirection === 'up' ? 'Dosificando pH+ ...' : 'Dosificando pH− ...';
        phCycleText.textContent = label;
    } else if (phCycleState === 'settling') {
        phCycleStatusEl.classList.remove('hidden');
        phCycleProgress.className = 'ph-cycle-progress settling';
        phCycleProgress.style.width = '100%';
        phCycleText.textContent = 'Esperando estabilización...';
    } else if (phAutoEnabled) {
        phCycleStatusEl.classList.remove('hidden');
        phCycleProgress.className = 'ph-cycle-progress idle';
        phCycleProgress.style.width = '100%';
        phCycleText.textContent = 'pH en rango ✓';
    } else {
        phCycleStatusEl.classList.add('hidden');
    }
}

// =============================================
//  Pump Button
// =============================================
function updatePumpUI() {
    const label = pumpBtn.querySelector('.pump-label');
    if (pumpIsOn) {
        pumpBtn.classList.add('on');
        pumpStatus.textContent = 'ON';
        pumpStatus.className = 'status-text on';
        label.textContent = 'APAGAR';
    } else {
        pumpBtn.classList.remove('on');
        pumpStatus.textContent = 'OFF';
        pumpStatus.className = 'status-text off';
        label.textContent = 'ENCENDER';
    }
}

pumpBtn.addEventListener('click', () => {
    pumpIsOn = !pumpIsOn;
    updatePumpUI();
    sendCommand('pump', pumpIsOn);
});

// =============================================
//  pH Auto-regulation: config → firmware via MQTT
// =============================================
function updatePhTargetUI() {
    phTargetValue.textContent = phTarget.toFixed(1);
    const lo = (phTarget - phTolerance).toFixed(1);
    const hi = (phTarget + phTolerance).toFixed(1);
    phRangeText.textContent = `${lo} – ${hi}`;
    localStorage.setItem('ph_target', phTarget.toFixed(1));
    localStorage.setItem('ph_tolerance', phTolerance.toFixed(1));
}

phTargetUp.addEventListener('click', () => {
    if (phTarget < 10.0) {
        phTarget = Math.round((phTarget + 0.1) * 10) / 10;
        updatePhTargetUI();
        sendPhConfig();
    }
});

phTargetDown.addEventListener('click', () => {
    if (phTarget > 2.0) {
        phTarget = Math.round((phTarget - 0.1) * 10) / 10;
        updatePhTargetUI();
        sendPhConfig();
    }
});

phAutoToggle.addEventListener('change', (e) => {
    phAutoEnabled = e.target.checked;
    sendCommand('phAuto', phAutoEnabled);
    if (phAutoEnabled) {
        phAutoStatus.textContent = 'AUTO';
        phAutoStatus.className = 'status-text on';
        sendPhConfig();
        addLog(`Auto-regulación enviada al firmware → pH objetivo: ${phTarget.toFixed(1)} (±${phTolerance.toFixed(1)})`, 'system');
    } else {
        phAutoStatus.textContent = 'MANUAL';
        phAutoStatus.className = 'status-text off';
        addLog('Auto-regulación desactivada en firmware', 'system');
        phCycleStatusEl.classList.add('hidden');
    }
});

// =============================================
//  Water Level Alert System
// =============================================
function playAlertBeep() {
    try {
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        for (let i = 0; i < 2; i++) {
            const osc = audioCtx.createOscillator();
            const gain = audioCtx.createGain();
            osc.connect(gain);
            gain.connect(audioCtx.destination);
            osc.type = 'sine';
            osc.frequency.setValueAtTime(880, audioCtx.currentTime);
            gain.gain.setValueAtTime(0.3, audioCtx.currentTime + i * 0.3);
            gain.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + i * 0.3 + 0.2);
            osc.start(audioCtx.currentTime + i * 0.3);
            osc.stop(audioCtx.currentTime + i * 0.3 + 0.2);
        }
    } catch (e) {}
}

function showAlert() { alertBanner.classList.add('visible'); document.body.classList.add('alert-active'); }
function hideAlert() { alertBanner.classList.remove('visible'); document.body.classList.remove('alert-active'); }

function handleLevelAlert(isLow) {
    if (isLow) {
        if (lastLevelState === 'OK') {
            alertSnoozed = false;
            if (snoozeTimeout) { clearTimeout(snoozeTimeout); snoozeTimeout = null; }
        }
        if (!alertSnoozed) { showAlert(); playAlertBeep(); }
    } else {
        hideAlert();
        alertSnoozed = false;
        if (snoozeTimeout) { clearTimeout(snoozeTimeout); snoozeTimeout = null; }
    }
    lastLevelState = isLow ? 'LOW' : 'OK';
}

alertDismiss.addEventListener('click', () => {
    alertSnoozed = true;
    hideAlert();
    addLog('Alerta de nivel silenciada por 5 minutos.', 'system');
    snoozeTimeout = setTimeout(() => {
        alertSnoozed = false;
        if (lastLevelState === 'LOW') { showAlert(); playAlertBeep(); addLog('Alerta reactivada.', 'error'); }
    }, SNOOZE_DURATION);
});

// =============================================
//  Humidity History (localStorage)
// =============================================
const HISTORY_KEY = 'humidity_history';
const MAX_DAYS = 30;

function getTodayKey() { return new Date().toISOString().split('T')[0]; }
function loadHistory() { try { return JSON.parse(localStorage.getItem(HISTORY_KEY)) || []; } catch { return []; } }
function saveHistory(h) { localStorage.setItem(HISTORY_KEY, JSON.stringify(h)); }

function recordHistory(hum, temp) {
    const today = getTodayKey();
    let history = loadHistory();
    let rec = history.find(r => r.date === today);
    if (!rec) {
        rec = { date: today, humMin: hum, humMax: hum, humSum: 0, humCount: 0, tempSum: 0, tempCount: 0 };
        history.unshift(rec);
    }
    rec.humMin = Math.min(rec.humMin, hum);
    rec.humMax = Math.max(rec.humMax, hum);
    rec.humSum += hum;
    rec.humCount += 1;
    rec.tempSum += temp;
    rec.tempCount += 1;
    if (history.length > MAX_DAYS) history = history.slice(0, MAX_DAYS);
    saveHistory(history);
    renderHistoryTable(history);
}

function formatDate(d) { const p = d.split('-'); return `${p[2]}/${p[1]}/${p[0]}`; }

function renderHistoryTable(history) {
    if (!history) history = loadHistory();
    daysBadge.textContent = `${history.length}/${MAX_DAYS} días`;
    if (history.length === 0) {
        historyBody.innerHTML = '<tr><td colspan="6" class="history-empty"><span class="empty-icon">📭</span>Sin registros aún.</td></tr>';
        return;
    }
    historyBody.innerHTML = history.map(r => {
        const hAvg = r.humCount > 0 ? (r.humSum / r.humCount) : 0;
        const tAvg = r.tempCount > 0 ? (r.tempSum / r.tempCount) : 0;
        return `<tr><td>${formatDate(r.date)}</td><td class="hum-min">${r.humMin.toFixed(1)}%</td><td class="hum-max">${r.humMax.toFixed(1)}%</td><td class="hum-avg">${hAvg.toFixed(1)}%</td><td class="temp-avg">${tAvg.toFixed(1)}°C</td><td>${r.humCount}</td></tr>`;
    }).join('');
}

// =============================================
//  Commands
// =============================================
function sendCommand(cmd, action) {
    if (!client || !client.connected) {
        alert('No estás conectado al Broker.');
        return;
    }
    const topic = `hidroponia/${currentDeviceToken}/cmd`;
    const payload = JSON.stringify({ [cmd]: action });
    client.publish(topic, payload);
    addLog(`Enviado: ${payload}`, 'out');
}

// =============================================
//  Event Listeners
// =============================================
connectBtn.addEventListener('click', connect);

peri1Btn.addEventListener('click', () => {
    if (phAutoEnabled) { addLog('Desactiva auto-regulación en el firmware para control manual.', 'error'); return; }
    const isActive = peri1Btn.classList.contains('active');
    sendCommand('peri1', !isActive);
});

peri2Btn.addEventListener('click', () => {
    if (phAutoEnabled) { addLog('Desactiva auto-regulación en el firmware para control manual.', 'error'); return; }
    const isActive = peri2Btn.classList.contains('active');
    sendCommand('peri2', !isActive);
});

document.getElementById('clear-logs').addEventListener('click', () => { logContent.innerHTML = ''; });

document.getElementById('clear-history').addEventListener('click', () => {
    if (confirm('¿Eliminar todo el historial de humedad?')) {
        localStorage.removeItem(HISTORY_KEY);
        renderHistoryTable([]);
        addLog('Historial eliminado.', 'system');
    }
});

// =============================================
//  Initialize
// =============================================
updatePhTargetUI();
renderHistoryTable();
connect();
