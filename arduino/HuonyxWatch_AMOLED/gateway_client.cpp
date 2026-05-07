/**
 * Huonyx Watch – Gateway WebSocket Client Implementation
 * Connects to HoC Gateway via Tailscale funnel (WSS)
 */
#include "gateway_client.h"

GatewayClient* GatewayClient::_instance = nullptr;

GatewayClient::GatewayClient()
    : _state(GW_DISCONNECTED)
    , _port(443)
    , _useSSL(true)
    , _lastTickMs(0)
    , _reconnectTimer(0)
    , _msgIdCounter(0)
    , _onChatDelta(nullptr)
    , _onStateChange(nullptr)
    , _onSessionList(nullptr)
{
    memset(_host, 0, sizeof(_host));
    memset(_password, 0, sizeof(_password));
    memset(_currentSessionKey, 0, sizeof(_currentSessionKey));
    _instance = this;
}

void GatewayClient::begin(const char* host, uint16_t port, const char* password, bool useSSL) {
    strncpy(_host, host, sizeof(_host) - 1);
    _port = port;
    strncpy(_password, password, sizeof(_password) - 1);
    _useSSL = useSSL;

    /* Default session key */
    if (_currentSessionKey[0] == '\0') {
        strncpy(_currentSessionKey, "huonyx-watch", sizeof(_currentSessionKey) - 1);
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

    Serial.printf("[GW] Connecting to %s:%d (SSL=%d)\n", _host, _port, _useSSL);
}

void GatewayClient::loop() {
    _ws.loop();

    /* Check for tick timeout */
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
    snprintf(_idBuf, sizeof(_idBuf), "watch-%08lx-%04x",
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
            Serial.printf("[GW] WebSocket connected to %s\n", _instance->_host);
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

    /* Client identification */
    JsonObject client = params["client"].to<JsonObject>();
    client["id"] = "huonyx-watch-amoled";
    client["displayName"] = DEVICE_NAME;
    client["version"] = FIRMWARE_VERSION;
    client["platform"] = DEVICE_PLATFORM;
    client["deviceFamily"] = DEVICE_FAMILY;
    client["mode"] = "chat";

    /* Password authentication */
    if (_password[0] != '\0') {
        JsonObject auth = params["auth"].to<JsonObject>();
        auth["password"] = _password;
    }

    /* Capabilities */
    JsonArray caps = params["caps"].to<JsonArray>();
    caps.add("chat");

    /* Role and scopes */
    params["role"] = "operator";
    JsonArray scopes = params["scopes"].to<JsonArray>();
    scopes.add("operator.admin");

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
    Serial.println("[GW] Sent connect frame");
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

    /* hello-ok response */
    if (payload.containsKey("type")) {
        const char* ptype = payload["type"] | "";
        if (strcmp(ptype, "hello-ok") == 0) {
            if (ok) {
                setState(GW_AUTHENTICATED);
                const char* version = payload["server"]["version"] | "?";
                Serial.printf("[GW] Authenticated! Server v%s\n", version);
            } else {
                Serial.println("[GW] Authentication FAILED");
                setState(GW_ERROR);
            }
            return;
        }
    }

    /* Check if this is the hello-ok at top level */
    if (ok && payload.containsKey("server")) {
        setState(GW_AUTHENTICATED);
        const char* version = payload["server"]["version"] | "?";
        Serial.printf("[GW] Authenticated! Server v%s\n", version);
        return;
    }

    /* Sessions list response */
    if (payload.containsKey("sessions")) {
        JsonArrayConst sessions = payload["sessions"];
        if (_onSessionList) _onSessionList(sessions);
        return;
    }

    /* Chat accepted */
    const char* status = payload["status"] | "";
    if (strcmp(status, "accepted") == 0) {
        Serial.println("[GW] Message accepted");
    }
}

void GatewayClient::handleEvent(JsonObjectConst doc) {
    const char* event = doc["event"] | "";

    /* Tick heartbeat */
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

        /* Filter by session */
        if (strlen(_currentSessionKey) > 0 && strcmp(sessionKey, _currentSessionKey) != 0) {
            return;
        }

        bool isFinal = (strcmp(state, "final") == 0);
        bool isDelta = (strcmp(state, "delta") == 0);
        bool isError = (strcmp(state, "error") == 0);

        if (isDelta || isFinal) {
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

    /* Snapshot/state events */
    if (strcmp(event, "snapshot") == 0 || strcmp(event, "state") == 0) {
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
    Serial.printf("[GW] Sent message to '%s'\n", sessionKey);
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
