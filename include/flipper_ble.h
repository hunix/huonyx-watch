/**
 * Flipper Zero BLE Serial Client
 * Huonyx AI Smartwatch
 *
 * Connects to a Flipper Zero's BLE Serial service and provides
 * a bidirectional command/response interface for CLI control.
 * Uses NimBLE for memory efficiency on ESP32-C3.
 */

#ifndef FLIPPER_BLE_H
#define FLIPPER_BLE_H

#include <Arduino.h>
#include "hw_config.h"

/* Forward declaration for NimBLE */
class NimBLERemoteCharacteristic;

/* Flipper connection states */
enum FlipperState : uint8_t {
    FLIP_IDLE = 0,        /* Not scanning or connecting */
    FLIP_SCANNING,        /* Actively scanning for Flipper */
    FLIP_CONNECTING,      /* Found device, establishing connection */
    FLIP_CONNECTED,       /* BLE link up, discovering services */
    FLIP_READY,           /* Services discovered, ready for commands */
    FLIP_BUSY,            /* Executing a command, waiting for response */
    FLIP_ERROR            /* Error state */
};

/* Command in the queue */
static constexpr size_t FLIPPER_CMD_MAX_LEN = 127;
struct FlipperCommand {
    char     cmd[FLIPPER_CMD_MAX_LEN + 1];
    uint32_t id;
    bool     pending;
};

/* Callback types */
typedef void (*OnFlipperResponse)(uint32_t cmdId, const char* response, bool complete);
typedef void (*OnFlipperStateChange)(FlipperState newState);

class FlipperBLE {
public:
    FlipperBLE();

    /**
     * Initialize BLE stack and prepare for scanning.
     * Must be called after WiFi is initialized.
     */
    void begin();

    /**
     * Main loop - handles scanning, connection, and command processing.
     * Call frequently from the main loop.
     */
    void loop();

    /**
     * Start scanning for a Flipper Zero device.
     * @param targetName Optional specific device name to connect to.
     *                   If empty, connects to first Flipper found.
     */
    void startScan(const char* targetName = "");

    /**
     * Stop scanning.
     */
    void stopScan();

    /**
     * Disconnect from the Flipper Zero.
     */
    void disconnect();

    /**
     * Send a CLI command to the Flipper Zero.
     * @param command The CLI command string (e.g., "ir tx NEC 0x04 0x08")
     * @return Command ID for tracking, or 0 if queue is full.
     */
    uint32_t sendCommand(const char* command);

    /**
     * Cancel the currently executing command.
     */
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

    /**
     * Command history ring-buffer access.
     * Returns the most-recent `count` commands (up to FLIPPER_CMD_HISTORY_SIZE),
     * newest first, into `out`. Returns actual number written.
     */
    int getHistory(const char** out, int maxCount) const;

private:
    FlipperState _state;
    char         _targetName[32];
    char         _deviceName[32];
    int          _rssi;
    bool         _bleInitialized;

    /* Command queue (ring buffer) */
    FlipperCommand _cmdQueue[FLIPPER_CMD_QUEUE_SIZE];
    uint8_t        _cmdHead;
    uint8_t        _cmdTail;
    uint32_t       _cmdIdCounter;

    /* Response accumulator */
    char           _responseBuf[FLIPPER_RESPONSE_BUF_SIZE];
    size_t         _responseLen;
    uint32_t       _activeCmdId;
    unsigned long  _cmdStartMs;

    /* Timing + reconnect backoff */
    unsigned long  _lastReconnectMs;
    unsigned long  _reconnectIntervalMs;  /* current backoff interval, doubles on each failure */
    unsigned long  _scanStartMs;

    /* Command history ring-buffer (newest overwrites oldest) */
    char    _cmdHistory[FLIPPER_CMD_HISTORY_SIZE][FLIPPER_CMD_MAX_LEN + 1];
    uint8_t _histHead;   /* next write slot */
    uint8_t _histCount;  /* valid entries (0..FLIPPER_CMD_HISTORY_SIZE) */

    /* Callbacks */
    OnFlipperResponse    _onResponse;
    OnFlipperStateChange _onStateChange;

    /* Internal */
    void setState(FlipperState s);
    void processCommandQueue();
    void dispatchCommand(FlipperCommand* cmd);   /* Send an already-dequeued command */
    void handleResponseData(const uint8_t* data, size_t len);
    bool enqueueCommand(const char* cmd, uint32_t id);
    FlipperCommand* dequeueCommand();

    /* NimBLE callbacks are handled via static friend functions */
    friend class FlipperBLECallbacks;
    friend class FlipperBLEAdvCallbacks;
    friend void notifyCallback(NimBLERemoteCharacteristic*, uint8_t*, size_t, bool);

    static FlipperBLE* _instance;
};

#endif /* FLIPPER_BLE_H */
