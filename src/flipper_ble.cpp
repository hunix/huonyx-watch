/**
 * Flipper Zero BLE Serial Client Implementation
 * Huonyx AI Smartwatch
 *
 * Uses NimBLE-Arduino for memory-efficient BLE Central operation.
 * The Flipper Zero exposes a Serial-over-BLE service that accepts
 * CLI commands as text strings and returns output the same way.
 */

#include <Arduino.h>
#include "flipper_ble.h"
#include <NimBLEDevice.h>

/* ── Static instance for callbacks ───────────────────────── */
FlipperBLE* FlipperBLE::_instance = nullptr;

/* ── NimBLE objects (file-scope) ─────────────────────────── */
static NimBLEScan*           s_pScan       = nullptr;
static NimBLEClient*         s_pClient     = nullptr;
static NimBLERemoteService*  s_pService    = nullptr;
static NimBLERemoteCharacteristic* s_pRxChar = nullptr;  /* Read/Notify from Flipper */
static NimBLERemoteCharacteristic* s_pTxChar = nullptr;  /* Write to Flipper */
static NimBLEAdvertisedDevice* s_pTargetDevice = nullptr;
static bool s_deviceFound   = false;
static bool s_scanComplete  = false;
static bool s_connected     = false;
static bool s_disconnected  = false;

/* ── Notification callback (data from Flipper) ───────────── */
static void notifyCallback(NimBLERemoteCharacteristic* pChar,
                           uint8_t* pData, size_t length, bool isNotify) {
    if (FlipperBLE::_instance && length > 0) {
        FlipperBLE::_instance->handleResponseData(pData, length);
    }
}

/* ── Client connection callbacks ─────────────────────────── */
class FlipperBLECallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        s_connected = true;
        Serial.println("[FLIP] BLE connected");
    }

    void onDisconnect(NimBLEClient* pClient, int reason) override {
        s_disconnected = true;
        s_connected = false;
        Serial.printf("[FLIP] BLE disconnected, reason=%d\n", reason);
    }
};

static FlipperBLECallbacks s_clientCallbacks;

/* ── Scan callbacks ──────────────────────────────────────── */
class FlipperBLEAdvCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        const char* name = advertisedDevice->getName().c_str();
        Serial.printf("[FLIP] Found BLE device: %s\n", name);

        /* Check if this is a Flipper Zero */
        bool isTarget = false;

        if (FlipperBLE::_instance) {
            /* If a specific target name is set, match it */
            if (strlen(FlipperBLE::_instance->_targetName) > 0) {
                isTarget = (strstr(name, FlipperBLE::_instance->_targetName) != nullptr);
            } else {
                /* Match any Flipper device */
                isTarget = (strstr(name, "Flipper") != nullptr);
            }
        }

        if (isTarget) {
            Serial.printf("[FLIP] Target Flipper found: %s\n", name);
            /* Copy the device - must use new to persist beyond callback */
            s_pTargetDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
            s_deviceFound = true;
            NimBLEDevice::getScan()->stop();
        }
    }

    void onScanEnd(const NimBLEScanResults& results, int reason) override {
        s_scanComplete = true;
        Serial.printf("[FLIP] Scan complete, found %d devices\n", results.getCount());
    }
};

static FlipperBLEAdvCallbacks s_scanCallbacks;

/* ══════════════════════════════════════════════════════════
 *  CONSTRUCTOR
 * ══════════════════════════════════════════════════════════ */

FlipperBLE::FlipperBLE()
    : _state(FLIP_IDLE)
    , _rssi(0)
    , _bleInitialized(false)
    , _cmdHead(0)
    , _cmdTail(0)
    , _cmdIdCounter(0)
    , _responseLen(0)
    , _activeCmdId(0)
    , _cmdStartMs(0)
    , _lastReconnectMs(0)
    , _reconnectIntervalMs(FLIPPER_RECONNECT_MS)
    , _scanStartMs(0)
    , _histHead(0)
    , _histCount(0)
    , _onResponse(nullptr)
    , _onStateChange(nullptr)
{
    memset(_targetName, 0, sizeof(_targetName));
    memset(_deviceName, 0, sizeof(_deviceName));
    memset(_cmdQueue,   0, sizeof(_cmdQueue));
    memset(_responseBuf, 0, sizeof(_responseBuf));
    memset(_cmdHistory, 0, sizeof(_cmdHistory));
    _instance = this;
}

