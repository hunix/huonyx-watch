/**
 * vision_trigger.h
 * Huonyx Watch M5StickC Plus2 — Visual Intelligence Trigger
 *
 * Sends a "capture_and_analyze" command to the Huonyx server via
 * the GatewayClient WebSocket. The server then:
 *   1. Grabs a frame from the configured camera stream (MJPEG/RTSP)
 *   2. Sends it to a local vision LLM (LLaVA / MiniCPM-V on RTX Titan)
 *   3. Returns the analysis as a normal text message
 *
 * The M5StickC Plus2 does NOT process the camera image itself —
 * it only triggers the analysis and displays the result.
 *
 * Camera setup:
 *   The camera (M5Stack UnitCamS3, UnitV2, or any MJPEG WiFi cam)
 *   must be configured in the Huonyx server settings with its
 *   stream URL. The M5StickC identifies which camera to use via
 *   the camera_id field.
 *
 * Usage:
 *   VisionTrigger vision;
 *   vision.begin("camera_01");
 *   vision.capture();                    // Grab + analyze
 *   vision.captureWithPrompt("What text is on this document?");
 */

#pragma once
#include <Arduino.h>

enum VisionMode {
    VISION_DESCRIBE,    /* "What do you see?" */
    VISION_READ_TEXT,   /* "Read all text in this image" */
    VISION_IDENTIFY,    /* "Identify the main object" */
    VISION_TRANSLATE,   /* "Translate all text in this image" */
    VISION_CUSTOM,      /* User-defined prompt */
};

enum VisionState {
    VISION_IDLE,
    VISION_WAITING,     /* Command sent, awaiting response */
    VISION_RECEIVED,    /* Response received */
    VISION_ERROR,
};

typedef void (*VisionResultCallback)(const char* analysis);
typedef void (*VisionStateCallback)(VisionState state);

class VisionTrigger {
public:
    VisionTrigger();
    void begin(const char* cameraId = "default");

    /* ── Trigger capture ────────────────────────────────── */
    void capture(VisionMode mode = VISION_DESCRIBE);
    void captureWithPrompt(const char* prompt);

    /* ── State ──────────────────────────────────────────── */
    VisionState state() const { return _state; }
    bool isWaiting() const { return _state == VISION_WAITING; }

    /* ── Called by GatewayClient when vision response arrives */
    void onVisionResponse(const char* analysis);

    /* ── Callbacks ──────────────────────────────────────── */
    void onResult(VisionResultCallback cb) { _onResult = cb; }
    void onStateChange(VisionStateCallback cb) { _onStateChange = cb; }

    /* ── Build the JSON command string ──────────────────── */
    String buildCommand(VisionMode mode, const char* customPrompt = nullptr);

private:
    void _setState(VisionState s);

    char        _cameraId[32];
    VisionState _state;
    uint32_t    _triggerMs;

    VisionResultCallback  _onResult;
    VisionStateCallback   _onStateChange;
};
