/**
 * Huonyx Watch – UI Manager Implementation
 * Circular AMOLED 412x412 touch interface
 */
#include "ui_manager.h"
#include "display_driver.h"
#include <math.h>

/* Quick reply options */
const char* QUICK_REPLIES[QUICK_REPLY_COUNT] = {
    "Yes",
    "No",
    "Status?",
    "Help"
};

/* ── Constructor ──────────────────────────────────────── */
UIManager::UIManager()
    : _gfx(nullptr)
    , _touch(nullptr)
    , _gw(nullptr)
    , _currentScreen(SCREEN_WATCHFACE)
    , _awake(true)
    , _dirty(true)
    , _lastActivityMs(0)
    , _lastRenderMs(0)
    , _animFrame(0)
    , _hour(0), _min(0), _sec(0)
    , _day(1), _month(1), _year(2026)
    , _battPercent(100)
    , _charging(false)
    , _wifiConnected(false)
    , _gwState(GW_DISCONNECTED)
    , _msgCount(0)
    , _chatScrollOffset(0)
    , _typing(false)
    , _showQuickReplies(false)
    , _notifExpireMs(0)
{
    memset(_messages, 0, sizeof(_messages));
    memset(_notifText, 0, sizeof(_notifText));
    _notifColor = CLR_PRIMARY;
}

/* ── Initialization ───────────────────────────────────── */
void UIManager::begin(Arduino_GFX *display, TouchDriver *touch) {
    _gfx = display;
    _touch = touch;
    _lastActivityMs = millis();
    _dirty = true;
    Serial.println("[UI] Initialized for 412x412 AMOLED");
}

/* ── Main Update Loop ─────────────────────────────────── */
void UIManager::update() {
    if (!_gfx || !_touch) return;

    uint32_t now = millis();

    /* Handle touch input */
    _touch->update();
    TouchGesture gesture = _touch->getGesture();

    if (gesture.event != TOUCH_NONE) {
        _lastActivityMs = now;
        if (!_awake) {
            wake();
            return; /* Consume the wake touch */
        }
        handleTouch(gesture);
    }

    /* Auto-sleep */
    if (_awake && (now - _lastActivityMs > SLEEP_TIMEOUT_MS)) {
        sleep();
        return;
    }

    if (!_awake) return;

    /* Render at target FPS */
    uint32_t frameInterval = 1000 / UI_FPS;
    if (now - _lastRenderMs < frameInterval && !_dirty) return;
    _lastRenderMs = now;
    _animFrame++;

    /* Clear screen */
    if (_dirty) {
        _gfx->fillScreen(CLR_BG);
    }

    /* Draw current screen */
    switch (_currentScreen) {
        case SCREEN_WATCHFACE: drawWatchface(); break;
        case SCREEN_CHAT:      drawChat();      break;
        case SCREEN_SESSIONS:  drawSessions();  break;
        case SCREEN_SETTINGS:  drawSettings();  break;
        case SCREEN_WIFI_SETUP: drawWifiSetup(); break;
        default: break;
    }

    /* Draw notification overlay */
    if (_notifExpireMs > 0 && now < _notifExpireMs) {
        drawNotification();
    } else {
        _notifExpireMs = 0;
    }

    _dirty = false;
}

/* ── Navigation ───────────────────────────────────────── */
void UIManager::navigateTo(ScreenId screen) {
    if (screen != _currentScreen) {
        _currentScreen = screen;
        _dirty = true;
        _chatScrollOffset = 0;
        _showQuickReplies = false;
    }
}

