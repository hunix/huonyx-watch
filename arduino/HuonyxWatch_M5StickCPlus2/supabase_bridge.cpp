/**
 * Supabase Realtime Bridge Implementation
 * Huonyx AI Smartwatch
 *
 * Implements the Phoenix Channel Protocol v1.0.0 over WebSocket
 * to communicate with Supabase Realtime Broadcast channels.
 */

#include "supabase_bridge.h"

SupabaseBridge* SupabaseBridge::_instance = nullptr;

/* ══════════════════════════════════════════════════════════
 *  CONSTRUCTOR
 * ══════════════════════════════════════════════════════════ */

SupabaseBridge::SupabaseBridge()
    : _state(BRIDGE_DISCONNECTED)
    , _refCounter(0)
    , _lastHeartbeatMs(0)
    , _reconnectTimer(0)
    , _onCommand(nullptr)
    , _onStateChange(nullptr)
    , _onAgentMessage(nullptr)
{
    memset(_projectUrl, 0, sizeof(_projectUrl));
    memset(_apiKey, 0, sizeof(_apiKey));
    memset(_joinRef, 0, sizeof(_joinRef));
    _instance = this;
}

/* ══════════════════════════════════════════════════════════
 *  INITIALIZATION
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::begin(const char* projectUrl, const char* apiKey) {
    if (!projectUrl || !apiKey || strlen(projectUrl) == 0 || strlen(apiKey) == 0) {
        Serial.println("[SUPA] Missing URL or API key");
        return;
    }

    strncpy(_projectUrl, projectUrl, sizeof(_projectUrl) - 1);
    strncpy(_apiKey, apiKey, sizeof(_apiKey) - 1);

    /* Extract host from URL (remove https:// prefix if present) */
    const char* host = _projectUrl;
    if (strncmp(host, "https://", 8) == 0) host += 8;
    if (strncmp(host, "http://", 7) == 0) host += 7;

    /* Remove trailing slash */
    char cleanHost[128];
    strncpy(cleanHost, host, sizeof(cleanHost) - 1);
    cleanHost[sizeof(cleanHost) - 1] = '\0';
    size_t len = strlen(cleanHost);
    if (len > 0 && cleanHost[len - 1] == '/') {
        cleanHost[len - 1] = '\0';
    }

    /* Build WebSocket path with API key and protocol version */
    char wsPath[256];
    snprintf(wsPath, sizeof(wsPath),
             "%s?apikey=%s&vsn=1.0.0",
             SUPABASE_WS_PATH, _apiKey);

    Serial.printf("[SUPA] Connecting to: %s (SSL, port %d)\n", cleanHost, SUPABASE_DEFAULT_PORT);
    Serial.printf("[SUPA] Path: %s\n", wsPath);

    setState(BRIDGE_CONNECTING);

    /* Connect via SSL (Supabase always uses HTTPS) */
    _ws.beginSSL(cleanHost, SUPABASE_DEFAULT_PORT, wsPath);
    _ws.onEvent(wsEventCallback);
    _ws.setReconnectInterval(SUPABASE_RECONNECT_MS);
    _ws.enableHeartbeat(0, 0, 0);  /* We handle heartbeat ourselves */
}

