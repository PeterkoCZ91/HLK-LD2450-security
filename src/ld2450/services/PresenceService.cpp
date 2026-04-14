#include "ld2450/services/PresenceService.h"
#include "ld2450/utils/TargetAssociation.h"

extern void safeRestart(const char* reason);

PresenceService::PresenceService() {
}

void PresenceService::begin(AppContext* ctx) {
    _ctx = ctx;
    pinMode(PANIC_LED_PIN, OUTPUT);
    digitalWrite(PANIC_LED_PIN, LOW);
    
    // Load persisted data
    loadNoiseMap();
    loadBlackoutZones();
    
    // Initialize NetQuality baseline
    _ctx->netQuality->rssiBaseline = WiFi.RSSI();
    
    // Initialize Watchdog timer
    _bootTime = millis();
    _lastMqttSuccess = _bootTime;

    // Load DMS restart count (reset on non-DMS boot)
    Preferences p;
    p.begin("ld2450_sys", true);
    String lastReason = p.getString("rst_reason", "");
    p.end();
    if (lastReason == "dms_no_publish") {
        p.begin("ld2450_sys", false);
        _dmsRestartCount = p.getUChar("dms_count", 0) + 1;
        p.putUChar("dms_count", _dmsRestartCount);
        p.end();
        Serial.printf("[DMS] Consecutive DMS restart #%d\n", _dmsRestartCount);
    } else {
        p.begin("ld2450_sys", false);
        p.putUChar("dms_count", 0);
        p.end();
        _dmsRestartCount = 0;
    }
    
    // Initialize EKF trackers (state: [x, y, vx, vy])
    for(int i=0; i<3; i++) {
        if(_ctx->targetHistory->ekf[i] == nullptr) {
            _ctx->targetHistory->ekf[i] = new EKF2D();
        }
    }
}

void PresenceService::update() {
    unsigned long now = millis();
    
    _ctx->radar->update();
    
    processNoiseLearning(now);
    checkWiFi(now);
    
    if (WiFi.status() == WL_CONNECTED) {
        _ctx->mqtt->update();
    }
    
    // Periodic checks
    if (now - _ctx->netQuality->lastHealthCheck > HEALTH_CHECK_INTERVAL) {
        performHealthCheck(now);
    }
    
    checkMqttWatchdog(now);
    checkRSSIAnomaly(now);
    updateAdaptiveFilter(now);
    
    // Main processing loop (10Hz limit)
    if (now - _lastTelemetryTime > 100) {
        processRadarData(now);
        _lastTelemetryTime = now;
    }
}

// --- Helpers ---
int PresenceService::toGridX(int16_t x) { 
    int idx = (x + GRID_OFFSET) / GRID_CELL_SIZE; 
    return (idx < 0) ? 0 : (idx >= GRID_SIZE ? GRID_SIZE-1 : idx);
}
int PresenceService::toGridY(int16_t y) { 
    int idx = y / GRID_CELL_SIZE; 
    return (idx < 0) ? 0 : (idx >= GRID_SIZE ? GRID_SIZE-1 : idx);
}

bool PresenceService::isPointInPolygon(int16_t x, int16_t y, const PolygonZone& poly) {
    if (!poly.enabled || poly.pointCount < 3) return false;
    bool inside = false;
    for (int i = 0, j = poly.pointCount - 1; i < poly.pointCount; j = i++) {
        if (((poly.points[i].y > y) != (poly.points[j].y > y)) &&
            (x < (poly.points[j].x - poly.points[i].x) * (y - poly.points[i].y) / 
             (poly.points[j].y - poly.points[i].y) + poly.points[i].x)) {
            inside = !inside;
        }
    }
    return inside;
}

void PresenceService::applyRotation(int16_t& x, int16_t& y) {
    int16_t rot = _ctx->zoneConfig->mapRotation;
    if (rot == 0) return;
    int16_t ox = x, oy = y;
    switch (rot) {
        case 90:  x = oy;  y = -ox; break;
        case 180: x = -ox; y = -oy; break;
        case 270: x = -oy; y = ox;  break;
        default: break;
    }
}

