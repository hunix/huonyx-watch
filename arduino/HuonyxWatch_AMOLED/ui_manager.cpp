/**
 * Huonyx Watch – UI Manager Implementation
 * Circular AMOLED 412x412 touch interface
 *
 * Rendering model:
 *   _screenChanged = true forces a full clear+redraw (on navigateTo / wake).
 *   Otherwise per-screen update paths only touch the regions whose state
 *   has changed (time, seconds, battery, status). This keeps the watchface
 *   from flickering and cuts CPU on idle frames.
 */
#include "ui_manager.h"
#include "display_driver.h"
#include "wifi_manager.h"
#include "wifi_portal.h"
#include <math.h>
#include <Preferences.h>
/* Adafruit GFX Free Fonts — copied into the sketch dir from TFT_eSPI's
 * Fonts/GFXFF/. Arduino_GFX_Library accepts the same GFXfont struct. */
#include "FreeSansBold24pt7b.h"
#include "FreeSans12pt7b.h"
#include "FreeSansBold12pt7b.h"
#include "FreeSans9pt7b.h"

UIManager* UIManager::_instance = nullptr;

const char* QUICK_REPLIES[QUICK_REPLY_COUNT] = {
    "Yes",
    "No",
    "Status?",
    "Help"
};

const TZPreset TZ_PRESETS[TZ_PRESET_COUNT] = {
    { "UTC",          "UTC0" },
    { "London",       "GMT0BST,M3.5.0/1,M10.5.0" },
    { "Berlin",       "CET-1CEST,M3.5.0,M10.5.0/3" },
    { "Istanbul",     "TRT-3" },
    { "Riyadh",       "AST-3" },
    { "Dubai",        "GST-4" },
    { "New York",     "EST5EDT,M3.2.0,M11.1.0" },
    { "Los Angeles",  "PST8PDT,M3.2.0,M11.1.0" }
};

/* ── Keyboard Layout ──────────────────────────────────── */
/* 4 rows. Inscribed in the lower portion of the 412x412 circle. */
static const char* KB_ROW1_LOWER = "qwertyuiop";
static const char* KB_ROW1_UPPER = "QWERTYUIOP";
static const char* KB_ROW1_SYM   = "1234567890";
static const char* KB_ROW2_LOWER = "asdfghjkl";
static const char* KB_ROW2_UPPER = "ASDFGHJKL";
static const char* KB_ROW2_SYM   = "@#$_&-+()/";
static const char* KB_ROW3_LOWER = "zxcvbnm";
static const char* KB_ROW3_UPPER = "ZXCVBNM";
static const char* KB_ROW3_SYM   = "*\"':;!?,.";

/* Keyboard region inside the 412x412 circle. */
#define KB_X_MIN     22
#define KB_X_MAX     390   /* width = 368 */
#define KB_Y_INPUT_TOP   46
#define KB_Y_INPUT_H     38
#define KB_Y_R1      120
#define KB_Y_R2      174
#define KB_Y_R3      228
#define KB_Y_R4      282
#define KB_KEY_H     46
#define KB_KEY_GAP   3

/* Special key codes returned from hit-test (negative values, since real chars are positive). */
#define KB_KEY_NONE       0
#define KB_KEY_SHIFT     -1
#define KB_KEY_BACKSPACE -2
#define KB_KEY_SYMBOLS   -3
#define KB_KEY_SPACE     -4
#define KB_KEY_OK        -5
#define KB_KEY_CANCEL    -6

/* ── Constructor ──────────────────────────────────────── */
UIManager::UIManager()
    : _gfx(nullptr), _touch(nullptr), _gw(nullptr), _wifi(nullptr), _portal(nullptr)
    , _currentScreen(SCREEN_WATCHFACE), _prevScreen(SCREEN_WATCHFACE)
    , _awake(true), _screenChanged(true)
    , _lastActivityMs(0), _lastRenderMs(0), _animFrame(0)
    , _hour(0), _minute(0), _sec(0)
    , _day(1), _month(1), _weekday(7), _year(2026)
    , _lastHour(255), _lastMinute(255), _lastSec(255), _lastDay(255), _lastMonth(255)
    , _battPercent(100), _lastBattPercent(255)
    , _charging(false), _lastCharging(false)
    , _wifiConnected(false), _lastWifiConnected(false)
    , _gwState(GW_DISCONNECTED), _lastGwState(GW_ERROR)
    , _msgCount(0), _chatScrollOffset(0)
    , _typing(false), _showQuickReplies(false)
    , _notifColor(CLR_PRIMARY), _notifExpireMs(0), _notifVisible(false)
    , _kbIsPassword(false), _kbMode(0), _kbCallback(nullptr)
    , _kbReturnScreen(SCREEN_SETTINGS), _kbPressedKey(-1)
    , _tzSelectedIdx(0)
{
    memset(_messages, 0, sizeof(_messages));
    memset(_notifText, 0, sizeof(_notifText));
    memset(_kbInput, 0, sizeof(_kbInput));
    memset(_kbTitle, 0, sizeof(_kbTitle));
    memset(_pendingSsid, 0, sizeof(_pendingSsid));
    _instance = this;
}

void UIManager::begin(Arduino_GFX *display, TouchDriver *touch) {
    _gfx = display;
    _touch = touch;
    _lastActivityMs = millis();
    _screenChanged = true;
    Serial.println("[UI] Initialized for 412x412 AMOLED");
}

/* ══════════════════════════════════════════════════════════
 *  MAIN UPDATE
 * ══════════════════════════════════════════════════════════ */
void UIManager::update() {
    if (!_gfx || !_touch) return;

    uint32_t now = millis();

    /* Touch */
    _touch->update();
    TouchGesture gesture = _touch->getGesture();
    if (gesture.event != TOUCH_NONE) {
        _lastActivityMs = now;
        if (!_awake) {
            wake();
            return;
        }
        handleTouch(gesture);
    }

    /* Auto-sleep */
    if (_awake && (now - _lastActivityMs > SLEEP_TIMEOUT_MS)) {
        sleep();
        return;
    }
    if (!_awake) return;

    /* Frame pacing */
    uint32_t frameInterval = 1000 / UI_FPS;
    if (now - _lastRenderMs < frameInterval) return;
    _lastRenderMs = now;
    _animFrame++;

    drawScreen();

    /* Notification overlay */
    if (_notifExpireMs > 0) {
        if (now < _notifExpireMs) {
            drawNotification();
            _notifVisible = true;
        } else if (_notifVisible) {
            /* Just expired — clear its rect and force a redraw of underlying screen */
            _gfx->fillRect(SCREEN_CENTER_X - 150, 24, 300, 50, CLR_BG);
            _notifExpireMs = 0;
            _notifVisible = false;
            _screenChanged = true;
        }
    }
}

