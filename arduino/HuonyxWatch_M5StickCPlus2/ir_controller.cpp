/**
 * ir_controller.cpp
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 *
 * Pure Arduino GPIO bit-banging. No external libraries required.
 * Works on all ESP32 Arduino versions (2.x and 3.x).
 */
#include "ir_controller.h"

/* ══════════════════════════════════════════════════════════
 *  CONSTRUCTOR / BEGIN
 * ══════════════════════════════════════════════════════════ */
IRController::IRController() : _pin(PIN_IR_TX) {}

void IRController::begin() {
    pinMode(_pin, OUTPUT);
    digitalWrite(_pin, LOW);
    Serial.printf("[IR] Controller ready on GPIO%d\n", _pin);
}

/* ══════════════════════════════════════════════════════════
 *  LOW-LEVEL CARRIER HELPERS
 *  _markUs  — emit 38kHz carrier for `us` microseconds
 *  _spaceUs — silence for `us` microseconds
 * ══════════════════════════════════════════════════════════ */
void IRController::_markUs(uint16_t us) {
    uint32_t end = micros() + us;
    while ((int32_t)(end - micros()) > 0) {
        digitalWrite(_pin, HIGH);
        delayMicroseconds(IR_HALF_PERIOD);
        digitalWrite(_pin, LOW);
        delayMicroseconds(IR_HALF_PERIOD);
    }
}

void IRController::_spaceUs(uint16_t us) {
    digitalWrite(_pin, LOW);
    delayMicroseconds(us);
}

/* ══════════════════════════════════════════════════════════
 *  NEC PROTOCOL
 *  Leader: 9ms mark + 4.5ms space
 *  Bit 1:  560µs mark + 1690µs space
 *  Bit 0:  560µs mark + 560µs space
 *  Stop:   560µs mark
 * ══════════════════════════════════════════════════════════ */
void IRController::_sendNEC(uint16_t addr, uint16_t cmd, uint8_t repeat) {
    for (uint8_t r = 0; r < repeat; r++) {
        /* Leader */
        _markUs(9000);
        _spaceUs(4500);
        /* Address (8 bits) + ~Address (8 bits) */
        uint8_t a = addr & 0xFF;
        uint8_t na = ~a;
        uint8_t c = cmd & 0xFF;
        uint8_t nc = ~c;
        uint32_t data = ((uint32_t)nc << 24) | ((uint32_t)c << 16) |
                        ((uint32_t)na << 8) | a;
        for (int i = 0; i < 32; i++) {
            _markUs(560);
            _spaceUs((data & (1UL << i)) ? 1690 : 560);
        }
        /* Stop bit */
        _markUs(560);
        _spaceUs(40000);  /* inter-frame gap */
    }
}

/* ══════════════════════════════════════════════════════════
 *  SONY SIRC-12 PROTOCOL
 *  Leader: 2400µs mark + 600µs space
 *  Bit 1:  1200µs mark + 600µs space
 *  Bit 0:  600µs mark  + 600µs space
 *  7 command bits (LSB first) + 5 address bits (LSB first)
 * ══════════════════════════════════════════════════════════ */