/* ── Wake / Sleep ─────────────────────────────────────── */
void UIManager::wake() {
    _awake = true;
    _dirty = true;
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

/* ── State Setters ────────────────────────────────────── */
void UIManager::setTime(uint8_t h, uint8_t m, uint8_t s) {
    if (h != _hour || m != _min || s != _sec) {
        _hour = h; _min = m; _sec = s;
        if (_currentScreen == SCREEN_WATCHFACE) _dirty = true;
    }
}

void UIManager::setDate(uint8_t day, uint8_t month, uint16_t year) {
    _day = day; _month = month; _year = year;
}

void UIManager::setBattery(uint8_t percent, bool charging) {
    _battPercent = percent;
    _charging = charging;
}

void UIManager::setWifiConnected(bool connected) {
    _wifiConnected = connected;
    _dirty = true;
}

void UIManager::setGatewayState(GatewayState state) {
    _gwState = state;
    _dirty = true;
}

void UIManager::addChatMessage(const char* text, bool isUser) {
    /* Shift messages up if full */
    if (_msgCount >= CHAT_MAX_MESSAGES) {
        memmove(&_messages[0], &_messages[1], sizeof(ChatMsg) * (CHAT_MAX_MESSAGES - 1));
        _msgCount = CHAT_MAX_MESSAGES - 1;
    }
    _messages[_msgCount].isUser = isUser;
    strncpy(_messages[_msgCount].text, text, CHAT_MAX_MSG_LEN - 1);
    _messages[_msgCount].text[CHAT_MAX_MSG_LEN - 1] = '\0';
    _msgCount++;
    if (_currentScreen == SCREEN_CHAT) _dirty = true;
}

void UIManager::setTypingIndicator(bool typing) {
    if (_typing != typing) {
        _typing = typing;
        if (_currentScreen == SCREEN_CHAT) _dirty = true;
    }
}

void UIManager::showNotification(const char* text, uint16_t color) {
    strncpy(_notifText, text, sizeof(_notifText) - 1);
    _notifColor = color;
    _notifExpireMs = millis() + 3000;
    _dirty = true;
}

/* ── Touch Handler ────────────────────────────────────── */
void UIManager::handleTouch(TouchGesture &gesture) {
    /* Global swipe navigation */
    if (gesture.event == TOUCH_SWIPE_LEFT) {
        uint8_t next = (uint8_t)_currentScreen + 1;
        if (next < SCREEN_COUNT) navigateTo((ScreenId)next);
        return;
    }
    if (gesture.event == TOUCH_SWIPE_RIGHT) {
        if (_currentScreen > 0) {
            navigateTo((ScreenId)((uint8_t)_currentScreen - 1));
        }
        return;
    }

    /* Screen-specific handling */
    switch (_currentScreen) {
        case SCREEN_WATCHFACE: handleWatchfaceTouch(gesture); break;
        case SCREEN_CHAT:      handleChatTouch(gesture);      break;
        case SCREEN_SESSIONS:  handleSessionsTouch(gesture);  break;
        case SCREEN_SETTINGS:  handleSettingsTouch(gesture);  break;
        default: break;
    }
}

void UIManager::handleWatchfaceTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_TAP) {
        /* Tap center to go to chat */
        int16_t dx = gesture.x - SCREEN_CENTER_X;
        int16_t dy = gesture.y - SCREEN_CENTER_Y;
        if (dx * dx + dy * dy < 80 * 80) {
            navigateTo(SCREEN_CHAT);
        }
    }
}

void UIManager::handleChatTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_SWIPE_UP) {
        /* Scroll chat up */
        if (_chatScrollOffset < _msgCount - 3) _chatScrollOffset++;
        _dirty = true;
    } else if (gesture.event == TOUCH_SWIPE_DOWN) {
        /* Scroll chat down */
        if (_chatScrollOffset > 0) _chatScrollOffset--;
        _dirty = true;
    } else if (gesture.event == TOUCH_LONG_PRESS) {
        /* Toggle quick replies */
        _showQuickReplies = !_showQuickReplies;
        _dirty = true;
    } else if (gesture.event == TOUCH_TAP && _showQuickReplies) {
        /* Check if tapped a quick reply */
        int16_t replyY = LCD_HEIGHT / 2 - (QUICK_REPLY_COUNT * 40) / 2;
        for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
            int16_t btnTop = replyY + i * 45;
            if (gesture.y >= btnTop && gesture.y < btnTop + 36 &&
                gesture.x >= 80 && gesture.x <= LCD_WIDTH - 80) {
                /* Send quick reply */
                if (_gw && _gw->isConnected()) {
                    _gw->sendMessage(_gw->getSessionKey(), QUICK_REPLIES[i]);
                    addChatMessage(QUICK_REPLIES[i], true);
                }
                _showQuickReplies = false;
                _dirty = true;
                break;
            }
        }
    }
}