void UIManager::drawScreen() {
    if (_screenChanged) {
        _gfx->fillScreen(CLR_BG);
        /* Reset all "last" state so the per-screen update paths redraw everything. */
        _lastHour = _lastMinute = _lastSec = 255;
        _lastDay = _lastMonth = 255;
        _lastBattPercent = 255;
        _lastCharging = !_charging;
        _lastWifiConnected = !_wifiConnected;
        _lastGwState = (GatewayState)0xFF;
    }

    switch (_currentScreen) {
        case SCREEN_WATCHFACE:
            if (_screenChanged) drawWatchfaceFull();
            else                drawWatchfaceUpdate();
            break;
        case SCREEN_CHAT:        drawChat();        break;
        case SCREEN_SESSIONS:    drawSessions();    break;
        case SCREEN_SETTINGS:    drawSettings();    break;
        case SCREEN_WIFI_SETUP:  drawWifiSetup();   break;
        case SCREEN_TIME_SETUP:  drawTimeSetup();   break;
        case SCREEN_KEYBOARD:    drawKeyboard();    break;
        case SCREEN_PORTAL:      drawPortal();      break;
        default: break;
    }

    _screenChanged = false;
}

/* ══════════════════════════════════════════════════════════
 *  NAVIGATION / WAKE
 * ══════════════════════════════════════════════════════════ */
void UIManager::navigateTo(ScreenId screen) {
    if (screen == _currentScreen) return;
    _prevScreen = _currentScreen;
    _currentScreen = screen;
    _screenChanged = true;
    _chatScrollOffset = 0;
    _showQuickReplies = false;
}

void UIManager::wake() {
    _awake = true;
    _screenChanged = true;
    _lastActivityMs = millis();
    display_sleep(false);
    Serial.println("[UI] Wake");
}

void UIManager::sleep() {
    _awake = false;
    _gfx->fillScreen(CLR_BG);
    display_sleep(true);
    Serial.println("[UI] Sleep");
}

/* ══════════════════════════════════════════════════════════
 *  STATE SETTERS
 * ══════════════════════════════════════════════════════════ */
void UIManager::setTime(uint8_t h, uint8_t m, uint8_t s) {
    _hour = h; _minute = m; _sec = s;
}

void UIManager::setDate(uint8_t day, uint8_t month, uint16_t year, uint8_t weekday) {
    _day = day; _month = month; _year = year; _weekday = weekday;
}

void UIManager::setBattery(uint8_t percent, bool charging) {
    _battPercent = percent;
    _charging = charging;
}

void UIManager::setWifiConnected(bool connected) {
    _wifiConnected = connected;
}

void UIManager::setGatewayState(GatewayState state) {
    _gwState = state;
}

void UIManager::addChatMessage(const char* text, bool isUser) {
    if (_msgCount >= CHAT_MAX_MESSAGES) {
        memmove(&_messages[0], &_messages[1], sizeof(ChatMsg) * (CHAT_MAX_MESSAGES - 1));
        _msgCount = CHAT_MAX_MESSAGES - 1;
    }
    _messages[_msgCount].isUser = isUser;
    strncpy(_messages[_msgCount].text, text, CHAT_MAX_MSG_LEN - 1);
    _messages[_msgCount].text[CHAT_MAX_MSG_LEN - 1] = '\0';
    _msgCount++;
    if (_currentScreen == SCREEN_CHAT) _screenChanged = true;
}

void UIManager::setTypingIndicator(bool typing) {
    if (_typing != typing) {
        _typing = typing;
        if (_currentScreen == SCREEN_CHAT) _screenChanged = true;
    }
}

void UIManager::showNotification(const char* text, uint16_t color) {
    strncpy(_notifText, text, sizeof(_notifText) - 1);
    _notifText[sizeof(_notifText) - 1] = '\0';
    _notifColor = color;
    _notifExpireMs = millis() + 3000;
    _notifVisible = false;
}

/* ══════════════════════════════════════════════════════════
 *  TOUCH ROUTER
 * ══════════════════════════════════════════════════════════ */
void UIManager::handleTouch(TouchGesture &gesture) {
    /* Modal screens don't respond to swipe-to-navigate. */
    bool modal = (_currentScreen == SCREEN_KEYBOARD ||
                  _currentScreen == SCREEN_WIFI_SETUP ||
                  _currentScreen == SCREEN_TIME_SETUP ||
                  _currentScreen == SCREEN_PORTAL);

    if (!modal) {
        if (gesture.event == TOUCH_SWIPE_LEFT) {
            uint8_t next = (uint8_t)_currentScreen + 1;
            if (next < SWIPE_SCREEN_COUNT) navigateTo((ScreenId)next);
            return;
        }
        if (gesture.event == TOUCH_SWIPE_RIGHT) {
            if (_currentScreen > 0 && (uint8_t)_currentScreen < SWIPE_SCREEN_COUNT) {
                navigateTo((ScreenId)((uint8_t)_currentScreen - 1));
            }
            return;
        }
    }

    switch (_currentScreen) {
        case SCREEN_WATCHFACE:    handleWatchfaceTouch(gesture);   break;
        case SCREEN_CHAT:         handleChatTouch(gesture);        break;
        case SCREEN_SESSIONS:     handleSessionsTouch(gesture);    break;
        case SCREEN_SETTINGS:     handleSettingsTouch(gesture);    break;
        case SCREEN_WIFI_SETUP:   handleWifiSetupTouch(gesture);   break;
        case SCREEN_TIME_SETUP:   handleTimeSetupTouch(gesture);   break;
        case SCREEN_KEYBOARD:     handleKeyboardTouch(gesture);    break;
        case SCREEN_PORTAL:       handlePortalTouch(gesture);      break;
        default: break;
    }
}

void UIManager::handleWatchfaceTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_TAP) {
        int16_t dx = gesture.x - SCREEN_CENTER_X;
        int16_t dy = gesture.y - SCREEN_CENTER_Y;
        if (dx * dx + dy * dy < 100 * 100) {
            navigateTo(SCREEN_CHAT);
        }
    }
}

