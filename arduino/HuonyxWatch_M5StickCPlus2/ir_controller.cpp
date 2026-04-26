/**
 * ir_controller.cpp
 * Huonyx Watch M5StickC Plus2 — Agentic IR Remote Controller
 *
 * Native ESP32 RMT implementation — no IRremoteESP8266 library needed.
 * Protocols: NEC, Sony SIRC, Samsung, LG, Panasonic, RC5, RAW
 */
#include "ir_controller.h"

/* ── RMT tick resolution: 1 µs (APB clock / 80) ─────────────────────────── */
#define RMT_CLK_DIV   80
#define US_TO_TICKS(us) ((us))   /* 1 tick = 1 µs at 80 MHz / 80 */

/* ── NEC timings (µs) ───────────────────────────────────────────────────── */
#define NEC_HDR_MARK   9000
#define NEC_HDR_SPACE  4500
#define NEC_BIT_MARK    560
#define NEC_ONE_SPACE  1690
#define NEC_ZERO_SPACE  560
#define NEC_TAIL_MARK   560

/* ── Sony SIRC timings (µs) ─────────────────────────────────────────────── */
#define SONY_HDR_MARK  2400
#define SONY_HDR_SPACE  600
#define SONY_BIT_MARK   600
#define SONY_ONE_SPACE 1200
#define SONY_ZERO_SPACE 600

/* ── Samsung timings (µs) ───────────────────────────────────────────────── */
#define SAM_HDR_MARK   4500
#define SAM_HDR_SPACE  4500
#define SAM_BIT_MARK    560
#define SAM_ONE_SPACE  1690
#define SAM_ZERO_SPACE  560
#define SAM_TAIL_MARK   560

/* ── LG timings (µs) ────────────────────────────────────────────────────── */
#define LG_HDR_MARK    9000
#define LG_HDR_SPACE   4500
#define LG_BIT_MARK     560
#define LG_ONE_SPACE   1690
#define LG_ZERO_SPACE   560
#define LG_TAIL_MARK    560

/* ── Panasonic timings (µs) ─────────────────────────────────────────────── */
#define PAN_HDR_MARK   3456
#define PAN_HDR_SPACE  1728
#define PAN_BIT_MARK    432
#define PAN_ONE_SPACE  1296
#define PAN_ZERO_SPACE  432
#define PAN_TAIL_MARK   432

/* ── RC5 timings (µs) ───────────────────────────────────────────────────── */
#define RC5_T1          889   /* half-bit period */

/* ── Max RMT items for one frame ────────────────────────────────────────── */
#define RMT_MAX_ITEMS  128

/* ─────────────────────────────────────────────────────────────────────────
 * Constructor / begin
 * ───────────────────────────────────────────────────────────────────────── */
IrController::IrController() : _initialized(false), _onResult(nullptr) {}

