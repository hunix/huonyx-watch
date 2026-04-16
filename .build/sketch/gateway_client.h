#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\gateway_client.h"
/**
 * Huonyx Gateway WebSocket Client
 * Implements the HoC Gateway Protocol for ESP32
 */

#ifndef GATEWAY_CLIENT_H
#define GATEWAY_CLIENT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "hw_config.h"

/* Connection states */
enum GatewayState : uint8_t {
    GW_DISCONNECTED = 0,
    GW_CONNECTING,
    GW_CONNECTED,
    GW_AUTHENTICATED,
    GW_ERROR
};

/* Chat message structure */
struct ChatMessage {
    bool   isUser;
    char   text[CHAT_MAX_MSG_LEN];
    char   timestamp[6];  /* HH:MM */
};

/* Callback types */
typedef void (*OnChatDelta)(const char* runId, const char* text, bool isFinal);
typedef void (*OnStateChange)(GatewayState newState);
typedef void (*OnSessionList)(JsonArrayConst sessions);
typedef void (*OnFlipperCommand)(const char* command, const char* source);

class GatewayClient {
public:
    GatewayClient();

    void begin(const char* host, uint16_t port, const char* token, bool useSSL = false);
    void loop();
    void disconnect();

    /* Chat operations */
    bool sendMessage(const char* sessionKey, const char* message);
    bool requestHistory(const char* sessionKey, int limit = 20);
    bool requestSessionList(int limit = 10);
    bool abortChat(const char* sessionKey);

    /* State */
    GatewayState getState() const { return _state; }
    bool isConnected() const { return _state == GW_AUTHENTICATED; }
    const char* getSessionKey() const { return _currentSessionKey; }
    void setSessionKey(const char* key);

    /* Callbacks */
    void onChatDelta(OnChatDelta cb) { _onChatDelta = cb; }
    void onStateChange(OnStateChange cb) { _onStateChange = cb; }
    void onSessionList(OnSessionList cb) { _onSessionList = cb; }
    void onFlipperCommand(OnFlipperCommand cb) { _onFlipperCommand = cb; }

    /* Stats */
    unsigned long getLastTickMs() const { return _lastTickMs; }

private:
    WebSocketsClient _ws;
    GatewayState     _state;
    char             _host[64];
    uint16_t         _port;
    char             _token[128];
    bool             _useSSL;
    char             _currentSessionKey[64];
    unsigned long    _lastTickMs;
    unsigned long    _reconnectTimer;
    uint32_t         _msgIdCounter;

    /* Callbacks */
    OnChatDelta      _onChatDelta;
    OnStateChange    _onStateChange;
    OnSessionList    _onSessionList;
    OnFlipperCommand _onFlipperCommand;

    /* Internal */
    void setState(GatewayState s);
    void sendConnectFrame();
    void handleMessage(uint8_t* payload, size_t length);
    void handleResponse(JsonObjectConst doc);
    void handleEvent(JsonObjectConst doc);
    const char* generateId();
    char _idBuf[28];

    /* WebSocket event handler */
    static void wsEventCallback(WStype_t type, uint8_t* payload, size_t length);
    static GatewayClient* _instance;
};

#endif /* GATEWAY_CLIENT_H */