void UIManager::handleChatTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_UP) {
        if (_chatScrollOffset < _msgCount - 3) _chatScrollOffset++;
        _screenChanged = true;
    } else if (gesture.event == TOUCH_SWIPE_DOWN) {
        if (_chatScrollOffset > 0) _chatScrollOffset--;
        _screenChanged = true;
    } else if (gesture.event == TOUCH_LONG_PRESS) {
        _showQuickReplies = !_showQuickReplies;
        _screenChanged = true;
    } else if (gesture.event == TOUCH_TAP && _showQuickReplies) {
        int16_t replyY = LCD_HEIGHT / 2 - (QUICK_REPLY_COUNT * 45) / 2;
        for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
            int16_t btnTop = replyY + i * 45;
            if (gesture.y >= btnTop && gesture.y < btnTop + 36 &&
                gesture.x >= 80 && gesture.x <= LCD_WIDTH - 80) {
                if (_gw && _gw->isConnected()) {
                    _gw->sendMessage(_gw->getSessionKey(), QUICK_REPLIES[i]);
                    addChatMessage(QUICK_REPLIES[i], true);
                }
                _showQuickReplies = false;
                _screenChanged = true;
                break;
            }
        }
    }
}

void UIManager::handleSessionsTouch(TouchGesture &gesture) {
    (void)gesture;
}

void UIManager::handleSettingsTouch(TouchGesture &gesture) {
    if (gesture.event != TOUCH_TAP) return;
    /* Settings list:
     *   y=80..130   WiFi
     *   y=142..192  Time
     *   y=204..254  Battery info (no tap)
     *   y=266..316  Firmware info (no tap)
     */
    if (gesture.y >= 80 && gesture.y < 130) {
        navigateTo(SCREEN_WIFI_SETUP);
    } else if (gesture.y >= 142 && gesture.y < 192) {
        navigateTo(SCREEN_TIME_SETUP);
    }
}

/* ══════════════════════════════════════════════════════════
 *  WATCHFACE  (minimal digital + dirty regions)
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawWatchfaceFull() {
    drawTopStatusArc();

    /* Time */
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _hour, _minute);
    drawCenteredTextFont(timeBuf, SCREEN_CENTER_Y + 12,
                         CLR_TEXT, &FreeSansBold24pt7b, 2);

    /* Seconds (smaller, right of time) */
    char secBuf[4];
    snprintf(secBuf, sizeof(secBuf), "%02d", _sec);
    int16_t timeW = textWidth(timeBuf, &FreeSansBold24pt7b, 2);
    _gfx->setFont(&FreeSans12pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_PRIMARY);
    _gfx->setCursor(SCREEN_CENTER_X + timeW / 2 + 6, SCREEN_CENTER_Y + 12);
    _gfx->print(secBuf);

    /* Date line */
    static const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat","   "};
    static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                   "Jul","Aug","Sep","Oct","Nov","Dec"};
    char dateBuf[24];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %s",
             days[_weekday > 7 ? 7 : _weekday], _day,
             months[(_month - 1) % 12]);
    drawCenteredTextFont(dateBuf, SCREEN_CENTER_Y + 60,
                         CLR_TEXT_DIM, &FreeSans12pt7b, 1);

    /* Tap hint (below date) */
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_TEXT_FAINT);
    const char* hint = "tap to chat";
    int16_t hw = textWidth(hint, &FreeSans9pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - hw / 2, SCREEN_CENTER_Y + 100);
    _gfx->print(hint);

    _lastHour = _hour; _lastMinute = _minute; _lastSec = _sec;
    _lastDay = _day; _lastMonth = _month;
    _lastBattPercent = _battPercent; _lastCharging = _charging;
    _lastWifiConnected = _wifiConnected; _lastGwState = _gwState;
}

void UIManager::drawWatchfaceUpdate() {
    /* Status arc — only redraw when battery / wifi / gateway changes */
    if (_battPercent != _lastBattPercent || _charging != _lastCharging ||
        _wifiConnected != _lastWifiConnected || _gwState != _lastGwState) {
        /* Clear top arc strip and redraw */
        _gfx->fillRect(0, 0, LCD_WIDTH, 60, CLR_BG);
        drawTopStatusArc();
        _lastBattPercent = _battPercent;
        _lastCharging = _charging;
        _lastWifiConnected = _wifiConnected;
        _lastGwState = _gwState;
    }

    /* Time HH:MM — redraw only on minute change. Clear band is generous to
     * cover the full ascender height of 24pt bold scaled to size 2. */
    if (_hour != _lastHour || _minute != _lastMinute) {
        _gfx->fillRect(20, SCREEN_CENTER_Y - 60, LCD_WIDTH - 40, 90, CLR_BG);
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _hour, _minute);
        drawCenteredTextFont(timeBuf, SCREEN_CENTER_Y + 12,
                             CLR_TEXT, &FreeSansBold24pt7b, 2);
        _lastHour = _hour; _lastMinute = _minute;
        /* Force seconds redraw too since the time width may have shifted them */
        _lastSec = 255;
    }

    /* Seconds — redraw every second */
    if (_sec != _lastSec) {
        char timeBuf[6];
        snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _hour, _minute);
        int16_t timeW = textWidth(timeBuf, &FreeSansBold24pt7b, 2);
        int16_t secX = SCREEN_CENTER_X + timeW / 2 + 6;
        _gfx->fillRect(secX, SCREEN_CENTER_Y - 12, 44, 32, CLR_BG);
        char secBuf[4];
        snprintf(secBuf, sizeof(secBuf), "%02d", _sec);
        _gfx->setFont(&FreeSans12pt7b);
        _gfx->setTextSize(1);
        _gfx->setTextColor(CLR_PRIMARY);
        _gfx->setCursor(secX, SCREEN_CENTER_Y + 12);
        _gfx->print(secBuf);
        _lastSec = _sec;
    }

    /* Date — only on day change */
    if (_day != _lastDay || _month != _lastMonth) {
        _gfx->fillRect(40, SCREEN_CENTER_Y + 40, LCD_WIDTH - 80, 32, CLR_BG);
        static const char* days[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat","   "};
        static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                       "Jul","Aug","Sep","Oct","Nov","Dec"};
        char dateBuf[24];
        snprintf(dateBuf, sizeof(dateBuf), "%s %d %s",
                 days[_weekday > 7 ? 7 : _weekday], _day,
                 months[(_month - 1) % 12]);
        drawCenteredTextFont(dateBuf, SCREEN_CENTER_Y + 60,
                             CLR_TEXT_DIM, &FreeSans12pt7b, 1);
        _lastDay = _day; _lastMonth = _month;
    }
}

