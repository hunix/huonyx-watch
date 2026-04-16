/**
 * Huonyx Gateway WebSocket Client Implementation
 * Implements the HoC Gateway Protocol for ESP32
 */

#include <Arduino.h>
#include "gateway_client.h"

GatewayClient* GatewayClient::_instance = nullptr;

GatewayClient::GatewayClient()
    : _state(GW_DISCONNECTED)
    , _port(GATEWAY_DEFAULT_PORT)
    , _useSSL(false)
    , _lastTickMs(0)
    , _reconnectTimer(0)
    , _msgIdCounter(0)
    , _onChatDelta(nullptr)
    , _onStateChange(nullptr)
    , _onSessionList(nullptr)
    , _onFlipperCommand(nullptr)
{
    memset(_host, 0, sizeof(_host));
    memset(_token, 0, sizeof(_token));
    memset(_currentSessionKey, 0, sizeof(_currentSessionKey));
    _instance = this;
}

void GatewayClient::begin(const char* host, uint16_t port, const char* token, bool useSSL) {
    strncpy(_host, host, sizeof(_host) - 1);
    _port = port;
    strncpy(_token, token, sizeof(_token) - 1);
    _useSSL = useSSL;

    /* Set default session key */
    if (_currentSessionKey[0] == '\0') {
        strncpy(_currentSessionKey, "esp32-watch", sizeof(_currentSessionKey) - 1);
    }

    setState(GW_CONNECTING);

    if (_useSSL) {
        _ws.beginSSL(_host, _port, "/");
    } else {
        _ws.begin(_host, _port, "/");
    }

    _ws.onEvent(wsEventCallback);
    _ws.setReconnectInterval(GATEWAY_RECONNECT_MS);
    _ws.enableHeartbeat(30000, 10000, 3);
}

void GatewayClient::loop() {
    _ws.loop();

    /* Check for tick timeout - gateway sends ticks every ~30s */
    if (_state == GW_AUTHENTICATED && _lastTickMs > 0) {
        if (millis() - _lastTickMs > GATEWAY_TICK_TIMEOUT_MS) {
            Serial.println("[GW] Tick timeout, reconnecting...");
            _ws.disconnect();
            setState(GW_CONNECTING);
            _lastTickMs = 0;
        }
    }
}

void GatewayClient::disconnect() {
    _ws.disconnect();
    setState(GW_DISCONNECTED);
}

void GatewayClient::setState(GatewayState s) {
    if (_state != s) {
        _state = s;
        if (_onStateChange) _onStateChange(s);
    }
}

const char* GatewayClient::generateId() {
    _msgIdCounter++;
    snprintf(_idBuf, sizeof(_idBuf), "esp32-%08lx-%04x",
             (unsigned long)millis(), (uint16_t)(_msgIdCounter & 0xFFFF));
    return _idBuf;
}

/* ── WebSocket Event Handler ──────────────────────────── */

void GatewayClient::wsEventCallback(WStype_t type, uint8_t* payload, size_t length) {
    if (!_instance) return;

    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[GW] Disconnected");
            _instance->setState(GW_CONNECTING);
            break;

        case WStype_CONNECTED:
            Serial.printf("[GW] Connected to %s\n", _instance->_host);
            _instance->setState(GW_CONNECTED);
            _instance->sendConnectFrame();
            break;

        case WStype_TEXT:
            _instance->handleMessage(payload, length);
            break;

        case WStype_ERROR:
            Serial.println("[GW] WebSocket error");
            _instance->setState(GW_ERROR);
            break;

        default:
            break;
    }
}

/* ── Protocol: Connect Frame ──────────────────────────── */

void GatewayClient::sendConnectFrame() {
    JsonDocument doc;
    doc["type"] = "req";
    const char* id = generateId();
    doc["id"] = id;
    doc["method"] = "connect";

    JsonObject params = doc["params"].to<JsonObject>();
    params["minProtocol"] = 1;
    params["maxProtocol"] = 1;

    JsonObject client = params["client"].to<JsonObject>();
    client["id"] = "huonyx-watch";
    client["displayName"] = "Huonyx Watch";
    client["version"] = FIRMWARE_VERSION;
    client["platform"] = "esp32-c3";
    client["deviceFamily"] = "smartwatch";
    client["mode"] = "chat";

    /* Auth with token */
    if (_token[0] != '\0') {
        JsonObject auth = params["auth"].to<JsonObject>();
        auth["token"] = _token;
    }

    JsonArray caps = params["caps"].to<JsonArray>();
    caps.add("chat");

    JsonArray scopes = params["scopes"].to<JsonArray>();
    scopes.add("operator.admin");

    params["role"] = "operator";

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
    Serial.println("[GW] Connect frame sent");
}

/* ── Message Handler ──────────────────────────────────── */

void GatewayClient::handleMessage(uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("[GW] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    if (strcmp(type, "res") == 0) {
        handleResponse(doc.as<JsonObjectConst>());
    } else if (strcmp(type, "event") == 0) {
        handleEvent(doc.as<JsonObjectConst>());
    }
}

