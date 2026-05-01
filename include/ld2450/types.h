#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "utils/EKF2D.h"  // 2D Extended Kalman Filter [x,y,vx,vy]

// Forward declarations
class MQTTService;
class LD2450Service;
class ConfigManager;
class SecurityMonitor;
class EventLog;
class TelegramService;
class BluetoothService;

// --- CONSTANTS ---
#define MAX_BLACKOUT_ZONES 5
#define MAX_POLYGONS 5
#define MAX_POLY_POINTS 8
#define GRID_SIZE 80

// --- ENUMS ---
enum class PresenceState {
  IDLE,
  PRESENCE_DETECTED,
  HOLD_TIMEOUT
};

// Security Alarm State Machine (ported from LD2412)
enum class SecurityState {
  DISARMED,
  ARMING,      // Exit delay countdown
  ARMED,       // Watching for intrusion
  PENDING,     // Entry delay countdown
  TRIGGERED    // Alarm active
};

// Event Types for EventLog
enum EventType {
  EVT_SYSTEM = 0,
  EVT_PRESENCE = 1,
  EVT_TAMPER = 2,
  EVT_WIFI = 3,
  EVT_HEARTBEAT = 4,
  EVT_SECURITY = 5
};

// --- LD2450 Target ---
struct LD2450Target {
    int16_t x;          // X coordinate (mm)
    int16_t y;          // Y coordinate (mm)
    int16_t speed;      // Speed (cm/s)
    uint16_t resolution; // Resolution/Confidence
    bool valid;
};

// --- STRUCTS ---
struct Topics {
  char availability[64];
  char boot[64];
  char rssi[64];
  char uptime[64];
  char heap[64];
  char max_alloc[64];
  char ip[64];
  char reason[64];
  char health[64];
  char tamper[64];
  char netquality[64];
  
  // Application Topics
  char presence_state[64];
  char tracking_count[64];
  char target_x[3][64]; // Array for 3 targets
  char target_y[3][64];
  char notification[64];

  // Security Topics
  char alarm_state[64];
  char alarm_command[64];

  // Analytics
  char entry_count[64];
  char exit_count[64];

  // Identity
  char radar_type[64];
};

struct TelemetryState {

  uint8_t targetCount = 0;
  long rssi = 0;
  unsigned long lastPush = 0;
};

struct NetworkQuality {
  int32_t rssiBaseline = 0;
  uint32_t mqttReconnects = 0;
  uint32_t wifiReconnects = 0;
  unsigned long lastDisconnect = 0;
  unsigned long lastHealthCheck = 0;
};

struct TamperState {
  uint8_t lastValidCount = 0;
  unsigned long lastTargetSeen = 0;
  bool tamperDetected = false;
  char tamperReason[64] = "";
};

struct ZoneConfig {
  int16_t xMin = -4000;
  int16_t xMax = 4000;
  int16_t yMin = 0;
  int16_t yMax = 8000;
  uint16_t minRes = 100; // Default
  uint16_t staticResThreshold = 100; // Lowered for sitting people (was 300)
  uint32_t ghostTimeout = 120000; // 2 minutes timeout for static targets
  uint16_t moveThreshold = 5;
  uint16_t posThreshold = 50;
  uint16_t persistenceMs = 4000; // 4s memory for lost targets
  int16_t mapRotation = 0; // 0, 90, 180, 270
};

struct BlackoutZone {
  int16_t xMin, xMax, yMin, yMax;
  bool enabled;
  char label[32];
};

struct Point {
  int16_t x;
  int16_t y;
};

struct PolygonZone {
  Point points[MAX_POLY_POINTS];
  uint8_t pointCount;
  bool enabled;
  char label[32];
};

struct NoiseMap {
    uint16_t energy[GRID_SIZE][GRID_SIZE];
    bool active = false;
    bool learning = false;
    bool pending = false;
    unsigned long learningStartTs = 0;
    unsigned long startTime = 0;
    uint32_t samples = 0;
};

struct GhostTracker {
    int16_t lastX[3] = {0};
    int16_t lastY[3] = {0};
    unsigned long staticSince[3] = {0}; // When target became static
    bool isGhost[3] = {false};
    bool inZone[3] = {false};           // Hysteresis: true once target enters zone
};

