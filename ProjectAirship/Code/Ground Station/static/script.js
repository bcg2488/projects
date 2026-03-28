const keyToId = {
    'ArrowUp': 'forward',
    'ArrowLeft': 'left',
    'ArrowRight': 'right',
    'w': 'up',
    's': 'down',
};
const activeKeys = new Set();

// --- Command functions ---
function sendStartCommand() {
    fetch('/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'command=start-blimp'
    }).then(res => {
        if (res.ok) console.log("Start command sent successfully");
        else console.error("Failed to send start command");
    }).catch(err => console.error("Error sending start command:", err));
}

function sendStopCommand() {
    fetch('/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'command=stop-blimp'
    }).then(res => {
        if (res.ok) console.log("Stop command sent successfully");
        else console.error("Failed to send stop command");
    }).catch(err => console.error("Error sending stop command:", err));
}

// --- Button activation logic ---
const startButton = document.getElementById('start');
const stopButton = document.getElementById('stop-btn');
const sendManualButton = document.getElementById('send-manual');

if (startButton && stopButton) {
    startButton.addEventListener('click', () => {
        sendStartCommand();
        activeKeys.add('start');
        activeKeys.delete('stop-btn');

        // Highlight start, remove highlight from stop
        startButton.classList.add('active');
        stopButton.classList.remove('active');
    });

    stopButton.addEventListener('click', () => {
        sendStopCommand();
        activeKeys.add('stop');
        activeKeys.delete('start');

        // Highlight stop, remove highlight from start
        stopButton.classList.add('active');
        startButton.classList.remove('active');
    });
}
if (sendManualButton) {
    sendManualButton.addEventListener('click', () => {
        sendManualButton.classList.add('active');
        sendManualCommand();

        setTimeout(() => {
            sendManualButton.classList.remove('active');
        }, 200);

    });
}

// --- Keyboard control ---
function sendCommand(cmd) {
    fetch('/command', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'command=' + encodeURIComponent(cmd)
    });
    console.log("Sent command:", cmd);
}

document.addEventListener('keydown', event => {
    const key = event.key;
    const command = keyToId[key];
    if (!command || activeKeys.has(key)) return;

    activeKeys.add(key);
    sendCommand(command);

    const box = document.getElementById(command);
    if (box) box.classList.add('active');
});

document.addEventListener('keyup', event => {
    const key = event.key;
    const command = keyToId[key];
    if (!command || !activeKeys.has(key)) return;

    activeKeys.delete(key);
    sendCommand('stop-' + command);

    const box = document.getElementById(command);
    if (box) box.classList.remove('active');
});

// --- PID toggle ---
document.addEventListener("DOMContentLoaded", () => {
    const pidSwitch = document.getElementById('pidSwitch');
    if (pidSwitch) {
        pidSwitch.addEventListener('change', () => {
            sendCommand('pid-toggle');
            console.log("PID toggle command sent!");
        });
    }
});

// --- Manual motor sliders ---
const motorIds = ['frontleft', 'frontright', 'back', 'Taltitude'];
motorIds.forEach(id => {
    const slider = document.getElementById(id + '-manual');
    const display = document.getElementById(id + '-value');
    if (slider && display) {
        slider.addEventListener('input', () => {
            display.textContent = slider.value;
        });
    }
});

function sendManualCommand() {
    const frontleft = document.getElementById('frontleft-manual').value;
    const frontright = document.getElementById('frontright-manual').value;
    const back = document.getElementById('back-manual').value;
    const altitude = document.getElementById('Taltitude-manual').value;

    const formData = new URLSearchParams();
    formData.append('frontleft', frontleft);
    formData.append('frontright', frontright);
    formData.append('back', back);
    formData.append('Taltitude', altitude);

    fetch('/manual', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: formData
    }).then(res => {
        if (res.ok) console.log("Manual command sent successfully");
        else console.error("Failed to send manual command");
    }).catch(err => console.error("Error sending manual command:", err));
}

// --- Map + Waypoint logic ---
let waypoints = [];
let blimpMarker = null;
let polyline = null;

const map = L.map('map').setView([32.731, -97.110], 16);
L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 19 }).addTo(map);

map.on('click', e => {
    const { lat, lng } = e.latlng;
    L.marker([lat, lng]).addTo(map).bindPopup(`Waypoint<br>Lat: ${lat.toFixed(5)}<br>Lng: ${lng.toFixed(5)}`).openPopup();
    waypoints.push({ lat, lng });

    console.log("Current waypoints:", waypoints);
    if (polyline) map.removeLayer(polyline);
    polyline = L.polyline(waypoints, { color: 'blue' }).addTo(map);
});

function sendWaypoints() {
    if (waypoints.length === 0) return alert("No waypoints to send");
    fetch('/send_waypoints', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(waypoints)
    }).then(res => {
        if (res.ok) {
            alert("Waypoints sent to the blimp");
            console.log("Waypoints sent:", waypoints);
            waypoints = [];
        } else alert("Failed to send waypoints");
    }).catch(err => console.error("Error sending waypoints", err));
}

// --- Telemetry (WebSocket + fallback) ---
const socket = io();
let socketConnected = false;

socket.on("connect", () => {
    console.log("Connected to server via WebSocket");
    socketConnected = true;
});

socket.on("telemetry_update", data => {
    socketConnected = true;
    console.log("Telemetry update:", data);

    Object.entries(data).forEach(([key, value]) => {
        const p = document.getElementById(key);
        if (p) p.textContent = value;
    });

    if (data.lat && data.lon) {
        const { lat, lon } = data;
        if (blimpMarker) blimpMarker.setLatLng([lat, lon]);
        else blimpMarker = L.marker([lat, lon], { color: 'red' })
            .addTo(map)
            .bindPopup("Blimp Location");
    }
});

socket.on("disconnect", () => {
    socketConnected = false;
    console.warn("Disconnected from server");
});

setInterval(() => {
    if (!socketConnected) {
        fetch("/blimp_position")
            .then(res => res.json())
            .then(data => updateTelemetryDisplay(data))
            .catch(err => console.error("Polling error:", err));
    }
}, 5000);

function updateTelemetryDisplay(data) {
    Object.entries(data).forEach(([key, val]) => {
        const el = document.getElementById(key);
        if (el) el.textContent = val;
    });
}

document.getElementById("connection-status").textContent = "Connected";

