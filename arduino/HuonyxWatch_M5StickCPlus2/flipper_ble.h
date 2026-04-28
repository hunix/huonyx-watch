/**
 * Flipper Zero BLE Serial Client
 * Huonyx AI Smartwatch
 *
 * Connects to a Flipper Zero's BLE Serial service and provides
 * a bidirectional command/response interface for CLI control.
 *
 * When ENABLE_FLIPPER_BLE is 0, this header provides a stub class
 * with the same API so the rest of the codebase compiles unchanged.
 * Flipper control remains available through the Supabase bridge.
 */

#ifndef FLIPPER_BLE_H
#define FLIPPER_BLE_H

#include <Arduino.h>
#include "build_config.h"
#include "hw_config.h"

/* Flipper connection states — always defined for UI code */
enum FlipperState : uint8_t {
    FLIP_IDLE = 0,        /* Not scanning or connecting */
    FLIP_SCANNING,        /* Actively scanning for Flipper */
    FLIP_CONNECTING,      /* Found device, establishing connection */
    FLIP_CONNECTED,       /* BLE link up, discovering services */
    FLIP_READY,           /* Services discovered, ready for commands */
    FLIP_BUSY,            /* Executing a command, waiting for response */
    FLIP_ERROR            /* Error state */
};

/* Command in the queue — always defined for struct compatibility */
struct FlipperCommand {
    char     cmd[128];
    uint32_t id;
    bool     pending;
};

/* Callback types — always defined */
typedef void (*OnFlipperResponse)(uint32_t cmdId, const char* response, bool complete);
typedef void (*OnFlipperStateChange)(FlipperState newState);

/* ══════════════════════════════════════════════════════════
 *  FULL BLE IMPLEMENTATION
 * ══════════════════════════════════════════════════════════ */
#if ENABLE_FLIPPER_BLE

/* Forward declaration for NimBLE */
class NimBLERemoteCharacteristic;

class FlipperBLE {
public:
    FlipperBLE();

    void begin();
    void loop();
    void startScan(const char* targetName = "");
    void stopScan();
    void disconnect();
    uint32_t sendCommand(const char* command);
    void cancelCommand();

    /* State queries */
    FlipperState getState() const { return _state; }
    bool isReady() const { return _state == FLIP_READY; }
    bool isBusy() const { return _state == FLIP_BUSY; }
    bool isConnected() const { return _state >= FLIP_CONNECTED; }
    const char* getDeviceName() const { return _deviceName; }
    int getRssi() const { return _rssi; }
    int getQueueDepth() const;

    /* Callbacks */
    void onResponse(OnFlipperResponse cb) { _onResponse = cb; }
    void onStateChange(OnFlipperStateChange cb) { _onStateChange = cb; }

private:
    FlipperState _state;
    char         _targetName[32];
    char         _deviceName[32];
    int          _rssi;
    bool         _bleInitialized;

    FlipperCommand _cmdQueue[FLIPPER_CMD_QUEUE_SIZE];
    uint8_t        _cmdHead;
    uint8_t        _cmdTail;
    uint32_t       _cmdIdCounter;

    char           _responseBuf[FLIPPER_RESPONSE_BUF_SIZE];
    size_t         _responseLen;
    uint32_t       _activeCmdId;
    unsigned long  _cmdStartMs;

    unsigned long  _lastReconnectMs;
    unsigned long  _scanStartMs;

    OnFlipperResponse    _onResponse;
    OnFlipperStateChange _onStateChange;

    void setState(FlipperState s);
    void processCommandQueue();
    void sendNextCommand();
    void handleResponseData(const uint8_t* data, size_t len);
    bool enqueueCommand(const char* cmd, uint32_t id);
    FlipperCommand* dequeueCommand();

    friend class FlipperBLECallbacks;
    friend class FlipperBLEAdvCallbacks;
    friend void notifyCallback(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool);

    static FlipperBLE* _instance;
};

/* ══════════════════════════════════════════════════════════
 *  STUB IMPLEMENTATION (BLE disabled — saves ~47KB IRAM)
 * ══════════════════════════════════════════════════════════ */
#else /* !ENABLE_FLIPPER_BLE */

class FlipperBLE {
public:
    FlipperBLE() {}

    void begin() {
        Serial.println("[FLIP] BLE disabled at compile time (ENABLE_FLIPPER_BLE=0)");
        Serial.println("[FLIP] Use Supabase bridge for Flipper control");
    }
    void loop() {}
    void startScan(const char* targetName = "") { (void)targetName; }
    void stopScan() {}
    void disconnect() {}
    uint32_t sendCommand(const char* command) { (void)command; return 0; }
    void cancelCommand() {}

    /* State queries — always report idle/disconnected */
    FlipperState getState() const { return FLIP_IDLE; }
    bool isReady() const { return false; }
    bool isBusy() const { return false; }
    bool isConnected() const { return false; }
    const char* getDeviceName() const { return ""; }
    int getRssi() const { return 0; }
    int getQueueDepth() const { return 0; }

    /* Callbacks — store but never fire */
    void onResponse(OnFlipperResponse cb) { (void)cb; }
    void onStateChange(OnFlipperStateChange cb) { (void)cb; }
};

#endif /* ENABLE_FLIPPER_BLE */

#endif /* FLIPPER_BLE_H */
