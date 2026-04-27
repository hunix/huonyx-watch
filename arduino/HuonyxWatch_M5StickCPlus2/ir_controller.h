/**
 * ir_controller.h
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 *
 * Exposes the M5StickC Plus2's integrated IR emitter as a tool
 * that the Huonyx agent can call via WebSocket JSON commands.
 *
 * The agent sends a JSON command to the GatewayClient:
 *   {
 *     "type": "ir_command",
 *     "protocol": "NEC",
 *     "address": 0x0,
 *     "command": 0x10,
 *     "repeat": 1
 *   }
 *
 * Supported protocols: NEC, SONY, RC5, SAMSUNG, LG, PANASONIC, RAW
 *
 * IR emitter is on GPIO19 (shared with Red LED on M5StickC Plus2).
 * The IRremoteESP8266 library is used for encoding.
 *
 * Practical use cases:
 *   - "Turn off the TV" → agent sends NEC TV power command
 *   - "Set AC to 22 degrees" → agent sends Panasonic AC command
 *   - "Mute the projector" → agent sends Sony mute command
 */

#pragma once
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ArduinoJson.h>
#include "hw_config.h"

/* ── IR command result ────────────────────────────────── */
enum IrResult {
    IR_OK,
    IR_UNKNOWN_PROTOCOL,
    IR_INVALID_COMMAND,
};

typedef void (*IrResultCallback)(IrResult result, const char* protocol);

class IrController {
public:
    IrController();
    bool begin();

    /* ── Command dispatch ───────────────────────────────── */
    /* Called when the GatewayClient receives an ir_command JSON */
    IrResult handleCommand(const JsonObject& cmd);

    /* ── Direct API ─────────────────────────────────────── */
    IrResult sendNEC(uint16_t address, uint16_t command, uint8_t repeat = 1);
    IrResult sendSony(uint16_t address, uint16_t command, uint8_t bits = 12);
    IrResult sendSamsung(uint16_t address, uint16_t command);
    IrResult sendLG(uint8_t address, uint16_t command);
    IrResult sendPanasonic(uint16_t address, uint32_t command);
    IrResult sendRC5(uint8_t address, uint8_t command);
    IrResult sendRaw(const uint16_t* data, uint16_t len, uint16_t freq = 38);

    /* ── Callback ───────────────────────────────────────── */
    void onResult(IrResultCallback cb) { _onResult = cb; }

private:
    IRsend          _ir;
    IrResultCallback _onResult;
};