void IRController::_sendSonySIRC12(uint16_t addr, uint16_t cmd, uint8_t repeat) {
    for (uint8_t r = 0; r < repeat; r++) {
        _markUs(2400);
        _spaceUs(600);
        for (int i = 0; i < 7; i++) {
            _markUs((cmd & (1 << i)) ? 1200 : 600);
            _spaceUs(600);
        }
        for (int i = 0; i < 5; i++) {
            _markUs((addr & (1 << i)) ? 1200 : 600);
            _spaceUs(600);
        }
        _spaceUs(45000);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SAMSUNG PROTOCOL (same as NEC but no ~address)
 * ══════════════════════════════════════════════════════════ */
void IRController::_sendSamsung(uint16_t addr, uint16_t cmd, uint8_t repeat) {
    for (uint8_t r = 0; r < repeat; r++) {
        _markUs(4500);
        _spaceUs(4500);
        uint8_t a = addr & 0xFF;
        uint8_t c = cmd & 0xFF;
        uint8_t nc = ~c;
        uint32_t data = ((uint32_t)nc << 24) | ((uint32_t)c << 16) |
                        ((uint32_t)a << 8) | a;
        for (int i = 0; i < 32; i++) {
            _markUs(560);
            _spaceUs((data & (1UL << i)) ? 1690 : 560);
        }
        _markUs(560);
        _spaceUs(40000);
    }
}

/* ══════════════════════════════════════════════════════════
 *  LG PROTOCOL
 *  Leader: 8500µs mark + 4250µs space
 *  Bit 1:  530µs mark + 1590µs space
 *  Bit 0:  530µs mark + 530µs space
 *  28-bit payload: 8-bit addr + 16-bit cmd + 4-bit checksum
 * ══════════════════════════════════════════════════════════ */
void IRController::_sendLG(uint16_t addr, uint16_t cmd, uint8_t repeat) {
    /* Compute 4-bit checksum: sum of nibbles of cmd */
    uint8_t chk = ((cmd >> 12) & 0xF) + ((cmd >> 8) & 0xF) +
                  ((cmd >> 4) & 0xF) + (cmd & 0xF);
    chk &= 0xF;
    uint32_t payload = ((uint32_t)(addr & 0xFF) << 20) |
                       ((uint32_t)(cmd & 0xFFFF) << 4) | chk;
    for (uint8_t r = 0; r < repeat; r++) {
        _markUs(8500);
        _spaceUs(4250);
        for (int i = 27; i >= 0; i--) {
            _markUs(530);
            _spaceUs((payload & (1UL << i)) ? 1590 : 530);
        }
        _markUs(530);
        _spaceUs(60000);
    }
}

/* ══════════════════════════════════════════════════════════
 *  RAW PROTOCOL
 *  data[] alternates mark/space durations in microseconds
 * ══════════════════════════════════════════════════════════ */
void IRController::_sendRaw(const uint16_t* data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        if (i % 2 == 0) _markUs(data[i]);
        else             _spaceUs(data[i]);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SEND (public)
 * ══════════════════════════════════════════════════════════ */
IRResult IRController::send(const IRCommand& cmd) {
    IRResult res;
    res.success = true;

    /* Disable interrupts during transmission for timing accuracy */
    portDISABLE_INTERRUPTS();

    switch (cmd.protocol) {
        case IR_PROTO_NEC:
            _sendNEC(cmd.address, cmd.command, cmd.repeat ? cmd.repeat : 1);
            snprintf(res.message, sizeof(res.message),
                     "NEC addr=0x%02X cmd=0x%02X x%d", cmd.address, cmd.command, cmd.repeat);
            break;
        case IR_PROTO_SONY_SIRC12:
            _sendSonySIRC12(cmd.address, cmd.command, cmd.repeat ? cmd.repeat : 3);
            snprintf(res.message, sizeof(res.message),
                     "SONY addr=0x%02X cmd=0x%02X x%d", cmd.address, cmd.command, cmd.repeat);
            break;
        case IR_PROTO_SAMSUNG:
            _sendSamsung(cmd.address, cmd.command, cmd.repeat ? cmd.repeat : 1);
            snprintf(res.message, sizeof(res.message),
                     "SAMSUNG addr=0x%02X cmd=0x%02X x%d", cmd.address, cmd.command, cmd.repeat);
            break;
        case IR_PROTO_LG:
            _sendLG(cmd.address, cmd.command, cmd.repeat ? cmd.repeat : 1);
            snprintf(res.message, sizeof(res.message),
                     "LG addr=0x%02X cmd=0x%04X x%d", cmd.address, cmd.command, cmd.repeat);
            break;
        case IR_PROTO_RAW:
            if (cmd.rawData && cmd.rawLen > 0) {
                _sendRaw(cmd.rawData, cmd.rawLen);
                snprintf(res.message, sizeof(res.message),
                         "RAW %d pulses sent", cmd.rawLen);
            } else {
                res.success = false;
                snprintf(res.message, sizeof(res.message), "RAW: no data");
            }
            break;
        default:
            res.success = false;
            snprintf(res.message, sizeof(res.message), "Unknown protocol");
            break;
    }

    portENABLE_INTERRUPTS();

    Serial.printf("[IR] %s\n", res.message);
    return res;
}

/* ══════════════════════════════════════════════════════════
 *  SEND FROM JSON (agent tool call handler)
 *  Expected JSON:
 *  { "type":"ir_command", "protocol":"NEC",
 *    "address":0, "command":16, "repeat":1 }
 * ══════════════════════════════════════════════════════════ */
IRResult IRController::sendFromJson(const char* json) {
    IRResult res;
    res.success = false;
    snprintf(res.message, sizeof(res.message), "Parse error");

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) return res;

    const char* protoStr = doc["protocol"] | "NEC";
    IRCommand cmd;
    cmd.address  = doc["address"]  | 0;
    cmd.command  = doc["command"]  | 0;
    cmd.repeat   = doc["repeat"]   | 1;
    cmd.rawData  = nullptr;
    cmd.rawLen   = 0;

    if      (strcmp(protoStr, "NEC")      == 0) cmd.protocol = IR_PROTO_NEC;
    else if (strcmp(protoStr, "SONY")     == 0) cmd.protocol = IR_PROTO_SONY_SIRC12;
    else if (strcmp(protoStr, "SAMSUNG")  == 0) cmd.protocol = IR_PROTO_SAMSUNG;
    else if (strcmp(protoStr, "LG")       == 0) cmd.protocol = IR_PROTO_LG;
    else {
        snprintf(res.message, sizeof(res.message), "Unknown protocol: %s", protoStr);
        return res;
    }

    return send(cmd);
}