bool PresenceService::isInBlackoutZone(int16_t x, int16_t y) {
    for (uint8_t i = 0; i < *(_ctx->blackoutZoneCount); i++) {
        if (!_ctx->blackoutZones[i].enabled) continue;
        if (x >= _ctx->blackoutZones[i].xMin && x <= _ctx->blackoutZones[i].xMax &&
            y >= _ctx->blackoutZones[i].yMin && y <= _ctx->blackoutZones[i].yMax) {
            return true;
        }
    }
    return false;
}

// --- Logic Blocks ---

void PresenceService::processNoiseLearning(unsigned long now) {
    if (_ctx->noiseMap && _ctx->noiseMap->pending) {
        if (now >= _ctx->noiseMap->learningStartTs) {
            _ctx->noiseMap->pending = false;
            _ctx->noiseMap->learning = true;
            _ctx->noiseMap->startTime = now;
            _ctx->noiseMap->samples = 0;
            Serial.println("[NoiseMap] 🚀 Learning STARTED!");
            if (_ctx->mqtt->connected()) {
                _ctx->mqtt->publish(_ctx->mqtt->getTopics().notification, "🤖 AI Learning STARTED. Do not move!", false);
            }
        }
    }
}

void PresenceService::checkWiFi(unsigned long now) {
    if (now - _lastWiFiCheckTime > WIFI_RECONNECT_INTERVAL) {
        _lastWiFiCheckTime = now;
        if (WiFi.status() != WL_CONNECTED) {
            _ctx->netQuality->wifiReconnects++;
            WiFi.reconnect();

            // Emergency BLE: re-activate after 2 min offline for WiFi recovery
            if (_wifiLostSince == 0) _wifiLostSince = now;
            if (now - _wifiLostSince > 120000 && _ctx->bluetooth && !_ctx->bluetooth->isRunning()) {
                _ctx->bluetooth->startEmergency();
            }
        } else {
            _wifiLostSince = 0;
        }
    }
}

void PresenceService::performHealthCheck(unsigned long now) {
    Serial.println("[HEALTH] Performing health check...");
    bool radarOk = true; 
    bool wifiOk = (WiFi.status() == WL_CONNECTED && WiFi.RSSI() > -90);
    bool heapOk = (ESP.getFreeHeap() > 50000);
    bool mqttOk = _ctx->mqtt->connected();

    static bool certChecked = false;
    static unsigned long lastCertCheck = 0;
    if (!certChecked || (now - lastCertCheck > 86400000)) {
        _ctx->mqtt->checkCertificateExpiry();
        certChecked = true;
        lastCertCheck = now;
    }

    String healthStatus = "OK";
    if (!radarOk) healthStatus = "RADAR_FAIL";
    else if (!wifiOk) healthStatus = "WIFI_WEAK";
    else if (!heapOk) healthStatus = "LOW_MEMORY";
    else if (!mqttOk) healthStatus = "MQTT_DISCONNECTED";

    if (_ctx->mqtt->connected()) {
        JsonDocument doc;
        doc["status"] = healthStatus;
        doc["radar_ok"] = radarOk;
        doc["wifi_rssi"] = WiFi.RSSI();
        doc["wifi_ok"] = wifiOk;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["max_alloc_heap"] = ESP.getMaxAllocHeap();
        doc["heap_ok"] = heapOk;
        doc["mqtt_ok"] = mqttOk;
        doc["mqtt_reconnects"] = _ctx->netQuality->mqttReconnects;
        doc["wifi_reconnects"] = _ctx->netQuality->wifiReconnects;

        String payload;
        serializeJson(doc, payload);
        _ctx->mqtt->publish(_ctx->mqtt->getTopics().health, payload.c_str(), false);
    }
    _ctx->netQuality->lastHealthCheck = now;
}

