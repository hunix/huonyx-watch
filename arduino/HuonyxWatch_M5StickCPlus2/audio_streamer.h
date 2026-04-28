/**
 * audio_streamer.h
 * Huonyx Watch M5StickC Plus2 — Voice-to-Huonyx Audio Pipeline
 *
 * Uses ESP32 Arduino 3.x esp_codec_dev / i2s_pdm approach via M5Unified's
 * built-in microphone support (M5.Mic). This avoids ALL legacy driver/ headers
 * and the I2S.h class (which is not available in the ESP32 board package).
 *
 * M5Unified already initialises the SPM1423 PDM microphone internally when
 * M5.begin() is called. We simply call M5.Mic.record() to capture audio.
 *
 * FreeRTOS dual-core architecture:
 *   Core 0: M5.Mic sampling → circular queue
 *   Core 1: queue → WebSocket binary frames → Huonyx Whisper STT
 */
#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "hw_config.h"
#include "gateway_client.h"

/* ── Audio parameters ─────────────────────────────────── */
#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BITS_PER_SAMPLE 16
#define AUDIO_DMA_BUF_LEN     256    /* samples per DMA read */
#define AUDIO_CHUNK_BYTES     1024   /* WebSocket frame size (bytes) */
#define AUDIO_QUEUE_SIZE      (AUDIO_CHUNK_BYTES * 8)
#define AUDIO_MAX_DURATION_MS 30000  /* 30s standard mode */

/* ── Streamer states ──────────────────────────────────── */
enum StreamerState {
    STREAMER_IDLE = 0,
    STREAMER_RECORDING,
    STREAMER_SENDING,
    STREAMER_ERROR
};

/* ── Recording modes ──────────────────────────────────── */
enum RecordingMode {
    REC_STANDARD = 0,   /* 30s max, silence-stop */
    REC_LONGFORM        /* Continuous until manually stopped */
};

typedef void (*OnStreamerStateChange)(StreamerState newState);
typedef void (*OnAudioChunk)(const uint8_t* data, size_t len);

class AudioStreamer {
public:
    AudioStreamer();

    bool begin(GatewayClient* gw);
    void loop();

    void startRecording(RecordingMode mode = REC_STANDARD);
    void stopRecording();

    StreamerState getState() const { return _state; }
    bool isRecording() const { return _state == STREAMER_RECORDING; }

    void onStateChange(OnStreamerStateChange cb) { _onStateChange = cb; }
    void onChunk(OnAudioChunk cb) { _onChunk = cb; }

private:
    StreamerState  _state;
    RecordingMode  _mode;
    bool           _micInitialized;
    uint32_t       _recordStartMs;
    TaskHandle_t   _samplingTaskHandle;
    QueueHandle_t  _queue;
    GatewayClient* _gw;

    OnStreamerStateChange _onStateChange;
    OnAudioChunk         _onChunk;

    bool _ensureInitialized();
    void _setState(StreamerState s);
    static void _samplingTask(void* param);
};
