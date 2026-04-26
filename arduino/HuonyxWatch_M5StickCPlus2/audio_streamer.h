/**
 * audio_streamer.h
 * Huonyx Watch M5StickC Plus2 — Voice-to-Huonyx Audio Pipeline
 *
 * Implements real-time I2S microphone streaming to the Huonyx server
 * using a dual-core FreeRTOS architecture:
 *
 *   Core 0 (high-priority task): SPM1423 I2S sampling → circular queue
 *   Core 1 (main loop):          queue → WebSocket binary frames → server
 *
 * The server receives raw 16kHz 16-bit mono PCM, runs Whisper STT,
 * and returns the transcription as a text message via the normal
 * GatewayClient WebSocket channel.
 *
 * Usage:
 *   AudioStreamer audio;
 *   audio.begin();
 *   audio.startRecording();   // Button A held
 *   audio.stopRecording();    // Button A released
 *   audio.loop();             // Call from main loop on Core 1
 */

#pragma once
#include <Arduino.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include "hw_config.h"

/* ── Audio parameters ─────────────────────────────────── */
#define AUDIO_SAMPLE_RATE     16000   /* Hz — optimal for Whisper */
#define AUDIO_BITS_PER_SAMPLE 16      /* bits */
#define AUDIO_CHANNELS        1       /* mono */
#define AUDIO_DMA_BUF_COUNT   8       /* DMA buffers */
#define AUDIO_DMA_BUF_LEN     256     /* samples per DMA buffer */
#define AUDIO_QUEUE_DEPTH     32      /* queue slots */
#define AUDIO_CHUNK_BYTES     512     /* bytes per WebSocket frame */

/* ── Recording modes ──────────────────────────────────── */
enum RecordMode {
    REC_STANDARD,    /* Auto-stop after 3s silence (VAD on server) */
    REC_LONGFORM,    /* Continuous — for meetings, dictation */
};

/* ── Streamer state ───────────────────────────────────── */
enum StreamerState {
    STREAMER_IDLE,
    STREAMER_RECORDING,
    STREAMER_SENDING,    /* Sending final buffer after button release */
    STREAMER_ERROR,
};

/* ── Callback types ───────────────────────────────────── */
typedef void (*AudioStateCallback)(StreamerState state);
typedef void (*AudioChunkCallback)(const uint8_t* data, size_t len);

class AudioStreamer {
public:
    AudioStreamer();

    /* ── Lifecycle ──────────────────────────────────────── */
    bool begin();
    void loop();  /* Call from Core 1 main loop */

    /* ── Recording control ──────────────────────────────── */
    void startRecording(RecordMode mode = REC_STANDARD);
    void stopRecording();

    /* ── State ──────────────────────────────────────────── */
    StreamerState state() const { return _state; }
    bool isRecording() const { return _state == STREAMER_RECORDING; }
    uint32_t recordingDurationMs() const;

    /* ── Callbacks ──────────────────────────────────────── */
    void onStateChange(AudioStateCallback cb) { _onStateChange = cb; }
    void onChunk(AudioChunkCallback cb) { _onChunk = cb; }

    /* ── Internal (called from FreeRTOS task) ───────────── */
    static void _samplingTask(void* param);
    QueueHandle_t _queue;

private:
    void _initI2S();
    void _deinitI2S();
    void _setState(StreamerState s);

    StreamerState       _state;
    RecordMode          _mode;
    bool                _i2sInitialized;
    uint32_t            _recordStartMs;
    TaskHandle_t        _samplingTaskHandle;

    AudioStateCallback  _onStateChange;
    AudioChunkCallback  _onChunk;
};