void UIManager::drawTopStatusArc() {
    /* Battery arc on outer rim, top half only (from -120° to -60° spanning
     * across the top). Empty arc dim, filled portion bright. */
    float pct = _battPercent / 100.0f;
    if (pct > 1.0f) pct = 1.0f;
    if (pct < 0.0f) pct = 0.0f;

    uint16_t fill = _charging   ? CLR_WARNING :
                    _battPercent > 20 ? CLR_RING : CLR_ERROR;

    /* Sweep from 220° (bottom-left) to 320° (bottom-right) going over the top
     * is awkward; use 200° → 340° clockwise via top. */
    /* Background */
    drawArcSegment(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_RADIUS - 6, 4,
                   200.0f, 340.0f, CLR_CARD_BG);
    /* Filled */
    float endDeg = 200.0f + 140.0f * pct;
    drawArcSegment(SCREEN_CENTER_X, SCREEN_CENTER_Y, SCREEN_RADIUS - 6, 4,
                   200.0f, endDeg, fill);

    /* Battery % text — small, centered top */
    char b[8];
    snprintf(b, sizeof(b), "%d%%%s", _battPercent, _charging ? "+" : "");
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_TEXT_DIM);
    int16_t bw = textWidth(b, &FreeSans9pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - bw / 2, 36);
    _gfx->print(b);

    /* Connectivity dots — top center, just under battery text */
    int16_t dotY = 50;
    int16_t dotSpacing = 18;
    int16_t cx = SCREEN_CENTER_X;
    uint16_t wifiCol = _wifiConnected ? CLR_SUCCESS : CLR_TEXT_FAINT;
    uint16_t gwCol = (_gwState == GW_AUTHENTICATED) ? CLR_PRIMARY :
                     (_gwState == GW_CONNECTING || _gwState == GW_CONNECTED) ? CLR_WARNING :
                     CLR_TEXT_FAINT;
    _gfx->fillCircle(cx - dotSpacing, dotY, 3, wifiCol);
    _gfx->fillCircle(cx,               dotY, 3, gwCol);
    _gfx->fillCircle(cx + dotSpacing,  dotY, 3, _gwState == GW_AUTHENTICATED ? CLR_PRIMARY : CLR_TEXT_FAINT);
}

/* ══════════════════════════════════════════════════════════
 *  CHAT
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawChat() {
    if (!_screenChanged) return;  /* chat redraws only on state change */

    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_PRIMARY);
    int16_t hw = textWidth("HUONYX", &FreeSansBold12pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - hw / 2, 38);
    _gfx->print("HUONYX");

    uint16_t statusColor = (_gwState == GW_AUTHENTICATED) ? CLR_SUCCESS : CLR_ERROR;
    _gfx->fillCircle(SCREEN_CENTER_X + hw / 2 + 14, 32, 4, statusColor);

    int16_t msgAreaTop = 64;
    int16_t msgAreaBottom = _showQuickReplies ? 240 : 360;
    int16_t padding = 30;

    int startIdx = max(0, (int)_msgCount - 6 - _chatScrollOffset);
    int endIdx = min((int)_msgCount, startIdx + 6);

    int16_t yPos = msgAreaTop;
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextSize(1);
    for (int i = startIdx; i < endIdx && yPos < msgAreaBottom; i++) {
        ChatMsg &msg = _messages[i];
        uint16_t bgColor = msg.isUser ? CLR_USER_MSG : CLR_AGENT_MSG;

        int16_t maxBubbleW = LCD_WIDTH - padding * 2 - 30;
        int16_t textW = strlen(msg.text) * 8;
        int16_t bubbleW = min((int16_t)(textW + 20), maxBubbleW);
        int16_t bubbleH = 32;
        int charsPerLine = (bubbleW - 16) / 8;
        if (charsPerLine < 8) charsPerLine = 8;
        int lines = ((int)strlen(msg.text) + charsPerLine - 1) / charsPerLine;
        if (lines > 1) bubbleH = 16 + lines * 18;

        int16_t bx = msg.isUser ? (LCD_WIDTH - padding - bubbleW) : padding;
        _gfx->fillRoundRect(bx, yPos, bubbleW, bubbleH, 12, bgColor);

        _gfx->setTextColor(CLR_TEXT);
        const char* ptr = msg.text;
        int remaining = strlen(msg.text);
        int16_t ty = yPos + 18;
        while (remaining > 0 && ty < yPos + bubbleH) {
            int lineLen = min(remaining, charsPerLine);
            char lineBuf[64];
            strncpy(lineBuf, ptr, lineLen);
            lineBuf[lineLen] = '\0';
            _gfx->setCursor(bx + 8, ty);
            _gfx->print(lineBuf);
            ptr += lineLen;
            remaining -= lineLen;
            ty += 18;
        }

        yPos += bubbleH + 8;
    }

    if (_typing) {
        int16_t dotX = padding + 20;
        int16_t dotY2 = yPos + 10;
        for (int i = 0; i < 3; i++) {
            uint8_t bright = ((_animFrame + i * 8) % 24 < 12) ? 255 : 100;
            uint16_t dotColor = (bright > 200) ? CLR_TEXT : CLR_TEXT_DIM;
            _gfx->fillCircle(dotX + i * 14, dotY2, 4, dotColor);
        }
    }

    if (_showQuickReplies) {
        int16_t replyY = LCD_HEIGHT / 2 - (QUICK_REPLY_COUNT * 45) / 2;
        _gfx->fillRoundRect(60, replyY - 10, LCD_WIDTH - 120,
                            QUICK_REPLY_COUNT * 45 + 20, 20, CLR_CARD_BG);
        for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
            int16_t btnY = replyY + i * 45;
            _gfx->fillRoundRect(80, btnY, LCD_WIDTH - 160, 36, 18, CLR_PRIMARY);
            _gfx->setFont(&FreeSansBold12pt7b);
            _gfx->setTextColor(CLR_BG);
            int16_t tw = textWidth(QUICK_REPLIES[i], &FreeSansBold12pt7b, 1);
            _gfx->setCursor(SCREEN_CENTER_X - tw / 2, btnY + 24);
            _gfx->print(QUICK_REPLIES[i]);
        }
    } else {
        _gfx->setFont(&FreeSans9pt7b);
        _gfx->setTextColor(CLR_TEXT_FAINT);
        const char* hint = "long-press for replies";
        int16_t hintW = textWidth(hint, &FreeSans9pt7b, 1);
        _gfx->setCursor(SCREEN_CENTER_X - hintW / 2, LCD_HEIGHT - 40);
        _gfx->print(hint);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SESSIONS / SETTINGS
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawSessions() {
    if (!_screenChanged) return;
    drawCenteredTextFont("SESSIONS", 56, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    _gfx->setFont(&FreeSans9pt7b);
    if (_gwState != GW_AUTHENTICATED) {
        drawCenteredTextFont("Not connected", SCREEN_CENTER_Y, CLR_TEXT_DIM, &FreeSans12pt7b, 1);
        return;
    }
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(60, 110);
    _gfx->print("Active session:");
    _gfx->setTextColor(CLR_SUCCESS);
    _gfx->setCursor(60, 138);
    if (_gw) _gfx->print(_gw->getSessionKey());

    _gfx->fillRoundRect(SCREEN_CENTER_X - 70, SCREEN_CENTER_Y + 60, 140, 44, 22, CLR_PRIMARY);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_BG);
    int16_t rw = textWidth("Refresh", &FreeSansBold12pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - rw / 2, SCREEN_CENTER_Y + 90);
    _gfx->print("Refresh");
}

void UIManager::drawSettings() {
    if (!_screenChanged) return;
    drawCenteredTextFont("SETTINGS", 50, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    int16_t pad = 36;
    int16_t w = LCD_WIDTH - pad * 2;
    int16_t y = 80;

    /* WiFi row */
    _gfx->fillRoundRect(pad, y, w, 50, 12, CLR_CARD_BG);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(pad + 16, y + 32);
    _gfx->print("WiFi");
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(CLR_TEXT_DIM);
    if (_wifi && _wifi->isConnected()) {
        _gfx->setCursor(pad + 80, y + 32);
        _gfx->print(_wifi->getSSID());
    } else {
        _gfx->setCursor(pad + 80, y + 32);
        _gfx->print("Not connected");
    }
    _gfx->fillCircle(pad + w - 22, y + 25, 6,
                     _wifiConnected ? CLR_SUCCESS : CLR_ERROR);
    y += 62;

    /* Time row */
    _gfx->fillRoundRect(pad, y, w, 50, 12, CLR_CARD_BG);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(pad + 16, y + 32);
    _gfx->print("Time");
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(CLR_TEXT_DIM);
    char tBuf[16];
    snprintf(tBuf, sizeof(tBuf), "%02d:%02d", _hour, _minute);
    _gfx->setCursor(pad + w - 64, y + 32);
    _gfx->print(tBuf);
    y += 62;

    /* Battery row */
    _gfx->fillRoundRect(pad, y, w, 50, 12, CLR_CARD_BG);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(pad + 16, y + 32);
    char battStr[16];
    snprintf(battStr, sizeof(battStr), "Battery %d%%", _battPercent);
    _gfx->print(battStr);
    y += 62;

    /* Firmware */
    _gfx->fillRoundRect(pad, y, w, 50, 12, CLR_CARD_BG);
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(pad + 16, y + 20);
    _gfx->print("Firmware");
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(pad + 16, y + 42);
    _gfx->print(FIRMWARE_VERSION);

    drawCenteredTextFont("< swipe >", LCD_HEIGHT - 30, CLR_TEXT_FAINT,
                         &FreeSans9pt7b, 1);
}

/* ══════════════════════════════════════════════════════════
 *  WIFI SETUP
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawWifiSetup() {
    if (!_screenChanged) return;
    drawCenteredTextFont("WIFI SETUP", 50, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    /* Status text */
    _gfx->setFont(&FreeSans9pt7b);
    if (_wifi && _wifi->isConnected()) {
        _gfx->setTextColor(CLR_SUCCESS);
        char buf[64];
        snprintf(buf, sizeof(buf), "Connected: %s", _wifi->getSSID());
        int16_t w = textWidth(buf, &FreeSans9pt7b, 1);
        _gfx->setCursor(SCREEN_CENTER_X - w / 2, 88);
        _gfx->print(buf);
    } else {
        _gfx->setTextColor(CLR_TEXT_DIM);
        const char* msg = _wifi && _wifi->hasCredentials() ? "Disconnected" : "Not configured";
        int16_t w = textWidth(msg, &FreeSans9pt7b, 1);
        _gfx->setCursor(SCREEN_CENTER_X - w / 2, 88);
        _gfx->print(msg);
    }

    /* Buttons */
    int16_t bx = 60;
    int16_t bw = LCD_WIDTH - bx * 2;
    drawCardButton(bx, 110, bw, 54, "Phone setup",   CLR_BG, CLR_PRIMARY, false);
    drawCardButton(bx, 174, bw, 54, "Type manually", CLR_TEXT, CLR_CARD_BG, false);
    drawCardButton(bx, 238, bw, 54, "Forget WiFi",   CLR_ERROR, CLR_CARD_BG, false);

    drawCenteredTextFont("swipe right to go back", LCD_HEIGHT - 40,
                         CLR_TEXT_FAINT, &FreeSans9pt7b, 1);
}

void UIManager::handleWifiSetupTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_RIGHT) {
        navigateTo(SCREEN_SETTINGS);
        return;
    }
    if (gesture.event != TOUCH_TAP) return;
    if (gesture.x < 60 || gesture.x > LCD_WIDTH - 60) return;

    if (gesture.y >= 110 && gesture.y < 164) {
        wifiSetupStartPortal();
    } else if (gesture.y >= 174 && gesture.y < 228) {
        wifiSetupStartManual();
    } else if (gesture.y >= 238 && gesture.y < 292) {
        wifiSetupForget();
    }
}

