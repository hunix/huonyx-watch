/**
 * ir_controller.cpp
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 */

#include "ir_controller.h"

IrController::IrController()
    : _ir(IR_TX_PIN)   /* GPIO19 */
    , _onResult(nullptr)
{}

bool IrController::begin() {
    _ir.begin();
    Serial.printf("[IR] Controller initialized on GPIO%d\n", IR_TX_PIN);
    return true;
}

IrResult IrController::handleCommand(const JsonObject& cmd) {
    const char* protocol = cmd["protocol"] | "NEC";
    uint32_t address     = cmd["address"] | 0;
    uint32_t command     = cmd["command"] | 0;
    uint8_t  repeat      = cmd["repeat"]  | 1;

    IrResult result = IR_UNKNOWN_PROTOCOL;

    if (strcmp(protocol, "NEC") == 0) {
        result = sendNEC((uint16_t)address, (uint16_t)command, repeat);
    } else if (strcmp(protocol, "SONY") == 0) {
        uint8_t bits = cmd["bits"] | 12;
        result = sendSony((uint16_t)address, (uint16_t)command, bits);
    } else if (strcmp(protocol, "SAMSUNG") == 0) {
        result = sendSamsung((uint16_t)address, (uint16_t)command);
    } else if (strcmp(protocol, "LG") == 0) {
        result = sendLG((uint8_t)address, (uint16_t)command);
    } else if (strcmp(protocol, "PANASONIC") == 0) {
        result = sendPanasonic((uint16_t)address, (uint32_t)command);
    } else if (strcmp(protocol, "RC5") == 0) {
        result = sendRC5((uint8_t)address, (uint8_t)command);
    } else if (strcmp(protocol, "RAW") == 0) {
        /* RAW: command field is unused; data array must be in "data" key */
        JsonArrayConst rawData = cmd["data"];
        if (rawData.isNull() || rawData.size() == 0) {
            result = IR_INVALID_COMMAND;
        } else {
            uint16_t len = rawData.size();
            uint16_t* buf = (uint16_t*)malloc(len * sizeof(uint16_t));
            if (buf) {
                for (uint16_t i = 0; i < len; i++) buf[i] = rawData[i];
                uint16_t freq = cmd["freq"] | 38;
                result = sendRaw(buf, len, freq);
                free(buf);
            } else {
                result = IR_INVALID_COMMAND;
            }
        }
    }

    if (_onResult) _onResult(result, protocol);

    Serial.printf("[IR] %s %s addr=0x%04X cmd=0x%04X\n",
                  protocol,
                  result == IR_OK ? "OK" : "FAIL",
                  address, command);
    return result;
}

IrResult IrController::sendNEC(uint16_t address, uint16_t command, uint8_t repeat) {
    /* NEC protocol: 32-bit (address 8-bit + ~address + command 8-bit + ~command) */
    uint64_t data = ((uint64_t)address << 24) |
                    ((uint64_t)(~address & 0xFF) << 16) |
                    ((uint64_t)command << 8) |
                    ((uint64_t)(~command & 0xFF));
    for (uint8_t i = 0; i < repeat; i++) {
        _ir.sendNEC(data, 32);
        if (i < repeat - 1) delay(40);
    }
    return IR_OK;
}

IrResult IrController::sendSony(uint16_t address, uint16_t command, uint8_t bits) {
    _ir.sendSony(((uint32_t)address << 7) | (command & 0x7F), bits);
    return IR_OK;
}

IrResult IrController::sendSamsung(uint16_t address, uint16_t command) {
    uint64_t data = ((uint64_t)address << 24) |
                    ((uint64_t)(address) << 16) |
                    ((uint64_t)command << 8) |
                    ((uint64_t)(~command & 0xFF));
    _ir.sendSAMSUNG(data, 32);
    return IR_OK;
}

IrResult IrController::sendLG(uint8_t address, uint16_t command) {
    uint32_t data = ((uint32_t)address << 16) | command;
    _ir.sendLG(data, 28);
    return IR_OK;
}

IrResult IrController::sendPanasonic(uint16_t address, uint32_t command) {
    _ir.sendPanasonic(address, command);
    return IR_OK;
}

IrResult IrController::sendRC5(uint8_t address, uint8_t command) {
    _ir.sendRC5(_ir.encodeRC5(address, command), RC5_BITS);
    return IR_OK;
}

IrResult IrController::sendRaw(const uint16_t* data, uint16_t len, uint16_t freq) {
    _ir.sendRaw(data, len, freq);
    return IR_OK;
}
