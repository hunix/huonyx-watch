/**
 * Flipper Zero BLE Serial Client Implementation
 * Huonyx AI Smartwatch
 *
 * Uses NimBLE-Arduino for memory-efficient BLE Central operation.
 * The Flipper Zero exposes a Serial-over-BLE service that accepts
 * CLI commands as text strings and returns output the same way.
 */

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
        String name = advertisedDevice->getName().c_str();
        Serial.printf("[FLIP] Found BLE device: %s\n", name.c_str());

        /* Check if this is a Flipper Zero */
        bool isTarget = false;

        if (FlipperBLE::_instance) {
            const char* target = FlipperBLE::_instance->getDeviceName();
            /* If a specific target name is set, match it */
            if (strlen(FlipperBLE::_instance->_targetName) > 0) {
                isTarget = (name.indexOf(FlipperBLE::_instance->_targetName) >= 0);
            } else {
                /* Match any Flipper device */
                isTarget = (name.indexOf("Flipper") >= 0);
            }
        }

        if (isTarget) {
            Serial.printf("[FLIP] Target Flipper found: %s\n", name.c_str());
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
    , _scanStartMs(0)
    , _onResponse(nullptr)
    , _onStateChange(nullptr)
{
    memset(_targetName, 0, sizeof(_targetName));
    memset(_deviceName, 0, sizeof(_deviceName));
    memset(_cmdQueue, 0, sizeof(_cmdQueue));
    memset(_responseBuf, 0, sizeof(_responseBuf));
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
            /* Deliver partial response */
            if (_onResponse && _activeCmdId > 0) {
                _responseBuf[_responseLen] = '\0';
                _onResponse(_activeCmdId, _responseBuf, true);
            }
            _responseLen = 0;
            _activeCmdId = 0;
            _cmdStartMs = 0;
            setState(FLIP_READY);
        }
    }

    /* ── Auto-reconnect ──────────────────────────────── */
    if (_state == FLIP_ERROR || (_state == FLIP_IDLE && _targetName[0] != '\0')) {
        if (millis() - _lastReconnectMs > FLIPPER_RECONNECT_MS) {
            _lastReconnectMs = millis();
            /* Only auto-reconnect if we had a target */
            if (_targetName[0] != '\0') {
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

    s_pRxChar = nullptr;
    s_pTxChar = nullptr;
    s_pService = nullptr;

    setState(FLIP_IDLE);
}

/* ══════════════════════════════════════════════════════════
 *  COMMAND INTERFACE
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
        _cmdStartMs = 0;
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
    _cmdQueue[_cmdHead].id = id;
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

void FlipperBLE::processCommandQueue() {
    if (_state != FLIP_READY) return;

    FlipperCommand* cmd = dequeueCommand();
    if (!cmd) return;

    sendNextCommand();
}

void FlipperBLE::sendNextCommand() {
    FlipperCommand* cmd = &_cmdQueue[(_cmdTail == 0 ? FLIPPER_CMD_QUEUE_SIZE - 1 : _cmdTail - 1)];
    if (!cmd->pending) return;

    if (!s_pTxChar) {
        Serial.println("[FLIP] TX characteristic not available");
        setState(FLIP_ERROR);
        return;
    }

    /* Prepare command string with CRLF */
    String cmdStr = String(cmd->cmd) + "\r\n";

    /* Write to Flipper */
    bool ok = s_pTxChar->writeValue((const uint8_t*)cmdStr.c_str(),
                                     cmdStr.length(), false);

    if (ok) {
        _activeCmdId = cmd->id;
        _responseLen = 0;
        _cmdStartMs = millis();
        cmd->pending = false;
        setState(FLIP_BUSY);
        Serial.printf("[FLIP] Sent command #%u: %s\n", cmd->id, cmd->cmd);
    } else {
        Serial.println("[FLIP] Failed to write command");
        if (_onResponse) {
            _onResponse(cmd->id, "BLE write failed", true);
        }
        cmd->pending = false;
    }
}

/* ══════════════════════════════════════════════════════════
 *  INTERNAL: RESPONSE HANDLING
 * ══════════════════════════════════════════════════════════ */

void FlipperBLE::handleResponseData(const uint8_t* data, size_t len) {
    /* Accumulate response data */
    for (size_t i = 0; i < len && _responseLen < FLIPPER_RESPONSE_BUF_SIZE - 1; i++) {
        _responseBuf[_responseLen++] = (char)data[i];
    }
    _responseBuf[_responseLen] = '\0';

    /* Check for CLI prompt indicating command completion.
     * Flipper CLI prompt ends with ">: " or just ">" after output. */
    bool complete = false;
    if (_responseLen >= 2) {
        /* Look for the Flipper prompt pattern at the end */
        const char* tail = _responseBuf + _responseLen - 1;
        /* Flipper prompt: ">:" followed by space, or just ">" at line start */
        if (*tail == '>' || (*tail == ' ' && _responseLen >= 3 && *(tail - 1) == ':' && *(tail - 2) == '>')) {
            complete = true;
        }
        /* Also check for common end patterns */
        if (strstr(_responseBuf, "\r\n>") != nullptr) {
            complete = true;
        }
    }

    if (complete && _activeCmdId > 0) {
        /* Clean up the response - remove the prompt */
        char* promptPos = strstr(_responseBuf, "\r\n>");
        if (promptPos) {
            *promptPos = '\0';
            _responseLen = promptPos - _responseBuf;
        }

        /* Strip leading echo of the command */
        char* responseStart = _responseBuf;
        char* firstNewline = strstr(_responseBuf, "\r\n");
        if (firstNewline) {
            responseStart = firstNewline + 2;
        }

        if (_onResponse) {
            _onResponse(_activeCmdId, responseStart, true);
        }

        _activeCmdId = 0;
        _responseLen = 0;
        _cmdStartMs = 0;
        setState(FLIP_READY);

        /* Process next command if any */
        processCommandQueue();
    } else if (_activeCmdId > 0 && _onResponse) {
        /* Partial response - stream it */
        /* Only send if we have meaningful data */
        if (_responseLen > 10) {
            _onResponse(_activeCmdId, _responseBuf, false);
        }
    }
}