struct TargetHistory {
    float smoothX[3] = {0};
    float smoothY[3] = {0};
    bool wasValid[3] = {false};
    unsigned long lastSeen[3] = {0};   // For persistence
    int16_t lastSentX[3] = {-9999, -9999, -9999};
    int16_t lastSentY[3] = {-9999, -9999, -9999};
    
    // Variance Analysis
    int16_t posXHistory[3][10] = {}; // Ring buffer for last 10 X positions
    int16_t posYHistory[3][10] = {}; // Ring buffer for last 10 Y positions
    uint8_t historyIdx[3] = {0};
    uint8_t varSamples[3] = {0}; // Skutečný počet záznamů, dokud nedosáhne 10 (cold-start fix)
    float variance[3] = {0};    // Calculated motion variance
    
    // EKF Trackers (one per target slot, state: [x,y,vx,vy])
    EKF2D* ekf[3] = {nullptr, nullptr, nullptr};
};

// Movement classification
enum class MoveClass : uint8_t { NONE=0, STANDING=1, WALKING=2, RUNNING=3 };
inline const char* moveClassStr(MoveClass c) {
    switch(c) { case MoveClass::STANDING: return "standing"; case MoveClass::WALKING: return "walking"; case MoveClass::RUNNING: return "running"; default: return "none"; }
}

// Tripwire (virtual line for entry/exit counting)
struct Tripwire {
    int16_t y = 3000;           // Y coordinate of the horizontal line (mm)
    bool enabled = false;
    int32_t entryCount = 0;     // Crossed from far→near (y decreasing past line)
    int32_t exitCount = 0;      // Crossed from near→far (y increasing past line)
    int8_t lastSide[3] = {0};   // -1=near, +1=far, 0=unknown per target
};

// Per-target security analytics
struct TargetAnalytics {
    MoveClass moveClass[3] = {MoveClass::NONE, MoveClass::NONE, MoveClass::NONE};
    unsigned long dwellStart[3] = {0};  // When target entered polygon zone
    uint32_t dwellMs[3] = {0};         // Accumulated dwell time in zone
    bool inPolyZone[3] = {false};
};

struct AdaptiveConfig {
    bool enabled = true;         // Master switch
    uint16_t decayRate = 5;      // Energy units to decay per interval
    uint32_t decayInterval = 60000; // 1 minute
    unsigned long lastDecay = 0;
    
    float humanVarianceLimit = 5.0; // Minimum variance to be considered "alive" (human)
    float ghostVarianceLimit = 2.0; // Below this is likely a static object
};

// --- APP CONTEXT ---
// Dependency Injection Container
struct AppContext {
    Preferences* preferences;
    ConfigManager* config;
    MQTTService* mqtt;
    LD2450Service* radar;
    SecurityMonitor* security;
    EventLog* eventLog;
    TelegramService* telegram;
    BluetoothService* bluetooth;

    // State Objects
    ZoneConfig* zoneConfig;
    AdaptiveConfig* adaptiveConfig;
    NoiseMap* noiseMap;
    TargetHistory* targetHistory;
    GhostTracker* ghostTracker;
    TamperState* tamperState;
    NetworkQuality* netQuality;
    Tripwire* tripwire;
    TargetAnalytics* analytics;

    // Arrays (pointers to first element)
    BlackoutZone* blackoutZones;
    uint8_t* blackoutZoneCount;

    PolygonZone* polygons;
    uint8_t* polyCount;

    // Day/Night profile masks (parallel arrays — bit0=day, bit1=night, default 0x03 = both)
    uint8_t* polygonMasks;
    uint8_t* blackoutMasks;
    uint8_t* currentProfile;  // Active profile bitmask (0x01=day, 0x02=night)

    // Config vars (pointers)
    char* deviceId;
    char* deviceHostname;
    char* authUser;
    char* authPass;
    bool* useNoiseFilter;

    // API Helpers
    void (*saveBlackoutZones)();
    void (*savePolygons)();
    void (*saveNoiseMap)();
    volatile bool* shouldReboot = nullptr;

    // Thread Safety
    SemaphoreHandle_t dataMutex;
};

// --- TOPIC CONSTANTS ---