void PresenceService::checkMqttWatchdog(unsigned long now) {
    // Only enforce watchdog if MQTT was ever connected (skip in LAB mode without broker)
    if (_ctx->mqtt->connected()) {
        _lastMqttSuccess = now;
        _mqttEverConnected = true;

        // Dead Man's Switch: broker connected but publish silently failing
        if ((now - _bootTime) > TIMEOUT_DMS_STARTUP_MS) { // Grace period after boot
            unsigned long lastPub = _ctx->mqtt->getLastSuccessfulPublish();
            if (lastPub > 0 && (now - lastPub > TIMEOUT_DMS_NO_PUBLISH_MS)) {
                if (_dmsRestartCount < DMS_MAX_RESTARTS) {
                    _dmsRestartCount++;
                    Serial.printf("[DMS] No successful publish for %lu s. Restart #%d\n",
                                  (now - lastPub) / 1000, _dmsRestartCount);
                    safeRestart("dms_no_publish");
                } else {
                    // Degraded mode — stop restarting, just log
                    static bool dmsWarned = false;
                    if (!dmsWarned) {
                        Serial.println("[DMS] Max restarts reached. Degraded mode (local only).");
                        dmsWarned = true;
                    }
                }
            }
        }
    } else if (_mqttEverConnected) {
        if (now - _lastMqttSuccess > 300000) { // 5 minutes
            Serial.println("[Watchdog] MQTT Disconnected for > 5 min. Restarting system...");
            delay(100);
            safeRestart("mqtt_watchdog");
        }
    }
}

void PresenceService::checkRSSIAnomaly(unsigned long now) {
    if (now - _lastRSSICheck > 60000) {
        int32_t currentRSSI = WiFi.RSSI();
        if (_ctx->netQuality->rssiBaseline == 0) {
            _ctx->netQuality->rssiBaseline = currentRSSI;
            return;
        }
        // EMA
        _ctx->netQuality->rssiBaseline = (_ctx->netQuality->rssiBaseline * 9 + currentRSSI) / 10;
        
        int32_t rssiDelta = abs(currentRSSI - _ctx->netQuality->rssiBaseline);
        if (rssiDelta > 20 && !_ctx->tamperState->tamperDetected) {
            _ctx->tamperState->tamperDetected = true;
            strncpy(_ctx->tamperState->tamperReason, "RSSI_ANOMALY_DETECTED", sizeof(_ctx->tamperState->tamperReason) - 1);
             if (_ctx->mqtt->connected()) {
                _ctx->mqtt->publish(_ctx->mqtt->getTopics().tamper, "RSSI_ANOMALY_DETECTED", false);
            }
        }
        _lastRSSICheck = now;
    }
}