void UIManager::wifiSetupStartPortal() {
    if (!_portal) return;
    _portal->start("HuonyxWatch-Setup");
    navigateTo(SCREEN_PORTAL);
}

void UIManager::wifiSetupStartManual() {
    _pendingSsid[0] = '\0';
    openKeyboard("WiFi SSID", false, "", &UIManager::onSsidEntered, SCREEN_WIFI_SETUP);
}

void UIManager::wifiSetupForget() {
    if (!_wifi) return;
    _wifi->clearCredentials();
    showNotification("WiFi forgotten", CLR_WARNING);
    _screenChanged = true;
}

/* Static keyboard callbacks — route back to instance */
void UIManager::onSsidEntered(const char* text, bool ok) {
    if (!_instance) return;
    if (!ok || strlen(text) == 0) {
        _instance->navigateTo(SCREEN_WIFI_SETUP);
        return;
    }
    strncpy(_instance->_pendingSsid, text, sizeof(_instance->_pendingSsid) - 1);
    _instance->_pendingSsid[sizeof(_instance->_pendingSsid) - 1] = '\0';
    _instance->openKeyboard("Password", true, "",
                            &UIManager::onPassEntered, SCREEN_WIFI_SETUP);
}

void UIManager::onPassEntered(const char* text, bool ok) {
    if (!_instance || !_instance->_wifi) return;
    if (!ok) {
        _instance->navigateTo(SCREEN_WIFI_SETUP);
        return;
    }
    _instance->_wifi->saveCredentials(_instance->_pendingSsid, text);
    _instance->_wifi->connect(_instance->_pendingSsid, text);
    _instance->showNotification("Connecting...", CLR_PRIMARY);
    _instance->navigateTo(SCREEN_WIFI_SETUP);
}

