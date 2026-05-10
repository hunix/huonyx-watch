/**
 * Huonyx Watch – UI Manager
 * Circular AMOLED touch interface (412x412)
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "hw_config.h"
#include "touch_driver.h"
#include "gateway_client.h"

class WiFiManager;
class WiFiPortal;

/* ── Color Palette (RGB565) ───────────────────────────── */
#define CLR_BG          0x0000  /* Pure black (AMOLED advantage) */
#define CLR_PRIMARY     0x07FF  /* Cyan */
#define CLR_ACCENT      0xF81F  /* Magenta */
#define CLR_SUCCESS     0x07E0  /* Green */
#define CLR_WARNING     0xFDA0  /* Amber */
#define CLR_ERROR       0xF800  /* Red */
#define CLR_TEXT        0xFFFF  /* White */
#define CLR_TEXT_DIM    0x7BEF  /* Grey */
#define CLR_TEXT_FAINT  0x4208  /* Darker grey */
#define CLR_CARD_BG     0x18E3  /* Dark grey card */
#define CLR_CARD_HI     0x2945  /* Lighter card for pressed state */
#define CLR_USER_MSG    0x0339  /* Dark blue user bubble */
#define CLR_AGENT_MSG   0x2104  /* Dark grey agent bubble */
#define CLR_RING        0x04FF  /* Teal ring */
#define CLR_KEY_BG      0x2104  /* Keyboard key background */
#define CLR_KEY_BG_HI   0x07FF  /* Keyboard key pressed */
#define CLR_KEY_FN      0x18E3  /* Function key (shift/space/enter) */

/* ── Screens ──────────────────────────────────────────── */
enum ScreenId : uint8_t {
    SCREEN_WATCHFACE = 0,
    SCREEN_CHAT,
    SCREEN_SESSIONS,
    SCREEN_SETTINGS,
    SCREEN_WIFI_SETUP,
    SCREEN_TIME_SETUP,
    SCREEN_KEYBOARD,
    SCREEN_PORTAL,
    SCREEN_COUNT
};

/* Number of screens reachable via swipe-left/right (excludes modal screens). */
#define SWIPE_SCREEN_COUNT  4

/* ── Chat Message ─────────────────────────────────────── */
struct ChatMsg {
    bool isUser;
    char text[CHAT_MAX_MSG_LEN];
};

/* ── Quick Replies ────────────────────────────────────── */
#define QUICK_REPLY_COUNT  4
extern const char* QUICK_REPLIES[QUICK_REPLY_COUNT];

/* ── Keyboard ─────────────────────────────────────────── */
typedef void (*KeyboardCallback)(const char* text, bool ok);
#define KEYBOARD_BUFFER_LEN 64

/* ── TZ Presets ───────────────────────────────────────── */
struct TZPreset {
    const char* label;     /* shown to user */
    const char* posix;     /* POSIX TZ string */
};
#define TZ_PRESET_COUNT 8
extern const TZPreset TZ_PRESETS[TZ_PRESET_COUNT];

class UIManager {
public:
    UIManager();

    void begin(Arduino_GFX *display, TouchDriver *touch);
    void update();

    void setGateway(GatewayClient *gw)      { _gw = gw; }
    void setWifi(WiFiManager *w)            { _wifi = w; }
    void setPortal(WiFiPortal *p)           { _portal = p; }

    /* External state updates */
    void setTime(uint8_t h, uint8_t m, uint8_t s);
    void setDate(uint8_t day, uint8_t month, uint16_t year, uint8_t weekday = 7);
    void setBattery(uint8_t percent, bool charging);
    void setWifiConnected(bool connected);
    void setGatewayState(GatewayState state);
    void addChatMessage(const char* text, bool isUser);
    void setTypingIndicator(bool typing);
    void showNotification(const char* text, uint16_t color = CLR_PRIMARY);

    /* Open the keyboard screen. cb gets the typed string (or "" + ok=false on cancel). */
    void openKeyboard(const char* title, bool isPassword,
                      const char* initial, KeyboardCallback cb,
                      ScreenId returnScreen);

    /* Navigation */
    void navigateTo(ScreenId screen);
    ScreenId currentScreen() const { return _currentScreen; }

    /* Wake / sleep */
    void wake();
    void sleep();
    bool isAwake() const { return _awake; }

