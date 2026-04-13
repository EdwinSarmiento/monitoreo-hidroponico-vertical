let client = null;
let currentDeviceToken = '';

// UI Elements
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

// Load saved config
document.getElementById('broker-host').value = localStorage.getItem('mqtt_host') || '';
document.getElementById('device-token').value = localStorage.getItem('mqtt_token') || 'esp32_hidro';
document.getElementById('mqtt-user').value = localStorage.getItem('mqtt_user') || '';
document.getElementById('mqtt-pass').value = localStorage.getItem('mqtt_pass') || '';

function addLog(msg, type = 'system') {
    const entry = document.createElement('div');
    const time = new Date().toLocaleTimeString();
    entry.className = `log-entry ${type}`;
    entry.textContent = `[${time}] ${msg}`;
    logContent.appendChild(entry);
    logContent.scrollTop = logContent.scrollHeight;
}

function connect() {
    const host = document.getElementById('broker-host').value;
    const token = document.getElementById('device-token').value;

    if (!host) {
        alert('Por favor, ingresa la IP del servidor.');
        return;
    }

    currentDeviceToken = token;
    const mqttUser = document.getElementById('mqtt-user').value.trim();
    const mqttPass = document.getElementById('mqtt-pass').value;
    localStorage.setItem('mqtt_host', host);
    localStorage.setItem('mqtt_token', token);
    localStorage.setItem('mqtt_user', mqttUser);
    localStorage.setItem('mqtt_pass', mqttPass);

    if (client) {
        client.end();
    }

    addLog(`Conectando al servidor ws://${host}:9001...`);
    const wsOpts = {
        clientId: 'web_ui_' + Math.random().toString(16).substring(2, 8),
        keepalive: 60,
    };
    if (mqttUser) {
        wsOpts.username = mqttUser;
        wsOpts.password = mqttPass;
    }
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
}

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

// Event Listeners
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
