/**
 * ir_controller.h
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 *
 * Uses the ESP32 RMT peripheral directly — NO external IR library required.
 * The M5StickC Plus2 has a built-in IR LED on GPIO19 (active-high).
 *
 * Supported protocols: NEC, Sony SIRC, Samsung, LG, Panasonic, RC5, RAW
 *
 * Agent command format (JSON via WebSocket):
 *   { "type": "ir_command", "protocol": "NEC", "address": 0, "command": 16 }
 *   { "type": "ir_command", "protocol": "RAW", "freq": 38, "data": [9000,4500,...] }
 */
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <driver/rmt.h>
#include "hw_config.h"

/* ── RMT channel & timing ────────────────────────────────────────────────── */
#ifndef IR_TX_PIN
  #define IR_TX_PIN     19   /* M5StickC Plus2 built-in IR LED GPIO */
#endif
#define IR_RMT_CHANNEL  RMT_CHANNEL_0
#define IR_CARRIER_HZ   38000         /* 38 kHz carrier */

/* ── Result codes ────────────────────────────────────────────────────────── */
enum IrResult : uint8_t {
    IR_OK               = 0,
    IR_UNKNOWN_PROTOCOL = 1,
    IR_INVALID_COMMAND  = 2,
    IR_SEND_FAILED      = 3,
};

typedef void (*IrResultCallback)(IrResult result, const char* protocol);

/* ── IrController class ──────────────────────────────────────────────────── */
class IrController {
public:
    IrController();
    bool begin();

    /* Called when the GatewayClient receives an ir_command JSON */
    IrResult handleCommand(const JsonObject& cmd);

    /* Direct protocol API */
    IrResult sendNEC(uint16_t address, uint16_t command, uint8_t repeat = 1);
    IrResult sendSony(uint16_t address, uint16_t command, uint8_t bits = 12);
    IrResult sendSamsung(uint16_t address, uint16_t command);
    IrResult sendLG(uint8_t address, uint16_t command);
    IrResult sendPanasonic(uint16_t address, uint32_t command);
    IrResult sendRC5(uint8_t address, uint8_t command);
    IrResult sendRaw(const uint16_t* timings, uint16_t len, uint16_t freqKHz = 38);

    void onResult(IrResultCallback cb) { _onResult = cb; }

private:
    bool             _initialized;
    IrResultCallback _onResult;

    /* Low-level RMT helpers */
    void _rmtSendBits(uint64_t data, uint8_t numBits,
                      uint16_t headerMarkUs, uint16_t headerSpaceUs,
                      uint16_t bitMarkUs,
                      uint16_t oneSpaceUs, uint16_t zeroSpaceUs,
                      uint16_t tailMarkUs);
    void _rmtSendRaw(const uint16_t* timings, uint16_t len, uint16_t freqKHz);
};