/* ══════════════════════════════════════════════════════════
 *  MAIN LOOP
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::loop() {
    _ws.loop();

    /* Send Phoenix heartbeat every 30 seconds when connected */
    if (_state >= BRIDGE_CONNECTED) {
        if (millis() - _lastHeartbeatMs >= SUPABASE_HEARTBEAT_MS) {
            _lastHeartbeatMs = millis();
            sendHeartbeat();
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  DISCONNECT
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::disconnect() {
    _ws.disconnect();
    setState(BRIDGE_DISCONNECTED);
}

/* ══════════════════════════════════════════════════════════
 *  SEND METHODS
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::sendFlipperResult(uint32_t cmdId, const char* result, bool success) {
    if (_state != BRIDGE_JOINED) return;

    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["cmd_id"] = cmdId;
    payload["result"] = result;
    payload["success"] = success;
    payload["source"] = "watch";
    payload["timestamp"] = millis();

    sendBroadcast(EVT_FLIPPER_RESULT, payload);
}

void SupabaseBridge::sendFlipperStatus(const char* state, const char* deviceName, int rssi) {
    if (_state != BRIDGE_JOINED) return;

    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["state"] = state;
    payload["device"] = deviceName;
    payload["rssi"] = rssi;
    payload["source"] = "watch";

    sendBroadcast(EVT_FLIPPER_STATUS, payload);
}

void SupabaseBridge::sendWatchStatus(int battery, bool charging, bool wifiConnected,
                                     bool gatewayConnected, bool flipperConnected) {
    if (_state != BRIDGE_JOINED) return;

    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["battery"] = battery;
    payload["charging"] = charging;
    payload["wifi"] = wifiConnected;
    payload["gateway"] = gatewayConnected;
    payload["flipper"] = flipperConnected;
    payload["uptime_ms"] = millis();
    payload["free_heap"] = ESP.getFreeHeap();

    sendBroadcast(EVT_WATCH_STATUS, payload);
}

/* ══════════════════════════════════════════════════════════
 *  INTERNAL: PROTOCOL
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::nextRef(char* buf, size_t bufSize) {
    _refCounter++;
    snprintf(buf, bufSize, "%lu", (unsigned long)_refCounter);
}

void SupabaseBridge::sendJoin() {
    /* Phoenix Channel Protocol v1.0.0 - phx_join */
    JsonDocument doc;
    doc["topic"] = SUPABASE_CHANNEL_TOPIC;
    doc["event"] = "phx_join";

    char ref[12];
    nextRef(ref, sizeof(ref));
    strncpy(_joinRef, ref, sizeof(_joinRef) - 1);

    doc["ref"] = ref;
    doc["join_ref"] = ref;

    JsonObject payload = doc["payload"].to<JsonObject>();
    JsonObject config = payload["config"].to<JsonObject>();

    JsonObject broadcast = config["broadcast"].to<JsonObject>();
    broadcast["ack"] = false;
    broadcast["self"] = false;  /* Don't receive our own broadcasts (saves processing) */

    JsonObject presence = config["presence"].to<JsonObject>();
    presence["enabled"] = false;

    config["private"] = false;

    /* Include access token */
    payload["access_token"] = _apiKey;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);

    Serial.printf("[SUPA] Sent phx_join for %s\n", SUPABASE_CHANNEL_TOPIC);
}

void SupabaseBridge::sendHeartbeat() {
    JsonDocument doc;
    doc["topic"] = "phoenix";
    doc["event"] = "heartbeat";
    doc["payload"] = JsonObject();
    char ref[12];
    nextRef(ref, sizeof(ref));
    doc["ref"] = ref;

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);
}

void SupabaseBridge::sendBroadcast(const char* event, JsonObject payload) {
    if (_state != BRIDGE_JOINED) return;

    JsonDocument doc;
    doc["topic"] = SUPABASE_CHANNEL_TOPIC;
    doc["event"] = "broadcast";
    char ref[12];
    nextRef(ref, sizeof(ref));
    doc["ref"] = ref;
    doc["join_ref"] = _joinRef;

    JsonObject outerPayload = doc["payload"].to<JsonObject>();
    outerPayload["type"] = "broadcast";
    outerPayload["event"] = event;

    /* Copy the inner payload */
    JsonObject innerPayload = outerPayload["payload"].to<JsonObject>();
    for (JsonPair kv : payload) {
        innerPayload[kv.key()] = kv.value();
    }

    String output;
    serializeJson(doc, output);
    _ws.sendTXT(output);

    Serial.printf("[SUPA] Broadcast: %s (%d bytes)\n", event, output.length());
}