/* ══════════════════════════════════════════════════════════
 *  PORTAL SCREEN
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawPortal() {
    if (!_screenChanged) return;
    drawCenteredTextFont("PHONE SETUP", 60, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    _gfx->setFont(&FreeSans12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    drawCenteredTextFont("Connect phone to:", 110, CLR_TEXT_DIM, &FreeSans9pt7b, 1);
    drawCenteredTextFont("HuonyxWatch-Setup", 138, CLR_TEXT, &FreeSansBold12pt7b, 1);

    drawCenteredTextFont("Then open:", 178, CLR_TEXT_DIM, &FreeSans9pt7b, 1);
    drawCenteredTextFont("192.168.4.1", 206, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    /* Cancel button */
    drawCardButton(80, 280, LCD_WIDTH - 160, 50, "Cancel",
                   CLR_TEXT, CLR_CARD_BG, false);
}

void UIManager::handlePortalTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_RIGHT ||
        (gesture.event == TOUCH_TAP &&
         gesture.x >= 80 && gesture.x <= LCD_WIDTH - 80 &&
         gesture.y >= 280 && gesture.y < 330)) {
        if (_portal) _portal->stop();
        navigateTo(SCREEN_WIFI_SETUP);
    }
}

/* ══════════════════════════════════════════════════════════
 *  TIME SETUP
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawTimeSetup() {
    if (!_screenChanged) return;
    drawCenteredTextFont("TIMEZONE", 44, CLR_PRIMARY, &FreeSansBold12pt7b, 1);

    /* Visible window of 4 entries centered on selection. */
    int top = (int)_tzSelectedIdx - 1;
    if (top < 0) top = 0;
    if (top + 4 > TZ_PRESET_COUNT) top = TZ_PRESET_COUNT - 4;

    int16_t y = 80;
    for (int i = top; i < top + 4 && i < TZ_PRESET_COUNT; i++) {
        bool sel = (i == _tzSelectedIdx);
        uint16_t bg = sel ? CLR_PRIMARY : CLR_CARD_BG;
        uint16_t fg = sel ? CLR_BG : CLR_TEXT;
        _gfx->fillRoundRect(40, y, LCD_WIDTH - 80, 48, 12, bg);
        _gfx->setFont(&FreeSansBold12pt7b);
        _gfx->setTextColor(fg);
        _gfx->setCursor(56, y + 30);
        _gfx->print(TZ_PRESETS[i].label);
        y += 56;
    }

    drawCenteredTextFont("tap to select • swipe right to back",
                         LCD_HEIGHT - 30, CLR_TEXT_FAINT, &FreeSans9pt7b, 1);
}

void UIManager::handleTimeSetupTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_RIGHT) {
        navigateTo(SCREEN_SETTINGS);
        return;
    }
    if (gesture.event == TOUCH_SWIPE_UP) {
        if (_tzSelectedIdx + 1 < TZ_PRESET_COUNT) {
            _tzSelectedIdx++;
            _screenChanged = true;
        }
        return;
    }
    if (gesture.event == TOUCH_SWIPE_DOWN) {
        if (_tzSelectedIdx > 0) {
            _tzSelectedIdx--;
            _screenChanged = true;
        }
        return;
    }
    if (gesture.event != TOUCH_TAP) return;

    int top = (int)_tzSelectedIdx - 1;
    if (top < 0) top = 0;
    if (top + 4 > TZ_PRESET_COUNT) top = TZ_PRESET_COUNT - 4;

    int16_t y = 80;
    for (int i = top; i < top + 4 && i < TZ_PRESET_COUNT; i++) {
        if (gesture.x >= 40 && gesture.x <= LCD_WIDTH - 40 &&
            gesture.y >= y && gesture.y < y + 48) {
            _tzSelectedIdx = i;
            /* Save TZ to NVS */
            Preferences p;
            p.begin("huonyx-time", false);
            p.putString("tz", TZ_PRESETS[i].posix);
            p.putString("label", TZ_PRESETS[i].label);
            p.end();
            /* Apply immediately */
            setenv("TZ", TZ_PRESETS[i].posix, 1);
            tzset();
            showNotification("Timezone updated", CLR_SUCCESS);
            _screenChanged = true;
            return;
        }
        y += 56;
    }
}

/* ══════════════════════════════════════════════════════════
 *  KEYBOARD
 * ══════════════════════════════════════════════════════════ */
void UIManager::openKeyboard(const char* title, bool isPassword,
                             const char* initial, KeyboardCallback cb,
                             ScreenId returnScreen) {
    strncpy(_kbTitle, title, sizeof(_kbTitle) - 1);
    _kbTitle[sizeof(_kbTitle) - 1] = '\0';
    _kbIsPassword = isPassword;
    _kbCallback = cb;
    _kbReturnScreen = returnScreen;
    _kbMode = 0;
    _kbPressedKey = -1;
    if (initial) {
        strncpy(_kbInput, initial, sizeof(_kbInput) - 1);
        _kbInput[sizeof(_kbInput) - 1] = '\0';
    } else {
        _kbInput[0] = '\0';
    }
    /* navigateTo bails out if we're already on SCREEN_KEYBOARD (re-open from
     * a chained callback like SSID→password). Force a redraw in that case. */
    if (_currentScreen == SCREEN_KEYBOARD) {
        _screenChanged = true;
    } else {
        navigateTo(SCREEN_KEYBOARD);
    }
}

