#ifndef TELEGRAM_SERVICE_H
#define TELEGRAM_SERVICE_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <AsyncTelegram2.h>
#include <Preferences.h>
#include "ld2450/services/LD2450Service.h"

class SecurityMonitor;

struct TelegramQueueItem {
    char text[256];
};

class TelegramService {
public:
    TelegramService();

    void begin(Preferences* prefs);
    void update();
    void setRadarService(LD2450Service* radar) { _radar = radar; }
    void setSecurityMonitor(SecurityMonitor* secMon) { _secMon = secMon; }
    void setRebootFlag(volatile bool* flag) { _shouldReboot = flag; }

    bool sendMessage(const String& text);
    bool sendAlert(const String& title, const String& details = "");

    void setEnabled(bool enabled);
    void setToken(const char* token);
    void setChatId(const char* chatId);

    bool isEnabled() const { return _enabled; }
    bool isConnected() const { return _connected; }

private:
    static void telegramTaskFunc(void* param);
    void telegramLoop();
    bool sendMessageDirect(const String& text);
    void handleNewMessages();
    void processCommand(const String& command, const String& chatId);

    WiFiClientSecure _client;
    AsyncTelegram2* _bot;
    Preferences* _prefs;
    LD2450Service* _radar = nullptr;
    SecurityMonitor* _secMon = nullptr;
    volatile bool* _shouldReboot = nullptr;

    char _token[64];
    char _chatId[24];
    bool _enabled;
    bool _connected;

    unsigned long _lastCheck;
    unsigned long _checkInterval;
    unsigned long _muteStartTime;
    unsigned long _muteDuration;

    QueueHandle_t _sendQueue = nullptr;
    TaskHandle_t _taskHandle = nullptr;
    static constexpr size_t QUEUE_SIZE = 16;
    uint32_t _droppedMessages = 0;

public:
    uint32_t getDroppedMessages() const { return _droppedMessages; }
};

#endif
