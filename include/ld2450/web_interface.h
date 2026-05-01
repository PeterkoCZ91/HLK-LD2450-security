const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="cs">
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
    <button id="lang_btn" onclick="setLang(LANG==='cs'?'en':'cs')" style="width:auto; padding:2px 8px; font-size:0.7rem; background:#333; border-radius:4px; margin:0; min-height:auto">🇬🇧 EN</button>
    <button onclick="toggleTheme()" style="background:transparent; border:none; font-size:1.2rem; cursor:pointer; padding:0; width:auto; margin:0" id="themeBtn">&#9728;&#65039;</button>
  </h2>

  <div id="security_warning" style="background:#cf6679; color:black; padding:10px; border-radius:8px; margin-bottom:10px; display:none; text-align:center; font-weight:bold; max-width:800px; margin-left:auto; margin-right:auto" data-i18n="default_pass_warn">
    Vychozi heslo admin/admin - zmente v sekci Sit
  </div>

  <div class="grid">
    <!-- STATUS -->
    <div class="card">
        <div class="gauge">
            <div id="alarm_badge" style="margin-bottom:8px; font-size:0.9rem; font-weight:bold; color:#888">---</div>
            <button id="btn_arm" onclick="toggleArm()" style="width:auto; padding:8px 20px; margin-bottom:10px; background:#3700b3" data-i18n="arm">ARM</button>
            <div class="big-val" id="trk_count" style="color:var(--accent)">0</div>
            <div class="unit" data-i18n="targets">TARGETS</div>
            <div id="trk_state" style="color:#888; font-weight:bold; letter-spacing:2px; margin-top:8px" data-i18n="state_idle">IDLE</div>
            <div id="tamper_status" style="font-size:0.7rem; color:#ff4444; margin-top:4px; display:none"></div>
        </div>
    </div>

    <!-- HEALTH -->
    <div class="card">
        <div class="stat-row"><span data-i18n="sensor_health">Zdravi senzoru</span><span id="h_score" class="stat-val">---</span></div>
        <div class="stat-row"><span data-i18n="fps">FPS</span><span id="h_fps">---</span></div>
        <div class="stat-row"><span data-i18n="uart_errors">UART Chyby</span><span id="h_err" style="color:var(--warn)">---</span></div>
        <div class="stat-row"><span data-i18n="ram_free">RAM (Volna)</span><span id="h_heap">---</span></div>
        <div class="stat-row"><span data-i18n="uptime">Doba behu</span><span id="h_uptime">---</span></div>
        <div class="stat-row"><span data-i18n="wifi_rssi">WiFi RSSI</span><span id="h_rssi">--- dBm</span></div>
        <div id="analytics_row" style="display:none">
            <div class="stat-row"><span data-i18n="entry_exit">Vstup / Vystup</span><span><span id="h_entry">0</span> / <span id="h_exit">0</span></span></div>
            <div class="stat-row"><span data-i18n="movement">Pohyb</span><span id="h_move">---</span></div>
        </div>
        <div style="display:flex; gap:5px; margin-top:8px">
            <button class="sec" style="flex:1" onclick="sendCmd('/api/restart')" data-i18n="btn_restart">Restart</button>
        </div>
    </div>
  </div>

  <!-- RADAROVA MAPA -->
  <div class="card cc">
      <div class="unit" style="margin-bottom:8px" data-i18n="radar_map">RADAROVA MAPA</div>
      <div id="canvasWrapper">
          <canvas id="radarMap" width="500" height="500"></canvas>
      </div>
      <div class="legend">
          <div class="legend-item"><div class="legend-dot" style="background:#ff0000"></div><span data-i18n="legend_target">Cil</span></div>
          <div class="legend-item"><div class="legend-dot" style="background:rgba(0,100,255,0.7)"></div><span data-i18n="legend_ghost">Duch</span></div>
          <div class="legend-item"><div class="legend-dot" style="background:#555"></div><span data-i18n="legend_noise">Sum</span></div>
          <div class="legend-item"><div class="legend-dot" style="background:#00ff66; border-radius:0; width:14px; height:3px"></div><span data-i18n="legend_polygon">Polygon</span></div>
          <div class="legend-item"><div class="legend-dot" style="background:#ff0000; border-radius:0; width:14px; height:3px"></div><span data-i18n="legend_blackout">Blackout</span></div>
      </div>
      <div style="text-align:center; margin-top:8px">
          <label style="font-size:0.8rem; cursor:pointer">
              <input type="checkbox" id="cb_debug_view" onchange="toggleDebugView()" style="width:auto">
              <span data-i18n="debug_view">Debug (Ghosts &amp; Noise)</span>
          </label>
      </div>
  </div>

  <!-- TABBED CONTROLS -->
  <div class="card cc">
      <div class="tabs">
          <div class="tab active" onclick="tab(0)" data-i18n="tab_radar">Radar</div>
          <div class="tab" onclick="tab(1)" data-i18n="tab_alarm">Alarm</div>
          <div class="tab" onclick="tab(2)" data-i18n="tab_zones">Zony</div>
          <div class="tab" onclick="tab(3)" data-i18n="tab_network">Sit</div>
          <div class="tab" onclick="tab(4)" data-i18n="tab_log">Log</div>
          <div class="tab" onclick="tab(5)" data-i18n="tab_system">System</div>
      </div>

      <!-- TAB 0: RADAR CONFIG -->
      <div id="tab0">
          <div class="stat-row"><span data-i18n="device_name">Nazev zarizeni (mDNS)</span></div>
          <div style="display:flex; gap:5px; margin-bottom:10px">
              <input type="text" id="inp_hostname" placeholder="esp32-ld2450-XXXX" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="rotation">Rotation</span>: <strong id="val_rotation">0</strong> <span data-i18n="rotation_unit">deg</span></label>
              <input type="range" id="sl_rotation" min="0" max="270" step="90" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="width">Width (+/- mm)</span>: <strong id="val_width">1000</strong></label>
              <input type="range" id="sl_width" min="500" max="4000" step="100" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="range">Range (mm)</span>: <strong id="val_length">6000</strong></label>
              <input type="range" id="sl_length" min="1000" max="8000" step="500" onchange="updateConfig()">
          </div>
          <div class="section-title" data-i18n="section_filtering">FILTRACE</div>
          <div class="slider-container">
              <label><span data-i18n="min_target_size">Min velikost cile</span>: <strong id="val_minres">200</strong></label>
              <input type="range" id="sl_minres" min="50" max="400" step="25" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="static_energy">Staticka energie</span>: <strong id="val_static_res">300</strong></label>
              <input type="range" id="sl_static_res" min="50" max="1000" step="50" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="ghost_timeout">Timeout duchu</span>: <strong id="val_ghost">60</strong>s</label>
              <input type="range" id="sl_ghost" min="5" max="120" step="5" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="move_threshold">Prah pohybu</span>: <strong id="val_move">5</strong> mm/s</label>
              <input type="range" id="sl_move" min="3" max="50" step="1" onchange="updateConfig()">
          </div>
          <div class="slider-container">
              <label><span data-i18n="pos_threshold">Prah pozice</span>: <strong id="val_pos">50</strong> mm</label>
              <input type="range" id="sl_pos" min="20" max="200" step="10" onchange="updateConfig()">
          </div>
      </div>

      <!-- TAB 1: ALARM / SECURITY -->
      <div id="tab1" class="hidden">
          <div class="section-title" data-i18n="section_alarm_delay">ZPOZDENI ALARMU</div>
          <div class="row-input">
              <span data-i18n="entry_delay">Vstupni zpozdeni (s)</span>
              <input type="number" id="i_entry_dl" placeholder="30" min="0" max="300" style="width:80px" onchange="saveAlarmConfig()">
          </div>
          <div class="row-input">
              <span data-i18n="exit_delay">Vystupni zpozdeni (s)</span>
              <input type="number" id="i_exit_dl" placeholder="30" min="0" max="300" style="width:80px" onchange="saveAlarmConfig()">
          </div>
          <div style="display:flex; align-items:center; gap:8px; margin:5px 0">
              <input type="checkbox" id="chk_dis_rem" style="width:auto" onchange="saveAlarmConfig()">
              <label for="chk_dis_rem" data-i18n="disarm_reminder">Pripominac "Stale ODSTREZENO"</label>
          </div>

          <div class="section-title" data-i18n="section_antimask">ANTI-MASKING (Zakryti senzoru)</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_am_en" style="width:auto" onchange="saveSec()">
              <label for="chk_am_en" data-i18n="antimask_alert">Upozorneni pri zakryti</label>
          </div>
          <div class="row-input">
              <span data-i18n="timeout_s">Timeout (s)</span>
              <input type="number" id="i_am" placeholder="300" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title" data-i18n="section_loiter">POBYHOVANI</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_loit_en" style="width:auto" onchange="saveSec()">
              <label for="chk_loit_en" data-i18n="loiter_alert">Upozorneni pri pobyhovani</label>
          </div>
          <div class="row-input">
              <span data-i18n="timeout_s">Timeout (s)</span>
              <input type="number" id="i_loit" placeholder="15" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title" data-i18n="section_heartbeat">SRDECNI TEP</div>
          <div class="row-input">
              <span data-i18n="interval_h">Interval (hodiny)</span>
              <input type="number" id="i_hb" placeholder="4" min="0" max="24" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title" data-i18n="section_wifi_sec">WIFI BEZPECNOST</div>
          <div class="row-input">
              <span data-i18n="rssi_threshold">Prah RSSI (dBm)</span>
              <input type="number" id="i_rssi_thresh" placeholder="-80" min="-95" max="-40" style="width:80px" onchange="saveSec()">
          </div>
          <div class="row-input">
              <span data-i18n="rssi_drop">Max pokles RSSI (dB)</span>
              <input type="number" id="i_rssi_drop" placeholder="20" min="5" max="50" style="width:80px" onchange="saveSec()">
          </div>

          <div class="section-title" data-i18n="section_schedule">ROZVRH</div>
          <div class="row-input">
              <span data-i18n="auto_arm_at">Auto strezeni v</span>
              <input type="time" id="i_sched_arm" style="width:100px">
          </div>
          <div class="row-input">
              <span data-i18n="auto_disarm_at">Auto odstrezeni v</span>
              <input type="time" id="i_sched_disarm" style="width:100px">
          </div>
          <div class="row-input">
              <span data-i18n="auto_arm_idle">Auto-strezeni po neaktivite (min)</span>
              <input type="number" id="i_auto_arm" placeholder="0" min="0" max="1440" style="width:80px">
          </div>
          <div class="section-title" style="margin-top:10px" data-i18n="section_night_profile">NOCNI PROFIL ZON</div>
          <div style="font-size:0.75rem;color:#888;margin-bottom:6px" data-i18n="night_help">Casy prepnuti den/noc profilu (prazdne = stale den).</div>
          <div class="row-input">
              <span data-i18n="night_start">Zacatek noci</span>
              <input type="time" id="i_night_start" style="width:100px">
          </div>
          <div class="row-input">
              <span data-i18n="night_end">Konec noci</span>
              <input type="time" id="i_night_end" style="width:100px">
          </div>
          <div class="row-input" style="margin-bottom:8px">
              <span data-i18n="active_profile">Aktivni profil</span>
              <span id="cur_profile" style="color:var(--accent); font-weight:bold">---</span>
          </div>
          <button onclick="saveSchedule()" class="sec" data-i18n="btn_save_schedule">Ulozit rozvrh</button>
      </div>

      <!-- TAB 2: ZONES -->
      <div id="tab2" class="hidden">
          <div class="section-title" data-i18n="section_polygons">POLYGONOVE ZONY</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:8px" data-i18n="poly_help">Stoupnete do rohu a kliknete pro pridani bodu.</div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:8px">
              <button onclick="addCurrentToPoly(0)" class="sec" style="width:auto; padding:8px 12px; font-size:0.8rem" data-i18n="zone1_point">Zone 1 Point</button>
              <button onclick="addCurrentToPoly(1)" class="sec" style="width:auto; padding:8px 12px; font-size:0.8rem" data-i18n="zone2_point">Zone 2 Point</button>
          </div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:5px">
              <button onclick="clearPoly(0)" class="sec" style="width:auto; padding:5px 10px; font-size:0.7rem" data-i18n="del_zone1">Smazat zonu 1</button>
              <button onclick="clearPoly(1)" class="sec" style="width:auto; padding:5px 10px; font-size:0.7rem" data-i18n="del_zone2">Smazat zonu 2</button>
          </div>
          <div style="font-size:0.7rem; color:#666; font-style:italic; margin-bottom:10px" data-i18n="poly_profile_note">Polygony jsou aktivni v obou profilech (zatim beze zmeny).</div>

          <div class="section-title" data-i18n="section_blackout">BLACKOUT ZONY</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:8px" data-i18n="blackout_help">Vyloucene oblasti (HVAC, anteny).</div>
          <button id="btnDrawMode" onclick="toggleDrawMode()" style="background:#ff6600; margin-bottom:8px" data-i18n="draw_off">
              Kresleni: VYP
          </button>
          <div id="drawModeHelp" style="display:none;font-size:0.75rem;color:#ff6600;margin-bottom:8px" data-i18n="draw_help">
              Kliknete a tahejte na mape pro vytvoreni blackout zony.
          </div>
          <div id="blackoutList" style="text-align:left"></div>
          <button onclick="showAddZoneDialog()" class="sec" style="margin-top:5px" data-i18n="add_manual">+ Pridat rucne</button>

          <div class="section-title" data-i18n="section_tripwire">TRIPWIRE (Vstup/Vystup)</div>
          <div style="display:flex; align-items:center; gap:8px; margin-bottom:5px">
              <input type="checkbox" id="chk_tripwire" style="width:auto" onchange="saveTripwire()">
              <label for="chk_tripwire" data-i18n="tripwire_enable">Zapnout tripwire</label>
          </div>
          <div class="row-input">
              <span data-i18n="tripwire_y">Pozice linie Y (mm)</span>
              <input type="number" id="i_tripwire_y" value="3000" min="500" max="8000" style="width:100px" onchange="saveTripwire()">
          </div>
          <button onclick="api('tripwire',{method:'POST',body:new URLSearchParams({reset:1})}).then(()=>showToast(t('counter_reset')))" class="sec" data-i18n="btn_reset_counter">Reset pocitadla</button>

          <div class="section-title" data-i18n="section_region_filter">RADAR REGION FILTER (HW)</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:8px" data-i18n="region_help">
              Filtr primo v radaru (cmd 0xC2). Cile mimo zony se neposilaji pres UART. Komplementarni k SW polygonum.
          </div>
          <div class="row-input">
              <span data-i18n="rf_mode">Rezim</span>
              <select id="rf_mode" style="width:160px">
                  <option value="0" data-i18n="rf_disabled">Vypnuto</option>
                  <option value="1" data-i18n="rf_detect">Pouze detekovat</option>
                  <option value="2" data-i18n="rf_exclude">Vyloucit</option>
              </select>
          </div>
          <div id="rf_zones_box"></div>
          <button onclick="saveRegionFilter()" class="sec" style="margin-top:8px" data-i18n="btn_save_rf">Ulozit region filter</button>
      </div>

      <!-- TAB 3: NETWORK -->
      <div id="tab3" class="hidden">
          <div class="section-title" data-i18n="section_mqtt">MQTT BROKER</div>
          <div id="mqtt_status_row" class="stat-row" style="margin-bottom:10px">
              <span data-i18n="status">Stav</span>
              <span id="mqtt_status_text" style="color:var(--warn)">---</span>
          </div>
          <div style="display:flex; align-items:center; gap:8px">
              <input type="checkbox" id="chk_mqtt_en" style="width:auto">
              <label for="chk_mqtt_en" data-i18n="enable_mqtt">Zapnout MQTT</label>
          </div>
          <div style="display:flex; align-items:center; gap:8px">
              <input type="checkbox" id="chk_mqtt_tls" style="width:auto">
              <label for="chk_mqtt_tls">TLS</label>
          </div>
          <input type="text" id="txt_mqtt_server" placeholder="Server IP">
          <div style="display:flex; gap:5px">
              <input type="text" id="txt_mqtt_port" placeholder="Port (1883)">
              <input type="text" id="txt_mqtt_user" placeholder="Username">
          </div>
          <input type="password" id="txt_mqtt_pass" placeholder="Password">
          <button onclick="saveMQTTConfig()" class="sec" data-i18n="btn_save_mqtt">Ulozit MQTT</button>

          <div class="section-title" data-i18n="section_backup_wifi">ZALOZNI WIFI</div>
          <input type="text" id="txt_bk_ssid" placeholder="SSID">
          <input type="password" id="txt_bk_pass" placeholder="Password">
          <button onclick="saveBackupWiFi()" class="sec" data-i18n="btn_save_wifi">Ulozit WiFi</button>

          <div class="section-title">TELEGRAM</div>
          <div style="display:flex; align-items:center; gap:8px">
              <input type="checkbox" id="chk_tg_en" style="width:auto">
              <label for="chk_tg_en" data-i18n="enable_bot">Zapnout bota</label>
          </div>
          <input type="text" id="txt_tg_token" data-i18n="bot_token" placeholder="Token bota">
          <input type="text" id="txt_tg_chat" placeholder="Chat ID">
          <div style="display:flex; gap:5px">
              <button onclick="saveTelegram()" class="sec" data-i18n="btn_save">Save</button>
              <button onclick="testTelegram()" class="sec" data-i18n="btn_test">Test</button>
          </div>

          <div class="section-title" data-i18n="section_login">PRIHLASENI</div>
          <input type="text" id="txt_auth_user" placeholder="Username">
          <input type="password" id="txt_auth_pass" data-i18n="new_password" placeholder="Nove heslo">
          <input type="password" id="txt_auth_pass2" data-i18n="pass_confirm" placeholder="Potvrzeni hesla">
          <button onclick="saveAuth()" class="warn" data-i18n="btn_change_pass">Zmenit heslo</button>
      </div>

      <!-- TAB 4: EVENTS -->
      <div id="tab4" class="hidden">
          <div style="display:flex; justify-content:space-between; align-items:center; margin-bottom:10px">
              <div class="section-title" style="margin:0; border:none" data-i18n="section_events">RECENT EVENTS</div>
              <button onclick="clearEvents()" class="warn" style="width:auto; padding:5px 10px; margin:0" data-i18n="btn_clear">Clear</button>
          </div>
          <div style="overflow-x:auto">
              <table style="width:100%; border-collapse:collapse; font-size:0.8rem; text-align:left">
                  <thead>
                      <tr style="border-bottom:1px solid #444; color:#888">
                          <th style="padding:5px" data-i18n="table_time">Time</th>
                          <th style="padding:5px" data-i18n="table_type">Type</th>
                          <th style="padding:5px" data-i18n="table_message">Message</th>
                      </tr>
                  </thead>
                  <tbody id="event_list"></tbody>
              </table>
          </div>
      </div>

      <!-- TAB 5: SYSTEM -->
      <div id="tab5" class="hidden">
          <div class="section-title" data-i18n="section_device_info">INFORMACE O ZARIZENI</div>
          <div class="stat-row"><span data-i18n="device_id">ID zarizeni</span><span id="sys_devid">---</span></div>
          <div class="stat-row"><span data-i18n="ip_address">IP adresa</span><span id="sys_ip">---</span></div>
          <div class="stat-row"><span data-i18n="mac_esp">MAC adresa (ESP)</span><span id="sys_mac">---</span></div>
          <div class="stat-row"><span data-i18n="mac_radar">MAC adresa (radar)</span><span id="sys_radar_mac">---</span></div>
          <div class="stat-row"><span data-i18n="firmware">Firmware</span><span id="sys_fw">---</span></div>

          <div class="section-title" data-i18n="section_radar_ble">RADAR BLE (BEZPECNOST)</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:10px" data-i18n="ble_warn">
              Modul LD2450 ma vlastni BLE konfiguracni rozhrani (neauten.). Pro snizeni utocne plochy doporucuji vypnout.
          </div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:15px">
              <button onclick="radarBT(0)" style="background:#cc3333; width:auto; padding:8px 15px" data-i18n="btn_disable_ble">Vypnout BLE radaru</button>
              <button onclick="radarBT(1)" style="background:#333; width:auto; padding:8px 15px" data-i18n="btn_enable_ble">Zapnout BLE radaru</button>
          </div>

          <div class="section-title" data-i18n="section_noise_ai">AI UCENI SUMU</div>
          <div style="font-size:0.8rem;color:#888;margin-bottom:10px" data-i18n="noise_help">
              Nechte senzor 1h v prazdne mistnosti. Nauci se ignorovat duchy.
          </div>
          <div id="noise_status" style="margin-bottom:10px; padding:8px; border-radius:8px; background:rgba(255,255,255,0.05)">
              <div style="font-size:0.7rem; color:#888" data-i18n="learning_status">STAV UCENI</div>
              <div id="noise_state_val" style="font-weight:bold; color:#aaa" data-i18n="inactive">Inactive</div>
              <div id="noise_stats" style="font-size:0.75rem; color:#666; margin-top:3px; display:none">
                  <span data-i18n="samples_label">Samples</span>: <span id="noise_samples">0</span> | <span data-i18n="time_label">Time</span>: <span id="noise_time">0</span>s
              </div>
          </div>
          <div style="display:flex; justify-content:center; gap:5px; margin-bottom:8px">
              <button id="btn_noise_start" onclick="noiseCmd('start')" style="background:#6200ee; width:auto; padding:8px 15px" data-i18n="btn_start">Start</button>
              <button id="btn_noise_stop" onclick="noiseCmd('stop')" style="background:#333; display:none; width:auto; padding:8px 15px" data-i18n="btn_save_noise">Save</button>
          </div>
          <div style="display:flex; align-items:center; justify-content:center; gap:8px; margin-bottom:15px">
              <input type="checkbox" id="cb_noise_filter" onchange="noiseCmd('toggle')" style="width:auto">
              <label for="cb_noise_filter" data-i18n="enable_noise_filter">Zapnout sum. filtr</label>
          </div>

          <div class="section-title" data-i18n="section_ota">AKTUALIZACE FW (OTA)</div>
          <input type="file" id="fw_file" accept=".bin">
          <div id="ota_bar" style="height:5px; background:#333; margin-top:5px; width:0%; transition:width 0.2s; border-radius:3px"></div>
          <div id="ota_status" style="font-size:0.75rem; color:#888; margin-top:3px"></div>
          <button onclick="uploadFW()" data-i18n="btn_upload_fw">Nahrat firmware</button>

          <div class="section-title" data-i18n="section_backup_restore">ZALOHA &amp; OBNOVA</div>
          <div style="display:flex; gap:5px">
              <button onclick="exportConfig()" class="sec" style="flex:1" data-i18n="btn_export">Export</button>
              <button onclick="document.getElementById('importFile').click()" class="sec" style="flex:1" data-i18n="btn_import">Import</button>
              <input type="file" id="importFile" accept=".json" style="display:none" onchange="importConfig()">
          </div>
      </div>
  </div>

  <div id="toast">OK</div>