/* ══════════════════════════════════════════════════════════
 *  INITIALIZATION
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::begin() {
    if (_bleInitialized) return;

    Serial.println("[FLIP] Initializing NimBLE...");

    /* Initialize NimBLE with a device name */
    NimBLEDevice::init("HuonyxWatch");

    /* Set power level for reasonable range */
    NimBLEDevice::setPower(6);  /* +6 dBm, NimBLE 2.x uses int dBm */

    /* Get the scan object */
    s_pScan = NimBLEDevice::getScan();
    s_pScan->setScanCallbacks(&s_scanCallbacks);
    s_pScan->setInterval(100);
    s_pScan->setWindow(99);
    s_pScan->setActiveScan(true);

    _bleInitialized = true;
    Serial.printf("[FLIP] NimBLE initialized, free heap: %d\n", ESP.getFreeHeap());
}

/* ══════════════════════════════════════════════════════════
 *  MAIN LOOP
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::loop() {
    if (!_bleInitialized) return;

    /* ── Handle scan results ─────────────────────────── */
    if (_state == FLIP_SCANNING) {
        if (s_deviceFound) {
            s_deviceFound = false;
            s_scanComplete = false;

            if (s_pTargetDevice) {
                strncpy(_deviceName, s_pTargetDevice->getName().c_str(),
                        sizeof(_deviceName) - 1);
                _rssi = s_pTargetDevice->getRSSI();
                Serial.printf("[FLIP] Connecting to: %s (RSSI: %d)\n",
                              _deviceName, _rssi);
                setState(FLIP_CONNECTING);
            }
        } else if (s_scanComplete) {
            s_scanComplete = false;
            Serial.println("[FLIP] Scan complete, no Flipper found");
            setState(FLIP_IDLE);
        }
    }

    /* ── Handle connection ───────────────────────────── */
    if (_state == FLIP_CONNECTING && s_pTargetDevice) {
        /* Create client if needed */
        if (!s_pClient) {
            s_pClient = NimBLEDevice::createClient();
            s_pClient->setClientCallbacks(&s_clientCallbacks);
            s_pClient->setConnectionParams(12, 12, 0, 150);
            s_pClient->setConnectTimeout(10);
        }

        /* Attempt connection */
        if (s_pClient->connect(s_pTargetDevice)) {
            setState(FLIP_CONNECTED);
            Serial.println("[FLIP] Connected, discovering services...");

            /* Discover the serial service */
            s_pService = s_pClient->getService(FLIPPER_BLE_SERVICE_UUID);
            if (s_pService) {
                /* Get RX characteristic (notifications from Flipper) */
                s_pRxChar = s_pService->getCharacteristic(FLIPPER_BLE_RX_CHAR_UUID);
                /* Get TX characteristic (write commands to Flipper) */
                s_pTxChar = s_pService->getCharacteristic(FLIPPER_BLE_TX_CHAR_UUID);

                if (s_pRxChar && s_pTxChar) {
                    /* Subscribe to notifications */
                    if (s_pRxChar->canNotify()) {
                        s_pRxChar->subscribe(true, notifyCallback);
                    }
                    setState(FLIP_READY);
                    Serial.println("[FLIP] Services discovered, READY");
                    _reconnectIntervalMs = FLIPPER_RECONNECT_MS;  /* reset backoff */

                    /* Send a newline to get a fresh prompt */
                    s_pTxChar->writeValue("\r\n", 2, false);
                } else {
                    Serial.println("[FLIP] Required characteristics not found");
                    s_pClient->disconnect();
                    setState(FLIP_ERROR);
                }
            } else {
                Serial.println("[FLIP] Serial service not found");
                s_pClient->disconnect();
                setState(FLIP_ERROR);
            }
        } else {
            Serial.println("[FLIP] Connection failed");
            setState(FLIP_ERROR);
        }

        /* Clean up target device */
        if (s_pTargetDevice) {
            delete s_pTargetDevice;
            s_pTargetDevice = nullptr;
        }
    }

    /* ── Handle disconnection ────────────────────────── */
    if (s_disconnected) {
        s_disconnected = false;
        s_pRxChar = nullptr;
        s_pTxChar = nullptr;
        s_pService = nullptr;

        if (_state != FLIP_IDLE) {
            setState(FLIP_IDLE);
        }
    }

    /* ── Process command queue when ready ─────────────── */
    if (_state == FLIP_READY) {
        processCommandQueue();
    }

    /* ── Command timeout ─────────────────────────────── */
    if (_state == FLIP_BUSY && _cmdStartMs > 0) {
        if (millis() - _cmdStartMs > FLIPPER_CMD_TIMEOUT_MS) {
            Serial.println("[FLIP] Command timeout");
            /* Deliver whatever we accumulated */
            if (_onResponse && _activeCmdId > 0) {
                _responseBuf[_responseLen] = '\0';
                _onResponse(_activeCmdId,
                            _responseLen > 0 ? _responseBuf : "Timeout",
                            true);
            }
            _responseLen = 0;
            _activeCmdId = 0;
            _cmdStartMs  = 0;
            setState(FLIP_READY);
        }
    }

    /* ── Auto-reconnect with exponential backoff ─────────────── */
    if (_state == FLIP_ERROR || (_state == FLIP_IDLE && _targetName[0] != '\0')) {
        if (millis() - _lastReconnectMs > _reconnectIntervalMs) {
            _lastReconnectMs = millis();
            if (_targetName[0] != '\0') {
                /* Double the interval for next failure, capped at max */
                _reconnectIntervalMs = min(_reconnectIntervalMs * 2UL,
                                          (unsigned long)FLIPPER_RECONNECT_MAX_MS);
                Serial.printf("[FLIP] Reconnecting (next backoff: %lums)\n",
                              _reconnectIntervalMs);
                startScan(_targetName);
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  SCANNING
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::startScan(const char* targetName) {
    if (!_bleInitialized) return;
    if (_state == FLIP_SCANNING) return;

    if (targetName && strlen(targetName) > 0) {
        strncpy(_targetName, targetName, sizeof(_targetName) - 1);
    }

    s_deviceFound = false;
    s_scanComplete = false;

    Serial.printf("[FLIP] Starting scan for: %s\n",
                  _targetName[0] ? _targetName : "any Flipper");

    setState(FLIP_SCANNING);
    _scanStartMs = millis();

    /* Start async scan */
    s_pScan->clearResults();
    s_pScan->start(FLIPPER_SCAN_TIMEOUT_SEC * 1000);  /* NimBLE 2.x: duration in ms */
}

void FlipperBLE::stopScan() {
    if (s_pScan && _state == FLIP_SCANNING) {
        s_pScan->stop();
        setState(FLIP_IDLE);
    }
}

/* ══════════════════════════════════════════════════════════
 *  DISCONNECT
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::disconnect() {
    /* Clear auto-reconnect target */
    memset(_targetName, 0, sizeof(_targetName));

    if (s_pClient && s_pClient->isConnected()) {
        s_pClient->disconnect();
    }

    s_pRxChar  = nullptr;
    s_pTxChar  = nullptr;
    s_pService = nullptr;

    setState(FLIP_IDLE);
}

/* ══════════════════════════════════════════════════════════
 *  COMMAND INTERFACE (public)
 * ══════════════════════════════════════════════════════════ */

uint32_t FlipperBLE::sendCommand(const char* command) {
    if (!command || strlen(command) == 0) return 0;

    _cmdIdCounter++;
    uint32_t id = _cmdIdCounter;

    if (!enqueueCommand(command, id)) {
        Serial.println("[FLIP] Command queue full");
        return 0;
    }

    Serial.printf("[FLIP] Queued command #%u: %s\n", id, command);

    /* If ready, start processing immediately */
    if (_state == FLIP_READY) {
        processCommandQueue();
    }

    return id;
}

void FlipperBLE::cancelCommand() {
    if (_state == FLIP_BUSY) {
        /* Send Ctrl+C to abort current command */
        if (s_pTxChar) {
            uint8_t ctrlC = 0x03;
            s_pTxChar->writeValue(&ctrlC, 1, false);
        }
        _responseLen = 0;
        _activeCmdId = 0;
        _cmdStartMs  = 0;
        setState(FLIP_READY);
    }
}

int FlipperBLE::getQueueDepth() const {
    if (_cmdHead >= _cmdTail) {
        return _cmdHead - _cmdTail;
    }
    return FLIPPER_CMD_QUEUE_SIZE - _cmdTail + _cmdHead;
}

/* ══════════════════════════════════════════════════════════
 *  INTERNAL: STATE
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::setState(FlipperState s) {
    if (_state != s) {
        _state = s;
        if (_onStateChange) _onStateChange(s);
    }
}

/* ══════════════════════════════════════════════════════════
 *  INTERNAL: COMMAND QUEUE
 * ══════════════════════════════════════════════════════════ */

bool FlipperBLE::enqueueCommand(const char* cmd, uint32_t id) {
    uint8_t nextHead = (_cmdHead + 1) % FLIPPER_CMD_QUEUE_SIZE;
    if (nextHead == _cmdTail) return false;  /* Full */

    strncpy(_cmdQueue[_cmdHead].cmd, cmd, sizeof(_cmdQueue[0].cmd) - 1);
    _cmdQueue[_cmdHead].cmd[sizeof(_cmdQueue[0].cmd) - 1] = '\0';
    _cmdQueue[_cmdHead].id      = id;
    _cmdQueue[_cmdHead].pending = true;
    _cmdHead = nextHead;
    return true;
}

FlipperCommand* FlipperBLE::dequeueCommand() {
    if (_cmdHead == _cmdTail) return nullptr;  /* Empty */

    FlipperCommand* cmd = &_cmdQueue[_cmdTail];
    _cmdTail = (_cmdTail + 1) % FLIPPER_CMD_QUEUE_SIZE;
    return cmd;
}

/**
 * FIX 2: Pull the next command from the queue and dispatch it.
 *
 * Previously, processCommandQueue() called dequeueCommand() (advancing
 * _cmdTail), then called sendNextCommand() which read back _cmdTail-1
 * to recover the just-dequeued entry — fragile and easy to break.
 *
 * Now the pointer returned by dequeueCommand() is passed directly into
 * dispatchCommand(), with no back-read required.
 */
void FlipperBLE::processCommandQueue() {
    if (_state != FLIP_READY) return;

    FlipperCommand* cmd = dequeueCommand();
    if (!cmd) return;

    dispatchCommand(cmd);
}

/**
 * Write an already-dequeued command to the Flipper Zero over BLE.
 */
void FlipperBLE::dispatchCommand(FlipperCommand* cmd) {
    if (!s_pTxChar) {
        Serial.println("[FLIP] TX characteristic not available");
        if (_onResponse) {
            _onResponse(cmd->id, "BLE not ready", true);
        }
        setState(FLIP_ERROR);
        return;
    }

    /* Build command string with CRLF line terminator */
    size_t cmdLen = strlen(cmd->cmd);
    char cmdBuf[132];  /* 128 cmd + CRLF + null */
    if (cmdLen > sizeof(cmdBuf) - 3) cmdLen = sizeof(cmdBuf) - 3;
    memcpy(cmdBuf, cmd->cmd, cmdLen);
    cmdBuf[cmdLen]     = '\r';
    cmdBuf[cmdLen + 1] = '\n';
    cmdBuf[cmdLen + 2] = '\0';

    bool ok = s_pTxChar->writeValue((const uint8_t*)cmdBuf, cmdLen + 2, false);
    if (ok) {
        _activeCmdId = cmd->id;
        _responseLen = 0;
        _cmdStartMs  = millis();

        /* Record in command history ring-buffer */
        strncpy(_cmdHistory[_histHead], cmd->cmd, FLIPPER_CMD_MAX_LEN);
        _cmdHistory[_histHead][FLIPPER_CMD_MAX_LEN] = '\0';
        _histHead  = (_histHead + 1) % FLIPPER_CMD_HISTORY_SIZE;
        if (_histCount < FLIPPER_CMD_HISTORY_SIZE) _histCount++;

        setState(FLIP_BUSY);
        Serial.printf("[FLIP] Sent command #%u: %s\n", cmd->id, cmd->cmd);
    } else {
        Serial.println("[FLIP] Failed to write command");
        if (_onResponse) {
            _onResponse(cmd->id, "BLE write failed", true);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  INTERNAL: RESPONSE HANDLING
 * ══════════════════════════════════════════════════════════ */

/**
 * FIX 1: Robust Flipper Zero CLI prompt detection.
 *
 * The Flipper Zero CLI prompt is ">: " (greater-than, colon, space)
 * appearing at the start of a new line after command output.
 *
 * Old behaviour: triggered on any lone '>' character, causing false
 * positives for command output that contained '>' (e.g., hex dumps,
 * IR signal data). Also streamed noisy partial responses.
 *
 * New behaviour:
 *   1. Primary:  look for "\r\n>: " or "\n>: " in the accumulated buffer.
 *   2. Fallback: buffer ends with ">: " (very first prompt, before any \n).
 *   3. Strips the prompt, the echoed command, and leading whitespace.
 *   4. Delivers exactly ONE callback with the clean response.
 *   5. No partial/streaming delivery — callers always get a complete result.
 */
void FlipperBLE::handleResponseData(const uint8_t* data, size_t len) {
    /* Accumulate response data into the buffer */
    for (size_t i = 0; i < len && _responseLen < FLIPPER_RESPONSE_BUF_SIZE - 1; i++) {
        _responseBuf[_responseLen++] = (char)data[i];
    }
    _responseBuf[_responseLen] = '\0';

    /* ── Detect CLI prompt ────────────────────────────────────────────────── */
    bool complete = false;

    if (strstr(_responseBuf, "\r\n>: ") != nullptr ||
        strstr(_responseBuf, "\n>: ")   != nullptr) {
        complete = true;
    }
    /* Buffer ends exactly with the prompt (first prompt before any newline) */
    if (!complete && _responseLen >= 3 &&
        memcmp(_responseBuf + _responseLen - 3, ">: ", 3) == 0) {
        complete = true;
    }

    if (complete && _activeCmdId > 0) {
        /* ── Strip the prompt ────────────────────────────────── */
        char* promptPos = strstr(_responseBuf, "\r\n>: ");
        if (!promptPos) promptPos = strstr(_responseBuf, "\n>: ");
        if (promptPos) {
            *promptPos = '\0';
            _responseLen = (size_t)(promptPos - _responseBuf);
        } else {
            /* Whole buffer IS the prompt (empty command result) */
            _responseLen = 0;
            _responseBuf[0] = '\0';
        }

        /* ── Strip leading echo (first line is the echoed command) ── */
        const char* responseStart = _responseBuf;
        const char* firstNewline  = strstr(_responseBuf, "\r\n");
        if (!firstNewline) firstNewline = strchr(_responseBuf, '\n');
        if (firstNewline) {
            responseStart = firstNewline + (firstNewline[0] == '\r' ? 2 : 1);
        }

        /* ── Trim leading whitespace / blank lines ──────────── */
        while (*responseStart == '\r' || *responseStart == '\n' ||
               *responseStart == ' '  || *responseStart == '\t') {
            responseStart++;
        }

        /* ── Deliver complete, clean response ────────────────── */
        if (_onResponse) {
            _onResponse(_activeCmdId, responseStart[0] ? responseStart : "OK", true);
        }

        _activeCmdId = 0;
        _responseLen = 0;
        _cmdStartMs  = 0;
        setState(FLIP_READY);

        /* Drain the queue — pick up the next waiting command */
        processCommandQueue();
    }
    /* No partial/streaming delivery — we always wait for the CLI prompt. */
}

/* ══════════════════════════════════════════════════════════
 *  COMMAND HISTORY
 * ══════════════════════════════════════════════════════════ */

/**
 * Returns the last N commands, newest first, into `out`.
 * `out` must point to at least `maxCount` const char* slots.
 * Returns actual number of entries written (≤ FLIPPER_CMD_HISTORY_SIZE).
 */
int FlipperBLE::getHistory(const char** out, int maxCount) const {
    if (!out || maxCount <= 0 || _histCount == 0) return 0;

    int count = min((int)_histCount, maxCount);
    /* Walk backwards from the last written slot */
    int slot = ((int)_histHead - 1 + FLIPPER_CMD_HISTORY_SIZE) % FLIPPER_CMD_HISTORY_SIZE;
    for (int i = 0; i < count; i++) {
        out[i] = _cmdHistory[slot];
        slot = (slot - 1 + FLIPPER_CMD_HISTORY_SIZE) % FLIPPER_CMD_HISTORY_SIZE;
    }
    return count;
}
