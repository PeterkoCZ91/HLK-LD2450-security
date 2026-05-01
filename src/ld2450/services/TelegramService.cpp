#include "services/TelegramService.h"
#include "services/SecurityMonitor.h"
#include "secrets.h"
#include <WiFi.h>
#include <inttypes.h>

TelegramService::TelegramService() : _bot(nullptr), _enabled(false), _connected(false),
                                      _lastCheck(0), _checkInterval(5000), _radar(nullptr),
                                      _muteStartTime(0), _muteDuration(0) {
    _chatId[0] = '\0';
}

void TelegramService::begin(Preferences* prefs) {
    _prefs = prefs;

    // Respektuj NVS — výchozí false, ale uživatel může v UI zapnout.
    _enabled = _prefs->getBool("tg_direct_en", false);
    String token = _prefs->getString("tg_token", TELEGRAM_TOKEN_DEFAULT);
    String chatId = _prefs->getString("tg_chat", TELEGRAM_CHAT_ID_DEFAULT);

    // Persist defaults from secrets.h to NVS
    if (strlen(TELEGRAM_TOKEN_DEFAULT) > 0) {
        if (!_prefs->isKey("tg_token")) {
            _prefs->putString("tg_token", token);
            _prefs->putString("tg_chat", chatId);
            Serial.println("[Telegram] NVS updated with compiled defaults");
        }
    }

    token.toCharArray(_token, sizeof(_token));
    chatId.toCharArray(_chatId, sizeof(_chatId));

    if (_enabled && strlen(_token) > 10 && strlen(_chatId) > 0) {
        _client.setInsecure();
        _bot = new AsyncTelegram2(_client);
        _bot->setUpdateTime(5000);
        _bot->setTelegramToken(_token);

        _sendQueue = xQueueCreate(QUEUE_SIZE, sizeof(TelegramQueueItem));
        if (_sendQueue) {
            // 16 KB stack — mbedtls SSL handshake si bere 8-10 KB, 8KB by overflowoval
            xTaskCreatePinnedToCore(telegramTaskFunc, "tg_task", 16384, this, 1, &_taskHandle, 0);
            Serial.println("[Telegram] Background task started");
        }

        Serial.println("[Telegram] Direct mode enabled");
        _connected = true;
    } else {
        Serial.println("[Telegram] Direct mode disabled");
    }
}

void TelegramService::telegramTaskFunc(void* param) {
    TelegramService* self = (TelegramService*)param;
    self->telegramLoop();
}