void PresenceService::processRadarData(unsigned long now) {
    uint8_t validCount = 0;
    bool currentTargetValid[3] = {false};

    // === TARGET ASSOCIATION ===
    // Read raw detections and match to existing tracks by proximity
    LD2450Target rawDet[3];
    float detX[3], detY[3];
    bool detValid[3];
    for (int i = 0; i < 3; i++) {
        rawDet[i] = _ctx->radar->getTarget(i);
        if (rawDet[i].valid) applyRotation(rawDet[i].x, rawDet[i].y);
        detX[i] = rawDet[i].x;
        detY[i] = rawDet[i].y;
        detValid[i] = rawDet[i].valid;
    }

    float trkX[3], trkY[3];
    bool trkValid[3];
    TargetHistory* th = _ctx->targetHistory;
    for (int i = 0; i < 3; i++) {
        trkX[i] = th->smoothX[i];
        trkY[i] = th->smoothY[i];
        trkValid[i] = th->wasValid[i] && (now - th->lastSeen[i] < 2000);
    }

    uint8_t mapping[3];
    associateTargets(detX, detY, detValid, trkX, trkY, trkValid, mapping);

    // Build associated targets: slot i gets detection mapping[i]
    LD2450Target assocTargets[3];
    for (int i = 0; i < 3; i++) {
        if (mapping[i] != NO_MATCH) {
            assocTargets[i] = rawDet[mapping[i]];
        } else {
            assocTargets[i] = {0, 0, 0, 0, false};
        }
    }

    // === PROCESS EACH TRACK SLOT ===
    for(int i=0; i<3; i++) {
        LD2450Target t = assocTargets[i];
        GhostTracker* gt = _ctx->ghostTracker;
        ZoneConfig* zc = _ctx->zoneConfig;

        // 1. Basic Validation
        if (!t.valid) {
            gt->staticSince[i] = 0;
            gt->isGhost[i] = false;
            gt->inZone[i] = false;
            continue;
        }

        // 2. Zone Filtering with Hysteresis
        // Once target enters zone, it stays "in zone" until it moves ZONE_HYSTERESIS_MM outside
        {
            int16_t margin = gt->inZone[i] ? ZONE_HYSTERESIS_MM : 0;
            bool inRect = (t.x >= zc->xMin - margin && t.x <= zc->xMax + margin &&
                           t.y >= zc->yMin - margin && t.y <= zc->yMax + margin);
            if (!inRect) {
                gt->isGhost[i] = true;
                gt->inZone[i] = false;
                continue;
            }

            // Polygon check with hysteresis (skip margin for polygons - complex geometry)
            if (*(_ctx->polyCount) > 0) {
                bool inPoly = false;
                for (uint8_t p = 0; p < *(_ctx->polyCount); p++) {
                    if (_ctx->polygons[p].enabled && isPointInPolygon(t.x, t.y, _ctx->polygons[p])) {
                        inPoly = true;
                        break;
                    }
                }
                // If already in zone (hysteresis), allow being slightly outside polygon
                if (!inPoly && !gt->inZone[i]) {
                    gt->isGhost[i] = true;
                    continue;
                }
            }

            gt->inZone[i] = true; // Mark as entered zone
        }

        // 3. Blackout Zones
        if (isInBlackoutZone(t.x, t.y)) {
            gt->isGhost[i] = true;
            continue;
        }

        // 4. Pet Immunity
        if (t.resolution < zc->minRes) {
            continue;
        }

        // Noise Learning / Filter
        if (_ctx->noiseMap && _ctx->noiseMap->learning) {
            int gx = toGridX(t.x), gy = toGridY(t.y);
            if (t.resolution > _ctx->noiseMap->energy[gy][gx]) {
                _ctx->noiseMap->energy[gy][gx] = t.resolution;
            }
            _ctx->noiseMap->samples++;
            gt->isGhost[i] = true;
            continue;
        }

        if (*(_ctx->useNoiseFilter) && _ctx->noiseMap && _ctx->noiseMap->active) {
            int gx = toGridX(t.x), gy = toGridY(t.y);
            uint16_t learnedNoise = _ctx->noiseMap->energy[gy][gx];
            if (learnedNoise > 0 && t.resolution < (learnedNoise + NOISE_MARGIN_MM)) {
                gt->isGhost[i] = true;
                continue;
            }
        }

        // 5. Variance Analysis (Motion Entropy)
        // LOCK CRITICAL SECTION
        float variance = 0;
        if (xSemaphoreTake(_ctx->dataMutex, 50 / portTICK_PERIOD_MS) == pdTRUE) {
            updateVariance(i, t.x, t.y);
            variance = th->variance[i];
            xSemaphoreGive(_ctx->dataMutex);
        } else {
            variance = th->variance[i]; 
        }

        // 6. PRODUCTION GHOST LOGIC (Relaxed)
        // Only classify as ghost if:
        // - Very low energy (close to noise) AND
        // - Zero speed AND
        // - Very low variance (perfectly static)
        // - AND has been static for a long time
        
        bool isStatic = (abs(t.speed) < zc->moveThreshold) && (variance < _ctx->adaptiveConfig->ghostVarianceLimit);
        bool isLowEnergy = (t.resolution < zc->staticResThreshold);
        
        if (isStatic && isLowEnergy) {
            if (gt->staticSince[i] == 0) gt->staticSince[i] = now;
            
            if (now - gt->staticSince[i] > zc->ghostTimeout) {
                gt->isGhost[i] = true; // Timeout reached
            }
        } else {
            // It's alive!
            gt->staticSince[i] = 0;
            gt->isGhost[i] = false;
        }
        
        gt->lastX[i] = t.x;
        gt->lastY[i] = t.y;

        // 7. Valid Target Processing with EKF TRACKING & TELEPORT DETECTION
        if (!gt->isGhost[i]) {
            currentTargetValid[i] = true;

            if (xSemaphoreTake(_ctx->dataMutex, 50 / portTICK_PERIOD_MS) == pdTRUE) {
                th->lastSeen[i] = now;
                th->wasValid[i] = true;

                EKF2D* ekf = th->ekf[i];
                if (ekf) {
                    // Teleport detection: if jump > 1.5m, reset EKF
                    float distJump = 0;
                    if (ekf->isInitialized()) {
                        float dx = t.x - ekf->getX();
                        float dy = t.y - ekf->getY();
                        distJump = sqrtf(dx*dx + dy*dy);
                    }

                    if (distJump > 1500.0f) {
                        ekf->reset(t.x, t.y);
                        th->smoothX[i] = t.x;
                        th->smoothY[i] = t.y;
                    } else {
                        ekf->update(t.x, t.y, now);
                        th->smoothX[i] = ekf->getX();
                        th->smoothY[i] = ekf->getY();
                    }
                } else {
                    th->smoothX[i] = t.x;
                    th->smoothY[i] = t.y;
                }

                xSemaphoreGive(_ctx->dataMutex);
            }
            validCount++;

            // --- ANALYTICS ---
            TargetAnalytics* an = _ctx->analytics;
            if (an) {
                // Movement classification (speed in cm/s)
                int16_t spd = abs(t.speed);
                if (spd < 3)       an->moveClass[i] = MoveClass::STANDING;
                else if (spd < 80) an->moveClass[i] = MoveClass::WALKING;
                else               an->moveClass[i] = MoveClass::RUNNING;

                // Dwell time in polygon zones
                bool nowInPoly = false;
                for (uint8_t p = 0; p < *(_ctx->polyCount); p++) {
                    if (_ctx->polygons[p].enabled && isPointInPolygon(t.x, t.y, _ctx->polygons[p])) {
                        nowInPoly = true; break;
                    }
                }
                if (nowInPoly && !an->inPolyZone[i]) an->dwellStart[i] = now;
                if (nowInPoly) an->dwellMs[i] = now - an->dwellStart[i];
                else { an->dwellMs[i] = 0; an->dwellStart[i] = 0; }
                an->inPolyZone[i] = nowInPoly;
            }

            // Tripwire entry/exit
            Tripwire* tw = _ctx->tripwire;
            if (tw && tw->enabled) {
                int8_t side = (t.y > tw->y) ? 1 : -1;
                if (tw->lastSide[i] != 0 && tw->lastSide[i] != side) {
                    if (side < 0) tw->entryCount++;  // far→near = entry
                    else          tw->exitCount++;   // near→far = exit
                }
                tw->lastSide[i] = side;
            }
        } else {
            // Target is ghost — reset analytics
            if (_ctx->analytics) {
                _ctx->analytics->moveClass[i] = MoveClass::NONE;
                _ctx->analytics->dwellMs[i] = 0;
                _ctx->analytics->inPolyZone[i] = false;
            }
            if (_ctx->tripwire) _ctx->tripwire->lastSide[i] = 0;
        }
    }

    // Persistence (Keep targets alive briefly if lost)
    for (int i=0; i<3; i++) {
        if (!currentTargetValid[i] && _ctx->targetHistory->wasValid[i]) { 
            if (now - _ctx->targetHistory->lastSeen[i] < _ctx->zoneConfig->persistenceMs) {
                currentTargetValid[i] = true; 
                validCount++; // Count persisted target
            } else {
                // Target definitely lost/expired
                if (xSemaphoreTake(_ctx->dataMutex, 20 / portTICK_PERIOD_MS) == pdTRUE) {
                    _ctx->targetHistory->wasValid[i] = false;
                    xSemaphoreGive(_ctx->dataMutex);
                }
            }
        }
    }
    
    handleTamperDetection(validCount, currentTargetValid, now);
    updateStateMachine(validCount, now);
    publishTelemetry(validCount, currentTargetValid, false, now);

    // Feed SecurityMonitor with target data
    if (_ctx->security) {
        LD2450Target targets[3];
        for (int i = 0; i < 3; i++) {
            targets[i].valid = currentTargetValid[i];
            targets[i].x = _ctx->targetHistory->smoothX[i];
            targets[i].y = _ctx->targetHistory->smoothY[i];
            targets[i].speed = _ctx->radar->getTarget(i).speed;
            targets[i].resolution = _ctx->radar->getTarget(i).resolution;
        }
        _ctx->security->processTargets(validCount, targets);
        _ctx->security->checkRadarHealth(_ctx->radar->isConnected());
    }

    // Flush EventLog periodically
    if (_ctx->eventLog) {
        _ctx->eventLog->flush();
    }
}

