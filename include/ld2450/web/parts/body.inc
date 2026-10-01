R"WEBBODY(<body>

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

)WEBBODY"
