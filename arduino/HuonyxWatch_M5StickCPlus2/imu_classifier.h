/**
 * imu_classifier.h
 * Huonyx Watch M5StickC Plus2 — IMU Context Classifier
 *
 * Reads the MPU6886 6-axis IMU and classifies the user's current
 * physical context. This context is injected as metadata into every
 * message sent to the Huonyx agent, allowing it to respond
 * intelligently based on what the user is actually doing.
 *
 * Activity States:
 *   ACTIVITY_STILL       — Sitting at desk, in a meeting
 *   ACTIVITY_WALKING     — Moving around
 *   ACTIVITY_LYING_DOWN  — Resting, sleeping
 *   ACTIVITY_DRIVING     — In a vehicle (periodic vibration)
 *   ACTIVITY_UNKNOWN     — Insufficient data
 *
 * Gesture Detection:
 *   GESTURE_WRIST_RAISE  — Wake display
 *   GESTURE_SHAKE        — Stop current agent task
 *   GESTURE_DOUBLE_TAP   — Repeat last response
 *   GESTURE_TILT_LEFT    — Scroll up
 *   GESTURE_TILT_RIGHT   — Scroll down
 */

#pragma once
#include <Arduino.h>

enum ActivityState {
    ACTIVITY_STILL,
    ACTIVITY_WALKING,
    ACTIVITY_LYING_DOWN,
    ACTIVITY_DRIVING,
    ACTIVITY_UNKNOWN,
};

enum GestureEvent {
    GESTURE_NONE,
    GESTURE_WRIST_RAISE,
    GESTURE_SHAKE,
    GESTURE_DOUBLE_TAP,
    GESTURE_TILT_LEFT,
    GESTURE_TILT_RIGHT,
};

typedef void (*GestureCallback)(GestureEvent gesture);
typedef void (*ActivityCallback)(ActivityState state);

class ImuClassifier {
public:
    ImuClassifier();
    bool begin();
    void loop();  /* Call from main loop every ~100ms */

    /* ── Current state ──────────────────────────────────── */
    ActivityState activity() const { return _activity; }
    const char*   activityName() const;
    bool          isSleeping() const { return _activity == ACTIVITY_LYING_DOWN; }

    /* ── Callbacks ──────────────────────────────────────── */
    void onGesture(GestureCallback cb) { _onGesture = cb; }
    void onActivity(ActivityCallback cb) { _onActivity = cb; }

    /* ── Context string for Huonyx agent ────────────────── */
    const char* contextString() const;

private:
    void _readIMU();
    void _classifyActivity();
    void _detectGestures();

    float _ax, _ay, _az;   /* Accelerometer (g) */
    float _gx, _gy, _gz;   /* Gyroscope (deg/s) */

    /* Moving average buffers for activity classification */
    static const int WINDOW = 10;
    float _accelMag[WINDOW];
    int   _windowIdx;
    float _accelMagAvg;
    float _accelMagVar;

    ActivityState _activity;
    ActivityState _prevActivity;

    /* Gesture state */
    uint32_t _lastGestureMs;
    float    _prevAz;
    bool     _wristRaiseArmed;

    uint32_t _lastImuMs;

    GestureCallback  _onGesture;
    ActivityCallback _onActivity;
};