void PresenceService::handleTamperDetection(uint8_t validCount, bool* currentTargetValid, unsigned long now) {
    TamperState* ts = _ctx->tamperState;
    
    for (int i=0; i<3; i++) {
        // Read SmoothX/Y and wasValid under lock
        float sx = 0, sy = 0;
        bool wasValid = false;
        
        if (xSemaphoreTake(_ctx->dataMutex, 20 / portTICK_PERIOD_MS) == pdTRUE) {
             sx = _ctx->targetHistory->smoothX[i];
             sy = _ctx->targetHistory->smoothY[i];
             wasValid = _ctx->targetHistory->wasValid[i];
             xSemaphoreGive(_ctx->dataMutex);
        } else {
             continue; // Skip check if busy to avoid stall
        }

        if (wasValid && !currentTargetValid[i]) {
            float lastDist = sqrt(pow(sx, 2) + pow(sy, 2));
            if (lastDist < TAMPER_DISTANCE_THRESHOLD_MM) {
                if (!ts->tamperDetected) {
                    ts->tamperDetected = true;
                    strncpy(ts->tamperReason, "BLOCKING_NEAR_SENSOR", 63);
                    if (_ctx->mqtt->connected()) _ctx->mqtt->publish(_ctx->mqtt->getTopics().tamper, "BLOCKING_NEAR_SENSOR", false);
                }
                ts->lastTargetSeen = now;
            }
        }
    }
    
    if (validCount > 0) {
        // Auto-clear tamper if valid targets visible for >5s
        if (ts->tamperDetected && (now - ts->lastTargetSeen > 5000)) {
            ts->tamperDetected = false;
             if (_ctx->mqtt->connected()) _ctx->mqtt->publish(_ctx->mqtt->getTopics().tamper, "OK", false);
        }
        ts->lastTargetSeen = now; // update AFTER tamper check
    } else if (ts->tamperDetected && (now - ts->lastTargetSeen > 10000)) {
        ts->tamperDetected = false;
        if (_ctx->mqtt->connected()) _ctx->mqtt->publish(_ctx->mqtt->getTopics().tamper, "OK", false);
    }
}

