/**
 * Supabase Realtime Bridge
 * Huonyx AI Smartwatch
 *
 * Connects to Supabase Realtime via WebSocket using the Phoenix
 * Channel Protocol v1.0.0. Bridges commands between the remote
 * Huonyx Agent and the local Flipper Zero BLE connection.
 *
 * Architecture:
 *   Agent -> Supabase Broadcast -> Watch (this) -> Flipper BLE
 *   Flipper BLE -> Watch (this) -> Supabase Broadcast -> Agent
 */

#ifndef SUPABASE_BRIDGE_H
#define SUPABASE_BRIDGE_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "hw_config.h"

/* Bridge connection states */
enum BridgeState : uint8_t {
    BRIDGE_DISCONNECTED = 0,
    BRIDGE_CONNECTING,
    BRIDGE_CONNECTED,      /* WebSocket up */
    BRIDGE_JOINED,         /* Channel joined */
    BRIDGE_ERROR
};

/* Broadcast event types */
#define EVT_FLIPPER_COMMAND   "flipper_command"
#define EVT_FLIPPER_RESULT    "flipper_result"
#define EVT_FLIPPER_STATUS    "flipper_status"
#define EVT_WATCH_STATUS      "watch_status"
#define EVT_AGENT_MESSAGE     "agent_message"

/* Callback for incoming commands from the agent */
typedef void (*OnBridgeCommand)(const char* command, const char* source);
typedef void (*OnBridgeStateChange)(BridgeState newState);
typedef void (*OnBridgeAgentMessage)(const char* message);

class SupabaseBridge {
public:
    SupabaseBridge();

    /**
     * Initialize and connect to Supabase Realtime.
     * @param projectUrl  Supabase project URL (e.g., "abcdef.supabase.co")
     * @param apiKey      Supabase anon/service key
     */
    void begin(const char* projectUrl, const char* apiKey,
               const char* fingerprint = "");

    /**
     * Main loop - handles WebSocket events and heartbeat.
     */
    void loop();

    /**
     * Disconnect from Supabase.
     */
    void disconnect();

    /**
     * Send a Flipper command result back to the agent.
     */
    void sendFlipperResult(uint32_t cmdId, const char* result, bool success);

    /**
     * Send Flipper connection status to the agent.
     */
    void sendFlipperStatus(const char* state, const char* deviceName, int rssi);

    /**
     * Send watch status update to the agent.
     */
    void sendWatchStatus(int battery, bool charging, bool wifiConnected,
                         bool gatewayConnected, bool flipperConnected);

    /* State */
    BridgeState getState() const { return _state; }
    bool isJoined() const { return _state == BRIDGE_JOINED; }

    /* Callbacks */
    void onCommand(OnBridgeCommand cb) { _onCommand = cb; }
    void onStateChange(OnBridgeStateChange cb) { _onStateChange = cb; }
    void onAgentMessage(OnBridgeAgentMessage cb) { _onAgentMessage = cb; }

private:
    WebSocketsClient _ws;
    BridgeState      _state;
    char             _projectUrl[128];
    char             _apiKey[128];
    char             _fingerprint[60];  /* SHA-1 TLS fingerprint or empty = skip verify */
    unsigned long    _reconnectIntervalMs;  /* Current exponential backoff interval */
    uint32_t         _refCounter;
    char             _joinRef[8];
    unsigned long    _lastHeartbeatMs;
    unsigned long    _reconnectTimer;

    /* Callbacks */
    OnBridgeCommand      _onCommand;
    OnBridgeStateChange  _onStateChange;
    OnBridgeAgentMessage _onAgentMessage;

    /* Internal */
    void setState(BridgeState s);
    void sendJoin();
    void sendHeartbeat();
    void sendBroadcast(const char* event, JsonObject payload);
    void handleMessage(uint8_t* payload, size_t length);
    void handleBroadcast(JsonObjectConst payload);
    void nextRef(char* buf, size_t bufSize);

    /* WebSocket event handler */
    static void wsEventCallback(WStype_t type, uint8_t* payload, size_t length);
    static SupabaseBridge* _instance;
};

#endif /* SUPABASE_BRIDGE_H */
