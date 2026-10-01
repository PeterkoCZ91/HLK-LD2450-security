#include "services/TelegramService.h"
#include "services/SecurityMonitor.h"
#include "secrets.h"
#include <WiFi.h>
#include <inttypes.h>
#include <time.h>

TelegramService::TelegramService() : _bot(nullptr), _enabled(false), _connected(false),
                                      _lastCheck(0), _checkInterval(5000), _radar(nullptr),
                                      _muteStartTime(0), _muteDuration(0) {
    _chatId[0] = '\0';
    _allowedSenderId[0] = '\0';
}

void TelegramService::begin(Preferences* prefs) {
    _prefs = prefs;

    // Respect NVS - default false, but the user can enable it in the UI.
    _enabled = _prefs->getBool("tg_direct_en", false);
    String token = _prefs->getString("tg_token", TELEGRAM_TOKEN_DEFAULT);
    String chatId = _prefs->getString("tg_chat", TELEGRAM_CHAT_ID_DEFAULT);
    String allowedSender = _prefs->getString("tg_sender", TELEGRAM_ALLOWED_SENDER_ID_DEFAULT);

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
    allowedSender.toCharArray(_allowedSenderId, sizeof(_allowedSenderId));

    if (_enabled && strlen(_token) > 10 && strlen(_chatId) > 0) {
        // Fail-closed TLS: verified certificate, no setInsecure().
        // Prefer a custom root from secrets.h (TELEGRAM_ROOT_CA), otherwise the library's
        // built-in root (Go Daddy Root CA G2 - api.telegram.org chains to it).
#ifdef TELEGRAM_ROOT_CA
        const char* caCert = TELEGRAM_ROOT_CA;
#else
        const char* caCert = telegram_cert;
#endif
        bool caValid = (caCert != nullptr) &&
                       (strstr(caCert, "BEGIN CERTIFICATE") != nullptr) &&
                       (strlen(caCert) > 200);
        if (!caValid) {
            // Without a trusted CA we do NOT connect - no setInsecure fallback.
            Serial.println("[Telegram] ERROR: CA cert missing/placeholder — refusing insecure connection (fail-closed)");
            snprintf(_lastStatus, sizeof(_lastStatus), "disabled: TLS CA unavailable");
            _connected = false;
            return;
        }
        _client.setCACert(caCert);
        _bot = new AsyncTelegram2(_client);
        _bot->setUpdateTime(5000);
        _bot->setTelegramToken(_token);

        _sendQueue = xQueueCreate(QUEUE_SIZE, sizeof(TelegramQueueItem));
        // 16 KB stack - the mbedtls SSL handshake needs 8-10 KB, 8 KB would overflow
        if (!_sendQueue || xTaskCreatePinnedToCore(telegramTaskFunc, "tg_task", 16384, this, 1, &_taskHandle, 0) != pdPASS) {
            // Without a queue/task sendMessage() would fill the queue forever and send nothing.
            if (_sendQueue) { vQueueDelete(_sendQueue); _sendQueue = nullptr; }
            snprintf(_lastStatus, sizeof(_lastStatus), "disabled: no task/heap");
            _connected = false;
            return;
        }
        Serial.println("[Telegram] Background task started");

        Serial.println("[Telegram] Direct mode enabled (verified TLS)");
        snprintf(_lastStatus, sizeof(_lastStatus), "enabled: verified TLS");
        _connected = true;
    } else {
        Serial.println("[Telegram] Direct mode disabled");
        snprintf(_lastStatus, sizeof(_lastStatus), "disabled");
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
                // Do not poll commands until NTP provides trusted time - otherwise the
                // TLS certificate cannot be verified and replay is possible. Retry next time.
                if (time(nullptr) > 1700000000) {
                    handleNewMessages();
                } else {
                    static bool warnedTime = false;
                    if (!warnedTime) {
                        Serial.println("[Telegram] Waiting for valid NTP time before polling commands");
                        warnedTime = true;
                    }
                }
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
    // Fail-closed: do not process commands without trusted time.
    if (time(nullptr) < 1700000000) return;

    TBMessage msg;
    MessageType mt = _bot->getNewMessage(msg);
    if (mt != MessageNoData) {
        char chatIdBuf[21];
        snprintf(chatIdBuf, sizeof(chatIdBuf), "%" PRId64, msg.chatId);
        char senderBuf[21];
        snprintf(senderBuf, sizeof(senderBuf), "%" PRId64, msg.sender.id);
        String text = msg.text;

        // Authorization must match both the chat and the specific sender (user id).
        // If no allowed sender is set, in a private chat chatId == sender.id,
        // so falling back to _chatId keeps single-user behaviour; for groups
        // it forces an explicit allowed sender (otherwise DENY).
        const char* allowedSender = (strlen(_allowedSenderId) > 0) ? _allowedSenderId : _chatId;
        bool chatOk = (strcmp(chatIdBuf, _chatId) == 0);
        bool senderOk = (msg.sender.id != 0) && (strcmp(senderBuf, allowedSender) == 0);

        if (chatOk && senderOk) {
            processCommand(text, String(chatIdBuf));
        } else {
            Serial.printf("[Telegram] Command rejected: unauthorized chat/sender (chat=%d sender=%d)\n",
                          (int)chatOk, (int)senderOk);
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
    if (!_prefs) return;
    _prefs->putBool("tg_direct_en", enabled);
}

void TelegramService::setToken(const char* token) {
    strncpy(_token, token, sizeof(_token) - 1);
    _token[sizeof(_token) - 1] = '\0';
    if (!_prefs) return;
    _prefs->putString("tg_token", token);
}

void TelegramService::setChatId(const char* chatId) {
    strncpy(_chatId, chatId, sizeof(_chatId) - 1);
    _chatId[sizeof(_chatId) - 1] = '\0';
    if (!_prefs) return;
    _prefs->putString("tg_chat", chatId);
}
