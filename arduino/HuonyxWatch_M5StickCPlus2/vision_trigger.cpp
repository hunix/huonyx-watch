/**
 * vision_trigger.cpp
 * Huonyx Watch M5StickC Plus2 — Visual Intelligence Trigger
 */

#include "vision_trigger.h"
#include <ArduinoJson.h>

VisionTrigger::VisionTrigger()
    : _state(VISION_IDLE)
    , _triggerMs(0)
    , _onResult(nullptr)
    , _onStateChange(nullptr)
{
    strncpy(_cameraId, "default", sizeof(_cameraId) - 1);
}

void VisionTrigger::begin(const char* cameraId) {
    strncpy(_cameraId, cameraId, sizeof(_cameraId) - 1);
    Serial.printf("[Vision] Trigger initialized for camera: %s\n", _cameraId);
}

void VisionTrigger::_setState(VisionState s) {
    if (_state == s) return;
    _state = s;
    if (_onStateChange) _onStateChange(s);
}

String VisionTrigger::buildCommand(VisionMode mode, const char* customPrompt) {
    StaticJsonDocument<256> doc;
    doc["type"]      = "vision_capture";
    doc["camera_id"] = _cameraId;

    switch (mode) {
        case VISION_DESCRIBE:
            doc["prompt"] = "Describe what you see in this image in detail.";
            break;
        case VISION_READ_TEXT:
            doc["prompt"] = "Read and transcribe all text visible in this image.";
            break;
        case VISION_IDENTIFY:
            doc["prompt"] = "What is the main object or subject in this image?";
            break;
        case VISION_TRANSLATE:
            doc["prompt"] = "Translate all text visible in this image to English.";
            break;
        case VISION_CUSTOM:
            doc["prompt"] = customPrompt ? customPrompt : "Describe this image.";
            break;
    }

    String output;
    serializeJson(doc, output);
    return output;
}

void VisionTrigger::capture(VisionMode mode) {
    if (_state == VISION_WAITING) {
        Serial.println("[Vision] Already waiting for response");
        return;
    }
    _triggerMs = millis();
    _setState(VISION_WAITING);
    /* The GatewayClient will call buildCommand() and send it */
    Serial.printf("[Vision] Capture triggered (mode=%d)\n", mode);
}

void VisionTrigger::captureWithPrompt(const char* prompt) {
    if (_state == VISION_WAITING) return;
    _triggerMs = millis();
    _setState(VISION_WAITING);
    Serial.printf("[Vision] Capture with custom prompt: %s\n", prompt);
}

void VisionTrigger::onVisionResponse(const char* analysis) {
    _setState(VISION_RECEIVED);
    uint32_t latencyMs = millis() - _triggerMs;
    Serial.printf("[Vision] Response received in %lums\n", latencyMs);
    if (_onResult) _onResult(analysis);
    _setState(VISION_IDLE);
}
