/**
 * audio_streamer.cpp
 * Huonyx Watch M5StickC Plus2 — Voice-to-Huonyx Audio Pipeline
 *
 * Uses M5Unified M5.Mic API — zero legacy driver/ headers.
 * M5.begin() already initialises the SPM1423 PDM mic internally.
 */
#include "audio_streamer.h"

/* ── Sample buffer (stack-allocated in task) ──────────── */
#define MIC_BUF_SAMPLES  AUDIO_DMA_BUF_LEN

/* ── Chunk buffer ──────────────────────────────────────── */
static uint8_t  _chunkBuf[AUDIO_CHUNK_BYTES];
static size_t   _chunkPos = 0;

/* ══════════════════════════════════════════════════════════
 *  FreeRTOS Sampling Task (Core 0)
 *  Uses M5.Mic.record() to fill a buffer, then pushes raw
 *  16-bit PCM bytes into the inter-core queue.
 * ══════════════════════════════════════════════════════════ */
void AudioStreamer::_samplingTask(void* param) {
    AudioStreamer* self = static_cast<AudioStreamer*>(param);
    int16_t micBuf[MIC_BUF_SAMPLES];

    while (true) {
        if (self->_state != STREAMER_RECORDING) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Record MIC_BUF_SAMPLES samples into micBuf */
        M5.Mic.record(micBuf, MIC_BUF_SAMPLES, AUDIO_SAMPLE_RATE);

        /* Push raw bytes into queue */
        uint8_t* b = (uint8_t*)micBuf;
        size_t   totalBytes = MIC_BUF_SAMPLES * sizeof(int16_t);
        for (size_t i = 0; i < totalBytes; i++) {
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
    , _micInitialized(false)
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

    /* Configure M5.Mic for the SPM1423 PDM microphone */
    auto micCfg = M5.Mic.config();
    micCfg.sample_rate = AUDIO_SAMPLE_RATE;
    micCfg.stereo      = false;
    M5.Mic.config(micCfg);

    if (!M5.Mic.begin()) {
        Serial.println("[Audio] M5.Mic.begin() failed");
        return false;
    }
    _micInitialized = true;

    /* Create inter-core queue */
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

    Serial.println("[Audio] Streamer initialized (M5.Mic)");
    return true;
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
    _mode     = mode;
    _chunkPos = 0;
    xQueueReset(_queue);
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
            if (_gw)      _gw->sendAudioChunk(_chunkBuf, AUDIO_CHUNK_BYTES);
            if (_onChunk) _onChunk(_chunkBuf, AUDIO_CHUNK_BYTES);
            _chunkPos = 0;
        }
    }
}