<script>
// --- i18n CZ/EN ---
const I18N = {
  cs: {
    title: "LD2450 Zabezpečení",
    default_pass_warn: "⚠️ Výchozí heslo admin/admin — změňte v sekci Síť",
    arm: "STŘEŽIT", disarm: "ZRUŠIT", cancel: "ZRUŠIT",
    disarmed: "🔓 NESTŘEŽENO", arming: "⏳ AKTIVUJI…", armed: "🔒 STŘEŽENO",
    pending: "⚠️ ČEKÁNÍ", triggered: "🚨 POPLACH!",
    targets: "CÍLE", state_idle: "KLID", state_detected: "DETEKCE", state_offline: "OFFLINE",
    sensor_health: "Zdraví senzoru", fps: "FPS", uart_errors: "UART chyby",
    ram_free: "RAM (volná)", uptime: "Doba běhu", wifi_rssi: "WiFi RSSI",
    entry_exit: "Vstup / Výstup", movement: "Pohyb", btn_restart: "Restart",
    radar_map: "RADAROVÁ MAPA",
    legend_target: "Cíl", legend_ghost: "Duch", legend_noise: "Šum",
    legend_polygon: "Polygon", legend_blackout: "Blackout",
    debug_view: "Debug (duchové &amp; šum)",
    tab_radar: "Radar", tab_alarm: "Alarm", tab_zones: "Zóny",
    tab_network: "Síť", tab_log: "Log", tab_system: "Systém",
    device_name: "Název zařízení (mDNS)",
    rotation: "Rotace", rotation_unit: "°",
    width: "Šířka (+/- mm)", range: "Dosah (mm)",
    section_filtering: "FILTRACE",
    min_target_size: "Min. velikost cíle", static_energy: "Statická energie",
    ghost_timeout: "Timeout duchů", move_threshold: "Práh pohybu",
    pos_threshold: "Práh pozice",
    section_alarm_delay: "ZPOŽDĚNÍ ALARMU",
    entry_delay: "Zpoždění vstupu (s)", exit_delay: "Zpoždění výstupu (s)",
    disarm_reminder: "Připomínka „Stále NESTŘEŽENO\"",
    section_antimask: "ANTI-MASKING (zakrytí senzoru)",
    antimask_alert: "Upozornění při zakrytí",
    timeout_s: "Timeout (s)",
    section_loiter: "POBÝVÁNÍ", loiter_alert: "Upozornění při pobývání",
    section_heartbeat: "SRDEČNÍ TEP", interval_h: "Interval (hodiny)",
    section_wifi_sec: "WIFI BEZPEČNOST",
    rssi_threshold: "Práh RSSI (dBm)", rssi_drop: "Max. pokles RSSI (dB)",
    section_schedule: "ROZVRH",
    auto_arm_at: "Auto střežení v", auto_disarm_at: "Auto odstřežení v",
    auto_arm_idle: "Auto-střežení po nečinnosti (min)",
    btn_save_schedule: "Uložit rozvrh",
    section_night_profile: "NOČNÍ PROFIL ZÓN",
    night_help: "Časy přepnutí den/noc profilu (prázdné = stále den).",
    night_start: "Začátek noci", night_end: "Konec noci",
    active_profile: "Aktivní profil",
    profile_day: "DEN", profile_night: "NOC",
    section_polygons: "POLYGONOVÉ ZÓNY",
    poly_help: "Stoupněte do rohu a klikněte pro přidání bodu.",
    poly_profile_note: "Polygony jsou aktivní v obou profilech (zatím beze změny).",
    zone1_point: "Zóna 1 — bod", zone2_point: "Zóna 2 — bod",
    del_zone1: "Smazat zónu 1", del_zone2: "Smazat zónu 2",
    section_blackout: "BLACKOUT ZÓNY",
    blackout_help: "Vyloučené oblasti (HVAC, antény).",
    draw_off: "Kreslení: VYP", draw_on: "Kreslení: ZAP",
    draw_help: "Klikněte a táhněte na mapě pro vytvoření blackout zóny.",
    add_manual: "+ Přidat ručně",
    section_tripwire: "TRIPWIRE (vstup/výstup)",
    tripwire_enable: "Zapnout tripwire", tripwire_y: "Pozice linie Y (mm)",
    btn_reset_counter: "Reset počítadla",
    section_region_filter: "FILTR REGIONU V RADARU (HW)",
    region_help: "Filtr přímo v radaru (cmd 0xC2). Cíle mimo zóny se neposílají přes UART. Komplementární k SW polygonům.",
    rf_mode: "Režim", rf_disabled: "Vypnuto",
    rf_detect: "Pouze detekovat", rf_exclude: "Vyloučit",
    btn_save_rf: "Uložit region filter", rf_saved: "Region filter uložen",
    section_mqtt: "MQTT BROKER", status: "Stav",
    enable_mqtt: "Zapnout MQTT", btn_save_mqtt: "Uložit MQTT",
    section_backup_wifi: "ZÁLOŽNÍ WIFI", btn_save_wifi: "Uložit WiFi",
    enable_bot: "Zapnout bota", bot_token: "Token bota",
    btn_save: "Uložit", btn_test: "Test",
    section_login: "PŘIHLÁŠENÍ",
    new_password: "Nové heslo", pass_confirm: "Potvrzení hesla",
    btn_change_pass: "Změnit heslo",
    section_events: "POSLEDNÍ UDÁLOSTI",
    btn_clear: "Vymazat",
    table_time: "Čas", table_type: "Typ", table_message: "Zpráva",
    no_events: "Žádné události",
    section_device_info: "INFORMACE O ZAŘÍZENÍ",
    device_id: "ID zařízení", ip_address: "IP adresa",
    mac_esp: "MAC adresa (ESP)", mac_radar: "MAC adresa (radar)", firmware: "Firmware",
    section_radar_ble: "RADAR BLE (BEZPEČNOST)",
    ble_warn: "Modul LD2450 má vlastní BLE konfigurační rozhraní (neautentizované). Pro snížení útočné plochy doporučuji vypnout.",
    btn_disable_ble: "Vypnout BLE radaru", btn_enable_ble: "Zapnout BLE radaru",
    section_noise_ai: "KALIBRACE POZADÍ",
    noise_help: "Nechte senzor 1 h v prázdné místnosti, naučí se ignorovat statické pozadí (duchy).",
    learning_status: "STAV UČENÍ", inactive: "Neaktivní",
    samples_label: "Vzorky", time_label: "Čas",
    btn_start: "Spustit", btn_save_noise: "Uložit",
    enable_noise_filter: "Filtr na základě kalibrace",
    section_ota: "AKTUALIZACE FW (OTA)", btn_upload_fw: "Nahrát firmware",
    section_backup_restore: "ZÁLOHA &amp; OBNOVA",
    btn_export: "Export", btn_import: "Import",
    running: "BĚŽÍ…", starts_in: "ZAČÍNÁ ZA", filter_active: "FILTR AKTIVNÍ", ready: "Připraveno",
    mqtt_connected: "Připojeno k", mqtt_disconnected: "Odpojeno",
    err: "Chyba", ok: "OK", saved: "Uloženo",
    counter_reset: "Počítadlo vynulováno",
    tripwire_saved: "Tripwire uloženo", schedule_saved: "Rozvrh uložen",
    telegram_ok: "Telegram OK!",
    fill_fields: "Vyplňte všechna pole", pass_mismatch: "Hesla se neshodují",
    creds_saved: "Údaje uloženy. Restartuji…",
    confirm_enable_ble: "Zapnout BLE rozhraní radaru? (zvyšuje útočnou plochu)",
    confirm_disable_ble: "Vypnout BLE radaru? (radar se nakrátko restartuje)",
    min_zone_size: "Min. 200×200 mm",
    prompt_name: "Název:", default_blackout_name: "Blackout zóna",
    zone_added: "Zóna přidána",
    confirm_delete: "Smazat?", deleted: "Smazáno",
    disabled: "Vypnuto", enabled: "Zapnuto",
    no_blackout: "Žádné blackout zóny",
    confirm_clear_poly: "Vymazat polygon?", cleared: "Vymazáno",
    confirm_clear_events: "Vymazat historii událostí?",
    prompt_x_min: "X min (mm):", prompt_x_max: "X max (mm):",
    prompt_y_min: "Y min (mm):", prompt_y_max: "Y max (mm):",
    ota_done: "Hotovo! Restartuji…", ota_error: "Chyba!",
    confirm_overwrite: "Přepsat konfiguraci? Zařízení se restartuje.",
    uploading: "Nahrávám…", loading: "NAČÍTÁM…",
  },
  en: {
    title: "LD2450 Security",
    default_pass_warn: "⚠️ Default password admin/admin — change in Network section",
    arm: "ARM", disarm: "DISARM", cancel: "CANCEL",
    disarmed: "🔓 DISARMED", arming: "⏳ ARMING…", armed: "🔒 ARMED",
    pending: "⚠️ PENDING", triggered: "🚨 TRIGGERED!",
    targets: "TARGETS", state_idle: "IDLE", state_detected: "DETECTED", state_offline: "OFFLINE",
    sensor_health: "Sensor Health", fps: "FPS", uart_errors: "UART Errors",
    ram_free: "RAM (Free)", uptime: "Uptime", wifi_rssi: "WiFi RSSI",
    entry_exit: "Entry / Exit", movement: "Motion", btn_restart: "Restart",
    radar_map: "RADAR MAP",
    legend_target: "Target", legend_ghost: "Ghost", legend_noise: "Noise",
    legend_polygon: "Polygon", legend_blackout: "Blackout",
    debug_view: "Debug (Ghosts &amp; Noise)",
    tab_radar: "Radar", tab_alarm: "Alarm", tab_zones: "Zones",
    tab_network: "Network", tab_log: "Log", tab_system: "System",
    device_name: "Device Name (mDNS)",
    rotation: "Rotation", rotation_unit: "°",
    width: "Width (+/- mm)", range: "Range (mm)",
    section_filtering: "FILTERING",
    min_target_size: "Min target size", static_energy: "Static energy",
    ghost_timeout: "Ghost timeout", move_threshold: "Motion threshold",
    pos_threshold: "Position threshold",
    section_alarm_delay: "ALARM DELAY",
    entry_delay: "Entry delay (s)", exit_delay: "Exit delay (s)",
    disarm_reminder: "Remind \"Still DISARMED\"",
    section_antimask: "ANTI-MASKING (sensor masking)",
    antimask_alert: "Alert on masking",
    timeout_s: "Timeout (s)",
    section_loiter: "LOITERING", loiter_alert: "Alert on loitering",
    section_heartbeat: "HEARTBEAT", interval_h: "Interval (hours)",
    section_wifi_sec: "WIFI SECURITY",
    rssi_threshold: "RSSI threshold (dBm)", rssi_drop: "Max RSSI drop (dB)",
    section_schedule: "SCHEDULE",
    auto_arm_at: "Auto arm at", auto_disarm_at: "Auto disarm at",
    auto_arm_idle: "Auto-arm after inactivity (min)",
    btn_save_schedule: "Save schedule",
    section_night_profile: "NIGHT ZONE PROFILE",
    night_help: "Day/night profile switch times (empty = always day).",
    night_start: "Night start", night_end: "Night end",
    active_profile: "Active profile",
    profile_day: "DAY", profile_night: "NIGHT",
    section_polygons: "POLYGON ZONES",
    poly_help: "Stand in corner and click to add point.",
    poly_profile_note: "Polygons are active in both profiles (not yet configurable).",
    zone1_point: "Zone 1 — point", zone2_point: "Zone 2 — point",
    del_zone1: "Delete zone 1", del_zone2: "Delete zone 2",
    section_blackout: "BLACKOUT ZONES",
    blackout_help: "Excluded areas (HVAC, antennas).",
    draw_off: "Drawing: OFF", draw_on: "Drawing: ON",
    draw_help: "Click and drag on map to create blackout zone.",
    add_manual: "+ Add manual",
    section_tripwire: "TRIPWIRE (entry/exit)",
    tripwire_enable: "Enable tripwire", tripwire_y: "Line Y position (mm)",
    btn_reset_counter: "Reset counter",
    section_region_filter: "RADAR REGION FILTER (HW)",
    region_help: "Filter directly in radar (cmd 0xC2). Targets outside zones never reach UART. Complementary to SW polygons.",
    rf_mode: "Mode", rf_disabled: "Disabled",
    rf_detect: "Detect only", rf_exclude: "Exclude",
    btn_save_rf: "Save region filter", rf_saved: "Region filter saved",
    section_mqtt: "MQTT BROKER", status: "Status",
    enable_mqtt: "Enable MQTT", btn_save_mqtt: "Save MQTT",
    section_backup_wifi: "BACKUP WIFI", btn_save_wifi: "Save WiFi",
    enable_bot: "Enable bot", bot_token: "Bot token",
    btn_save: "Save", btn_test: "Test",
    section_login: "LOGIN",
    new_password: "New password", pass_confirm: "Password confirmation",
    btn_change_pass: "Change password",
    section_events: "RECENT EVENTS",
    btn_clear: "Clear",
    table_time: "Time", table_type: "Type", table_message: "Message",
    no_events: "No events",
    section_device_info: "DEVICE INFORMATION",
    device_id: "Device ID", ip_address: "IP address",
    mac_esp: "MAC address (ESP)", mac_radar: "MAC address (radar)", firmware: "Firmware",
    section_radar_ble: "RADAR BLE (SECURITY)",
    ble_warn: "LD2450 module has its own BLE configuration interface (unauthenticated). To reduce attack surface I recommend disabling it.",
    btn_disable_ble: "Disable radar BLE", btn_enable_ble: "Enable radar BLE",
    section_noise_ai: "BACKGROUND CALIBRATION",
    noise_help: "Leave the sensor 1 h in an empty room. It learns to ignore static background (ghosts).",
    learning_status: "LEARNING STATUS", inactive: "Inactive",
    samples_label: "Samples", time_label: "Time",
    btn_start: "Start", btn_save_noise: "Save",
    enable_noise_filter: "Use calibrated background filter",
    section_ota: "FIRMWARE UPDATE (OTA)", btn_upload_fw: "Upload firmware",
    section_backup_restore: "BACKUP &amp; RESTORE",
    btn_export: "Export", btn_import: "Import",
    running: "RUNNING…", starts_in: "STARTS IN", filter_active: "FILTER ACTIVE", ready: "Ready",
    mqtt_connected: "Connected to", mqtt_disconnected: "Disconnected",
    err: "Error", ok: "OK", saved: "Saved",
    counter_reset: "Counter reset",
    tripwire_saved: "Tripwire saved", schedule_saved: "Schedule saved",
    telegram_ok: "Telegram OK!",
    fill_fields: "Fill in all fields", pass_mismatch: "Passwords do not match",
    creds_saved: "Credentials saved. Restarting…",
    confirm_enable_ble: "Enable radar BLE interface? (increases attack surface)",
    confirm_disable_ble: "Disable radar BLE? (radar will briefly restart)",
    min_zone_size: "Min 200×200 mm",
    prompt_name: "Name:", default_blackout_name: "Blackout Zone",
    zone_added: "Zone added",
    confirm_delete: "Delete?", deleted: "Deleted",
    disabled: "Disabled", enabled: "Enabled",
    no_blackout: "No blackout zones",
    confirm_clear_poly: "Clear polygon?", cleared: "Cleared",
    confirm_clear_events: "Clear event history?",
    prompt_x_min: "X Min (mm):", prompt_x_max: "X Max (mm):",
    prompt_y_min: "Y Min (mm):", prompt_y_max: "Y Max (mm):",
    ota_done: "Done! Restarting…", ota_error: "Error!",
    confirm_overwrite: "Overwrite configuration? Device will restart.",
    uploading: "Uploading…", loading: "LOADING…",
  }
};
let LANG = localStorage.getItem('lang') || 'cs';
function t(k){ return (I18N[LANG] && I18N[LANG][k]) || (I18N.en && I18N.en[k]) || k; }
function setLang(l){ LANG=l; localStorage.setItem('lang', l); applyLang(); }
function renderCurProfile(){
    const el=document.getElementById('cur_profile'); if(!el) return;
    const p=window._curProfile||'day';
    el.innerText=(p==='night')?('\u{1F319} '+t('profile_night')):('\u{2600}\u{FE0F} '+t('profile_day'));
}
function applyLang(){
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const k = el.getAttribute('data-i18n');
        if(el.tagName==='INPUT' || el.tagName==='TEXTAREA') el.placeholder = t(k);
        else el.innerHTML = t(k);
    });
    const lb = document.getElementById('lang_btn');
    if(lb) lb.textContent = LANG==='cs' ? '🇬🇧 EN' : '🇨🇿 CZ';
    document.title = t('title');
    document.documentElement.lang = LANG;
    renderCurProfile();
    if(window._curAlarmState) updateAlarmUI(window._curAlarmState);
}