void UIManager::handleSessionsTouch(TouchGesture &gesture) {
    /* Tap to select session - handled in drawSessions */
    (void)gesture;
}

void UIManager::handleSettingsTouch(TouchGesture &gesture) {
    if (gesture.event == TOUCH_TAP) {
        /* WiFi setup button area */
        if (gesture.y >= 160 && gesture.y <= 210) {
            navigateTo(SCREEN_WIFI_SETUP);
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  DRAWING METHODS
 * ══════════════════════════════════════════════════════════ */

/* ── Watchface ────────────────────────────────────────── */
void UIManager::drawWatchface() {
    /* Outer status ring */
    drawStatusRing();

    /* Time - large centered */
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", _hour, _min);
    _gfx->setTextSize(5);
    _gfx->setTextColor(CLR_TEXT);
    int16_t tw = strlen(timeBuf) * 6 * 5; /* approximate */
    _gfx->setCursor(SCREEN_CENTER_X - tw / 2, SCREEN_CENTER_Y - 30);
    _gfx->print(timeBuf);

    /* Seconds - smaller below */
    char secBuf[4];
    snprintf(secBuf, sizeof(secBuf), ":%02d", _sec);
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(SCREEN_CENTER_X + tw / 2 + 4, SCREEN_CENTER_Y - 10);
    _gfx->print(secBuf);

    /* Date */
    static const char* months[] = {"JAN","FEB","MAR","APR","MAY","JUN",
                                    "JUL","AUG","SEP","OCT","NOV","DEC"};
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d", months[(_month - 1) % 12], _day);
    drawCenteredText(dateBuf, SCREEN_CENTER_Y + 40, CLR_TEXT_DIM, 2);

    /* Battery indicator */
    char battBuf[8];
    snprintf(battBuf, sizeof(battBuf), "%d%%", _battPercent);
    uint16_t battColor = _battPercent > 20 ? CLR_SUCCESS : CLR_ERROR;
    if (_charging) battColor = CLR_WARNING;
    _gfx->setTextSize(1);
    _gfx->setTextColor(battColor);
    _gfx->setCursor(SCREEN_CENTER_X - 12, SCREEN_CENTER_Y + 70);
    _gfx->print(battBuf);
    if (_charging) {
        _gfx->print(" +");
    }

    /* Connection status dots at bottom */
    int16_t dotY = SCREEN_CENTER_Y + 100;
    int16_t dotSpacing = 30;
    int16_t dotStartX = SCREEN_CENTER_X - dotSpacing;

    /* WiFi dot */
    _gfx->fillCircle(dotStartX, dotY, 5, _wifiConnected ? CLR_SUCCESS : CLR_ERROR);
    /* Gateway dot */
    uint16_t gwColor = CLR_ERROR;
    if (_gwState == GW_AUTHENTICATED) gwColor = CLR_SUCCESS;
    else if (_gwState == GW_CONNECTING || _gwState == GW_CONNECTED) gwColor = CLR_WARNING;
    _gfx->fillCircle(dotStartX + dotSpacing, dotY, 5, gwColor);
    /* Agent dot */
    _gfx->fillCircle(dotStartX + dotSpacing * 2, dotY, 5,
                     _gwState == GW_AUTHENTICATED ? CLR_PRIMARY : CLR_TEXT_DIM);

    /* Labels under dots */
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(dotStartX - 8, dotY + 12);
    _gfx->print("WiFi");
    _gfx->setCursor(dotStartX + dotSpacing - 4, dotY + 12);
    _gfx->print("GW");
    _gfx->setCursor(dotStartX + dotSpacing * 2 - 8, dotY + 12);
    _gfx->print("Agent");

    /* Tap hint */
    if (_animFrame % 60 < 40) {
        drawCenteredText("tap to chat", SCREEN_CENTER_Y + 140, CLR_TEXT_DIM, 1);
    }
}

/* ── Status Ring ──────────────────────────────────────── */
void UIManager::drawStatusRing() {
    /* Draw a thin ring around the edge showing battery level */
    float battAngle = (float)_battPercent / 100.0f * 360.0f;
    uint16_t ringColor = _battPercent > 20 ? CLR_RING : CLR_ERROR;

    /* Background ring (dim) */
    for (int a = 0; a < 360; a += 2) {
        float rad = (float)a * PI / 180.0f - PI / 2.0f;
        int16_t px = SCREEN_CENTER_X + (int16_t)(cos(rad) * (SCREEN_RADIUS - 8));
        int16_t py = SCREEN_CENTER_Y + (int16_t)(sin(rad) * (SCREEN_RADIUS - 8));
        _gfx->drawPixel(px, py, CLR_CARD_BG);
    }

    /* Battery arc (bright) */
    for (int a = 0; a < (int)battAngle; a += 2) {
        float rad = (float)a * PI / 180.0f - PI / 2.0f;
        int16_t px = SCREEN_CENTER_X + (int16_t)(cos(rad) * (SCREEN_RADIUS - 8));
        int16_t py = SCREEN_CENTER_Y + (int16_t)(sin(rad) * (SCREEN_RADIUS - 8));
        _gfx->drawPixel(px, py, ringColor);
        /* Make it 2px thick */
        px = SCREEN_CENTER_X + (int16_t)(cos(rad) * (SCREEN_RADIUS - 9));
        py = SCREEN_CENTER_Y + (int16_t)(sin(rad) * (SCREEN_RADIUS - 9));
        _gfx->drawPixel(px, py, ringColor);
    }
}

/* ── Chat Screen ──────────────────────────────────────── */
void UIManager::drawChat() {
    /* Header */
    drawCenteredText("HUONYX", 30, CLR_PRIMARY, 2);

    /* Connection indicator */
    uint16_t statusColor = (_gwState == GW_AUTHENTICATED) ? CLR_SUCCESS : CLR_ERROR;
    _gfx->fillCircle(SCREEN_CENTER_X + 60, 35, 4, statusColor);

    /* Chat messages area (circular clipping) */
    int16_t msgAreaTop = 60;
    int16_t msgAreaBottom = _showQuickReplies ? 240 : 370;
    int16_t msgHeight = 50;
    int16_t padding = 30; /* Horizontal padding for circular display */

    int startIdx = max(0, (int)_msgCount - 6 - _chatScrollOffset);
    int endIdx = min((int)_msgCount, startIdx + 6);

    int16_t yPos = msgAreaTop;
    for (int i = startIdx; i < endIdx && yPos < msgAreaBottom; i++) {
        ChatMsg &msg = _messages[i];
        uint16_t bgColor = msg.isUser ? CLR_USER_MSG : CLR_AGENT_MSG;

        /* Calculate bubble width based on text length */
        int16_t maxBubbleW = LCD_WIDTH - padding * 2 - 40;
        int16_t textW = strlen(msg.text) * 6;
        int16_t bubbleW = min(textW + 16, maxBubbleW);
        int16_t bubbleH = 32;

        /* Wrap text if needed */
        int charsPerLine = (bubbleW - 12) / 6;
        int lines = (strlen(msg.text) + charsPerLine - 1) / charsPerLine;
        if (lines > 1) bubbleH = 16 + lines * 14;

        /* Position: user right-aligned, agent left-aligned */
        int16_t bx;
        if (msg.isUser) {
            bx = LCD_WIDTH - padding - bubbleW;
        } else {
            bx = padding;
        }

        /* Draw bubble */
        _gfx->fillRoundRect(bx, yPos, bubbleW, bubbleH, 12, bgColor);

        /* Draw text with word wrap */
        _gfx->setTextSize(1);
        _gfx->setTextColor(CLR_TEXT);
        int16_t tx = bx + 8;
        int16_t ty = yPos + 8;
        const char* ptr = msg.text;
        int remaining = strlen(msg.text);
        while (remaining > 0 && ty < yPos + bubbleH - 4) {
            int lineLen = min(remaining, charsPerLine);
            char lineBuf[64];
            strncpy(lineBuf, ptr, lineLen);
            lineBuf[lineLen] = '\0';
            _gfx->setCursor(tx, ty);
            _gfx->print(lineBuf);
            ptr += lineLen;
            remaining -= lineLen;
            ty += 14;
        }

        yPos += bubbleH + 8;
    }

    /* Typing indicator */
    if (_typing) {
        int16_t dotX = padding + 20;
        int16_t dotY2 = yPos + 10;
        for (int i = 0; i < 3; i++) {
            uint8_t brightness = ((_animFrame + i * 8) % 24 < 12) ? 255 : 100;
            uint16_t dotColor = (brightness > 200) ? CLR_TEXT : CLR_TEXT_DIM;
            _gfx->fillCircle(dotX + i * 14, dotY2, 4, dotColor);
        }
    }

    /* Quick replies overlay */
    if (_showQuickReplies) {
        int16_t replyY = LCD_HEIGHT / 2 - (QUICK_REPLY_COUNT * 45) / 2;
        /* Semi-transparent background */
        _gfx->fillRoundRect(60, replyY - 10, LCD_WIDTH - 120,
                           QUICK_REPLY_COUNT * 45 + 20, 20, CLR_CARD_BG);

        for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
            int16_t btnY = replyY + i * 45;
            _gfx->fillRoundRect(80, btnY, LCD_WIDTH - 160, 36, 18, CLR_PRIMARY);
            _gfx->setTextSize(2);
            _gfx->setTextColor(CLR_BG);
            int16_t tw2 = strlen(QUICK_REPLIES[i]) * 12;
            _gfx->setCursor(SCREEN_CENTER_X - tw2 / 2, btnY + 10);
            _gfx->print(QUICK_REPLIES[i]);
        }
    }

    /* Bottom hint */
    if (!_showQuickReplies) {
        drawCenteredText("long-press for replies", LCD_HEIGHT - 40, CLR_TEXT_DIM, 1);
    }
}

/* ── Sessions Screen ──────────────────────────────────── */
void UIManager::drawSessions() {
    drawCenteredText("SESSIONS", 40, CLR_PRIMARY, 2);

    if (_gwState != GW_AUTHENTICATED) {
        drawCenteredText("Not connected", SCREEN_CENTER_Y, CLR_TEXT_DIM, 2);
        return;
    }

    /* Show current session */
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(60, 80);
    _gfx->print("Active:");
    _gfx->setTextColor(CLR_SUCCESS);
    _gfx->setCursor(60, 100);
    if (_gw) {
        _gfx->print(_gw->getSessionKey());
    }

    /* Refresh button */
    _gfx->fillRoundRect(SCREEN_CENTER_X - 60, SCREEN_CENTER_Y + 80, 120, 40, 20, CLR_PRIMARY);
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_BG);
    _gfx->setCursor(SCREEN_CENTER_X - 42, SCREEN_CENTER_Y + 92);
    _gfx->print("Refresh");
}

/* ── Settings Screen ──────────────────────────────────── */
void UIManager::drawSettings() {
    drawCenteredText("SETTINGS", 40, CLR_PRIMARY, 2);

    int16_t itemY = 90;
    int16_t itemH = 50;
    int16_t itemPad = 50;

    /* WiFi */
    _gfx->fillRoundRect(itemPad, itemY, LCD_WIDTH - itemPad * 2, itemH, 12, CLR_CARD_BG);
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(itemPad + 16, itemY + 16);
    _gfx->print("WiFi");
    _gfx->fillCircle(LCD_WIDTH - itemPad - 30, itemY + itemH / 2, 6,
                     _wifiConnected ? CLR_SUCCESS : CLR_ERROR);
    itemY += itemH + 12;

    /* Gateway */
    _gfx->fillRoundRect(itemPad, itemY, LCD_WIDTH - itemPad * 2, itemH, 12, CLR_CARD_BG);
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(itemPad + 16, itemY + 16);
    _gfx->print("Gateway");
    uint16_t gwDotColor = CLR_ERROR;
    if (_gwState == GW_AUTHENTICATED) gwDotColor = CLR_SUCCESS;
    else if (_gwState == GW_CONNECTING) gwDotColor = CLR_WARNING;
    _gfx->fillCircle(LCD_WIDTH - itemPad - 30, itemY + itemH / 2, 6, gwDotColor);
    itemY += itemH + 12;

    /* Battery info */
    _gfx->fillRoundRect(itemPad, itemY, LCD_WIDTH - itemPad * 2, itemH, 12, CLR_CARD_BG);
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(itemPad + 16, itemY + 16);
    char battStr[16];
    snprintf(battStr, sizeof(battStr), "Battery %d%%", _battPercent);
    _gfx->print(battStr);
    itemY += itemH + 12;

    /* Firmware version */
    _gfx->fillRoundRect(itemPad, itemY, LCD_WIDTH - itemPad * 2, itemH, 12, CLR_CARD_BG);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_TEXT_DIM);
    _gfx->setCursor(itemPad + 16, itemY + 12);
    _gfx->print("Firmware");
    _gfx->setTextSize(2);
    _gfx->setTextColor(CLR_TEXT);
    _gfx->setCursor(itemPad + 16, itemY + 28);
    _gfx->print(FIRMWARE_VERSION);

    /* Page indicator dots at bottom */
    drawCenteredText("< swipe >", LCD_HEIGHT - 40, CLR_TEXT_DIM, 1);
}