bool IrController::begin() {
    rmt_config_t cfg = {};
    cfg.rmt_mode                  = RMT_MODE_TX;
    cfg.channel                   = IR_RMT_CHANNEL;
    cfg.gpio_num                  = (gpio_num_t)IR_TX_PIN;
    cfg.clk_div                   = RMT_CLK_DIV;
    cfg.mem_block_num             = 1;
    cfg.tx_config.loop_en         = false;
    cfg.tx_config.carrier_en      = true;
    cfg.tx_config.carrier_freq_hz = IR_CARRIER_HZ;
    cfg.tx_config.carrier_duty_percent = 33;
    cfg.tx_config.carrier_level   = RMT_CARRIER_LEVEL_HIGH;
    cfg.tx_config.idle_output_en  = true;
    cfg.tx_config.idle_level      = RMT_IDLE_LEVEL_LOW;

    esp_err_t err = rmt_config(&cfg);
    if (err != ESP_OK) {
        Serial.printf("[IR] rmt_config failed: %d\n", err);
        return false;
    }
    err = rmt_driver_install(IR_RMT_CHANNEL, 0, 0);
    if (err != ESP_OK) {
        Serial.printf("[IR] rmt_driver_install failed: %d\n", err);
        return false;
    }
    _initialized = true;
    Serial.printf("[IR] RMT initialized on GPIO%d @ %d Hz\n", IR_TX_PIN, IR_CARRIER_HZ);
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
 * handleCommand — dispatch from agent JSON
 * ───────────────────────────────────────────────────────────────────────── */
IrResult IrController::handleCommand(const JsonObject& cmd) {
    const char* protocol = cmd["protocol"] | "NEC";
    uint32_t address     = cmd["address"]  | 0;
    uint32_t command     = cmd["command"]  | 0;
    uint8_t  repeat      = cmd["repeat"]   | 1;
    IrResult result      = IR_UNKNOWN_PROTOCOL;

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

/* ─────────────────────────────────────────────────────────────────────────
 * _rmtSendBits — generic bit-bang via RMT items
 * MSB first, mark/space encoding
 * ───────────────────────────────────────────────────────────────────────── */
void IrController::_rmtSendBits(uint64_t data, uint8_t numBits,
                                  uint16_t headerMarkUs, uint16_t headerSpaceUs,
                                  uint16_t bitMarkUs,
                                  uint16_t oneSpaceUs, uint16_t zeroSpaceUs,
                                  uint16_t tailMarkUs) {
    if (!_initialized) return;

    rmt_item32_t items[RMT_MAX_ITEMS];
    int idx = 0;

    /* Header */
    items[idx].level0    = 1; items[idx].duration0 = US_TO_TICKS(headerMarkUs);
    items[idx].level1    = 0; items[idx].duration1 = US_TO_TICKS(headerSpaceUs);
    idx++;

    /* Data bits — MSB first */
    for (int8_t bit = numBits - 1; bit >= 0 && idx < RMT_MAX_ITEMS - 2; bit--) {
        bool one = (data >> bit) & 1;
        items[idx].level0    = 1; items[idx].duration0 = US_TO_TICKS(bitMarkUs);
        items[idx].level1    = 0; items[idx].duration1 = US_TO_TICKS(one ? oneSpaceUs : zeroSpaceUs);
        idx++;
    }

    /* Tail mark */
    items[idx].level0    = 1; items[idx].duration0 = US_TO_TICKS(tailMarkUs);
    items[idx].level1    = 0; items[idx].duration1 = 0;
    idx++;

    /* Terminator */
    items[idx].val = 0;
    idx++;

    rmt_write_items(IR_RMT_CHANNEL, items, idx, true);
    rmt_wait_tx_done(IR_RMT_CHANNEL, pdMS_TO_TICKS(200));
}

/* ─────────────────────────────────────────────────────────────────────────
 * _rmtSendRaw — send raw timing array via RMT
 * timings[] alternates mark/space in µs, starting with mark
 * ───────────────────────────────────────────────────────────────────────── */
void IrController::_rmtSendRaw(const uint16_t* timings, uint16_t len, uint16_t freqKHz) {
    if (!_initialized) return;

    /* Reconfigure carrier frequency if different */
    rmt_set_tx_carrier(IR_RMT_CHANNEL, true, freqKHz * 1000, freqKHz * 1000 / 3, RMT_CARRIER_LEVEL_HIGH);

    rmt_item32_t items[RMT_MAX_ITEMS];
    uint16_t itemCount = 0;
    uint16_t pairs = len / 2;
    if (pairs > RMT_MAX_ITEMS - 1) pairs = RMT_MAX_ITEMS - 1;

    for (uint16_t i = 0; i < pairs; i++) {
        items[itemCount].level0    = 1;
        items[itemCount].duration0 = US_TO_TICKS(timings[i * 2]);
        items[itemCount].level1    = 0;
        items[itemCount].duration1 = US_TO_TICKS(timings[i * 2 + 1]);
        itemCount++;
    }
    /* Handle odd trailing mark */
    if (len % 2 == 1 && itemCount < RMT_MAX_ITEMS - 1) {
        items[itemCount].level0    = 1;
        items[itemCount].duration0 = US_TO_TICKS(timings[len - 1]);
        items[itemCount].level1    = 0;
        items[itemCount].duration1 = 0;
        itemCount++;
    }
    items[itemCount].val = 0;
    itemCount++;

    rmt_write_items(IR_RMT_CHANNEL, items, itemCount, true);
    rmt_wait_tx_done(IR_RMT_CHANNEL, pdMS_TO_TICKS(500));

    /* Restore default 38 kHz carrier */
    rmt_set_tx_carrier(IR_RMT_CHANNEL, true, IR_CARRIER_HZ, IR_CARRIER_HZ / 3, RMT_CARRIER_LEVEL_HIGH);
}

/* ─────────────────────────────────────────────────────────────────────────
 * Protocol implementations
 * ───────────────────────────────────────────────────────────────────────── */
IrResult IrController::sendNEC(uint16_t address, uint16_t command, uint8_t repeat) {
    /* NEC: 8-bit addr + ~addr + 8-bit cmd + ~cmd = 32 bits */
    uint32_t data = ((uint32_t)(address & 0xFF) << 24) |
                    ((uint32_t)((~address) & 0xFF) << 16) |
                    ((uint32_t)(command & 0xFF) << 8) |
                    ((uint32_t)((~command) & 0xFF));
    for (uint8_t i = 0; i < repeat; i++) {
        _rmtSendBits(data, 32,
                     NEC_HDR_MARK, NEC_HDR_SPACE,
                     NEC_BIT_MARK, NEC_ONE_SPACE, NEC_ZERO_SPACE,
                     NEC_TAIL_MARK);
        if (i < repeat - 1) delay(40);
    }
    return IR_OK;
}

IrResult IrController::sendSony(uint16_t address, uint16_t command, uint8_t bits) {
    /* Sony SIRC: command (7 bits) + address (5 or 8 bits) */
    uint32_t data = ((uint32_t)(address & 0x1F) << 7) | (command & 0x7F);
    /* Sony requires 3 transmissions */
    for (uint8_t i = 0; i < 3; i++) {
        _rmtSendBits(data, bits,
                     SONY_HDR_MARK, SONY_HDR_SPACE,
                     SONY_BIT_MARK, SONY_ONE_SPACE, SONY_ZERO_SPACE,
                     0);
        delay(25);
    }
    return IR_OK;
}

IrResult IrController::sendSamsung(uint16_t address, uint16_t command) {
    uint32_t data = ((uint32_t)(address & 0xFF) << 24) |
                    ((uint32_t)(address & 0xFF) << 16) |
                    ((uint32_t)(command & 0xFF) << 8) |
                    ((uint32_t)((~command) & 0xFF));
    _rmtSendBits(data, 32,
                 SAM_HDR_MARK, SAM_HDR_SPACE,
                 SAM_BIT_MARK, SAM_ONE_SPACE, SAM_ZERO_SPACE,
                 SAM_TAIL_MARK);
    return IR_OK;
}

IrResult IrController::sendLG(uint8_t address, uint16_t command) {
    uint32_t data = ((uint32_t)(address & 0xFF) << 20) |
                    ((uint32_t)(command & 0xFFFF) << 4);
    /* Simple 4-bit checksum */
    uint8_t chk = 0;
    uint32_t tmp = command;
    for (int i = 0; i < 4; i++) { chk += (tmp & 0xF); tmp >>= 4; }
    data |= (chk & 0xF);
    _rmtSendBits(data, 28,
                 LG_HDR_MARK, LG_HDR_SPACE,
                 LG_BIT_MARK, LG_ONE_SPACE, LG_ZERO_SPACE,
                 LG_TAIL_MARK);
    return IR_OK;
}

IrResult IrController::sendPanasonic(uint16_t address, uint32_t command) {
    /* Panasonic: 48-bit frame = 0x4004 prefix (16) + address (16) + command (16) */
    uint64_t data = ((uint64_t)0x4004 << 32) |
                    ((uint64_t)(address & 0xFFFF) << 16) |
                    (command & 0xFFFF);
    _rmtSendBits(data, 48,
                 PAN_HDR_MARK, PAN_HDR_SPACE,
                 PAN_BIT_MARK, PAN_ONE_SPACE, PAN_ZERO_SPACE,
                 PAN_TAIL_MARK);
    return IR_OK;
}

IrResult IrController::sendRC5(uint8_t address, uint8_t command) {
    /* RC5: 14 bits — start(1) + field(1) + toggle(0) + address(5) + command(6) */
    uint16_t data = 0x3000 | ((address & 0x1F) << 6) | (command & 0x3F);
    /* RC5 uses Manchester encoding — build raw timings */
    uint16_t timings[30];
    uint8_t  tIdx = 0;
    bool     lastLevel = false;
    for (int8_t bit = 13; bit >= 0; bit--) {
        bool level = (data >> bit) & 1;
        if (level != lastLevel) {
            timings[tIdx++] = RC5_T1;
            timings[tIdx++] = RC5_T1;
        } else {
            timings[tIdx++] = RC5_T1 * 2;
        }
        lastLevel = level;
    }
    _rmtSendRaw(timings, tIdx, 36);
    return IR_OK;
}

IrResult IrController::sendRaw(const uint16_t* timings, uint16_t len, uint16_t freqKHz) {
    _rmtSendRaw(timings, len, freqKHz);
    return IR_OK;
}