const $ = id => document.getElementById(id);
const api = (ep, opts) => fetch('/api/'+ep, opts).then(r => {
    if(r.ok) showToast(t("ok")); else showToast(t("err"));
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
        $('trk_state').innerText=anyInZone?t("state_detected"):t("state_idle");
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
        $('trk_state').innerText=t("state_offline");
        $('trk_state').style.color="#ff4444";
    });
}

// --- NOISE UI ---
function updateNoiseUI(data) {
    const nsVal=$('noise_state_val'), nsStats=$('noise_stats');
    const btnStart=$('btn_noise_start'), btnStop=$('btn_noise_stop');
    $('cb_noise_filter').checked=data.noise_filter;
    if(data.noise_learning) {
        nsVal.innerText=t("running"); nsVal.style.color="#bb86fc";
        nsStats.style.display="block";
        $('noise_samples').innerText=data.noise_samples;
        $('noise_time').innerText=data.noise_elapsed;
        btnStart.style.display="none"; btnStop.style.display="inline-block";
    } else if(data.noise_pending) {
        nsVal.innerText=t("starts_in")+": "+data.noise_countdown+"s"; nsVal.style.color="#ff8800";
        nsStats.style.display="none"; btnStart.style.display="none"; btnStop.style.display="none";
    } else {
        nsVal.innerText=data.noise_filter?t("filter_active"):t("ready");
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
        if(d.mqtt_enabled!==undefined) $('chk_mqtt_en').checked=!!d.mqtt_enabled;
        if(d.mqtt_tls!==undefined) $('chk_mqtt_tls').checked=!!d.mqtt_tls;
        if(d.mqtt_server) { $('txt_mqtt_server').value=d.mqtt_server; $('txt_mqtt_port').value=d.mqtt_port||''; }
        // MQTT status text
        let ms=$('mqtt_status_text');
        if(ms){
            if(d.mqtt_connected){ms.innerText=t("mqtt_connected")+" "+d.mqtt_server+":"+d.mqtt_port;ms.style.color="var(--accent)";}
            else{ms.innerText=t("mqtt_disconnected");ms.style.color="var(--warn)";}
        }
        // System info
        if(d.device_id) $('sys_devid').innerText=d.device_id;
        if(d.ip_address) $('sys_ip').innerText=d.ip_address;
        if(d.mac_address) $('sys_mac').innerText=d.mac_address;
        if(d.radar_mac) $('sys_radar_mac').innerText=d.radar_mac;
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
    if(n===2) loadRegionFilter();
    if(n===4) loadEvents();
}

// --- REGION FILTER (HW radar 0xC2) ---
function loadRegionFilter() {
    fetch('/api/zones/region_filter').then(r=>r.json()).then(d=>{
        $('rf_mode').value=d.mode||0;
        let html='';
        for(let z=0;z<3;z++){
            const zn=(d.zones&&d.zones[z])||{x1:0,y1:0,x2:0,y2:0};
            html+='<div style="display:grid;grid-template-columns:30px 1fr 1fr 1fr 1fr;gap:4px;margin-bottom:4px;align-items:center;font-size:0.8rem">'+
                '<span>Z'+(z+1)+'</span>'+
                '<input type="number" id="rf_'+z+'_x1" value="'+zn.x1+'" placeholder="x1">'+
                '<input type="number" id="rf_'+z+'_y1" value="'+zn.y1+'" placeholder="y1">'+
                '<input type="number" id="rf_'+z+'_x2" value="'+zn.x2+'" placeholder="x2">'+
                '<input type="number" id="rf_'+z+'_y2" value="'+zn.y2+'" placeholder="y2">'+
                '</div>';
        }
        $('rf_zones_box').innerHTML=html;
    }).catch(()=>{});
}
function saveRegionFilter() {
    const body={mode:parseInt($('rf_mode').value)||0,zones:[]};
    for(let z=0;z<3;z++){
        body.zones.push({
            x1:parseInt($('rf_'+z+'_x1').value)||0,
            y1:parseInt($('rf_'+z+'_y1').value)||0,
            x2:parseInt($('rf_'+z+'_x2').value)||0,
            y2:parseInt($('rf_'+z+'_y2').value)||0
        });
    }
    fetch('/api/zones/region_filter',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})
        .then(r=>{ if(r.ok) showToast(t('rf_saved')); else showToast(t('err')); })
        .catch(()=>showToast(t('err')));
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
    window._curAlarmState=state;
    let b=$('alarm_badge'), btn=$('btn_arm');
    if(state==='disarmed'){b.innerText=t('disarmed');b.style.color='#888';btn.innerText=t('arm');btn.style.background='#b00020';}
    else if(state==='arming'){b.innerText=t('arming');b.style.color='orange';btn.innerText=t('cancel');btn.style.background='#3700b3';}
    else if(state==='armed_away'){b.innerText=t('armed');b.style.color='#00ff00';btn.innerText=t('disarm');btn.style.background='#3700b3';}
    else if(state==='pending'){b.innerText=t('pending');b.style.color='orange';btn.innerText=t('cancel');btn.style.background='#3700b3';}
    else if(state==='triggered'){b.innerText=t('triggered');b.style.color='red';btn.innerText=t('disarm');btn.style.background='#3700b3';}
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
    })}).then(()=>showToast(t("tripwire_saved"))).catch(()=>showToast(t("err")));
}

