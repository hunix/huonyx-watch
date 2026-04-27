/**
 * audio_streamer.cpp
 * Huonyx Watch M5StickC Plus2 — Voice-to-Huonyx Audio Pipeline
 *
 * Uses ESP32 Arduino 3.x I2S.h — NO driver/i2s.h legacy API.
 * SPM1423 PDM microphone: CLK=GPIO0, DATA=GPIO34.
 */
#include "audio_streamer.h"

/* ── Static I2S instance ───────────────────────────────── */
static I2SClass i2sMic;

/* ── Chunk buffer ──────────────────────────────────────── */
static uint8_t  _chunkBuf[AUDIO_CHUNK_BYTES];
static size_t   _chunkPos = 0;

/* ══════════════════════════════════════════════════════════
 *  FreeRTOS Sampling Task (Core 0)
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::_samplingTask(void* param) {
    AudioStreamer* self = static_cast<AudioStreamer*>(param);
    int16_t sample;
    while (true) {
        if (self->_state != STREAMER_RECORDING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        /* Read one 16-bit sample from I2S */
        int bytesRead = i2sMic.readBytes((char*)&sample, sizeof(sample));
        if (bytesRead <= 0) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        /* Push bytes into queue */
        uint8_t* b = (uint8_t*)&sample;
        for (int i = 0; i < (int)sizeof(sample); i++) {
            if (xQueueSend(self->_queue, &b[i], 0) != pdTRUE) {
                uint8_t dummy;
                xQueueReceive(self->_queue, &dummy, 0);
                xQueueSend(self->_queue, &b[i], 0);
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  CONSTRUCTOR
 * ══════════════════════════════════════════════════════════ */
AudioStreamer::AudioStreamer()
    : _state(STREAMER_IDLE)
    , _mode(REC_STANDARD)
    , _i2sInitialized(false)
    , _recordStartMs(0)
    , _samplingTaskHandle(nullptr)
    , _queue(nullptr)
    , _gw(nullptr)
    , _onStateChange(nullptr)
    , _onChunk(nullptr)
{}

/* ══════════════════════════════════════════════════════════
 *  BEGIN
 * ══════════════════════════════════════════════════════════ */
bool AudioStreamer::begin(GatewayClient* gw) {
    _gw = gw;
    _queue = xQueueCreate(AUDIO_QUEUE_SIZE, sizeof(uint8_t));
    if (!_queue) {
        Serial.println("[Audio] Queue alloc failed");
        return false;
    }
    /* Launch sampling task on Core 0 */
    xTaskCreatePinnedToCore(
        _samplingTask, "AudioSample",
        4096, this, 10,
        &_samplingTaskHandle, 0
    );
    Serial.println("[Audio] Streamer initialized");
    return true;
}

/* ══════════════════════════════════════════════════════════
 *  I2S INIT / DEINIT  (Arduino 3.x I2S.h API)
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::_initI2S() {
    if (_i2sInitialized) return;
    /* Configure I2S for PDM microphone (SPM1423) */
    i2sMic.setPins(MIC_CLK_PIN, -1, MIC_DATA_PIN);  /* SCK, WS, SD */
    if (!i2sMic.begin(I2S_MODE_PDM_RX, AUDIO_SAMPLE_RATE,
                      AUDIO_BITS_PER_SAMPLE, AUDIO_CHANNELS)) {
        Serial.println("[Audio] I2S begin failed");
        return;
    }
    i2sMic.setBufferSize(AUDIO_DMA_BUF_LEN);
    _i2sInitialized = true;
    Serial.println("[Audio] I2S started (PDM)");
}

void AudioStreamer::_deinitI2S() {
    if (!_i2sInitialized) return;
    i2sMic.end();
    _i2sInitialized = false;
    Serial.println("[Audio] I2S stopped");
}

/* ══════════════════════════════════════════════════════════
 *  STATE
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::_setState(StreamerState s) {
    _state = s;
    if (_onStateChange) _onStateChange(s);
}

/* ══════════════════════════════════════════════════════════
 *  START / STOP RECORDING
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::startRecording(RecordingMode mode) {
    if (_state == STREAMER_RECORDING) return;
    _mode = mode;
    _chunkPos = 0;
    xQueueReset(_queue);
    _initI2S();
    _recordStartMs = millis();
    _setState(STREAMER_RECORDING);
    Serial.printf("[Audio] Recording started (mode=%d)\n", mode);
}

void AudioStreamer::stopRecording() {
    if (_state != STREAMER_RECORDING) return;
    _setState(STREAMER_SENDING);
    /* Flush remaining buffer */
    if (_chunkPos > 0 && _gw) {
        _gw->sendAudioChunk(_chunkBuf, _chunkPos);
        _chunkPos = 0;
    }
    _deinitI2S();
    _setState(STREAMER_IDLE);
    Serial.println("[Audio] Recording stopped");
}

/* ══════════════════════════════════════════════════════════
 *  MAIN LOOP (Core 1) — drain queue → WebSocket chunks
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::loop() {
    if (_state != STREAMER_RECORDING) return;

    /* Auto-stop on timeout (standard mode) */
    if (_mode == REC_STANDARD &&
        (millis() - _recordStartMs) > AUDIO_MAX_DURATION_MS) {
        stopRecording();
        return;
    }

    /* Drain queue into chunk buffer */
    uint8_t byte;
    while (xQueueReceive(_queue, &byte, 0) == pdTRUE) {
        _chunkBuf[_chunkPos++] = byte;
        if (_chunkPos >= AUDIO_CHUNK_BYTES) {
            if (_gw) _gw->sendAudioChunk(_chunkBuf, AUDIO_CHUNK_BYTES);
            if (_onChunk) _onChunk(_chunkBuf, AUDIO_CHUNK_BYTES);
            _chunkPos = 0;
        }
    }
}