void TelegramService::telegramLoop() {
    TelegramQueueItem item;

    for (;;) {
        if (_sendQueue && xQueueReceive(_sendQueue, &item, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (WiFi.status() == WL_CONNECTED && _bot) {
                sendMessageDirect(String(item.text));
            }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (_enabled && _connected && _bot && WiFi.status() == WL_CONNECTED) {
            // SSL/TLS needs ~40KB free heap - skip if too low to avoid abort()
            if (ESP.getFreeHeap() < 45000) {
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
            unsigned long now = millis();
            if (now - _lastCheck > _checkInterval) {
                _lastCheck = now;
                handleNewMessages();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TelegramService::update() {
    // Background task handles everything
}

bool TelegramService::sendMessage(const String& text) {
    if (!_enabled || !_connected || !_sendQueue) return false;

    TelegramQueueItem item;
    strncpy(item.text, text.c_str(), sizeof(item.text) - 1);
    item.text[sizeof(item.text) - 1] = '\0';

    if (xQueueSend(_sendQueue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
        _droppedMessages++;
        return false;
    }
    return true;
}

bool TelegramService::sendMessageDirect(const String& text) {
    if (!_bot || strlen(_chatId) == 0) return false;
    if (ESP.getFreeHeap() < 45000) return false;

    int64_t chatIdNum = strtoll(_chatId, nullptr, 10);
    TBMessage msg;
    msg.chatId = chatIdNum;

    return _bot->sendMessage(msg, text);
}

bool TelegramService::sendAlert(const String& title, const String& details) {
    if (_muteDuration > 0 && (millis() - _muteStartTime) < _muteDuration) return false;

    String message = "* " + title + "*\n\n";
    if (details.length() > 0) message += details;
    message += "\n\n_" + String(millis() / 1000) + "s uptime_";

    return sendMessage(message);
}

void TelegramService::handleNewMessages() {
    if (!_bot) return;

    TBMessage msg;
    MessageType mt = _bot->getNewMessage(msg);
    if (mt != MessageNoData) {
        char chatIdBuf[21];
        snprintf(chatIdBuf, sizeof(chatIdBuf), "%" PRId64, msg.chatId);
        String text = msg.text;

        if (strcmp(chatIdBuf, _chatId) == 0) {
            processCommand(text, String(chatIdBuf));
        }
    }
}

void TelegramService::processCommand(const String& command, const String& chatId) {
    String cmd = command;
    int atPos = cmd.indexOf('@');
    if (atPos > 0) cmd = cmd.substring(0, atPos);

    if (cmd == "/start" || cmd == "/help") {
        sendMessage("*LD2450 Security Node*\n\n"
                     "/status - System status\n"
                     "/arm - Arm alarm (with exit delay)\n"
                     "/disarm - Disarm alarm\n"
                     "/arm_now - Immediate arm\n"
                     "/mute - Mute notifications for 10 min\n"
                     "/unmute - Re-enable notifications\n"
                     "/restart - Reboot device");
    }
    else if (cmd == "/arm") {
        if (_secMon) _secMon->setArmed(true, false);
        else sendMessage("SecurityMonitor unavailable");
    }
    else if (cmd == "/arm_now") {
        if (_secMon) _secMon->setArmed(true, true);
        else sendMessage("SecurityMonitor unavailable");
    }
    else if (cmd == "/disarm") {
        if (_secMon) _secMon->setArmed(false);
        else sendMessage("SecurityMonitor unavailable");
    }
    else if (cmd == "/status") {
        String msg = "*Status Report*\n\n";

        if (_secMon) {
            msg += "Alarm: " + String(_secMon->getAlarmStateStr()) + "\n\n";
        }

        if (_radar) {
            msg += "*Radar*\n";
            msg += "Targets: " + String(_radar->getTargetCount()) + "\n";
            msg += "Connected: " + String(_radar->isConnected() ? "Yes" : "No") + "\n\n";
        }

        msg += "*Network*\n";
        msg += "IP: " + WiFi.localIP().toString() + "\n";
        msg += "RSSI: " + String(WiFi.RSSI()) + " dBm\n\n";

        msg += "*System*\n";
        msg += "Uptime: " + String(millis() / 60000) + " min\n";
        msg += "Heap: " + String(ESP.getFreeHeap() / 1024) + " kB";

        sendMessage(msg);
    }
    else if (cmd == "/restart") {
        sendMessage("Restarting...");
        if (_shouldReboot) *_shouldReboot = true;
    }
    else if (cmd == "/mute") {
        _muteStartTime = millis();
        _muteDuration = 600000;
        sendMessage("Notifications muted for 10 minutes.");
    }
    else if (cmd == "/unmute") {
        _muteDuration = 0;
        sendMessage("Notifications re-enabled.");
    }
    else {
        sendMessage("Unknown command. Try /help");
    }
}

void TelegramService::setEnabled(bool enabled) {
    _enabled = enabled;
    _prefs->putBool("tg_direct_en", enabled);
}

void TelegramService::setToken(const char* token) {
    strncpy(_token, token, sizeof(_token) - 1);
    _token[sizeof(_token) - 1] = '\0';
    _prefs->putString("tg_token", token);
}

void TelegramService::setChatId(const char* chatId) {
    strncpy(_chatId, chatId, sizeof(_chatId) - 1);
    _chatId[sizeof(_chatId) - 1] = '\0';
    _prefs->putString("tg_chat", chatId);
}