void PresenceService::updateStateMachine(uint8_t validCount, unsigned long now) {
    PresenceState newState = _currentState;
    switch (_currentState) {
        case PresenceState::IDLE:
            if (validCount > 0) newState = PresenceState::PRESENCE_DETECTED;
            break;
        case PresenceState::PRESENCE_DETECTED:
            if (validCount == 0) {
                newState = PresenceState::HOLD_TIMEOUT;
                _holdStartTime = now;
            }
            break;
        case PresenceState::HOLD_TIMEOUT:
            if (validCount > 0) newState = PresenceState::PRESENCE_DETECTED;
            else if (now - _holdStartTime > HOLD_TIMEOUT_MS) newState = PresenceState::IDLE;
            break;
        default: break;
    }
    
    // We update state here, but return changed flag for publishing?
    // Let's just update member and detect change in publishing
    bool changed = (newState != _currentState);
    _currentState = newState;
    
    if (changed) publishTelemetry(validCount, nullptr, true, now); // Just trigger invalidation
}

void PresenceService::publishTelemetry(uint8_t validCount, bool* currentTargetValid, bool stateChanged, unsigned long now) {
    
    bool forceUpdate = (now - _tCache.lastPush > HEARTBEAT_INTERVAL);
    
    if (!_ctx->mqtt->connected()) return;

    char buf[32]; // Static buffer for conversions

    // Always publish count when it changes
    bool countChanged = (validCount != _tCache.targetCount);

    if (stateChanged || forceUpdate) {
        const char* stateStr = "idle"; // Default
        const char* binSensor = "Clear";

        if (_currentState == PresenceState::PRESENCE_DETECTED) {
            stateStr = "detected";
            binSensor = "Detected";
        }
        else if (_currentState == PresenceState::HOLD_TIMEOUT) {
            stateStr = "hold";
            binSensor = "Detected"; // Keep binary sensor ON during hold
        }

        // Use dynamic topics
        _ctx->mqtt->publish(_ctx->mqtt->getTopics().presence_state, binSensor, true);

        if (forceUpdate) {
             snprintf(buf, sizeof(buf), "%ld", WiFi.RSSI());
             _ctx->mqtt->publish(_ctx->mqtt->getTopics().rssi, buf);

             snprintf(buf, sizeof(buf), "%lu", millis() / 1000);
             _ctx->mqtt->publish(_ctx->mqtt->getTopics().uptime, buf);

             snprintf(buf, sizeof(buf), "%u", ESP.getFreeHeap());
             _ctx->mqtt->publish(_ctx->mqtt->getTopics().heap, buf);

             snprintf(buf, sizeof(buf), "%u", ESP.getMaxAllocHeap());
             _ctx->mqtt->publish(_ctx->mqtt->getTopics().max_alloc, buf);
        }
        _tCache.lastPush = now;
    }

    // Publish count whenever it changes (independent of state change)
    if (countChanged || stateChanged || forceUpdate) {
        snprintf(buf, sizeof(buf), "%d", validCount);
        _ctx->mqtt->publish(_ctx->mqtt->getTopics().tracking_count, buf);
        _tCache.targetCount = validCount;
    }
    
    if(!currentTargetValid) return; 

    TargetHistory* th = _ctx->targetHistory;
    
    for(int i=0; i<3; i++) {
        const char* topicX = _ctx->mqtt->getTopics().target_x[i];
        const char* topicY = _ctx->mqtt->getTopics().target_y[i];

        if (currentTargetValid[i]) {
            // Read smoothed values protected by mutex?
            // Actually smoothX is updated in this thread just before.
            // But good practice if we want to be 100% atomic vs web.
            // However, here we are the WRITER. We don't need to lock against ourselves.
            // But we need to make sure we read consistent state if we were preempted?
            // No, FreeRTOS tasks. We are the task.
            
            int16_t curX = (int16_t)th->smoothX[i];
            int16_t curY = (int16_t)th->smoothY[i];
            
            bool posChanged = (abs(curX - th->lastSentX[i]) > MQTT_MOVE_THRESHOLD) ||
                              (abs(curY - th->lastSentY[i]) > MQTT_MOVE_THRESHOLD);
                              
            if (posChanged || forceUpdate) {
                 snprintf(buf, sizeof(buf), "%d", curX);
                 _ctx->mqtt->publish(topicX, buf);
                 
                 snprintf(buf, sizeof(buf), "%d", curY);
                 _ctx->mqtt->publish(topicY, buf);
                 
                 th->lastSentX[i] = curX;
                 th->lastSentY[i] = curY;
            }
        } else {
            if (th->wasValid[i] || forceUpdate) {
                  _ctx->mqtt->publish(topicX, "-9999");
                  _ctx->mqtt->publish(topicY, "-9999");
                  th->lastSentX[i] = -9999; 
                  th->lastSentY[i] = -9999;
            }
        }
        // wasValid is updated in processRadarData under mutex; don't write here
    }
}

