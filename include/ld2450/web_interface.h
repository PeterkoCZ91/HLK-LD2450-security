const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <title>LD2450 Security</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    :root { --bg: #0a0a0a; --card: #161616; --text: #e0e0e0; --accent: #03dac6; --warn: #cf6679; --sec: #333; }
    body.light-mode { --bg: #f0f2f5; --card: #fff; --text: #333; --accent: #018786; --warn: #b00020; --sec: #ddd; }
    * { box-sizing: border-box; }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); margin: 0; padding: 8px; padding-bottom: 20px; transition: background 0.3s, color 0.3s; }
    h2 { color: var(--accent); margin: 4px 0; font-size: 1.3rem; display: flex; align-items: center; justify-content: center; gap: 8px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 8px; max-width: 700px; margin: 0 auto; }
    .card { background: var(--card); padding: 10px; border-radius: 10px; box-shadow: 0 3px 8px rgba(0,0,0,0.5); transition: background 0.3s; }
    .cc { max-width: 550px; margin: 8px auto; }
    .stat-row { display: flex; justify-content: space-between; margin-bottom: 5px; font-size: 0.85rem; border-bottom: 1px solid #222; padding-bottom: 3px; }
    .stat-val { font-weight: bold; color: #fff; }
    .gauge { text-align: center; margin-bottom: 5px; }
    .big-val { font-size: 2rem; font-weight: bold; line-height: 1; }
    .unit { font-size: 0.8rem; color: #888; }
    .icon { width: 16px; height: 16px; display: inline-block; vertical-align: middle; border-radius: 50%; }
    .icon.ok { background: #00ff00; box-shadow: 0 0 5px #00ff00; }
    .icon.warn { background: orange; }
    .icon.err { background: #ff0000; }
    input[type=range] { width: 100%; accent-color: var(--accent); }
    input[type=text], input[type=password], input[type=number], select { background: #222; border: 1px solid #444; color: white; padding: 8px; border-radius: 4px; width: 100%; margin-top: 2px; }
    body.light-mode input[type=text], body.light-mode input[type=password], body.light-mode input[type=number], body.light-mode select { background: #f5f5f5; border-color: #ccc; color: #333; }
    .row-input { display: flex; justify-content: space-between; align-items: center; margin-bottom: 5px; gap: 10px; }
    button { width: 100%; padding: 8px; border: none; border-radius: 6px; background: #3700b3; color: white; cursor: pointer; margin-top: 4px; }
    button:hover { opacity: 0.9; }
    button.sec { background: var(--sec); }
    button.warn { background: var(--warn); color: black; font-weight: bold; }
    .tabs { display: flex; gap: 4px; margin-bottom: 10px; flex-wrap: wrap; }
    .tab { flex: 1; min-width: 65px; padding: 8px; background: #222; text-align: center; cursor: pointer; border-radius: 6px; font-size: 0.8rem; }
    .tab.active { background: var(--accent); color: black; font-weight: bold; }
    body.light-mode .tab { background: #e0e0e0; }
    body.light-mode .tab.active { background: var(--accent); color: white; }
    .hidden { display: none; }
    .section-title { color: #888; font-size: 0.8rem; margin: 10px 0 4px 0; text-transform: uppercase; border-bottom: 1px solid #333; }
    #canvasWrapper { position: relative; width: 100%; max-width: 400px; margin: 0 auto; }
    canvas { background: #000; border-radius: 8px; border: 2px solid #222; width: 100%; height: auto; }
    .legend { display: flex; justify-content: center; gap: 12px; margin-top: 8px; font-size: 0.7rem; flex-wrap: wrap; }
    .legend-item { display: flex; align-items: center; gap: 4px; }
    .legend-dot { width: 10px; height: 10px; border-radius: 50%; }
    .slider-container { text-align: left; margin: 5px 0; }
    .slider-container label { font-size: 0.85rem; color: #aaa; }
    #toast { position: fixed; bottom: 20px; left: 50%; transform: translateX(-50%); background: #333; padding: 10px 20px; border-radius: 20px; opacity: 0; transition: opacity 0.3s; pointer-events: none; z-index: 10; }
    @media (max-width: 480px) {
        .grid { grid-template-columns: 1fr; gap: 8px; }
        body { padding: 5px; padding-bottom: 60px; }
        .big-val { font-size: 2rem; }
        .tabs { gap: 3px; }
        .tab { min-width: 50px; font-size: 0.7rem; padding: 8px 4px; }
        button { padding: 12px; min-height: 44px; font-size: 1rem; }
        .row-input { flex-direction: column; align-items: stretch; gap: 2px; }
        input[type=text], input[type=number], select { padding: 10px; font-size: 1rem; }
    }
  </style>
</head>
<body>

  <h2>
    LD2450 <span style="font-size:0.7em; color:var(--accent)" id="fw_ver"></span>
    <span id="sse_icon" class="icon" title="SSE"></span>
    <span id="wifi_icon" class="icon" title="WiFi"></span>
    <span id="mqtt_icon" class="icon" title="MQTT"></span>
    <button onclick="toggleTheme()" style="background:transparent; border:none; font-size:1.2rem; cursor:pointer; padding:0; width:auto; margin:0" id="themeBtn">&#9728;&#65039;</button>
  </h2>

  <div id="security_warning" style="background:#cf6679; color:black; padding:10px; border-radius:8px; margin-bottom:10px; display:none; text-align:center; font-weight:bold; max-width:800px; margin-left:auto; margin-right:auto">
    Default password admin/admin - change in Network section
  </div>

  <div class="grid">
    <!-- STATUS -->
    <div class="card">
        <div class="gauge">
            <div id="alarm_badge" style="margin-bottom:8px; font-size:0.9rem; font-weight:bold; color:#888">---</div>
            <button id="btn_arm" onclick="toggleArm()" style="width:auto; padding:8px 20px; margin-bottom:10px; background:#3700b3">ARM</button>
            <div class="big-val" id="trk_count" style="color:var(--accent)">0</div>
            <div class="unit">TARGETS</div>
            <div id="trk_state" style="color:#888; font-weight:bold; letter-spacing:2px; margin-top:8px">IDLE</div>
            <div id="tamper_status" style="font-size:0.7rem; color:#ff4444; margin-top:4px; display:none"></div>
        </div>
    </div>

    <!-- HEALTH -->
    <div class="card">
        <div class="stat-row"><span>Sensor Health</span><span id="h_score" class="stat-val">---</span></div>
        <div class="stat-row"><span>FPS</span><span id="h_fps">---</span></div>
        <div class="stat-row"><span>UART Errors</span><span id="h_err" style="color:var(--warn)">---</span></div>
        <div class="stat-row"><span>RAM (Free)</span><span id="h_heap">---</span></div>
        <div class="stat-row"><span>Uptime</span><span id="h_uptime">---</span></div>
        <div class="stat-row"><span>WiFi RSSI</span><span id="h_rssi">--- dBm</span></div>
        <div id="analytics_row" style="display:none">
            <div class="stat-row"><span>Entry / Exit</span><span><span id="h_entry">0</span> / <span id="h_exit">0</span></span></div>
            <div class="stat-row"><span>Movement</span><span id="h_move">---</span></div>
        </div>
        <div style="display:flex; gap:5px; margin-top:8px">
            <button class="sec" style="flex:1" onclick="sendCmd('/api/restart')">Restart ESP</button>
        </div>
    </div>
  </div>

  <!-- RADAR MAP -->
  <div class="card cc">
      <div class="unit" style="margin-bottom:8px">RADAR MAP</div>
      <div id="canvasWrapper">
          <canvas id="radarMap" width="500" height="500"></canvas>
      </div>
      <div class="legend">
          <div class="legend-item"><div class="legend-dot" style="background:#ff0000"></div>Target</div>
          <div class="legend-item"><div class="legend-dot" style="background:rgba(0,100,255,0.7)"></div>Ghost</div>
          <div class="legend-item"><div class="legend-dot" style="background:#555"></div>Noise</div>
          <div class="legend-item"><div class="legend-dot" style="background:#00ff66; border-radius:0; width:14px; height:3px"></div>Polygon</div>
          <div class="legend-item"><div class="legend-dot" style="background:#ff0000; border-radius:0; width:14px; height:3px"></div>Blackout</div>
      </div>
      <div style="text-align:center; margin-top:8px">
          <label style="font-size:0.8rem; cursor:pointer">
              <input type="checkbox" id="cb_debug_view" onchange="toggleDebugView()" style="width:auto">
              Debug (Ghosts &amp; Noise)
          </label>
      </div>
  </div>

  <!-- TABBED CONTROLS -->
  <div class="card cc">
      <div class="tabs">
          <div class="tab active" onclick="tab(0)">Radar</div>
          <div class="tab" onclick="tab(1)">Alarm</div>
          <div class="tab" onclick="tab(2)">Zones</div>
          <div class="tab" onclick="tab(3)">Network</div>
          <div class="tab" onclick="tab(4)">Log</div>
          <div class="tab" onclick="tab(5)">System</div>
      </div>

      <!-- TAB 0: RADAR CONFIG -->
      <div id="tab0">
          <div class="stat-row"><span>Hostname (mDNS)</span></div>
          <div style="display:flex; gap:5px; margin-bottom:10px">
              <input type="text" id="inp_hostname" placeholder="esp32-ld2450-XXXX" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Rotation: <strong id="val_rotation">0</strong> deg</label>
              <input type="range" id="sl_rotation" min="0" max="270" step="90" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Width (+/- mm): <strong id="val_width">1000</strong></label>
              <input type="range" id="sl_width" min="500" max="4000" step="100" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Range (mm): <strong id="val_length">6000</strong></label>
              <input type="range" id="sl_length" min="1000" max="8000" step="500" onchange="updateConfig()">
          </div>
          <div class="section-title">FILTERING</div>
          <div class="slider-container">
              <label>Min target size: <strong id="val_minres">200</strong></label>
              <input type="range" id="sl_minres" min="50" max="400" step="25" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Static energy: <strong id="val_static_res">300</strong></label>
              <input type="range" id="sl_static_res" min="50" max="1000" step="50" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Ghost timeout: <strong id="val_ghost">60</strong>s</label>
              <input type="range" id="sl_ghost" min="5" max="120" step="5" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Motion threshold: <strong id="val_move">5</strong> mm/s</label>
              <input type="range" id="sl_move" min="3" max="50" step="1" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label>Position threshold: <strong id="val_pos">50</strong> mm</label>
              <input type="range" id="sl_pos" min="20" max="200" step="10" onchange="updateConfig()">
          </div>
      </div>

      <!-- TAB 1: ALARM / SECURITY -->
      <div id="tab1" class="hidden">
          <div class="section-title">ALARM DELAYS</div>
          <div class="row-input">
              <span>Entry delay (sec)</span>
              <input type="number" id="i_entry_dl" placeholder="30" min="0" max="300" style="width:80px" onchange="saveAlarmConfig()">
          </div>
          <div class="row-input">
              <span>Exit delay (sec)</span>
              <input type="number" id="i_exit_dl" placeholder="30" min="0" max="300" style="width:80px" onchange="saveAlarmConfig()">
          </div>
          <div style="display:flex; align-items:center; gap:8px; margin:5px 0">
              <input type="checkbox" id="chk_dis_rem" style="width:auto" onchange="saveAlarmConfig()">
              <label for="chk_dis_rem">Reminder "Still DISARMED"</label>
          </div>

          <div class="section-title">ANTI-MASKING (Cover tamper)</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_am_en" style="width:auto" onchange="saveSec()">
              <label for="chk_am_en">Alert on silence (covering)</label>
          </div>
          <div class="row-input">
              <span>Timeout (sec)</span>
              <input type="number" id="i_am" placeholder="300" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title">LOITERING</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_loit_en" style="width:auto" onchange="saveSec()">
              <label for="chk_loit_en">Alert on loitering</label>
          </div>
          <div class="row-input">
              <span>Timeout (sec)</span>
              <input type="number" id="i_loit" placeholder="15" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title">HEARTBEAT</div>
          <div class="row-input">
              <span>Interval (hours)</span>
              <input type="number" id="i_hb" placeholder="4" min="0" max="24" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title">WIFI SECURITY</div>
          <div class="row-input">
              <span>RSSI threshold (dBm)</span>
              <input type="number" id="i_rssi_thresh" placeholder="-80" min="-95" max="-40" style="width:80px" onchange="saveSec()">
          </div>
          <div class="row-input">
              <span>Max RSSI drop (dB)</span>
              <input type="number" id="i_rssi_drop" placeholder="20" min="5" max="50" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title">SCHEDULE</div>
          <div class="row-input">
              <span>Auto arm at</span>
              <input type="time" id="i_sched_arm" style="width:100px">
          </div>
          <div class="row-input">
              <span>Auto disarm at</span>
              <input type="time" id="i_sched_disarm" style="width:100px">
          </div>
          <div class="row-input">
              <span>Auto-arm after inactivity (min)</span>
              <input type="number" id="i_auto_arm" placeholder="0" min="0" max="1440" style="width:80px">
          </div>
          <button onclick="saveSchedule()" class="sec">Save Schedule</button>
      </div>

      <!-- TAB 2: ZONES -->
      <div id="tab2" class="hidden">
          <div class="section-title">POLYGON ZONES</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:8px">Stand in a corner and click to add a point.</div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:8px">
              <button onclick="addCurrentToPoly(0)" class="sec" style="width:auto; padding:8px 12px; font-size:0.8rem">Zone 1 Point</button>
              <button onclick="addCurrentToPoly(1)" class="sec" style="width:auto; padding:8px 12px; font-size:0.8rem">Zone 2 Point</button>
          </div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:15px">
              <button onclick="clearPoly(0)" class="sec" style="width:auto; padding:5px 10px; font-size:0.7rem">Clear Zone 1</button>
              <button onclick="clearPoly(1)" class="sec" style="width:auto; padding:5px 10px; font-size:0.7rem">Clear Zone 2</button>
          </div>

          <div class="section-title">BLACKOUT ZONES</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:8px">Excluded areas (HVAC, antennas).</div>
          <button id="btnDrawMode" onclick="toggleDrawMode()" style="background:#ff6600; margin-bottom:8px">
              Draw mode: OFF
          </button>
          <div id="drawModeHelp" style="display:none;font-size:0.75rem;color:#ff6600;margin-bottom:8px">
              Click and drag on the map to create a blackout zone.
          </div>
          <div id="blackoutList" style="text-align:left"></div>
          <button onclick="showAddZoneDialog()" class="sec" style="margin-top:5px">+ Add manually</button>

          <div class="section-title">TRIPWIRE (Entry/Exit)</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_tripwire" style="width:auto" onchange="saveTripwire()">
              <label for="chk_tripwire">Enable tripwire</label>
          </div>
          <div class="row-input">
              <span>Line Y position (mm)</span>
              <input type="number" id="i_tripwire_y" value="3000" min="500" max="8000" style="width:100px" onchange="saveTripwire()">
          </div>
          <button onclick="api('tripwire',{method:'POST',body:new URLSearchParams({reset:1})}).then(()=>showToast('Counter reset'))" class="sec">Reset Counter</button>
      </div>

      <!-- TAB 3: NETWORK -->
      <div id="tab3" class="hidden">
          <div class="section-title">MQTT BROKER</div>
          <div id="mqtt_status_row" class="stat-row" style="margin-bottom:10px">
              <span>Status</span>
              <span id="mqtt_status_text" style="color:var(--warn)">---</span>
          </div>
          <div style="display:flex; align-items:center; gap:8px">
              <input type="checkbox" id="chk_mqtt_en" style="width:auto">
              <label for="chk_mqtt_en">Enable MQTT</label>
          </div>
          <input type="text" id="txt_mqtt_server" placeholder="Server IP">
          <div style="display:flex; gap:5px">
              <input type="text" id="txt_mqtt_port" placeholder="Port (1883)">
              <input type="text" id="txt_mqtt_user" placeholder="Username">
          </div>
          <input type="password" id="txt_mqtt_pass" placeholder="Password">
          <button onclick="saveMQTTConfig()" class="sec">Save MQTT</button>

          <div class="section-title">BACKUP WIFI</div>
          <input type="text" id="txt_bk_ssid" placeholder="SSID">
          <input type="password" id="txt_bk_pass" placeholder="Password">
          <button onclick="saveBackupWiFi()" class="sec">Save WiFi</button>

          <div class="section-title">TELEGRAM</div>
          <div style="display:flex; align-items:center; gap:8px">
              <input type="checkbox" id="chk_tg_en" style="width:auto">
              <label for="chk_tg_en">Enable Bot</label>
          </div>
          <input type="text" id="txt_tg_token" placeholder="Bot token">
          <input type="text" id="txt_tg_chat" placeholder="Chat ID">
          <div style="display:flex; gap:5px">
              <button onclick="saveTelegram()" class="sec">Save</button>
              <button onclick="testTelegram()" class="sec">Test</button>
          </div>

          <div class="section-title">CREDENTIALS</div>
          <input type="text" id="txt_auth_user" placeholder="Username">
          <input type="password" id="txt_auth_pass" placeholder="New password">
          <input type="password" id="txt_auth_pass2" placeholder="Confirm password">
          <button onclick="saveAuth()" class="warn">Change password</button>
      </div>

      <!-- TAB 4: EVENTS -->
      <div id="tab4" class="hidden">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px">
              <div class="section-title" style="margin:0; border:none">RECENT EVENTS</div>
              <button onclick="clearEvents()" class="warn" style="width:auto; padding:5px 10px; margin:0">Clear</button>
          </div>
          <div style="overflow-x:auto">
              <table style="width:100%; border-collapse:collapse; font-size:0.8rem; text-align:left">
                  <thead>
                      <tr style="border-bottom:1px solid #444; color:#888">
                          <th style="padding:5px">Time</th>
                          <th style="padding:5px">Type</th>
                          <th style="padding:5px">Message</th>
                      </tr>
                  </thead>
                  <tbody id="event_list"></tbody>
              </table>
          </div>
      </div>

      <!-- TAB 5: SYSTEM -->
      <div id="tab5" class="hidden">
          <div class="section-title">DEVICE INFO</div>
          <div class="stat-row"><span>Device ID</span><span id="sys_devid">---</span></div>
          <div class="stat-row"><span>IP Address</span><span id="sys_ip">---</span></div>
          <div class="stat-row"><span>MAC Address</span><span id="sys_mac">---</span></div>
          <div class="stat-row"><span>Firmware</span><span id="sys_fw">---</span></div>

          <div class="section-title">AI NOISE LEARNING</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:10px">
              Leave the sensor 1h in an empty room. It will learn to ignore ghosts.
          </div>
          <div id="noise_status" style="margin-bottom:10px; padding:8px; border-radius:8px; background:rgba(255,255,255,0.05)">
              <div style="font-size:0.7rem; color:#888">LEARNING STATUS</div>
              <div id="noise_state_val" style="font-weight:bold; color:#aaa">Inactive</div>
              <div id="noise_stats" style="font-size:0.75rem; color:#666; margin-top:3px; display:none">
                  Samples: <span id="noise_samples">0</span> | Time: <span id="noise_time">0</span>s
              </div>
          </div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:8px">
              <button id="btn_noise_start" onclick="noiseCmd('start')" style="background:#6200ee; width:auto; padding:8px 15px">Start</button>
              <button id="btn_noise_stop" onclick="noiseCmd('stop')" style="background:#333; display:none; width:auto; padding:8px 15px">Save</button>
          </div>
          <div style="display:flex; align-items:center; justify-content:center; gap:8px; margin-bottom:15px">
              <input type="checkbox" id="cb_noise_filter" onchange="noiseCmd('toggle')" style="width:auto">
              <label for="cb_noise_filter">Enable noise filter</label>
          </div>

          <div class="section-title">FIRMWARE UPDATE (OTA)</div>
          <input type="file" id="fw_file" accept=".bin">
          <div id="ota_bar" style="height:5px; background:#333; margin-top:5px; width:0%; transition:width 0.2s; border-radius:3px"></div>
          <div id="ota_status" style="font-size:0.75rem; color:#888; margin-top:3px"></div>
          <button onclick="uploadFW()">Upload Firmware</button>

          <div class="section-title">BACKUP &amp; RESTORE</div>
          <div style="display:flex; gap:5px">
              <button onclick="exportConfig()" class="sec" style="flex:1">Export</button>
              <button onclick="document.getElementById('importFile').click()" class="sec" style="flex:1">Import</button>
              <input type="file" id="importFile" accept=".json" style="display:none" onchange="importConfig()">
          </div>
      </div>
  </div>

  <div id="toast">OK</div>

<script>
const $ = id => document.getElementById(id);
const api = (ep, opts) => fetch('/api/'+ep, opts).then(r => {
    if(r.ok) showToast("OK"); else showToast("Error");
    return r;
});
function showToast(msg) { $('toast').innerText=msg; $('toast').style.opacity=1; setTimeout(()=>$('toast').style.opacity=0, 2000); }

// --- STATE ---
let zMinX=-1000, zMaxX=1000, zMaxY=6000;
let minRes=200, staticResThreshold=300, ghostTimeout=60000;
let moveThreshold=5, posThreshold=50, mapRotation=0;
let blackoutZones=[], polygons=[];
let drawMode=false, drawStart=null;
let trails=[[],[],[]];
const TRAIL_LENGTH=15;
const TRAIL_COLORS=["rgba(255,68,68,","rgba(255,136,0,","rgba(255,204,0,"];
const TARGET_COLORS=["#ff4444","#ff8800","#ffcc00"];
let debugView=false, noiseMapData=null;
let alarmArmed=false, configLoaded=false;

// --- THEME ---
function toggleTheme() {
    document.body.classList.toggle('light-mode');
    const isLight = document.body.classList.contains('light-mode');
    localStorage.setItem('theme', isLight ? 'light' : 'dark');
    $('themeBtn').innerText = isLight ? '\u{1F311}' : '\u{2600}\u{FE0F}';
}

// --- SSE (alarm events) ---
let evtSource=null, reconnectTimeout=null;
function connectSSE() {
    if(evtSource) evtSource.close();
    evtSource = new EventSource('/events');
    evtSource.addEventListener('alarm', e => {
        const d = JSON.parse(e.data);
        if(d.state) updateAlarmUI(d.state);
        if(d.armed !== undefined) alarmArmed = d.armed;
    });
    evtSource.onerror = () => {
        $('sse_icon').className='icon err';
        evtSource.close();
        if(reconnectTimeout) clearTimeout(reconnectTimeout);
        reconnectTimeout = setTimeout(connectSSE, 3000);
    };
    evtSource.onopen = () => { $('sse_icon').className='icon ok'; };
}

// --- INIT ---
function init() {
    if(localStorage.getItem('theme')==='light') {
        document.body.classList.add('light-mode');
        $('themeBtn').innerText='\u{1F311}';
    }
    connectSSE();
    fetch('/api/version').then(r=>r.text()).then(v => $('fw_ver').innerText=v);
    fetchDiagnostics();
    setInterval(fetchDiagnostics, 5000);
    setInterval(fetchTelemetry, 200);
    loadAlarmStatus();
    loadTelegramConfig();
    loadBackupWiFi();
}

// --- TELEMETRY (polling 5 FPS for canvas) ---
function fetchTelemetry() {
    fetch('/api/telemetry').then(r=>r.json()).then(data => {
        if(!configLoaded && data.zone) {
            zMinX=data.zone.xmin; zMaxX=data.zone.xmax; zMaxY=data.zone.ymax;
            minRes=data.zone.min_res||200;
            staticResThreshold=data.zone.static_res_threshold||300;
            ghostTimeout=data.zone.ghost_timeout||60000;
            moveThreshold=data.zone.move_threshold||5;
            posThreshold=data.zone.pos_threshold||50;
            mapRotation=data.zone.map_rotation||0;
            $('sl_width').value=zMaxX; $('sl_length').value=zMaxY;
            $('sl_minres').value=minRes; $('sl_static_res').value=staticResThreshold;
            $('sl_ghost').value=ghostTimeout/1000; $('sl_move').value=moveThreshold;
            $('sl_pos').value=posThreshold; $('sl_rotation').value=mapRotation;
            if(data.hostname && !$('inp_hostname').value) $('inp_hostname').value=data.hostname;
            if(data.tripwire_en!==undefined) { $('chk_tripwire').checked=data.tripwire_en; $('i_tripwire_y').value=data.tripwire_y||3000; }
            configLoaded=true;
        }
        if(data.blackout_zones) blackoutZones=data.blackout_zones;
        if(data.polygons) polygons=data.polygons;

        $('trk_count').innerText=data.count||0;
        let anyInZone=false;
        if(data.targets) {
            data.targets.forEach(t => {
                if(t.x>=zMinX && t.x<=zMaxX && t.y<=zMaxY && t.y>0) anyInZone=true;
            });
        }
        $('trk_state').innerText=anyInZone?"DETECTED":"IDLE";
        $('trk_state').style.color=anyInZone?"var(--accent)":"#888";

        if(data.tamper && data.tamper!=="OK") {
            $('tamper_status').style.display='block';
            $('tamper_status').innerText=data.tamper;
        } else { $('tamper_status').style.display='none'; }

        // Analytics
        if(data.entry_count!==undefined) {
            $('analytics_row').style.display='block';
            $('h_entry').innerText=data.entry_count;
            $('h_exit').innerText=data.exit_count;
        }
        if(data.targets && data.targets.length>0) {
            let moves=data.targets.filter(t=>t.move&&t.move!=='none').map(t=>t.move);
            $('h_move').innerText=moves.length?moves.join(', '):'---';
            // Show dwell for first target in zone
            let dwell=data.targets.find(t=>t.dwell>0);
            if(dwell) $('h_move').innerText+=' ('+dwell.dwell+'s in zone)';
        }

        updateNoiseUI(data);

        const now=Date.now();
        if(data.targets) {
            for(let i=0;i<3;i++) {
                if(data.targets[i]) {
                    trails[i].push({x:data.targets[i].x, y:data.targets[i].y, t:now});
                    if(trails[i].length>TRAIL_LENGTH) trails[i].shift();
                }
                trails[i]=trails[i].filter(p=>(now-p.t)<3000);
            }
            drawMap(data.targets);
        }

        $('val_width').innerText="+/- "+zMaxX;
        $('val_length').innerText=zMaxY;
        $('val_minres').innerText=minRes;
        $('val_ghost').innerText=(ghostTimeout/1000);
        $('val_move').innerText=moveThreshold;
        $('val_pos').innerText=posThreshold;
        $('val_rotation').innerText=mapRotation;
        updateBlackoutList();
        if(data.rssi) $('h_rssi').innerText=data.rssi+" dBm";
    }).catch(()=>{
        $('trk_state').innerText="OFFLINE";
        $('trk_state').style.color="#ff4444";
    });
}

// --- NOISE UI ---
function updateNoiseUI(data) {
    const nsVal=$('noise_state_val'), nsStats=$('noise_stats');
    const btnStart=$('btn_noise_start'), btnStop=$('btn_noise_stop');
    $('cb_noise_filter').checked=data.noise_filter;
    if(data.noise_learning) {
        nsVal.innerText="RUNNING..."; nsVal.style.color="#bb86fc";
        nsStats.style.display="block";
        $('noise_samples').innerText=data.noise_samples;
        $('noise_time').innerText=data.noise_elapsed;
        btnStart.style.display="none"; btnStop.style.display="inline-block";
    } else if(data.noise_pending) {
        nsVal.innerText="STARTS IN: "+data.noise_countdown+"s"; nsVal.style.color="#ff8800";
        nsStats.style.display="none"; btnStart.style.display="none"; btnStop.style.display="none";
    } else {
        nsVal.innerText=data.noise_filter?"FILTER ACTIVE":"Ready";
        nsVal.style.color=data.noise_filter?"#03dac6":"#555";
        nsStats.style.display="none"; btnStart.style.display="inline-block"; btnStop.style.display="none";
    }
}

// --- DIAGNOSTICS ---
function fetchDiagnostics() {
    fetch('/api/diagnostics').then(r=>r.json()).then(d => {
        $('mqtt_icon').className="icon "+(d.mqtt_connected?"ok":"err");
        if(d.wifi_rssi) {
            $('wifi_icon').className="icon "+(d.wifi_rssi>-75?"ok":"warn");
            $('h_rssi').innerText=d.wifi_rssi+" dBm";
        }
        if(d.uptime) {
            let u=d.uptime;
            $('h_uptime').innerText=Math.floor(u/3600)+"h "+Math.floor((u%3600)/60)+"m";
        }
        if(d.free_heap) $('h_heap').innerText=(d.free_heap/1024).toFixed(1)+" KB";
        if(d.health_score!==undefined) $('h_score').innerText=d.health_score+"%";
        if(d.frame_rate!==undefined) $('h_fps').innerText=d.frame_rate.toFixed(1);
        if(d.error_count!==undefined) $('h_err').innerText=d.error_count;
        if(d.is_default_pass) $('security_warning').style.display='block';
        else $('security_warning').style.display='none';
        if(d.auth_user) $('txt_auth_user').value=d.auth_user;
        if(d.mqtt_server) { $('txt_mqtt_server').value=d.mqtt_server; $('txt_mqtt_port').value=d.mqtt_port||''; }
        // MQTT status text
        let ms=$('mqtt_status_text');
        if(ms){
            if(d.mqtt_connected){ms.innerText="Connected to "+d.mqtt_server+":"+d.mqtt_port;ms.style.color="var(--accent)";}
            else{ms.innerText="Disconnected";ms.style.color="var(--warn)";}
        }
        // System info
        if(d.device_id) $('sys_devid').innerText=d.device_id;
        if(d.ip_address) $('sys_ip').innerText=d.ip_address;
        if(d.mac_address) $('sys_mac').innerText=d.mac_address;
        let fv=$('fw_ver'); if(fv&&fv.innerText) $('sys_fw').innerText=fv.innerText;
    }).catch(()=>{});
}

// --- CANVAS ---
function toggleDebugView() {
    debugView=$('cb_debug_view').checked;
    if(debugView && !noiseMapData) {
        fetch('/api/noisemap').then(r=>{if(r.ok) return r.arrayBuffer(); throw 0;})
        .then(buf=>{ noiseMapData=new Uint16Array(buf); }).catch(()=>{});
    }
}

function drawMap(targets) {
    const c=$('radarMap'), ctx=c.getContext('2d'), w=c.width, h=c.height;
    ctx.fillStyle="#050505"; ctx.fillRect(0,0,w,h);
    ctx.save();
    if(mapRotation) { ctx.translate(w/2,h/2); ctx.rotate(mapRotation*Math.PI/180); ctx.translate(-w/2,-h/2); }
    const scX=w/8000, scY=h/8000, cx=w/2;
    const toPxX=mm=>cx+(mm*scX), toPxY=mm=>h-(mm*scY);

    // Grid
    ctx.strokeStyle="#1a1a1a"; ctx.lineWidth=1;
    for(let i=1000;i<8000;i+=1000) {
        let y=toPxY(i);
        ctx.beginPath(); ctx.moveTo(0,y); ctx.lineTo(w,y); ctx.stroke();
        ctx.fillStyle="#333"; ctx.font="10px Arial"; ctx.fillText(i/1000+"m",5,y-3);
    }
    ctx.beginPath(); ctx.moveTo(cx,0); ctx.lineTo(cx,h); ctx.stroke();

    // Zone bounding box
    ctx.strokeStyle="rgba(3,218,198,0.2)"; ctx.lineWidth=1; ctx.setLineDash([5,5]);
    ctx.strokeRect(toPxX(zMinX),toPxY(zMaxY),toPxX(zMaxX)-toPxX(zMinX),toPxY(0)-toPxY(zMaxY));
    ctx.setLineDash([]);

    // Polygons
    polygons.forEach(poly => {
        if(poly.points && poly.points.length>2) {
            ctx.fillStyle="rgba(0,255,100,0.1)"; ctx.strokeStyle="#00ff66"; ctx.lineWidth=2;
            ctx.beginPath(); ctx.moveTo(toPxX(poly.points[0].x),toPxY(poly.points[0].y));
            for(let i=1;i<poly.points.length;i++) ctx.lineTo(toPxX(poly.points[i].x),toPxY(poly.points[i].y));
            ctx.closePath(); ctx.fill(); ctx.stroke();
            ctx.fillStyle="#00ff66"; ctx.font="12px Arial";
            ctx.fillText(poly.label||"Zone",toPxX(poly.points[0].x),toPxY(poly.points[0].y)-5);
        }
    });

    // Blackout zones
    blackoutZones.forEach(zone => {
        if(!zone.enabled) return;
        let bzx=toPxX(zone.xmin), bzy=toPxY(zone.ymax);
        let bzw=toPxX(zone.xmax)-bzx, bzh=toPxY(zone.ymin)-bzy;
        ctx.fillStyle="rgba(255,0,0,0.15)"; ctx.fillRect(bzx,bzy,bzw,bzh);
        ctx.strokeStyle="#ff0000"; ctx.lineWidth=2; ctx.strokeRect(bzx,bzy,bzw,bzh);
        ctx.strokeStyle="rgba(255,0,0,0.3)"; ctx.lineWidth=1; ctx.beginPath();
        for(let i=0;i<bzw+bzh;i+=20) { ctx.moveTo(bzx+i,bzy); ctx.lineTo(bzx+i-bzh,bzy+bzh); }
        ctx.stroke();
        ctx.fillStyle="#ff0000"; ctx.font="bold 12px Arial"; ctx.fillText(zone.label||"Blackout",bzx+5,bzy+15);
    });

    // Sensor icon
    ctx.fillStyle="#03dac6";
    ctx.beginPath(); ctx.arc(cx,h-8,6,Math.PI,0); ctx.fill();
    ctx.fillRect(cx-6,h-8,12,8);

    // Trails
    const now=Date.now();
    for(let i=0;i<3;i++) {
        trails[i].forEach(pos => {
            let alpha=Math.max(0.1,0.6-((now-pos.t)/3000)*0.5);
            ctx.fillStyle=TRAIL_COLORS[i]+alpha+")";
            ctx.beginPath(); ctx.arc(toPxX(pos.x),toPxY(pos.y),4,0,2*Math.PI); ctx.fill();
        });
    }

    // Noise map
    if(debugView && noiseMapData) {
        ctx.fillStyle="rgba(100,100,100,0.4)";
        for(let i=0;i<noiseMapData.length;i++) {
            if(noiseMapData[i]>0) {
                let gx=i%80, gy=Math.floor(i/80);
                ctx.beginPath(); ctx.arc(toPxX((gx*100)-4000+50),toPxY(gy*100+50),2,0,2*Math.PI); ctx.fill();
            }
        }
    }

    // Targets
    targets.forEach((t,i) => {
        let isGhost=(t.type==='ghost');
        if(isGhost && !debugView) return;
        let px=toPxX(t.x), py=toPxY(t.y);
        let color=isGhost?"rgba(0,100,255,0.7)":TARGET_COLORS[i];
        ctx.fillStyle=isGhost?"rgba(0,100,255,0.2)":(color+"33");
        ctx.beginPath(); ctx.arc(px,py,14,0,2*Math.PI); ctx.fill();
        ctx.fillStyle=color;
        ctx.beginPath(); ctx.arc(px,py,8,0,2*Math.PI); ctx.fill();
        ctx.fillStyle=isGhost?"#88ccff":"#fff";
        ctx.font=isGhost?"italic 10px Arial":"bold 11px Arial";
        ctx.fillText(isGhost?"GHOST":"T"+(i+1),px+12,py-5);
        ctx.font="10px Arial"; ctx.fillStyle="#aaa";
        ctx.fillText(t.x+", "+t.y,px+12,py+8);
        if(t.spd!==undefined) { ctx.fillStyle="#666"; ctx.fillText("spd:"+t.spd,px+12,py+20); }
    });
    ctx.restore();
}

// --- TABS ---
function tab(n) {
    ['tab0','tab1','tab2','tab3','tab4','tab5'].forEach((id,i) => {
        $(id).classList.toggle('hidden',i!==n);
        document.querySelectorAll('.tab')[i].classList.toggle('active',i===n);
    });
    if(n===1) { loadSecurityConfig(); loadAlarmStatus(); loadSchedule(); }
    if(n===4) loadEvents();
}

// --- ALARM ---
function loadAlarmStatus() {
    fetch('/api/alarm/state').then(r=>r.json()).then(d => {
        if(d.armed!==undefined) alarmArmed=d.armed;
        if(d.state) updateAlarmUI(d.state);
        if(d.entry_delay!==undefined) $('i_entry_dl').value=d.entry_delay;
        if(d.exit_delay!==undefined) $('i_exit_dl').value=d.exit_delay;
        if(d.disarm_reminder!==undefined) $('chk_dis_rem').checked=d.disarm_reminder;
    }).catch(()=>{});
}
function updateAlarmUI(state) {
    let b=$('alarm_badge'), btn=$('btn_arm');
    if(state==='disarmed'){b.innerText='DISARMED';b.style.color='#888';btn.innerText='ARM';btn.style.background='#b00020';}
    else if(state==='arming'){b.innerText='ARMING...';b.style.color='orange';btn.innerText='CANCEL';btn.style.background='#3700b3';}
    else if(state==='armed_away'){b.innerText='ARMED';b.style.color='#00ff00';btn.innerText='DISARM';btn.style.background='#3700b3';}
    else if(state==='pending'){b.innerText='PENDING';b.style.color='orange';btn.innerText='CANCEL';btn.style.background='#3700b3';}
    else if(state==='triggered'){b.innerText='TRIGGERED!';b.style.color='red';btn.innerText='DISARM';btn.style.background='#3700b3';}
}
function toggleArm() {
    if(alarmArmed) api('alarm/disarm',{method:'POST'}).then(()=>{alarmArmed=false;loadAlarmStatus();});
    else api('alarm/arm',{method:'POST'}).then(()=>{alarmArmed=true;loadAlarmStatus();});
}
function saveAlarmConfig() {
    api('alarm/config',{method:'POST',body:new URLSearchParams({
        'entry_delay':$('i_entry_dl').value,'exit_delay':$('i_exit_dl').value,
        'disarm_reminder':$('chk_dis_rem').checked?1:0
    })});
}

// --- SECURITY CONFIG ---
function loadSecurityConfig() {
    fetch('/api/security/config').then(r=>r.json()).then(d => {
        $('i_am').value=d.antimask_time||300;
        $('chk_am_en').checked=d.antimask_enabled||false;
        $('i_loit').value=d.loiter_time||15;
        $('chk_loit_en').checked=d.loiter_alert!==false;
        $('i_hb').value=d.heartbeat||4;
        $('i_rssi_thresh').value=d.rssi_threshold||-80;
        $('i_rssi_drop').value=d.rssi_drop||20;
    }).catch(()=>{});
}
function saveSec() {
    api('security/config',{method:'POST',body:new URLSearchParams({
        'antimask':$('i_am').value,'antimask_en':$('chk_am_en').checked?1:0,
        'loiter':$('i_loit').value,'loiter_alert':$('chk_loit_en').checked?1:0,
        'heartbeat':$('i_hb').value,
        'rssi_threshold':$('i_rssi_thresh').value,'rssi_drop':$('i_rssi_drop').value
    })});
}

// --- TRIPWIRE ---
function saveTripwire() {
    api('tripwire',{method:'POST',body:new URLSearchParams({
        enabled:$('chk_tripwire').checked?1:0, y:$('i_tripwire_y').value
    })}).then(()=>showToast("Tripwire saved")).catch(()=>showToast("Error"));
}

// --- SCHEDULE ---
function loadSchedule() {
    fetch('/api/schedule').then(r=>r.json()).then(d=>{
        if(d.arm_time) $('i_sched_arm').value=d.arm_time;
        if(d.disarm_time) $('i_sched_disarm').value=d.disarm_time;
        $('i_auto_arm').value=d.auto_arm_minutes||0;
    }).catch(()=>{});
}
function saveSchedule() {
    api('schedule',{method:'POST',body:new URLSearchParams({
        'arm_time':$('i_sched_arm').value,'disarm_time':$('i_sched_disarm').value,
        'auto_arm_minutes':$('i_auto_arm').value
    })}).then(()=>showToast("Schedule saved")).catch(()=>showToast("Error"));
}

// --- EVENTS ---
function loadEvents() {
    fetch('/api/events').then(r=>r.json()).then(d => {
        let h='';
        d.forEach(e => {
            let tc='#fff',tn='?';
            switch(e.type){case 0:tn='SYS';tc='#888';break;case 1:tn='MOV';tc='#03dac6';break;
            case 2:tn='TMP';tc='#cf6679';break;case 3:tn='NET';tc='#bb86fc';break;
            case 4:tn='HB';tc='#4caf50';break;case 5:tn='SEC';tc='#ff9800';break;}
            let u=e.ts;
            let ts=Math.floor(u/3600)+"h "+Math.floor((u%3600)/60)+"m";
            h+='<tr style="border-bottom:1px solid #222"><td style="padding:5px;white-space:nowrap">'+ts+'</td><td style="padding:5px;color:'+tc+';font-weight:bold">'+tn+'</td><td style="padding:5px">'+e.msg+'</td></tr>';
        });
        $('event_list').innerHTML=h||'<tr><td colspan="3" style="text-align:center;padding:10px;color:#666">No events</td></tr>';
    }).catch(()=>{});
}
function clearEvents() {
    if(confirm("Clear event history?")) api('events/clear',{method:'POST'}).then(()=>loadEvents());
}

// --- NETWORK CONFIG ---
function saveMQTTConfig() {
    api('mqtt/config',{method:'POST',body:new URLSearchParams({
        'enabled':$('chk_mqtt_en').checked?1:0,
        'server':$('txt_mqtt_server').value,'port':$('txt_mqtt_port').value,
        'user':$('txt_mqtt_user').value,'pass':$('txt_mqtt_pass').value
    })});
}
function loadBackupWiFi() {
    fetch('/api/wifi/config').then(r=>r.json()).then(d=>{$('txt_bk_ssid').value=d.bk_ssid||'';}).catch(()=>{});
}
function saveBackupWiFi() {
    api('wifi/config',{method:'POST',body:new URLSearchParams({'bk_ssid':$('txt_bk_ssid').value,'bk_pass':$('txt_bk_pass').value})});
}
function loadTelegramConfig() {
    fetch('/api/telegram/config').then(r=>r.json()).then(d=>{
        $('chk_tg_en').checked=d.enabled; $('txt_tg_token').value=d.token||''; $('txt_tg_chat').value=d.chat_id||'';
    }).catch(()=>{});
}
function saveTelegram() {
    api('telegram/config',{method:'POST',body:new URLSearchParams({
        'enabled':$('chk_tg_en').checked?1:0,'token':$('txt_tg_token').value,'chat_id':$('txt_tg_chat').value
    })});
}
function testTelegram() {
    fetch('/api/telegram/test',{method:'POST'}).then(r=>r.json())
    .then(d=>showToast(d.success?"Telegram OK!":"Error: "+(d.error||"?")))
    .catch(()=>showToast("Error"));
}
function saveAuth() {
    let p=$('txt_auth_pass').value,p2=$('txt_auth_pass2').value;
    if(!$('txt_auth_user').value||!p){showToast("Fill in all fields");return;}
    if(p!==p2){showToast("Passwords do not match");return;}
    api('auth/config',{method:'POST',body:new URLSearchParams({'user':$('txt_auth_user').value,'pass':p})})
    .then(r=>{if(r.ok) showToast("Credentials saved. Restarting...");}).catch(()=>showToast("Error"));
}

// --- RADAR CONFIG ---
function updateConfig() {
    zMaxX=parseInt($('sl_width').value); zMinX=-zMaxX;
    zMaxY=parseInt($('sl_length').value);
    minRes=parseInt($('sl_minres').value);
    staticResThreshold=parseInt($('sl_static_res').value);
    ghostTimeout=parseInt($('sl_ghost').value)*1000;
    moveThreshold=parseInt($('sl_move').value);
    posThreshold=parseInt($('sl_pos').value);
    mapRotation=parseInt($('sl_rotation').value);
    let fd=new FormData();
    fd.append("z_x_min",zMinX);fd.append("z_x_max",zMaxX);fd.append("z_y_max",zMaxY);
    fd.append("min_res",minRes);fd.append("static_res_threshold",staticResThreshold);
    fd.append("ghost_timeout",ghostTimeout);fd.append("move_threshold",moveThreshold);
    fd.append("pos_threshold",posThreshold);fd.append("map_rotation",mapRotation);
    let hn=$('inp_hostname').value; if(hn) fd.append("hostname",hn);
    fetch('/api/config',{method:'POST',body:fd}).then(()=>showToast("Saved"));
}
function noiseCmd(cmd) {
    fetch('/api/noise/'+cmd,{method:'POST'}).then(r=>r.text().then(t=>showToast(t)));
}

// --- ZONES ---
function toggleDrawMode() {
    drawMode=!drawMode;
    let btn=$('btnDrawMode');
    if(drawMode) {
        btn.innerText='Drawing: ON'; btn.style.background='#00ff00';
        $('drawModeHelp').style.display='block'; $('radarMap').style.cursor='crosshair';
    } else {
        btn.innerText='Drawing: OFF'; btn.style.background='#ff6600';
        $('drawModeHelp').style.display='none'; $('radarMap').style.cursor='default'; drawStart=null;
    }
}
function setupCanvasDrawing() {
    const canvas=$('radarMap');
    canvas.onmousedown=e=>{
        if(!drawMode) return;
        const rect=canvas.getBoundingClientRect();
        drawStart={x:e.clientX-rect.left, y:e.clientY-rect.top};
    };
    canvas.onmouseup=e=>{
        if(!drawMode||!drawStart) return;
        const rect=canvas.getBoundingClientRect();
        const w=canvas.width,h=canvas.height,scX=8000/w,scY=8000/h,cxp=w/2;
        const x1=Math.round((drawStart.x-cxp)*scX),y1=Math.round((h-drawStart.y)*scY);
        const x2=Math.round(((e.clientX-rect.left)-cxp)*scX),y2=Math.round((h-(e.clientY-rect.top))*scY);
        const xmin=Math.min(x1,x2),xmax=Math.max(x1,x2),ymin=Math.min(y1,y2),ymax=Math.max(y1,y2);
        if(Math.abs(xmax-xmin)<200||Math.abs(ymax-ymin)<200){showToast("Min 200x200mm");drawStart=null;return;}
        let label=prompt("Name:","Blackout Zone")||"Blackout Zone";
        addBlackoutZone(xmin,xmax,ymin,ymax,label); drawStart=null;
    };
}
function addBlackoutZone(xmin,xmax,ymin,ymax,label) {
    let fd=new FormData();
    fd.append("xmin",xmin);fd.append("xmax",xmax);fd.append("ymin",ymin);fd.append("ymax",ymax);
    fd.append("label",label);fd.append("enabled","true");
    fetch('/api/blackout/add',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast("Zone added"); else showToast("Error");});
}
function deleteZone(id) {
    if(!confirm("Delete?")) return;
    let fd=new FormData(); fd.append("id",id);
    fetch('/api/blackout/delete',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast("Deleted");});
}
function toggleZone(id) {
    const zone=blackoutZones[id]; if(!zone) return;
    let fd=new FormData(); fd.append("id",id); fd.append("enabled",(!zone.enabled).toString());
    fetch('/api/blackout/update',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast(zone.enabled?"Disabled":"Enabled");});
}
function updateBlackoutList() {
    const list=$('blackoutList');
    if(!blackoutZones||blackoutZones.length===0){list.innerHTML='<div style="color:#555;font-size:0.8rem">No blackout zones</div>';return;}
    let h='';
    blackoutZones.forEach((z,i)=>{
        h+='<div style="background:#222;padding:6px;margin:4px 0;border-radius:5px;display:flex;justify-content:space-between;align-items:center">'+
        '<div><div style="font-weight:bold;color:'+(z.enabled?'#ff0000':'#555')+'">'+z.label+'</div>'+
        '<div style="font-size:0.65rem;color:#666">X:['+z.xmin+','+z.xmax+'] Y:['+z.ymin+','+z.ymax+']</div></div>'+
        '<div><button onclick="toggleZone('+i+')" style="padding:4px 8px;font-size:0.7rem;background:'+(z.enabled?'#666':'#00aa00')+';width:auto;margin:0 2px">'+(z.enabled?'OFF':'ON')+'</button>'+
        '<button onclick="deleteZone('+i+')" style="padding:4px 8px;font-size:0.7rem;background:#cc0000;width:auto;margin:0">X</button></div></div>';
    });
    list.innerHTML=h;
}
function addCurrentToPoly(id) {
    let fd=new FormData(); fd.append("id",id);
    fetch('/api/polygon/add_current',{method:'POST',body:fd}).then(r=>r.text().then(t=>showToast(t)));
}
function clearPoly(id) {
    if(!confirm("Clear polygon "+(id+1)+"?")) return;
    let fd=new FormData(); fd.append("id",id); fd.append("points","");
    fetch('/api/polygon/set',{method:'POST',body:fd}).then(()=>showToast("Cleared"));
}
function showAddZoneDialog() {
    let xmin=prompt("X Min (mm):","-1000"),xmax=prompt("X Max (mm):","1000");
    let ymin=prompt("Y Min (mm):","0"),ymax=prompt("Y Max (mm):","2000");
    let label=prompt("Name:","Blackout Zone");
    if(xmin&&xmax&&ymin&&ymax) addBlackoutZone(parseInt(xmin),parseInt(xmax),parseInt(ymin),parseInt(ymax),label||"Blackout Zone");
}

// --- OTA ---
function uploadFW() {
    let f=$('fw_file').files[0]; if(!f) return;
    let fd=new FormData(); fd.append('file',f);
    let xhr=new XMLHttpRequest();
    xhr.open('POST','/api/ota');
    xhr.upload.onprogress=e=>{
        let pct=(e.loaded/e.total*100);
        $('ota_bar').style.width=pct+"%"; $('ota_bar').style.background="var(--accent)";
        $('ota_status').innerText=Math.round(pct)+"%";
    };
    xhr.onload=()=>{$('ota_status').innerText="Done! Restarting...";setTimeout(()=>location.reload(),30000);};
    xhr.onerror=()=>{$('ota_status').innerText="Error!";$('ota_bar').style.background="#cc0000";};
    xhr.send(fd);
}
function exportConfig(){window.location.href="/api/config/export";}
function importConfig() {
    const f=$('importFile').files[0]; if(!f) return;
    if(!confirm("Overwrite configuration? Device will restart.")){$('importFile').value='';return;}
    let fd=new FormData(); fd.append("config",f);
    showToast("Uploading...");
    fetch('/api/config/import',{method:'POST',body:fd}).then(r=>r.text()).then(t=>{
        showToast(t); if(t.includes("Restart")) setTimeout(()=>location.reload(),5000);
    }).catch(e=>showToast("Error: "+e));
}
function sendCmd(url){fetch(url,{method:'POST'}).then(r=>showToast(r.ok?"OK":"Error"));}

window.onload=()=>{init();setupCanvasDrawing();};
</script>
</body>
</html>
)rawliteral";