/* ══════════════════════════════════════════════════════════
 *  WEBSOCKET EVENT HANDLER
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::wsEventCallback(WStype_t type, uint8_t* payload, size_t length) {
    if (!_instance) return;

    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[SUPA] WebSocket disconnected");
            _instance->setState(BRIDGE_CONNECTING);
            break;

        case WStype_CONNECTED:
            Serial.println("[SUPA] WebSocket connected");
            _instance->setState(BRIDGE_CONNECTED);
            _instance->_lastHeartbeatMs = millis();
            /* Join the channel */
            _instance->sendJoin();
            break;

        case WStype_TEXT:
            _instance->handleMessage(payload, length);
            break;

        case WStype_ERROR:
            Serial.println("[SUPA] WebSocket error");
            _instance->setState(BRIDGE_ERROR);
            break;

        default:
            break;
    }
}

/* ══════════════════════════════════════════════════════════
 *  MESSAGE HANDLER
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::handleMessage(uint8_t* payload, size_t length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) {
        Serial.printf("[SUPA] JSON parse error: %s\n", err.c_str());
        return;
    }

    const char* event = doc["event"] | "";
    const char* topic = doc["topic"] | "";

    /* Handle phx_reply (join confirmation, heartbeat ack) */
    if (strcmp(event, "phx_reply") == 0) {
        JsonObjectConst respPayload = doc["payload"];
        const char* status = respPayload["status"] | "";

        if (strcmp(topic, SUPABASE_CHANNEL_TOPIC) == 0) {
            if (strcmp(status, "ok") == 0) {
                Serial.println("[SUPA] Successfully joined channel");
                setState(BRIDGE_JOINED);
            } else {
                Serial.printf("[SUPA] Join failed: %s\n", status);
                setState(BRIDGE_ERROR);
            }
        }
        return;
    }

    /* Handle broadcast events */
    if (strcmp(event, "broadcast") == 0) {
        JsonObjectConst bPayload = doc["payload"];
        handleBroadcast(bPayload);
        return;
    }

    /* Handle system events */
    if (strcmp(event, "system") == 0) {
        JsonObjectConst sysPayload = doc["payload"];
        const char* status = sysPayload["status"] | "";
        const char* message = sysPayload["message"] | "";
        Serial.printf("[SUPA] System: %s - %s\n", status, message);
        return;
    }

    /* Handle presence_state (ignored) */
    if (strcmp(event, "presence_state") == 0 || strcmp(event, "presence_diff") == 0) {
        return;
    }

    Serial.printf("[SUPA] Unhandled event: %s on %s\n", event, topic);
}

void SupabaseBridge::handleBroadcast(JsonObjectConst payload) {
    const char* event = payload["event"] | "";
    JsonObjectConst data = payload["payload"];

    Serial.printf("[SUPA] Received broadcast: %s\n", event);

    /* Flipper command from agent */
    if (strcmp(event, EVT_FLIPPER_COMMAND) == 0) {
        const char* command = data["command"] | "";
        const char* source = data["source"] | "agent";

        if (strlen(command) > 0 && _onCommand) {
            Serial.printf("[SUPA] Flipper command from %s: %s\n", source, command);
            _onCommand(command, source);
        }
        return;
    }

    /* Agent message (text message from agent to watch) */
    if (strcmp(event, EVT_AGENT_MESSAGE) == 0) {
        const char* message = data["message"] | "";
        if (strlen(message) > 0 && _onAgentMessage) {
            _onAgentMessage(message);
        }
        return;
    }

    /* Ignore our own broadcasts (flipper_result, flipper_status, watch_status) */
    if (strcmp(event, EVT_FLIPPER_RESULT) == 0 ||
        strcmp(event, EVT_FLIPPER_STATUS) == 0 ||
        strcmp(event, EVT_WATCH_STATUS) == 0) {
        const char* source = data["source"] | "";
        if (strcmp(source, "watch") == 0) {
            return;  /* Our own echo */
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  STATE MANAGEMENT
 * ══════════════════════════════════════════════════════════ */

void SupabaseBridge::setState(BridgeState s) {
    if (_state != s) {
        _state = s;
        if (_onStateChange) _onStateChange(s);
    }
}