// Persistence
void PresenceService::loadNoiseMap() {
    if (LittleFS.exists("/noisemap.bin")) {
        File f = LittleFS.open("/noisemap.bin", FILE_READ);
        if (f && f.size() == sizeof(_ctx->noiseMap->energy)) {
            f.read((uint8_t*)_ctx->noiseMap->energy, sizeof(_ctx->noiseMap->energy));
            f.close();
            _ctx->noiseMap->active = true;
            *(_ctx->useNoiseFilter) = true;
            Serial.println("[NoiseMap] Loaded from LittleFS");
        }
    }
}

void PresenceService::saveNoiseMap() {
    File f = LittleFS.open("/noisemap.bin", FILE_WRITE);
    if (f) {
        f.write((uint8_t*)_ctx->noiseMap->energy, sizeof(_ctx->noiseMap->energy));
        f.close();
        Serial.println("[NoiseMap] Saved");
    }
}

void PresenceService::loadBlackoutZones() {
  // Load from dedicated namespace (matching saveBlackoutZonesFn in main)
  Preferences p;
  p.begin("ld2450-zones", true);
  *(_ctx->blackoutZoneCount) = p.getUChar("bz_count", 0);
  if (*(_ctx->blackoutZoneCount) > MAX_BLACKOUT_ZONES) *(_ctx->blackoutZoneCount) = 0;
  if (*(_ctx->blackoutZoneCount) > 0) {
    p.getBytes("blackout", _ctx->blackoutZones, sizeof(BlackoutZone) * MAX_BLACKOUT_ZONES);
  }
  p.end();
}

