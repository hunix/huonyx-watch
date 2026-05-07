/**
 * Huonyx Watch – UI Manager
 * Circular AMOLED touch interface (412x412)
 * Designed for AI-era smartwatch UX
 */
#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "hw_config.h"
#include "touch_driver.h"
#include "gateway_client.h"

/* ── Color Palette (RGB565) ───────────────────────────── */
#define CLR_BG          0x0000  /* Pure black (AMOLED advantage) */
#define CLR_PRIMARY     0x07FF  /* Cyan */
#define CLR_ACCENT      0xF81F  /* Magenta */
#define CLR_SUCCESS     0x07E0  /* Green */
#define CLR_WARNING     0xFDA0  /* Amber */
#define CLR_ERROR       0xF800  /* Red */
#define CLR_TEXT        0xFFFF  /* White */
#define CLR_TEXT_DIM    0x7BEF  /* Grey */
#define CLR_CARD_BG     0x18E3  /* Dark grey card */
#define CLR_USER_MSG    0x0339  /* Dark blue user bubble */
#define CLR_AGENT_MSG   0x2104  /* Dark grey agent bubble */
#define CLR_RING        0x04FF  /* Teal ring */

/* ── Screens ──────────────────────────────────────────── */
enum ScreenId : uint8_t {
    SCREEN_WATCHFACE = 0,
    SCREEN_CHAT,
    SCREEN_SESSIONS,
    SCREEN_SETTINGS,
    SCREEN_WIFI_SETUP,
    SCREEN_COUNT
};

/* ── Chat Message ─────────────────────────────────────── */
struct ChatMsg {
    bool   isUser;
    char   text[CHAT_MAX_MSG_LEN];
};

/* ── Quick Replies ────────────────────────────────────── */
#define QUICK_REPLY_COUNT  4
extern const char* QUICK_REPLIES[QUICK_REPLY_COUNT];

class UIManager {
public:
    UIManager();

    void begin(Arduino_GFX *display, TouchDriver *touch);
    void update();
    void setGateway(GatewayClient *gw) { _gw = gw; }

    /* External state updates */
    void setTime(uint8_t h, uint8_t m, uint8_t s);
    void setDate(uint8_t day, uint8_t month, uint16_t year);
    void setBattery(uint8_t percent, bool charging);
    void setWifiConnected(bool connected);
    void setGatewayState(GatewayState state);
    void addChatMessage(const char* text, bool isUser);
    void setTypingIndicator(bool typing);
    void showNotification(const char* text, uint16_t color = CLR_PRIMARY);

    /* Navigation */
    void navigateTo(ScreenId screen);
    ScreenId currentScreen() const { return _currentScreen; }

    /* Wake/sleep */
    void wake();
    void sleep();
    bool isAwake() const { return _awake; }

private:
    Arduino_GFX   *_gfx;
    TouchDriver   *_touch;
    GatewayClient *_gw;

    /* State */
    ScreenId  _currentScreen;
    bool      _awake;
    bool      _dirty;
    uint32_t  _lastActivityMs;
    uint32_t  _lastRenderMs;
    uint32_t  _animFrame;

    /* Time */
    uint8_t   _hour, _min, _sec;
    uint8_t   _day, _month;
    uint16_t  _year;

    /* Battery */
    uint8_t   _battPercent;
    bool      _charging;

    /* Connectivity */
    bool         _wifiConnected;
    GatewayState _gwState;

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

    /* ── Drawing Methods ──────────────────────────────── */
    void drawWatchface();
    void drawChat();
    void drawSessions();
    void drawSettings();
    void drawWifiSetup();
    void drawNotification();

    /* Helpers */
    void drawCircularMask();
    void drawStatusRing();
    void drawCenteredText(const char* text, int16_t y, uint16_t color, uint8_t size = 1);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color);
    void drawArc(int16_t cx, int16_t cy, int16_t r, float startAngle, float endAngle, uint16_t color);

    /* Touch handling */
    void handleTouch(TouchGesture &gesture);
    void handleWatchfaceTouch(TouchGesture &gesture);
    void handleChatTouch(TouchGesture &gesture);
    void handleSessionsTouch(TouchGesture &gesture);
    void handleSettingsTouch(TouchGesture &gesture);
};

#endif /* UI_MANAGER_H */
