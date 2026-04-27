/**
 * ir_controller.h
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 *
 * Exposes the M5StickC Plus2's built-in IR emitter (GPIO19) as a
 * Huonyx agent tool. Zero external libraries — pure Arduino GPIO
 * bit-banging with delayMicroseconds() for carrier generation.
 *
 * The agent sends a JSON command via WebSocket:
 *   { "type":"ir_command", "protocol":"NEC",
 *     "address":0x0, "command":0x10, "repeat":1 }
 *
 * Supported protocols: NEC, SONY_SIRC12, SAMSUNG, LG, RAW
 *
 * GPIO19 is shared with the Red LED. The LED is disabled during
 * IR transmission (RMT owns the pin). Do not call led_blink()
 * while IR is transmitting.
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "hw_config.h"

/* ── IR carrier frequency ─────────────────────────────── */
#define IR_CARRIER_HZ   38000   /* 38 kHz standard */
#define IR_HALF_PERIOD  (1000000 / IR_CARRIER_HZ / 2)  /* ~13 µs */

/* ── Supported protocols ──────────────────────────────── */
enum IRProtocol {
    IR_PROTO_NEC = 0,
    IR_PROTO_SONY_SIRC12,
    IR_PROTO_SAMSUNG,
    IR_PROTO_LG,
    IR_PROTO_RAW
};

/* ── IR command struct ────────────────────────────────── */
struct IRCommand {
    IRProtocol  protocol;
    uint16_t    address;
    uint16_t    command;
    uint8_t     repeat;        /* number of times to send */
    uint16_t*   rawData;       /* only for IR_PROTO_RAW */
    uint16_t    rawLen;        /* length of rawData array */
};

/* ── Result ───────────────────────────────────────────── */
struct IRResult {
    bool    success;
    char    message[64];
};

class IRController {
public:
    IRController();

    void begin();

    /* Send a structured IR command */
    IRResult send(const IRCommand& cmd);

    /* Parse and send from a JSON string received from the agent */
    IRResult sendFromJson(const char* json);

private:
    uint8_t _pin;

    /* Low-level carrier burst helpers */
    void _markUs(uint16_t us);
    void _spaceUs(uint16_t us);

    /* Protocol encoders */
    void _sendNEC(uint16_t addr, uint16_t cmd, uint8_t repeat);
    void _sendSonySIRC12(uint16_t addr, uint16_t cmd, uint8_t repeat);
    void _sendSamsung(uint16_t addr, uint16_t cmd, uint8_t repeat);
    void _sendLG(uint16_t addr, uint16_t cmd, uint8_t repeat);
    void _sendRaw(const uint16_t* data, uint16_t len);
};