/* ── WiFi Setup Screen ────────────────────────────────── */
void UIManager::drawWifiSetup() {
    drawCenteredText("WiFi SETUP", 40, CLR_PRIMARY, 2);

    if (_wifiConnected) {
        drawCenteredText("Connected!", SCREEN_CENTER_Y - 20, CLR_SUCCESS, 2);
        _gfx->setTextSize(1);
        _gfx->setTextColor(CLR_TEXT_DIM);
        _gfx->setCursor(60, SCREEN_CENTER_Y + 20);
        _gfx->print("Gateway: ");
        _gfx->print(GATEWAY_HOST);
    } else {
        drawCenteredText("Connecting...", SCREEN_CENTER_Y - 20, CLR_WARNING, 2);
        drawCenteredText("Check WiFi credentials", SCREEN_CENTER_Y + 20, CLR_TEXT_DIM, 1);
        drawCenteredText("in config", SCREEN_CENTER_Y + 36, CLR_TEXT_DIM, 1);
    }

    /* Back hint */
    drawCenteredText("swipe right to go back", LCD_HEIGHT - 40, CLR_TEXT_DIM, 1);
}

/* ── Notification Overlay ─────────────────────────────── */
void UIManager::drawNotification() {
    int16_t notifW = 300;
    int16_t notifH = 50;
    int16_t nx = SCREEN_CENTER_X - notifW / 2;
    int16_t ny = 30;

    _gfx->fillRoundRect(nx, ny, notifW, notifH, 25, _notifColor);
    _gfx->setTextSize(1);
    _gfx->setTextColor(CLR_BG);
    int16_t tw3 = strlen(_notifText) * 6;
    _gfx->setCursor(SCREEN_CENTER_X - tw3 / 2, ny + 20);
    _gfx->print(_notifText);
}

/* ── Helper: Draw Centered Text ───────────────────────── */
void UIManager::drawCenteredText(const char* text, int16_t y, uint16_t color, uint8_t size) {
    _gfx->setTextSize(size);
    _gfx->setTextColor(color);
    int16_t tw4 = strlen(text) * 6 * size;
    _gfx->setCursor(SCREEN_CENTER_X - tw4 / 2, y);
    _gfx->print(text);
}
