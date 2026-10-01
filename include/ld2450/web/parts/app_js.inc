R"WEBJS(let LANG = localStorage.getItem('lang') || 'cs';
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
)WEBJS"
