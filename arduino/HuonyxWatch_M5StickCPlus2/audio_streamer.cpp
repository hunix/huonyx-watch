/**
 * audio_streamer.cpp
 * Huonyx Watch M5StickC Plus2 — Voice-to-Huonyx Audio Pipeline
 */

#include "audio_streamer.h"

/* ── I2S port and pin config (SPM1423 on M5StickC Plus2) ─ */
static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int        I2S_CLK  = MIC_CLK_PIN;   /* G0 */
static const int        I2S_DATA = MIC_DATA_PIN;   /* G34 */

/* ── Chunk buffer (PSRAM-backed) ──────────────────────── */
static uint8_t _chunkBuf[AUDIO_CHUNK_BYTES];
static size_t  _chunkPos = 0;

/* ══════════════════════════════════════════════════════════
 *  FreeRTOS Sampling Task (Core 0)
 *  Reads I2S DMA data and pushes raw bytes into the queue.
 *  This task NEVER touches WiFi or WebSockets.
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::_samplingTask(void* param) {
    AudioStreamer* self = static_cast<AudioStreamer*>(param);
    uint8_t dmaBuf[AUDIO_DMA_BUF_LEN * 2];  /* 16-bit samples */

    while (true) {
        if (self->_state != STREAMER_RECORDING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        size_t bytesRead = 0;
        esp_err_t err = i2s_read(I2S_PORT, dmaBuf, sizeof(dmaBuf),
                                  &bytesRead, pdMS_TO_TICKS(50));
        if (err != ESP_OK || bytesRead == 0) continue;

        /* Push raw bytes to queue in AUDIO_CHUNK_BYTES chunks */
        for (size_t i = 0; i < bytesRead; i++) {
            uint8_t byte = dmaBuf[i];
            if (xQueueSend(self->_queue, &byte, 0) != pdTRUE) {
                /* Queue full — drop oldest byte (overflow protection) */
                uint8_t dummy;
                xQueueReceive(self->_queue, &dummy, 0);
                xQueueSend(self->_queue, &byte, 0);
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  AudioStreamer
 * ══════════════════════════════════════════════════════════ */
AudioStreamer::AudioStreamer()
    : _state(STREAMER_IDLE)
    , _mode(REC_STANDARD)
    , _i2sInitialized(false)
    , _recordStartMs(0)
    , _samplingTaskHandle(nullptr)
    , _onStateChange(nullptr)
    , _onChunk(nullptr)
{
    _queue = xQueueCreate(AUDIO_QUEUE_DEPTH * AUDIO_DMA_BUF_LEN * 2, sizeof(uint8_t));
}

bool AudioStreamer::begin() {
    if (!_queue) {
        Serial.println("[Audio] Failed to create queue");
        return false;
    }

    /* Create the sampling task pinned to Core 0 */
    BaseType_t ret = xTaskCreatePinnedToCore(
        _samplingTask,
        "AudioSample",
        4096,       /* Stack size */
        this,       /* Parameter */
        10,         /* Priority (high) */
        &_samplingTaskHandle,
        0           /* Core 0 */
    );

    if (ret != pdPASS) {
        Serial.println("[Audio] Failed to create sampling task");
        return false;
    }

    Serial.println("[Audio] Streamer initialized");
    return true;
}

void AudioStreamer::_initI2S() {
    if (_i2sInitialized) return;

    i2s_config_t i2sCfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate          = AUDIO_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_PCM_SHORT,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = AUDIO_DMA_BUF_COUNT,
        .dma_buf_len          = AUDIO_DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = false,
        .fixed_mclk           = 0,
    };

    i2s_pin_config_t pinCfg = {
        .bck_io_num   = I2S_PIN_NO_CHANGE,
        .ws_io_num    = I2S_CLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num  = I2S_DATA,
    };

    esp_err_t err = i2s_driver_install(I2S_PORT, &i2sCfg, 0, nullptr);
    if (err != ESP_OK) {
        Serial.printf("[Audio] I2S install failed: %d\n", err);
        return;
    }

    err = i2s_set_pin(I2S_PORT, &pinCfg);
    if (err != ESP_OK) {
        Serial.printf("[Audio] I2S pin config failed: %d\n", err);
        return;
    }

    i2s_start(I2S_PORT);
    _i2sInitialized = true;
    Serial.println("[Audio] I2S started");
}

void AudioStreamer::_deinitI2S() {
    if (!_i2sInitialized) return;
    i2s_stop(I2S_PORT);
    i2s_driver_uninstall(I2S_PORT);
    _i2sInitialized = false;
    Serial.println("[Audio] I2S stopped");
}

void AudioStreamer::_setState(StreamerState s) {
    if (_state == s) return;
    _state = s;
    if (_onStateChange) _onStateChange(s);
}

void AudioStreamer::startRecording(RecordMode mode) {
    if (_state == STREAMER_RECORDING) return;
    _mode = mode;
    _recordStartMs = millis();
    xQueueReset(_queue);
    _chunkPos = 0;
    _initI2S();
    _setState(STREAMER_RECORDING);
    Serial.printf("[Audio] Recording started (mode=%d)\n", mode);
}

void AudioStreamer::stopRecording() {
    if (_state != STREAMER_RECORDING) return;
    _setState(STREAMER_SENDING);

    /* Flush remaining bytes in chunk buffer */
    if (_chunkPos > 0 && _onChunk) {
        _onChunk(_chunkBuf, _chunkPos);
        _chunkPos = 0;
    }

    /* Send end-of-stream marker (4 zero bytes) */
    static const uint8_t EOS[4] = {0, 0, 0, 0};
    if (_onChunk) _onChunk(EOS, sizeof(EOS));

    _deinitI2S();
    _setState(STREAMER_IDLE);
    Serial.printf("[Audio] Recording stopped (%lums)\n",
                  millis() - _recordStartMs);
}

uint32_t AudioStreamer::recordingDurationMs() const {
    if (_state != STREAMER_RECORDING) return 0;
    return millis() - _recordStartMs;
}

/* ── Main loop (Core 1) ───────────────────────────────── */
void AudioStreamer::loop() {
    if (_state != STREAMER_RECORDING) return;

    uint8_t byte;
    while (xQueueReceive(_queue, &byte, 0) == pdTRUE) {
        _chunkBuf[_chunkPos++] = byte;
        if (_chunkPos >= AUDIO_CHUNK_BYTES) {
            if (_onChunk) _onChunk(_chunkBuf, AUDIO_CHUNK_BYTES);
            _chunkPos = 0;
        }
    }
}
