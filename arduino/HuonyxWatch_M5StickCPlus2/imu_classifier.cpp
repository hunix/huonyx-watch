/**
 * imu_classifier.cpp
 * Huonyx Watch M5StickC Plus2 — IMU Context Classifier
 */

#include "imu_classifier.h"
#include <M5Unified.h>

/* ── Thresholds ───────────────────────────────────────── */
#define STILL_VAR_THRESHOLD    0.02f   /* Low variance = still */
#define WALKING_VAR_THRESHOLD  0.15f   /* Medium variance = walking */
#define DRIVING_VAR_THRESHOLD  0.08f   /* Periodic medium variance */
#define LYING_AZ_THRESHOLD     0.7f    /* Z-axis near 1g when lying */
#define SHAKE_THRESHOLD        2.5f    /* Sudden large acceleration */
#define WRIST_RAISE_THRESHOLD  0.4f    /* Z-axis change for wrist raise */
#define GESTURE_COOLDOWN_MS    800     /* Minimum ms between gestures */
#define IMU_POLL_MS            100     /* Poll interval */

ImuClassifier::ImuClassifier()
    : _ax(0), _ay(0), _az(0)
    , _gx(0), _gy(0), _gz(0)
    , _windowIdx(0)
    , _accelMagAvg(1.0f)
    , _accelMagVar(0.0f)
    , _activity(ACTIVITY_UNKNOWN)
    , _prevActivity(ACTIVITY_UNKNOWN)
    , _lastGestureMs(0)
    , _prevAz(0)
    , _wristRaiseArmed(false)
    , _lastImuMs(0)
    , _onGesture(nullptr)
    , _onActivity(nullptr)
{
    memset(_accelMag, 0, sizeof(_accelMag));
}

bool ImuClassifier::begin() {
    /* MPU6886 is initialized by M5Unified in m5_driver_init() */
    /* Just verify it's accessible */
    M5.Imu.begin();
    Serial.println("[IMU] Classifier initialized");
    return true;
}

void ImuClassifier::_readIMU() {
    M5.Imu.getAccel(&_ax, &_ay, &_az);
    M5.Imu.getGyro(&_gx, &_gy, &_gz);
}

void ImuClassifier::_classifyActivity() {
    /* Compute acceleration magnitude */
    float mag = sqrtf(_ax * _ax + _ay * _ay + _az * _az);

    /* Update rolling window */
    _accelMag[_windowIdx] = mag;
    _windowIdx = (_windowIdx + 1) % WINDOW;

    /* Compute mean */
    float sum = 0;
    for (int i = 0; i < WINDOW; i++) sum += _accelMag[i];
    _accelMagAvg = sum / WINDOW;

    /* Compute variance */
    float varSum = 0;
    for (int i = 0; i < WINDOW; i++) {
        float diff = _accelMag[i] - _accelMagAvg;
        varSum += diff * diff;
    }
    _accelMagVar = varSum / WINDOW;

    /* Classify */
    ActivityState newActivity;

    if (fabsf(_az) > LYING_AZ_THRESHOLD && fabsf(_ax) < 0.3f) {
        /* Z-axis dominant → lying down or face-up on desk */
        newActivity = ACTIVITY_LYING_DOWN;
    } else if (_accelMagVar < STILL_VAR_THRESHOLD) {
        newActivity = ACTIVITY_STILL;
    } else if (_accelMagVar > WALKING_VAR_THRESHOLD) {
        newActivity = ACTIVITY_WALKING;
    } else if (_accelMagVar > DRIVING_VAR_THRESHOLD) {
        /* Periodic medium variance — likely vehicle vibration */
        newActivity = ACTIVITY_DRIVING;
    } else {
        newActivity = ACTIVITY_STILL;
    }

    if (newActivity != _activity) {
        _prevActivity = _activity;
        _activity = newActivity;
        if (_onActivity) _onActivity(_activity);
        Serial.printf("[IMU] Activity: %s\n", activityName());
    }
}

void ImuClassifier::_detectGestures() {
    uint32_t now = millis();
    if (now - _lastGestureMs < GESTURE_COOLDOWN_MS) return;

    float mag = sqrtf(_ax * _ax + _ay * _ay + _az * _az);

    /* ── Shake detection ──────────────────────────────── */
    if (mag > SHAKE_THRESHOLD) {
        _lastGestureMs = now;
        if (_onGesture) _onGesture(GESTURE_SHAKE);
        Serial.println("[IMU] Gesture: SHAKE");
        return;
    }

    /* ── Wrist raise detection ────────────────────────── */
    /* Wrist raise: device goes from face-down to face-up */
    float azDelta = _az - _prevAz;
    if (!_wristRaiseArmed && _az < -0.5f) {
        _wristRaiseArmed = true;  /* Armed when face-down */
    }
    if (_wristRaiseArmed && azDelta > WRIST_RAISE_THRESHOLD && _az > 0.3f) {
        _wristRaiseArmed = false;
        _lastGestureMs = now;
        if (_onGesture) _onGesture(GESTURE_WRIST_RAISE);
        Serial.println("[IMU] Gesture: WRIST_RAISE");
        _prevAz = _az;
        return;
    }
    _prevAz = _az;

    /* ── Tilt left/right detection ────────────────────── */
    if (_ax > 0.7f && fabsf(_ay) < 0.3f) {
        _lastGestureMs = now;
        if (_onGesture) _onGesture(GESTURE_TILT_RIGHT);
        Serial.println("[IMU] Gesture: TILT_RIGHT");
        return;
    }
    if (_ax < -0.7f && fabsf(_ay) < 0.3f) {
        _lastGestureMs = now;
        if (_onGesture) _onGesture(GESTURE_TILT_LEFT);
        Serial.println("[IMU] Gesture: TILT_LEFT");
        return;
    }
}

void ImuClassifier::loop() {
    uint32_t now = millis();
    if (now - _lastImuMs < IMU_POLL_MS) return;
    _lastImuMs = now;

    _readIMU();
    _classifyActivity();
    _detectGestures();
}

const char* ImuClassifier::activityName() const {
    switch (_activity) {
        case ACTIVITY_STILL:       return "still";
        case ACTIVITY_WALKING:     return "walking";
        case ACTIVITY_LYING_DOWN:  return "lying_down";
        case ACTIVITY_DRIVING:     return "driving";
        default:                   return "unknown";
    }
}

const char* ImuClassifier::contextString() const {
    /* Returns a short context string for injection into agent prompts */
    switch (_activity) {
        case ACTIVITY_STILL:
            return "[Context: User is stationary, likely at desk or in a meeting]";
        case ACTIVITY_WALKING:
            return "[Context: User is currently walking or moving]";
        case ACTIVITY_LYING_DOWN:
            return "[Context: User appears to be resting or lying down]";
        case ACTIVITY_DRIVING:
            return "[Context: User appears to be in a moving vehicle]";
        default:
            return "[Context: User activity unknown]";
    }
}