    /* Static accessor for callbacks */
    static UIManager* instance() { return _instance; }

private:
    Arduino_GFX   *_gfx;
    TouchDriver   *_touch;
    GatewayClient *_gw;
    WiFiManager   *_wifi;
    WiFiPortal    *_portal;

    static UIManager *_instance;

    /* State */
    ScreenId  _currentScreen;
    ScreenId  _prevScreen;
    bool      _awake;
    bool      _screenChanged;     /* full clear+redraw on next frame */
    uint32_t  _lastActivityMs;
    uint32_t  _lastRenderMs;
    uint32_t  _animFrame;

    /* Time. _minute (not _min) because Arduino.h defines _min(a,b) macro. */
    uint8_t   _hour, _minute, _sec;
    uint8_t   _day, _month, _weekday;
    uint16_t  _year;
    uint8_t   _lastHour, _lastMinute, _lastSec;
    uint8_t   _lastDay, _lastMonth;

    /* Battery */
    uint8_t   _battPercent;
    uint8_t   _lastBattPercent;
    bool      _charging;
    bool      _lastCharging;

    /* Connectivity */
    bool         _wifiConnected;
    bool         _lastWifiConnected;
    GatewayState _gwState;
    GatewayState _lastGwState;

    /* Chat */
    ChatMsg   _messages[CHAT_MAX_MESSAGES];
    uint8_t   _msgCount;
    int8_t    _chatScrollOffset;
    bool      _typing;
    bool      _showQuickReplies;

    /* Notification */
    char      _notifText[64];
    uint16_t  _notifColor;
    uint32_t  _notifExpireMs;
    bool      _notifVisible;        /* tracks whether overlay is currently drawn */

    /* Keyboard */
    char              _kbInput[KEYBOARD_BUFFER_LEN];
    char              _kbTitle[24];
    bool              _kbIsPassword;
    uint8_t           _kbMode;       /* 0=lower, 1=upper/shift, 2=symbols */
    KeyboardCallback  _kbCallback;
    ScreenId          _kbReturnScreen;
    int8_t            _kbPressedKey; /* index of key currently pressed, -1 = none */

    /* WiFi setup transient state */
    char              _pendingSsid[33];

    /* Time setup */
    uint8_t           _tzSelectedIdx;

    /* ── Drawing ──────────────────────────────────────── */
    void drawScreen();
    void drawWatchface();
    void drawWatchfaceFull();
    void drawWatchfaceUpdate();
    void drawTopStatusArc();
    void drawChat();
    void drawSessions();
    void drawSettings();
    void drawWifiSetup();
    void drawTimeSetup();
    void drawKeyboard();
    void drawPortal();
    void drawNotification();

    /* Helpers */
    void drawCenteredText(const char* text, int16_t y, uint16_t color, uint8_t size = 1);
    void drawCenteredTextFont(const char* text, int16_t y, uint16_t color,
                              const GFXfont* font, uint8_t sizeMult = 1);
    int16_t textWidth(const char* text, const GFXfont* font, uint8_t sizeMult);
    void drawArcSegment(int16_t cx, int16_t cy, int16_t r, int16_t thickness,
                        float startDeg, float endDeg, uint16_t color);
    void drawCardButton(int16_t x, int16_t y, int16_t w, int16_t h,
                        const char* label, uint16_t fg, uint16_t bg, bool pressed);
    void drawKey(int16_t x, int16_t y, int16_t w, int16_t h,
                 const char* label, uint16_t bg, uint16_t fg, bool pressed);

    /* Touch handling */
    void handleTouch(TouchGesture &gesture);
    void handleWatchfaceTouch(TouchGesture &gesture);
    void handleChatTouch(TouchGesture &gesture);
    void handleSessionsTouch(TouchGesture &gesture);
    void handleSettingsTouch(TouchGesture &gesture);
    void handleWifiSetupTouch(TouchGesture &gesture);
    void handleTimeSetupTouch(TouchGesture &gesture);
    void handleKeyboardTouch(TouchGesture &gesture);
    void handlePortalTouch(TouchGesture &gesture);

    /* Keyboard layout */
    int keyboardHitTest(int16_t x, int16_t y, char* outChar);
    void keyboardCommit(bool ok);

    /* WiFi setup actions (internal because static callbacks need to call them) */
    void wifiSetupStartManual();
    void wifiSetupStartPortal();
    void wifiSetupForget();
    static void onSsidEntered(const char* text, bool ok);
    static void onPassEntered(const char* text, bool ok);
};

#endif /* UI_MANAGER_H */