// --- Adaptive Filtering ---

void PresenceService::updateVariance(int idx, int16_t x, int16_t y) {
    TargetHistory* th = _ctx->targetHistory;
    
    // Add to ring buffer
    th->posXHistory[idx][th->historyIdx[idx]] = x;
    th->posYHistory[idx][th->historyIdx[idx]] = y;
    th->historyIdx[idx] = (th->historyIdx[idx] + 1) % 10;
    
    // Calculate Mean
    float meanX = 0, meanY = 0;
    for(int k=0; k<10; k++) { 
        meanX += th->posXHistory[idx][k]; 
        meanY += th->posYHistory[idx][k]; 
    }
    meanX /= 10.0; 
    meanY /= 10.0;
    
    // Calculate Variance (Standard Deviation squared)
    float varSum = 0;
    for(int k=0; k<10; k++) { 
        varSum += (pow(th->posXHistory[idx][k] - meanX, 2) + pow(th->posYHistory[idx][k] - meanY, 2)); 
    }
    
    // Store as "Motion Entropy" or Variance
    // We normalize slightly to make numbers manageable
    th->variance[idx] = sqrt(varSum / 10.0); // Actually using Standard Deviation for intuitive mm scale
}

void PresenceService::updateAdaptiveFilter(unsigned long now) {
    if (!_ctx->adaptiveConfig || !_ctx->adaptiveConfig->enabled) return;
    
    // Decay Logic (Self-Healing)
    if (now - _ctx->adaptiveConfig->lastDecay > _ctx->adaptiveConfig->decayInterval) {
        _ctx->adaptiveConfig->lastDecay = now;
        
        if (!_ctx->noiseMap->active) return;
        
        bool changed = false;
        
        for (int y=0; y<GRID_SIZE; y++) {
            for (int x=0; x<GRID_SIZE; x++) {
                if (_ctx->noiseMap->energy[y][x] > 0) {
                    if (_ctx->noiseMap->energy[y][x] > _ctx->adaptiveConfig->decayRate) {
                        _ctx->noiseMap->energy[y][x] -= _ctx->adaptiveConfig->decayRate;
                    } else {
                        _ctx->noiseMap->energy[y][x] = 0;
                    }
                    changed = true;
                }
            }
        }
        
        if(changed) {
            Serial.println("[Adaptive] Noise Map Decayed (Self-Healing)");
        }
    }
}
