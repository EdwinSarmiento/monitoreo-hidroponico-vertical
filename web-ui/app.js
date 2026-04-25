let client = null;
let currentDeviceToken = '';

// =============================================
//  Alert System State
// =============================================
let alertSnoozed = false;
let snoozeTimeout = null;
let lastLevelState = 'OK';
const SNOOZE_DURATION = 5 * 60 * 1000; // 5 minutes

// =============================================
//  UI Elements
// =============================================
const statusBadge = document.getElementById('connection-status');
const connectBtn = document.getElementById('connect-btn');
const logContent = document.getElementById('log-content');
const phValue = document.getElementById('ph-value');
const phVoltage = document.getElementById('ph-voltage');
const phMarker = document.getElementById('ph-marker');
const pumpToggle = document.getElementById('pump-toggle');
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

// Load saved config
document.getElementById('broker-host').value = localStorage.getItem('mqtt_host') || '';
document.getElementById('device-token').value = localStorage.getItem('mqtt_token') || 'esp32_hidro';

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
    const host = document.getElementById('broker-host').value;
    const token = document.getElementById('device-token').value;

    if (!host) {
        alert('Por favor, ingresa la IP del servidor.');
        return;
    }

    currentDeviceToken = token;
    localStorage.setItem('mqtt_host', host);
    localStorage.setItem('mqtt_token', token);

    if (client) {
        client.end();
    }

    addLog(`Conectando al servidor ws://${host}:9001...`);
    const wsOpts = {
        clientId: 'web_ui_' + Math.random().toString(16).substring(2, 8),
        keepalive: 60,
    };
    client = mqtt.connect(`ws://${host}:9001`, wsOpts);

    client.on('connect', () => {
        addLog('Conectado al Broker MQTT!', 'in');
        statusBadge.textContent = 'Conectado';
        statusBadge.className = 'status-badge connected';
        connectBtn.textContent = 'Reconectar';
        
        // Subscribe to topics
        const stateTopic = `hidroponia/${token}/state`;
        client.subscribe(stateTopic, (err) => {
            if (!err) {
                addLog(`Suscrito a: ${stateTopic}`, 'system');
            }
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
        phValue.textContent = data.ph.toFixed(2);
        phVoltage.textContent = `Voltaje: ${data.phV.toFixed(3)} V`;
        // Map pH 0-14 to 0-100%
        const pos = (Math.min(Math.max(data.ph, 0), 14) / 14) * 100;
        phMarker.style.left = `${pos}%`;
    }

    if (data.pump !== undefined) {
        pumpToggle.checked = data.pump;
        pumpStatus.textContent = data.pump ? 'ON' : 'OFF';
        pumpStatus.className = `status-text ${data.pump ? 'on' : 'off'}`;
    }

    if (data.peri1 !== undefined) {
        peri1Btn.className = `btn-circular ${data.peri1 ? 'active' : ''}`;
        peri1Btn.textContent = data.peri1 ? 'OFF' : 'ON';
    }

    if (data.peri2 !== undefined) {
        peri2Btn.className = `btn-circular btn-red ${data.peri2 ? 'active' : ''}`;
        peri2Btn.textContent = data.peri2 ? 'OFF' : 'ON';
    }

    if (data.temp !== undefined) {
        tempValue.textContent = data.temp.toFixed(1);
    }

    if (data.hum !== undefined) {
        humValue.textContent = data.hum.toFixed(1);
    }

    if (data.level !== undefined) {
        const isLow = data.level === 'LOW';
        levelBadge.textContent = isLow ? 'BAJO' : 'OPTIMO';
        levelBadge.className = `badge ${isLow ? 'low' : 'ok'}`;
        levelDesc.textContent = isLow ? '¡ALERTA! Depósito de agua casi vacío.' : 'Depósito con agua suficiente.';
        
        // Handle water level alert
        handleLevelAlert(isLow);
    }

    // Record humidity & temperature history
    if (data.hum !== undefined && data.temp !== undefined) {
        recordHistory(data.hum, data.temp);
    }
}

// =============================================
//  Water Level Alert System
// =============================================
function playAlertBeep() {
    try {
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();
        
        // Play two short beeps
        for (let i = 0; i < 2; i++) {
            const oscillator = audioCtx.createOscillator();
            const gainNode = audioCtx.createGain();
            
            oscillator.connect(gainNode);
            gainNode.connect(audioCtx.destination);
            
            oscillator.type = 'sine';
            oscillator.frequency.setValueAtTime(880, audioCtx.currentTime); // A5 note
            
            gainNode.gain.setValueAtTime(0.3, audioCtx.currentTime + i * 0.3);
            gainNode.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + i * 0.3 + 0.2);
            
            oscillator.start(audioCtx.currentTime + i * 0.3);
            oscillator.stop(audioCtx.currentTime + i * 0.3 + 0.2);
        }
    } catch (e) {
        // Web Audio API not available — silently skip
    }
}

function showAlert() {
    alertBanner.classList.add('visible');
    document.body.classList.add('alert-active');
}

function hideAlert() {
    alertBanner.classList.remove('visible');
    document.body.classList.remove('alert-active');
}

function handleLevelAlert(isLow) {
    if (isLow) {
        // If transitioning from OK → LOW, reset snooze
        if (lastLevelState === 'OK') {
            alertSnoozed = false;
            if (snoozeTimeout) {
                clearTimeout(snoozeTimeout);
                snoozeTimeout = null;
            }
        }
        
        if (!alertSnoozed) {
            showAlert();
            playAlertBeep();
        }
    } else {
        // Water level is OK — hide alert & reset snooze
        hideAlert();
        alertSnoozed = false;
        if (snoozeTimeout) {
            clearTimeout(snoozeTimeout);
            snoozeTimeout = null;
        }
    }
    
    lastLevelState = isLow ? 'LOW' : 'OK';
}

// Dismiss/snooze alert for 5 minutes
alertDismiss.addEventListener('click', () => {
    alertSnoozed = true;
    hideAlert();
    addLog('Alerta de nivel silenciada por 5 minutos.', 'system');
    
    snoozeTimeout = setTimeout(() => {
        alertSnoozed = false;
        // If still low, re-show alert
        if (lastLevelState === 'LOW') {
            showAlert();
            playAlertBeep();
            addLog('Alerta de nivel reactivada — El nivel sigue bajo.', 'error');
        }
    }, SNOOZE_DURATION);
});

// =============================================
//  Humidity History (localStorage)
// =============================================
const HISTORY_KEY = 'humidity_history';
const MAX_DAYS = 30;

function getTodayKey() {
    const now = new Date();
    return now.toISOString().split('T')[0]; // "2026-04-25"
}

function loadHistory() {
    try {
        const raw = localStorage.getItem(HISTORY_KEY);
        return raw ? JSON.parse(raw) : [];
    } catch (e) {
        return [];
    }
}

function saveHistory(history) {
    localStorage.setItem(HISTORY_KEY, JSON.stringify(history));
}

function recordHistory(hum, temp) {
    const today = getTodayKey();
    let history = loadHistory();
    
    // Find or create today's record
    let todayRecord = history.find(r => r.date === today);
    
    if (!todayRecord) {
        todayRecord = {
            date: today,
            humMin: hum,
            humMax: hum,
            humSum: 0,
            humCount: 0,
            tempSum: 0,
            tempCount: 0,
        };
        history.unshift(todayRecord); // Add at beginning (newest first)
    }
    
    // Update stats
    todayRecord.humMin = Math.min(todayRecord.humMin, hum);
    todayRecord.humMax = Math.max(todayRecord.humMax, hum);
    todayRecord.humSum += hum;
    todayRecord.humCount += 1;
    todayRecord.tempSum += temp;
    todayRecord.tempCount += 1;
    
    // Enforce 30-day limit
    if (history.length > MAX_DAYS) {
        history = history.slice(0, MAX_DAYS);
    }
    
    saveHistory(history);
    renderHistoryTable(history);
}

function formatDate(dateStr) {
    // "2026-04-25" → "25/04/2026"
    const parts = dateStr.split('-');
    return `${parts[2]}/${parts[1]}/${parts[0]}`;
}

function renderHistoryTable(history) {
    if (!history) {
        history = loadHistory();
    }
    
    // Update days badge
    daysBadge.textContent = `${history.length}/${MAX_DAYS} días`;
    
    if (history.length === 0) {
        historyBody.innerHTML = `
            <tr>
                <td colspan="6" class="history-empty">
                    <span class="empty-icon">📭</span>
                    Sin registros aún. Los datos se almacenan automáticamente al conectar.
                </td>
            </tr>
        `;
        return;
    }
    
    historyBody.innerHTML = history.map(record => {
        const humAvg = record.humCount > 0 ? (record.humSum / record.humCount) : 0;
        const tempAvg = record.tempCount > 0 ? (record.tempSum / record.tempCount) : 0;
        
        return `
            <tr>
                <td>${formatDate(record.date)}</td>
                <td class="hum-min">${record.humMin.toFixed(1)}%</td>
                <td class="hum-max">${record.humMax.toFixed(1)}%</td>
                <td class="hum-avg">${humAvg.toFixed(1)}%</td>
                <td class="temp-avg">${tempAvg.toFixed(1)}°C</td>
                <td>${record.humCount}</td>
            </tr>
        `;
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

pumpToggle.addEventListener('change', (e) => {
    sendCommand('pump', e.target.checked);
});

peri1Btn.addEventListener('click', () => {
    const isActive = peri1Btn.classList.contains('active');
    sendCommand('peri1', !isActive);
});

peri2Btn.addEventListener('click', () => {
    const isActive = peri2Btn.classList.contains('active');
    sendCommand('peri2', !isActive);
});

document.getElementById('clear-logs').addEventListener('click', () => {
    logContent.innerHTML = '';
});

document.getElementById('clear-history').addEventListener('click', () => {
    if (confirm('¿Eliminar todo el historial de humedad?')) {
        localStorage.removeItem(HISTORY_KEY);
        renderHistoryTable([]);
        addLog('Historial de humedad eliminado.', 'system');
    }
});

// =============================================
//  Initialize
// =============================================
renderHistoryTable();