void UIManager::drawKeyboard() {
    if (!_screenChanged) return;

    /* Title */
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(CLR_TEXT_DIM);
    int16_t tw = textWidth(_kbTitle, &FreeSans9pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - tw / 2, 30);
    _gfx->print(_kbTitle);

    /* Input field */
    _gfx->fillRoundRect(KB_X_MIN, KB_Y_INPUT_TOP,
                        KB_X_MAX - KB_X_MIN, KB_Y_INPUT_H, 8, CLR_CARD_BG);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(KB_X_MIN + 12, KB_Y_INPUT_TOP + 26);
    if (_kbIsPassword) {
        for (size_t i = 0; i < strlen(_kbInput); i++) _gfx->print('*');
    } else {
        _gfx->print(_kbInput);
    }
    /* Blinking caret */
    if ((_animFrame / 15) & 1) {
        int16_t caretX = KB_X_MIN + 12 + (int)strlen(_kbInput) * 12;
        if (caretX < KB_X_MAX - 8)
            _gfx->drawFastVLine(caretX, KB_Y_INPUT_TOP + 8, 22, CLR_PRIMARY);
    }

    /* Pick rows based on mode */
    const char* row1 = _kbMode == 0 ? KB_ROW1_LOWER :
                       _kbMode == 1 ? KB_ROW1_UPPER : KB_ROW1_SYM;
    const char* row2 = _kbMode == 0 ? KB_ROW2_LOWER :
                       _kbMode == 1 ? KB_ROW2_UPPER : KB_ROW2_SYM;
    const char* row3 = _kbMode == 0 ? KB_ROW3_LOWER :
                       _kbMode == 1 ? KB_ROW3_UPPER : KB_ROW3_SYM;

    int16_t kw = (KB_X_MAX - KB_X_MIN - 9 * KB_KEY_GAP) / 10; /* row1 = 10 keys */
    /* Row 1 */
    for (int i = 0; i < 10 && row1[i]; i++) {
        char lbl[2] = { row1[i], 0 };
        int16_t kx = KB_X_MIN + i * (kw + KB_KEY_GAP);
        drawKey(kx, KB_Y_R1, kw, KB_KEY_H, lbl, CLR_KEY_BG, CLR_TEXT, false);
    }
    /* Row 2 — 9 keys, indented */
    int n2 = strlen(row2);
    int16_t row2Start = KB_X_MIN + (kw + KB_KEY_GAP) / 2;
    for (int i = 0; i < n2; i++) {
        char lbl[2] = { row2[i], 0 };
        int16_t kx = row2Start + i * (kw + KB_KEY_GAP);
        drawKey(kx, KB_Y_R2, kw, KB_KEY_H, lbl, CLR_KEY_BG, CLR_TEXT, false);
    }
    /* Row 3 — shift + 7 keys + backspace */
    int16_t fnW = kw + (kw / 2);   /* shift / backspace are wider */
    int16_t r3x = KB_X_MIN;
    drawKey(r3x, KB_Y_R3, fnW, KB_KEY_H,
            _kbMode == 1 ? "SHIFT" : "shift",
            _kbMode == 1 ? CLR_PRIMARY : CLR_KEY_FN,
            _kbMode == 1 ? CLR_BG : CLR_TEXT, false);
    r3x += fnW + KB_KEY_GAP;
    int n3 = strlen(row3);
    for (int i = 0; i < n3 && i < 7; i++) {
        char lbl[2] = { row3[i], 0 };
        drawKey(r3x, KB_Y_R3, kw, KB_KEY_H, lbl, CLR_KEY_BG, CLR_TEXT, false);
        r3x += kw + KB_KEY_GAP;
    }
    drawKey(r3x, KB_Y_R3, fnW, KB_KEY_H, "<-", CLR_KEY_FN, CLR_TEXT, false);

    /* Row 4 — [123 / abc] [space] [OK] */
    int16_t r4x = KB_X_MIN;
    int16_t modeW = fnW + 4;
    drawKey(r4x, KB_Y_R4, modeW, KB_KEY_H,
            _kbMode == 2 ? "abc" : "123",
            CLR_KEY_FN, CLR_TEXT, false);
    r4x += modeW + KB_KEY_GAP;
    int16_t spaceW = (KB_X_MAX - KB_X_MIN) - 2 * (modeW + KB_KEY_GAP);
    drawKey(r4x, KB_Y_R4, spaceW, KB_KEY_H, " ", CLR_KEY_FN, CLR_TEXT, false);
    r4x += spaceW + KB_KEY_GAP;
    drawKey(r4x, KB_Y_R4, modeW, KB_KEY_H, "OK", CLR_PRIMARY, CLR_BG, false);

    /* Cancel hint */
    _gfx->setFont(&FreeSans9pt7b);
    _gfx->setTextColor(CLR_TEXT_FAINT);
    const char* hint = "swipe right to cancel";
    int16_t hw = textWidth(hint, &FreeSans9pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - hw / 2, KB_Y_R4 + KB_KEY_H + 22);
    _gfx->print(hint);
}

int UIManager::keyboardHitTest(int16_t x, int16_t y, char* outChar) {
    *outChar = 0;
    int16_t kw = (KB_X_MAX - KB_X_MIN - 9 * KB_KEY_GAP) / 10;
    int16_t fnW = kw + (kw / 2);

    const char* row1 = _kbMode == 0 ? KB_ROW1_LOWER :
                       _kbMode == 1 ? KB_ROW1_UPPER : KB_ROW1_SYM;
    const char* row2 = _kbMode == 0 ? KB_ROW2_LOWER :
                       _kbMode == 1 ? KB_ROW2_UPPER : KB_ROW2_SYM;
    const char* row3 = _kbMode == 0 ? KB_ROW3_LOWER :
                       _kbMode == 1 ? KB_ROW3_UPPER : KB_ROW3_SYM;

    /* Row 1 */
    if (y >= KB_Y_R1 && y < KB_Y_R1 + KB_KEY_H) {
        for (int i = 0; i < 10 && row1[i]; i++) {
            int16_t kx = KB_X_MIN + i * (kw + KB_KEY_GAP);
            if (x >= kx && x < kx + kw) { *outChar = row1[i]; return row1[i]; }
        }
    }
    /* Row 2 */
    if (y >= KB_Y_R2 && y < KB_Y_R2 + KB_KEY_H) {
        int n2 = strlen(row2);
        int16_t row2Start = KB_X_MIN + (kw + KB_KEY_GAP) / 2;
        for (int i = 0; i < n2; i++) {
            int16_t kx = row2Start + i * (kw + KB_KEY_GAP);
            if (x >= kx && x < kx + kw) { *outChar = row2[i]; return row2[i]; }
        }
    }
    /* Row 3 */
    if (y >= KB_Y_R3 && y < KB_Y_R3 + KB_KEY_H) {
        int16_t r3x = KB_X_MIN;
        if (x >= r3x && x < r3x + fnW) return KB_KEY_SHIFT;
        r3x += fnW + KB_KEY_GAP;
        int n3 = strlen(row3);
        for (int i = 0; i < n3 && i < 7; i++) {
            if (x >= r3x && x < r3x + kw) { *outChar = row3[i]; return row3[i]; }
            r3x += kw + KB_KEY_GAP;
        }
        if (x >= r3x && x < r3x + fnW) return KB_KEY_BACKSPACE;
    }
    /* Row 4 */
    if (y >= KB_Y_R4 && y < KB_Y_R4 + KB_KEY_H) {
        int16_t r4x = KB_X_MIN;
        int16_t modeW = fnW + 4;
        if (x >= r4x && x < r4x + modeW) return KB_KEY_SYMBOLS;
        r4x += modeW + KB_KEY_GAP;
        int16_t spaceW = (KB_X_MAX - KB_X_MIN) - 2 * (modeW + KB_KEY_GAP);
        if (x >= r4x && x < r4x + spaceW) return KB_KEY_SPACE;
        r4x += spaceW + KB_KEY_GAP;
        if (x >= r4x && x < r4x + modeW) return KB_KEY_OK;
    }
    return KB_KEY_NONE;
}