// --- ROZVRH ---
function loadSchedule() {
    fetch('/api/schedule').then(r=>r.json()).then(d=>{
        if(d.arm_time) $('i_sched_arm').value=d.arm_time;
        if(d.disarm_time) $('i_sched_disarm').value=d.disarm_time;
        $('i_auto_arm').value=d.auto_arm_minutes||0;
        $('i_night_start').value=d.night_start||'';
        $('i_night_end').value=d.night_end||'';
        window._curProfile=d.current_profile||'day';
        renderCurProfile();
    }).catch(()=>{});
}
function saveSchedule() {
    api('schedule',{method:'POST',body:new URLSearchParams({
        'arm_time':$('i_sched_arm').value,'disarm_time':$('i_sched_disarm').value,
        'auto_arm_minutes':$('i_auto_arm').value,
        'night_start':$('i_night_start').value,'night_end':$('i_night_end').value
    })}).then(()=>showToast(t("schedule_saved"))).catch(()=>showToast(t("err")));
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
        $('event_list').innerHTML=h||'<tr><td colspan="3" style="text-align:center;padding:10px;color:#666">'+t("no_events")+'</td></tr>';
    }).catch(()=>{});
}
function clearEvents() {
    if(confirm(t("confirm_clear_events"))) api('events/clear',{method:'POST'}).then(()=>loadEvents());
}

