#pragma once
#include <Arduino.h>

// AP mode pages
const char INDEX_HTML_TEMPLATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>AdSink WiFi Setup</title>
<style>
body { font-family: Arial; background:#f2f2f2; text-align:center; }
.card { background:white; width:90%; max-width:420px; margin:20px auto;
        padding:20px; border-radius:12px; box-shadow:0 2px 6px rgba(0,0,0,0.15); }
input, select { width:100%; padding:10px; margin-top:10px; border-radius:8px; border:1px solid #ccc; }
button { margin-top:15px; padding:12px 20px; border:none; border-radius:8px;
         background:#0078ff; color:white; cursor:pointer; width:100%; }
button:hover { background:#005fcc; }
</style>
</head>
<body>

<div class="card">
<h2>AdSink WiFi Setup</h2>

<form action="/save" method="POST">

<p><b>Select WiFi Network</b></p>

<!-- FIXED: SSID + Scan button now aligned in a centered column -->
<div style="display:flex; flex-direction:column; align-items:center; width:100%;">
    <select id="ssid" name="ssid" style="width:100%;"></select>
    <button type="button" onclick="scanNetworks()">Scan Networks</button>
</div>

<p><b>Password</b></p>

<div style="display:flex; flex-direction:column; align-items:center; width:100%;">
    <input type="password" name="pass" style="width:100%;">
</div>


<button type="submit">Save</button>
</form>
</div>

<script>
function scanNetworks() {
    fetch('/scan')
        .then(r => r.json())
        .then(list => {
            let sel = document.getElementById('ssid');
            sel.innerHTML = "";
            list.forEach(ssid => {
                let opt = document.createElement('option');
                opt.value = ssid;
                opt.textContent = ssid;
                sel.appendChild(opt);
            });
        });
}

window.onload = scanNetworks;
</script>

</body>
</html>
)rawliteral";


const char SAVED_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head><meta name="viewport" content="width=device-width, initial-scale=1"><title>Saved</title></head>
<body style="font-family:Arial;text-align:center;background:#f2f2f2;">
<div style="background:white;width:90%;max-width:420px;margin:20px auto;padding:20px;border-radius:12px;">
<h2>Settings Saved</h2>
<p>The device will reconnect now.</p>
</div>
</body>
</html>
)rawliteral";

// Dashboard Navigation
const char DASHBOARD_NAV[] PROGMEM = R"rawliteral(
<style>
body { 
    font-family: Arial; 
    background:#f2f2f2; 
    text-align:center; 
    font-size:130%; 
    margin:0;
}
.navbar {
    background:#0078ff;
    padding:16px 0;
    display:flex;
    justify-content:center;
    gap:40px;
}
.navbar a {
    color:white;
    text-decoration:none;
    font-weight:bold;
    padding:10px 18px;
}
.navbar a:hover {
    background:#005fcc;
    border-radius:6px;
}
.card {
    background:white;
    width:95%;
    max-width:540px;
    margin:30px auto;
    padding:30px;
    border-radius:16px;
    box-shadow:0 2px 8px rgba(0,0,0,0.2);
}
h2 {
    margin-bottom: 10px;
    font-size: 30px;
    position: relative;
    padding-bottom: 10px;
    color: #0078ff;
    border-bottom: 2px solid #0078ff;
}
p { font-size:22px; margin:14px 0; }
.value { font-weight:bold; color:#0078ff; }

/* Protection card colors */
.protection-active {
    border-left: 10px solid #00a000;
    background: #e8ffe8;
}
.protection-inactive {
    border-left: 10px solid #d40000;
    background: #ffe8e8;
}

/* Signal dots */
.dot {
    display:inline-block;
    width:14px;
    height:14px;
    border-radius:50%;
    margin-left:4px;
    background:#cccccc; /* empty */
}
.dot-green { background:#00aa00; }
.dot-yellow { background:#e6c300; }
.dot-red { background:#cc0000; }

/* Loading animation bar */
.loading-bar {
    width: 100%;
    height: 6px;
    background: #ddd;
    border-radius: 3px;
    overflow: hidden;
    margin-top: 10px;
    display: none;
}
.loading-bar .bar {
    width: 30%;
    height: 100%;
    background: #0078ff;
    animation: loadingMove 1.2s linear infinite;
}
@keyframes loadingMove {
    0%   { transform: translateX(-100%); }
    100% { transform: translateX(300%); }
}
</style>

<div class="navbar">
  <a href="/dashboard">Status</a>
  <a href="/logs">Logs</a>
  <a href="/dnsstats">DNS Stats</a>
  <a href="/settings">Settings</a>
</div>
)rawliteral";

// Dashboard page
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(

<!-- PROTECTION STATUS CARD -->
<div id="protection_card" class="card protection-inactive">
    <h2 id="protection_title" style="color:#d40000;">Protection</h2>
    <p id="protection_status" class="value">INACTIVE</p>
</div>

<!-- ETHERNET STATUS CARD -->
<div id="eth_card" class="card" style="display:none;">
<h2>Ethernet</h2>
<p>IP Address: <span id="eth_ip" class="value">--</span></p>
<p>Link: <span id="eth_link" class="value">--</span></p>
<p>Speed: <span id="eth_speed" class="value">--</span> Mbps</p>
</div>

<!-- WIFI STATUS CARD -->
<div id="wifi_card" class="card">
<h2>Wi-Fi</h2>
<p>SSID: <span id="wifi_ssid" class="value">--</span></p>
<p>Signal: <span id="wifi_quality" class="value">--</span></p>
<div id="wifi_dots"></div>
<p>RSSI: <span id="wifi_rssi" class="value">--</span> dBm</p>
</div>

<!-- SYSTEM STATUS CARD -->
<div class="card">
<h2>Status</h2>
<p>Uptime: <span id="uptime" class="value">--</span></p>
<p>Filter Loaded: <span id="filter" class="value">--</span></p>
<p>Free Heap: <span id="heap" class="value">--</span></p>
<p>Min Free Heap: <span id="minheap" class="value">--</span></p>
</div>

<script>
function makeDots(level, colorClass) {
    let html = "";
    for (let i = 0; i < 6; i++) {
        if (i < level) html += "<span class='dot " + colorClass + "'></span>";
        else html += "<span class='dot'></span>";
    }
    return html;
}

function updateDashboard() {
    fetch('/dashboard_data')
        .then(r => r.json())
        .then(data => {

            // ============================
            // PROTECTION CARD UPDATE
            // ============================
            if (data.protection_active) {
                document.getElementById('protection_card').className = "card protection-active";
                document.getElementById('protection_title').style.color = "#00a000";
                document.getElementById('protection_status').textContent = "ACTIVE";
            } else {
                document.getElementById('protection_card').className = "card protection-inactive";
                document.getElementById('protection_title').style.color = "#d40000";
                document.getElementById('protection_status').textContent = "INACTIVE";
            }

            // Ethernet or Wi-Fi switching
            if (data.eth_connected) {
                document.getElementById('eth_card').style.display = "block";
                document.getElementById('wifi_card').style.display = "none";

                document.getElementById('eth_ip').textContent = data.eth_ip;
                document.getElementById('eth_link').textContent = "Up";
                document.getElementById('eth_speed').textContent = data.eth_speed;
            } else {
                document.getElementById('eth_card').style.display = "none";
                document.getElementById('wifi_card').style.display = "block";

                document.getElementById('wifi_ssid').textContent = data.wifi_ssid || "--";
                document.getElementById('wifi_rssi').textContent = data.wifi_rssi;

                let rssi = data.wifi_rssi;
                let quality = "";
                let dots = "";

                if (rssi > -55) {
                    quality = "Excellent";
                    dots = makeDots(6, "dot-green");
                } else if (rssi > -67) {
                    quality = "Good";
                    dots = makeDots(5, "dot-green");
                } else if (rssi > -75) {
                    quality = "Fair";
                    dots = makeDots(3, "dot-yellow");
                } else {
                    quality = "Weak";
                    dots = makeDots(2, "dot-red");
                }

                document.getElementById('wifi_quality').textContent = quality;
                document.getElementById('wifi_dots').innerHTML = dots;
            }

            // System
            document.getElementById('uptime').textContent = data.uptime;
            document.getElementById('filter').textContent = data.filter_loaded ? "YES" : "NO";
            document.getElementById('heap').textContent = data.heap + " bytes";
            document.getElementById('minheap').textContent = data.minheap + " bytes";
        });
}

setInterval(updateDashboard, 1000);
updateDashboard();
</script>
)rawliteral";

// Logs Page 
const char LOGS_HTML[] PROGMEM = R"rawliteral(
<div class="card">
<h2>Logs</h2>

<pre id="logbox" style="
    background:#111;
    color:#0f0;
    padding:15px;
    height:400px;
    overflow:auto;
    text-align:left;
    font-size:14px;
    border-radius:8px;
    white-space:pre-wrap;
"></pre>

<button onclick="refreshLogs()" style="
    margin-top:15px;
    padding:12px 20px;
    border:none;
    border-radius:8px;
    background:#0078ff;
    color:white;
    cursor:pointer;
">Refresh Logs</button>

<script>
function refreshLogs() {
    fetch('/logs_data')
        .then(r => r.text())
        .then(t => {
            let box = document.getElementById('logbox');
            box.textContent = t;
            box.scrollTop = box.scrollHeight;
        });
}
refreshLogs();
</script>
</div>
)rawliteral";

// DNS stats page
const char DNSSTATS_HTML[] PROGMEM = R"rawliteral(
<div class="card">
<h2>DNS Statistics</h2>

<p>Total Queries: <span id="dns_total" class="value">--</span></p>
<p>Blocked Queries: <span id="dns_blocked" class="value">--</span></p>
<p>Blocked Percentage: <span id="dns_percent" class="value">--</span>%</p>

<p>Last Blocked Domain:</p>
<p><span id="dns_last" class="value" style="font-size:80%;">--</span></p>

<hr style="margin:25px 0;">

<h3>Query Activity</h3>
<canvas id="dns_spark" width="500" height="80" 
        style="width:100%;max-width:500px;height:80px;"></canvas>

</div>

<script>
let sparkData = [];
const sparkMaxPoints = 60; // 60 seconds of data

function drawSparkline() {
    const canvas = document.getElementById('dns_spark');
    const ctx = canvas.getContext('2d');

    ctx.clearRect(0, 0, canvas.width, canvas.height);

    if (sparkData.length < 2) return;

    const maxVal = Math.max(...sparkData, 1);
    const stepX = canvas.width / (sparkData.length - 1);

    ctx.beginPath();
    ctx.strokeStyle = "#0078ff";
    ctx.lineWidth = 2;

    for (let i = 0; i < sparkData.length; i++) {
        const x = i * stepX;
        const y = canvas.height - (sparkData[i] / maxVal) * canvas.height;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
    }

    ctx.stroke();
}

function updateDnsStats() {
    fetch('/dnsstats_data')
        .then(r => r.json())
        .then(data => {
            document.getElementById('dns_total').textContent = data.total;
            document.getElementById('dns_blocked').textContent = data.blocked;
            document.getElementById('dns_percent').textContent = data.percent.toFixed(2);
            document.getElementById('dns_last').textContent = data.last_blocked || "--";

            // Sparkline update
            sparkData.push(data.total);
            if (sparkData.length > sparkMaxPoints) sparkData.shift();
            drawSparkline();
        });
}

setInterval(updateDnsStats, 1000);
updateDnsStats();
</script>
)rawliteral";

// Settings page
const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<div class="card">
<h2>Settings</h2>

<h3>Blocklist URL</h3>
<p><b>Current URL:</b></p>
<div id="current_url" style="word-break:break-word;max-width:100%;overflow-wrap:anywhere;font-size:90%;color:#333;background:#eee;padding:8px;border-radius:6px;margin-bottom:20px;">--</div>

<form action="/save_blocklist_url" method="POST">
    <input type="text" name="url" placeholder="https://example.com/blocklist.txt"
           style="width:100%;padding:12px;font-size:18px;border-radius:8px;border:1px solid #ccc;">
    <button type="submit" style="margin-top:15px;padding:12px 20px;border:none;border-radius:8px;background:#0078ff;color:white;cursor:pointer;">
        Save Blocklist URL
    </button>
</form>

<hr style="margin:25px 0;">

<h3>Blocklist Status</h3>
<p><b>Filter Loaded:</b> <span id="filter_loaded">--</span></p>
<p><b>Last Updated:</b> <span id="last_update">Never</span></p>

<div id="blocklist_notice" style="margin-top:15px;font-size:18px;color:#d40000;"></div>

<button onclick="downloadBlocklist()" 
        style="margin-top:15px;padding:12px 20px;border:none;border-radius:8px;background:#0078ff;color:white;cursor:pointer;">
    Download Blocklist Now
</button>

<div id="loadingBar" class="loading-bar">
    <div class="bar"></div>
</div>

<p id="progress" style="margin-top:15px;font-size:20px;color:#0078ff;display:none;">
    Downloading blocklist...
</p>

<hr style="margin:25px 0;">

<h3>Test Domain</h3>
<input id="test_domain" type="text" placeholder="doubleclick.net"
       style="width:100%;padding:12px;font-size:18px;border-radius:8px;border:1px solid #ccc;">
<button onclick="testDomain()" 
        style="margin-top:15px;padding:12px 20px;border:none;border-radius:8px;background:#0078ff;color:white;cursor:pointer;">
    Test Domain
</button>

<p id="test_result" style="margin-top:15px;font-size:22px;font-weight:bold;"></p>

<hr style="margin:25px 0;">

<h3>Reset Device</h3>
<p>This will reboot & reset Wi-Fi credentials.</p>
<button onclick="resetDevice()" 
        style="margin-top:15px;padding:12px 20px;border:none;border-radius:8px;background:#d40000;color:white;cursor:pointer;">
    Reset Wi-Fi & Restart
</button>

</div>

<script>
function refreshStatus() {
    fetch('/blocklist_status')
        .then(r => r.json())
        .then(data => {
            document.getElementById('filter_loaded').textContent = data.filter_loaded ? "YES" : "NO";
            document.getElementById('last_update').textContent = data.last_update || "Never";
            document.getElementById('current_url').textContent = data.url || "Not set";

            if (data.downloading) {
                document.getElementById('progress').style.display = "block";
                document.getElementById('loadingBar').style.display = "block";
            } else {
                document.getElementById('progress').style.display = "none";
                document.getElementById('loadingBar').style.display = "none";
            }
        });
}

function downloadBlocklist() {
    document.getElementById('blocklist_notice').textContent = "";
    document.getElementById('progress').style.display = "block";
    document.getElementById('loadingBar').style.display = "block";

    fetch('/download_blocklist')
        .then(r => r.text())
        .then(msg => {
            document.getElementById('progress').style.display = "none";
            document.getElementById('loadingBar').style.display = "none";

            if (msg && msg !== "OK") {
                document.getElementById('blocklist_notice').textContent = "Notice: " + msg;
            }

            refreshStatus();
        });
}

function testDomain() {
    let d = document.getElementById('test_domain').value;
    fetch('/test_domain?d=' + encodeURIComponent(d))
        .then(r => r.text())
        .then(t => {
            document.getElementById('test_result').textContent = t;
            document.getElementById('test_result').style.color = 
                (t === "BLOCKED") ? "#d40000" : "#007800";
        });
}

function resetDevice() {
    if (!confirm("Reset Wi-Fi credentials and restart?")) return;

    fetch('/reset_device')
        .then(r => r.text())
        .then(() => {
            alert("Device restarting...");
        });
}

setInterval(refreshStatus, 5000);
refreshStatus();
</script>
)rawliteral";