void GatewayClient::handleResponse(JsonObjectConst doc) {
    bool ok = doc["ok"] | false;
    JsonObjectConst payload = doc["payload"];

    /* Check if this is a hello-ok response */
    const char* helloType = payload["type"] | "";
    if (strcmp(helloType, "hello-ok") == 0) {
        if (ok) {
            Serial.println("[GW] Authenticated successfully");
            _lastTickMs = millis();
            setState(GW_AUTHENTICATED);

            /* Extract server info */
            JsonObjectConst server = payload["server"];
            const char* version = server["version"] | "unknown";
            Serial.printf("[GW] Server version: %s\n", version);
        } else {
            Serial.println("[GW] Authentication failed");
            setState(GW_ERROR);
        }
        return;
    }

    /* Handle sessions.list response */
    if (payload.containsKey("sessions")) {
        JsonArrayConst sessions = payload["sessions"];
        if (_onSessionList) _onSessionList(sessions);
        return;
    }

    /* Handle chat.send accepted */
    const char* status = payload["status"] | "";
    if (strcmp(status, "accepted") == 0) {
        Serial.println("[GW] Chat message accepted");
    }
}

void GatewayClient::handleEvent(JsonObjectConst doc) {
    const char* event = doc["event"] | "";

    /* Tick event - heartbeat */
    if (strcmp(event, "tick") == 0) {
        _lastTickMs = millis();
        return;
    }

    /* Chat event */
    if (strcmp(event, "chat") == 0) {
        JsonObjectConst payload = doc["payload"];
        const char* runId = payload["runId"] | "";
        const char* state = payload["state"] | "";
        const char* sessionKey = payload["sessionKey"] | "";

        /* Only process events for our session */
        if (strlen(_currentSessionKey) > 0 && strcmp(sessionKey, _currentSessionKey) != 0) {
            return;
        }

        bool isFinal = (strcmp(state, "final") == 0);
        bool isDelta = (strcmp(state, "delta") == 0);
        bool isError = (strcmp(state, "error") == 0);

        if (isDelta || isFinal) {
            /* Extract message text - can be string or object with content */
            const char* text = "";
            JsonVariantConst msg = payload["message"];
            if (msg.is<const char*>()) {
                text = msg.as<const char*>();
            } else if (msg.is<JsonObjectConst>()) {
                text = msg["content"] | msg["text"] | "";
            }

            if (_onChatDelta && strlen(text) > 0) {
                _onChatDelta(runId, text, isFinal);
            }
        } else if (isError) {
            const char* errMsg = payload["errorMessage"] | "Unknown error";
            if (_onChatDelta) {
                _onChatDelta(runId, errMsg, true);
            }
        }
        return;
    }

    /* Flipper command event - agent sends flipper commands via gateway */
    if (strcmp(event, "flipper") == 0) {
        JsonObjectConst payload = doc["payload"];
        const char* command = payload["command"] | "";
        const char* source = payload["source"] | "gateway";
        if (_onFlipperCommand && strlen(command) > 0) {
            _onFlipperCommand(command, source);
        }
        return;
    }

    /* State change events */
    if (strcmp(event, "snapshot") == 0 || strcmp(event, "state") == 0) {
        /* Could update UI with agent state */
        return;
    }
}

/* ── Chat Operations ──────────────────────────────────── */

bool GatewayClient::sendMessage(const char* sessionKey, const char* message) {
    if (_state != GW_AUTHENTICATED) return false;

    JsonDocument doc;
    doc["type"] = "req";
    const char* id = generateId();
    doc["id"] = id;
    doc["method"] = "chat.send";

    JsonObject params = doc["params"].to<JsonObject>();
    params["sessionKey"] = sessionKey;
    params["message"] = message;
    params["idempotencyKey"] = id;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);

    Serial.printf("[GW] Sent message to session '%s'\n", sessionKey);
    return true;
}

bool GatewayClient::requestHistory(const char* sessionKey, int limit) {
    if (_state != GW_AUTHENTICATED) return false;

    JsonDocument doc;
    doc["type"] = "req";
    doc["id"] = generateId();
    doc["method"] = "chat.history";

    JsonObject params = doc["params"].to<JsonObject>();
    params["sessionKey"] = sessionKey;
    params["limit"] = limit;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
    return true;
}

bool GatewayClient::requestSessionList(int limit) {
    if (_state != GW_AUTHENTICATED) return false;

    JsonDocument doc;
    doc["type"] = "req";
    doc["id"] = generateId();
    doc["method"] = "sessions.list";

    JsonObject params = doc["params"].to<JsonObject>();
    params["limit"] = limit;
    params["includeDerivedTitles"] = true;
    params["includeLastMessage"] = true;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
    return true;
}

bool GatewayClient::abortChat(const char* sessionKey) {
    if (_state != GW_AUTHENTICATED) return false;

    JsonDocument doc;
    doc["type"] = "req";
    doc["id"] = generateId();
    doc["method"] = "chat.abort";

    JsonObject params = doc["params"].to<JsonObject>();
    params["sessionKey"] = sessionKey;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
    return true;
}

void GatewayClient::setSessionKey(const char* key) {
    strncpy(_currentSessionKey, key, sizeof(_currentSessionKey) - 1);
    _currentSessionKey[sizeof(_currentSessionKey) - 1] = '\0';
}