// --- NETWORK CONFIG ---
function saveMQTTConfig() {
    api('mqtt/config',{method:'POST',body:new URLSearchParams({
        'enabled':$('chk_mqtt_en').checked?1:0,
        'tls':$('chk_mqtt_tls').checked?1:0,
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
    .then(d=>showToast(d.success?t("telegram_ok"):t("err")+": "+(d.error||"?")))
    .catch(()=>showToast(t("err")));
}
function saveAuth() {
    let p=$('txt_auth_pass').value,p2=$('txt_auth_pass2').value;
    if(!$('txt_auth_user').value||!p){showToast(t("fill_fields"));return;}
    if(p!==p2){showToast(t("pass_mismatch"));return;}
    api('auth/config',{method:'POST',body:new URLSearchParams({'user':$('txt_auth_user').value,'pass':p})})
    .then(r=>{if(r.ok) showToast(t("creds_saved"));}).catch(()=>showToast(t("err")));
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
    fetch('/api/config',{method:'POST',body:fd}).then(()=>showToast(t("saved")));
}
function noiseCmd(cmd) {
    fetch('/api/noise/'+cmd,{method:'POST'}).then(r=>r.text().then(t=>showToast(t)));
}

function radarBT(en) {
    if(!confirm(en?t("confirm_enable_ble"):t("confirm_disable_ble"))) return;
    let fd=new FormData(); fd.append('enabled', en?'1':'0');
    fetch('/api/radar/bluetooth',{method:'POST',body:fd}).then(r=>r.text().then(txt=>showToast(txt)));
}

// --- ZONES ---
function toggleDrawMode() {
    drawMode=!drawMode;
    let btn=$('btnDrawMode');
    if(drawMode) {
        btn.innerText=t('draw_on'); btn.style.background='#00ff00';
        $('drawModeHelp').style.display='block'; $('radarMap').style.cursor='crosshair';
    } else {
        btn.innerText=t('draw_off'); btn.style.background='#ff6600';
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
        if(Math.abs(xmax-xmin)<200||Math.abs(ymax-ymin)<200){showToast(t("min_zone_size"));drawStart=null;return;}
        let label=prompt(t("prompt_name"),t("default_blackout_name"))||t("default_blackout_name");
        addBlackoutZone(xmin,xmax,ymin,ymax,label); drawStart=null;
    };
}
function addBlackoutZone(xmin,xmax,ymin,ymax,label) {
    let fd=new FormData();
    fd.append("xmin",xmin);fd.append("xmax",xmax);fd.append("ymin",ymin);fd.append("ymax",ymax);
    fd.append("label",label);fd.append("enabled","true");
    fetch('/api/blackout/add',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast(t("zone_added")); else showToast(t("err"));});
}
function deleteZone(id) {
    if(!confirm(t("confirm_delete"))) return;
    let fd=new FormData(); fd.append("id",id);
    fetch('/api/blackout/delete',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast(t("deleted"));});
}
function toggleZone(id) {
    const zone=blackoutZones[id]; if(!zone) return;
    let fd=new FormData(); fd.append("id",id); fd.append("enabled",(!zone.enabled).toString());
    fetch('/api/blackout/update',{method:'POST',body:fd}).then(r=>{if(r.ok) showToast(zone.enabled?t("disabled"):t("enabled"));});
}
function updateBlackoutList() {
    const list=$('blackoutList');
    if(!blackoutZones||blackoutZones.length===0){list.innerHTML='<div style="color:#555;font-size:0.8rem">'+t("no_blackout")+'</div>';return;}
    let h='';
    blackoutZones.forEach((z,i)=>{
        const m=(z.mask===undefined)?3:z.mask;
        const dayChk=(m&1)?'checked':'';
        const nightChk=(m&2)?'checked':'';
        h+='<div style="background:#222;padding:6px;margin:4px 0;border-radius:5px;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:4px">'+
        '<div><div style="font-weight:bold;color:'+(z.enabled?'#ff0000':'#555')+'">'+z.label+'</div>'+
        '<div style="font-size:0.65rem;color:#666">X:['+z.xmin+','+z.xmax+'] Y:['+z.ymin+','+z.ymax+']</div></div>'+
        '<div style="display:flex;align-items:center;gap:2px">'+
        '<label style="font-size:0.7rem;margin-left:6px;display:inline-flex;align-items:center;gap:2px;width:auto"><input type="checkbox" data-mask-bit="1" '+dayChk+' onchange="setBzMask('+i+')" style="width:auto;margin:0">\u{2600}\u{FE0F}</label>'+
        '<label style="font-size:0.7rem;margin-left:4px;display:inline-flex;align-items:center;gap:2px;width:auto"><input type="checkbox" data-mask-bit="2" '+nightChk+' onchange="setBzMask('+i+')" style="width:auto;margin:0">\u{1F319}</label>'+
        '<button onclick="toggleZone('+i+')" style="padding:4px 8px;font-size:0.7rem;background:'+(z.enabled?'#666':'#00aa00')+';width:auto;margin:0 2px">'+(z.enabled?'OFF':'ON')+'</button>'+
        '<button onclick="deleteZone('+i+')" style="padding:4px 8px;font-size:0.7rem;background:#cc0000;width:auto;margin:0">X</button></div></div>';
    });
    list.innerHTML=h;
}
function setBzMask(id) {
    const cbs=document.querySelectorAll('#blackoutList input[onchange="setBzMask('+id+')"]');
    let m=0;
    cbs.forEach(cb=>{ if(cb.checked) m|=parseInt(cb.dataset.maskBit); });
    if(m===0) m=3;
    const fd=new URLSearchParams(); fd.append('id',id); fd.append('mask',m);
    fetch('/api/blackout/update',{method:'POST',body:fd}).then(r=>{ if(!r.ok) showToast(t('err')); });
}
function addCurrentToPoly(id) {
    let fd=new FormData(); fd.append("id",id);
    fetch('/api/polygon/add_current',{method:'POST',body:fd}).then(r=>r.text().then(t=>showToast(t)));
}
function clearPoly(id) {
    if(!confirm(t("confirm_clear_poly")+" "+(id+1)+"?")) return;
    let fd=new FormData(); fd.append("id",id); fd.append("points","");
    fetch('/api/polygon/set',{method:'POST',body:fd}).then(()=>showToast(t("cleared")));
}
function showAddZoneDialog() {
    let xmin=prompt(t("prompt_x_min"),"-1000"),xmax=prompt(t("prompt_x_max"),"1000");
    let ymin=prompt(t("prompt_y_min"),"0"),ymax=prompt(t("prompt_y_max"),"2000");
    let label=prompt(t("prompt_name"),t("default_blackout_name"));
    if(xmin&&xmax&&ymin&&ymax) addBlackoutZone(parseInt(xmin),parseInt(xmax),parseInt(ymin),parseInt(ymax),label||t("default_blackout_name"));
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
    xhr.onload=()=>{$('ota_status').innerText=t("ota_done");setTimeout(()=>location.reload(),30000);};
    xhr.onerror=()=>{$('ota_status').innerText=t("ota_error");$('ota_bar').style.background="#cc0000";};
    xhr.send(fd);
}
function exportConfig(){window.location.href="/api/config/export";}
function importConfig() {
    const f=$('importFile').files[0]; if(!f) return;
    if(!confirm(t("confirm_overwrite"))){$('importFile').value='';return;}
    let fd=new FormData(); fd.append("config",f);
    showToast(t("uploading"));
    fetch('/api/config/import',{method:'POST',body:fd}).then(r=>r.text()).then(t=>{
        showToast(t); if(t.includes("Restart")) setTimeout(()=>location.reload(),5000);
    }).catch(e=>showToast(t("err")+": "+e));
}
function sendCmd(url){fetch(url,{method:'POST'}).then(r=>showToast(r.ok?t("ok"):t("err")));}

window.onload=()=>{applyLang();init();setupCanvasDrawing();};
</script>
</body>
</html>
)rawliteral";