void UIManager::handleKeyboardTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_RIGHT) {
        keyboardCommit(false);
        return;
    }
    if (gesture.event != TOUCH_TAP) return;

    char ch = 0;
    int code = keyboardHitTest(gesture.x, gesture.y, &ch);
    if (code == KB_KEY_NONE) return;

    if (code > 0) {
        /* Real character */
        size_t len = strlen(_kbInput);
        if (len + 1 < sizeof(_kbInput)) {
            _kbInput[len] = (char)code;
            _kbInput[len + 1] = '\0';
        }
        /* After typing in shift mode, drop back to lowercase. */
        if (_kbMode == 1) _kbMode = 0;
        _screenChanged = true;
        return;
    }
    switch (code) {
        case KB_KEY_SHIFT:
            _kbMode = (_kbMode == 1) ? 0 : 1;
            _screenChanged = true;
            break;
        case KB_KEY_BACKSPACE: {
            size_t len = strlen(_kbInput);
            if (len > 0) {
                _kbInput[len - 1] = '\0';
                _screenChanged = true;
            }
            break;
        }
        case KB_KEY_SYMBOLS:
            _kbMode = (_kbMode == 2) ? 0 : 2;
            _screenChanged = true;
            break;
        case KB_KEY_SPACE: {
            size_t len = strlen(_kbInput);
            if (len + 1 < sizeof(_kbInput)) {
                _kbInput[len] = ' ';
                _kbInput[len + 1] = '\0';
                _screenChanged = true;
            }
            break;
        }
        case KB_KEY_OK:
            keyboardCommit(true);
            break;
    }
}

void UIManager::keyboardCommit(bool ok) {
    KeyboardCallback cb = _kbCallback;
    ScreenId ret = _kbReturnScreen;
    char snap[KEYBOARD_BUFFER_LEN];
    strncpy(snap, _kbInput, sizeof(snap));
    snap[sizeof(snap) - 1] = '\0';
    /* Reset state before firing callback so callback can re-open keyboard. */
    _kbCallback = nullptr;
    _kbInput[0] = '\0';
    if (cb) cb(snap, ok);
    /* If the callback re-opened the keyboard (SSID→password chain), it will
     * have set _kbCallback again — leave the user on the keyboard. */
    if (_kbCallback != nullptr) return;
    /* Otherwise close the keyboard, unless the callback already navigated. */
    if (_currentScreen == SCREEN_KEYBOARD) navigateTo(ret);
}

/* ══════════════════════════════════════════════════════════
 *  NOTIFICATION
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawNotification() {
    int16_t notifW = 300;
    int16_t notifH = 50;
    int16_t nx = SCREEN_CENTER_X - notifW / 2;
    int16_t ny = 24;

    _gfx->fillRoundRect(nx, ny, notifW, notifH, 25, _notifColor);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_BG);
    int16_t tw = textWidth(_notifText, &FreeSansBold12pt7b, 1);
    _gfx->setCursor(SCREEN_CENTER_X - tw / 2, ny + 32);
    _gfx->print(_notifText);
}

/* ══════════════════════════════════════════════════════════
 *  HELPERS
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawCenteredText(const char* text, int16_t y,
                                 uint16_t color, uint8_t size) {
    _gfx->setFont(nullptr);
    _gfx->setTextSize(size);
    _gfx->setTextColor(color);
    int16_t tw = strlen(text) * 6 * size;
    _gfx->setCursor(SCREEN_CENTER_X - tw / 2, y);
    _gfx->print(text);
}

void UIManager::drawCenteredTextFont(const char* text, int16_t y,
                                     uint16_t color, const GFXfont* font,
                                     uint8_t sizeMult) {
    _gfx->setFont(font);
    _gfx->setTextSize(sizeMult);
    _gfx->setTextColor(color);
    int16_t tw = textWidth(text, font, sizeMult);
    _gfx->setCursor(SCREEN_CENTER_X - tw / 2, y);
    _gfx->print(text);
}

int16_t UIManager::textWidth(const char* text, const GFXfont* font, uint8_t sizeMult) {
    if (!_gfx) return strlen(text) * 6 * sizeMult;
    _gfx->setFont(font);
    _gfx->setTextSize(sizeMult);
    int16_t x1, y1; uint16_t w, h;
    _gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return (int16_t)w;
}

void UIManager::drawArcSegment(int16_t cx, int16_t cy, int16_t r, int16_t thickness,
                               float startDeg, float endDeg, uint16_t color) {
    /* Step in 1° increments. Rough but fine for status arcs. */
    if (endDeg < startDeg) return;
    for (float a = startDeg; a <= endDeg; a += 1.0f) {
        float rad = a * (float)M_PI / 180.0f - (float)M_PI / 2.0f;
        for (int t = 0; t < thickness; t++) {
            int16_t px = cx + (int16_t)(cosf(rad) * (r - t));
            int16_t py = cy + (int16_t)(sinf(rad) * (r - t));
            _gfx->drawPixel(px, py, color);
        }
    }
}

void UIManager::drawCardButton(int16_t x, int16_t y, int16_t w, int16_t h,
                               const char* label, uint16_t fg, uint16_t bg, bool pressed) {
    _gfx->fillRoundRect(x, y, w, h, h / 2, pressed ? CLR_CARD_HI : bg);
    _gfx->setFont(&FreeSansBold12pt7b);
    _gfx->setTextSize(1);
    _gfx->setTextColor(fg);
    int16_t tw = textWidth(label, &FreeSansBold12pt7b, 1);
    _gfx->setCursor(x + (w - tw) / 2, y + (h / 2) + 8);
    _gfx->print(label);
}

void UIManager::drawKey(int16_t x, int16_t y, int16_t w, int16_t h,
                        const char* label, uint16_t bg, uint16_t fg, bool pressed) {
    _gfx->fillRoundRect(x, y, w, h, 6, pressed ? CLR_KEY_BG_HI : bg);
    /* Single-character keys use bold; multi-char (shift/SHIFT/OK/<-/123/abc/space) use small */
    bool single = strlen(label) == 1 && label[0] != ' ';
    if (single) {
        _gfx->setFont(&FreeSansBold12pt7b);
        _gfx->setTextSize(1);
        _gfx->setTextColor(fg);
        int16_t tw = textWidth(label, &FreeSansBold12pt7b, 1);
        _gfx->setCursor(x + (w - tw) / 2, y + (h / 2) + 8);
        _gfx->print(label);
    } else if (label[0] == ' ') {
        /* Spacebar — draw a thin underline */
        _gfx->drawFastHLine(x + 12, y + h - 12, w - 24, fg);
    } else {
        _gfx->setFont(&FreeSans9pt7b);
        _gfx->setTextSize(1);
        _gfx->setTextColor(fg);
        int16_t tw = textWidth(label, &FreeSans9pt7b, 1);
        _gfx->setCursor(x + (w - tw) / 2, y + (h / 2) + 6);
        _gfx->print(label);
    }
}
