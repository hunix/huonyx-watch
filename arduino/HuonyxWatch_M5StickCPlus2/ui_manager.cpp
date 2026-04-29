/*  ═══════════════════════════════════════════════════════
 *  ui_manager.cpp – M5.Display (LovyanGFX) direct rendering
 *  135x240 portrait, button-driven navigation
 *  No LVGL dependency.
 *  ═══════════════════════════════════════════════════════ */
#include "build_config.h"
#include "ui_manager.h"

/* ── Layout constants ────────────────────────────────── */
static const int W = TFT_WIDTH;   /* 135 */
static const int H = TFT_HEIGHT;  /* 240 */
static const int HDR_H   = 22;
static const int HINT_H  = 20;
static const int BODY_Y  = HDR_H;
static const int BODY_H  = H - HDR_H - HINT_H;

#define NOTIF_DURATION_MS 3000

/* ══════════════════════════════════════════════════════════
 *  BEGIN
 * ══════════════════════════════════════════════════════════ */
void UIManager::begin(GatewayClient* gw, ConfigManager* cfg,
                      FlipperBLE* flipper, SupabaseBridge* bridge) {
    _gw      = gw;
    _cfg     = cfg;
    _flipper = flipper;
    _bridge  = bridge;
    _currentScreen   = SCREEN_WATCHFACE;
    _lastInteraction = millis();
    _isSleeping      = false;
    _dirty           = true;

    M5.Display.setRotation(0);
    M5.Display.setTextWrap(true, true);
    M5.Display.fillScreen(CLR_BG);
    M5.Display.setBrightness(_targetBacklight);
}

/* ══════════════════════════════════════════════════════════
 *  TICK – called every loop()
 * ══════════════════════════════════════════════════════════ */
void UIManager::tick() {
    if (!_isSleeping && (millis() - _lastInteraction > SLEEP_TIMEOUT_MS)) {
        sleepDisplay();
    }
    if (_notifDismissTime > 0 && millis() > _notifDismissTime) {
        _notifDismissTime = 0;
        _dirty = true;
    }
    if (_dirty && !_isSleeping) {
        _dirty = false;
        M5.Display.startWrite();
        switch (_currentScreen) {
            case SCREEN_WATCHFACE:       drawWatchface();      break;
            case SCREEN_CHAT:            drawChatScreen();     break;
            case SCREEN_FLIPPER:         drawFlipperScreen();  break;
            case SCREEN_SETTINGS:        drawSettingsScreen(); break;
            case SCREEN_WIFI_SETUP:      drawWifiSetup();      break;
            case SCREEN_GATEWAY_SETUP:   drawGatewaySetup();   break;
            case SCREEN_SUPABASE_SETUP:  drawSupabaseSetup();  break;
            case SCREEN_FLIPPER_SETUP:   drawFlipperSetup();   break;
            default:                     drawWatchface();      break;
        }
        if (_notifDismissTime > 0) drawNotification();
        M5.Display.endWrite();
    }
}

/* ══════════════════════════════════════════════════════════
 *  DRAWING HELPERS
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawHeader(const char* title, uint16_t color) {
    M5.Display.fillRect(0, 0, W, HDR_H, CLR_HEADER);
    M5.Display.setTextColor(color, CLR_HEADER);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(4, 6);
    M5.Display.print(title);
}

void UIManager::drawHintBar(const char* text) {
    M5.Display.fillRect(0, H - HINT_H, W, HINT_H, CLR_HEADER);
    M5.Display.setTextColor(CLR_SUBTEXT, CLR_HEADER);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    int tw = strlen(text) * 6;
    int x = (W - tw) / 2;
    if (x < 2) x = 2;
    M5.Display.setCursor(x, H - HINT_H + 6);
    M5.Display.print(text);
}

void UIManager::drawStatusDot(int x, int y, uint16_t color) {
    M5.Display.fillCircle(x, y, 4, color);
}

/* ══════════════════════════════════════════════════════════
 *  WATCHFACE
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawWatchface() {
    M5.Display.fillScreen(CLR_BG);

    /* Status bar */
    M5.Display.fillRect(0, 0, W, HDR_H, CLR_HEADER);

    uint16_t gwCol = (_gwState == GW_AUTHENTICATED) ? CLR_SUCCESS :
                     (_gwState == GW_CONNECTING)    ? CLR_WARNING : CLR_ERROR;
    uint16_t flCol = (_flipState == FLIP_CONNECTED) ? CLR_SUCCESS :
                     (_flipState == FLIP_SCANNING)  ? CLR_WARNING : CLR_ERROR;
    uint16_t brCol = (_brState == BRIDGE_JOINED)    ? CLR_SUCCESS :
                     (_brState == BRIDGE_CONNECTING) ? CLR_WARNING : CLR_ERROR;
    drawStatusDot(8,  HDR_H / 2, gwCol);
    drawStatusDot(22, HDR_H / 2, flCol);
    drawStatusDot(36, HDR_H / 2, brCol);

    uint16_t wifiCol = (_wifiRssi == 0)  ? CLR_SUBTEXT :
                       (_wifiRssi > -60) ? CLR_SUCCESS :
                       (_wifiRssi > -75) ? CLR_WARNING : CLR_ERROR;
    M5.Display.setTextColor(wifiCol, CLR_HEADER);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(W - 60, 6);
    M5.Display.print("WiFi");

    uint16_t batCol = (_battPct > 20) ? CLR_SUCCESS :
                      (_battPct > 10) ? CLR_WARNING : CLR_ERROR;
    if (_charging) batCol = CLR_ACCENT;
    M5.Display.setTextColor(batCol, CLR_HEADER);
    M5.Display.setCursor(W - 28, 6);
    char batBuf[6];
    snprintf(batBuf, sizeof(batBuf), "%d%%", _battPct >= 0 ? _battPct : 0);
    M5.Display.print(batBuf);

    /* Time */
    M5.Display.setTextColor(CLR_ACCENT, CLR_BG);
    M5.Display.setFont(&fonts::Font4);
    M5.Display.setTextSize(1);
    char timeBuf[12];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d",
             _hour >= 0 ? _hour : 0,
             _min  >= 0 ? _min  : 0,
             _sec  >= 0 ? _sec  : 0);
    int tw = M5.Display.textWidth(timeBuf);
    M5.Display.setCursor((W - tw) / 2, 70);
    M5.Display.print(timeBuf);

    /* Date */
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    tw = M5.Display.textWidth(_dateStr);
    M5.Display.setCursor((W - tw) / 2, 110);
    M5.Display.print(_dateStr);

    /* Connection summary */
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_SUBTEXT, CLR_BG);
    const char* gwTxt = (_gwState == GW_AUTHENTICATED) ? "GW:OK" :
                        (_gwState == GW_CONNECTING)    ? "GW:.." : "GW:--";
    const char* brTxt = (_brState == BRIDGE_JOINED)    ? "SB:OK" :
                        (_brState == BRIDGE_CONNECTING) ? "SB:.." : "SB:--";
    char connBuf[32];
    snprintf(connBuf, sizeof(connBuf), "%s  %s", gwTxt, brTxt);
    tw = strlen(connBuf) * 6;
    M5.Display.setCursor((W - tw) / 2, 145);
    M5.Display.print(connBuf);

    drawHintBar("[A] Next  [B] Chat");
}

/* ══════════════════════════════════════════════════════════
 *  CHAT SCREEN
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawChatScreen() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Chat");

    int y = BODY_Y + 2;
    int maxY = H - HINT_H - 16;

    int startIdx = _chatScrollPos;
    if (startIdx < 0) startIdx = 0;
    if (startIdx >= _chatCount) startIdx = (_chatCount > 0) ? _chatCount - 1 : 0;

    for (int i = startIdx; i < _chatCount && y < maxY; i++) {
        bool isUser = _chatMsgs[i].isUser;
        uint16_t bubbleColor = isUser ? CLR_USER_BUB : CLR_AGENT_BUB;

        const char* prefix = isUser ? "> " : "< ";
        int bubbleX = isUser ? 10 : 2;
        int bubbleW = W - 12;

        int textLen = strlen(_chatMsgs[i].text);
        int lines = (textLen / 20) + 1;
        if (lines > 4) lines = 4;
        int bubbleH = lines * 12 + 6;

        M5.Display.fillRoundRect(bubbleX, y, bubbleW, bubbleH, 4, bubbleColor);
        M5.Display.setTextColor(CLR_TEXT, bubbleColor);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(bubbleX + 3, y + 3);
        M5.Display.print(prefix);

        int cx = bubbleX + 3 + strlen(prefix) * 6;
        int cy = y + 3;
        for (int c = 0; c < textLen && cy < y + bubbleH - 2; c++) {
            if (cx > bubbleX + bubbleW - 8) {
                cx = bubbleX + 3;
                cy += 10;
                if (cy >= y + bubbleH - 2) break;
                M5.Display.setCursor(cx, cy);
            }
            M5.Display.print(_chatMsgs[i].text[c]);
            cx += 6;
        }
        y += bubbleH + 3;
    }

    /* Quick reply overlay */
    if (_quickReplyVisible) {
        int qrY = H - HINT_H - (QUICK_REPLY_COUNT * 18 + 6);
        M5.Display.fillRoundRect(4, qrY, W - 8, QUICK_REPLY_COUNT * 18 + 4, 6, CLR_CARD);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
            bool sel = (i == _quickReplyIdx);
            uint16_t bg = sel ? CLR_ACCENT : CLR_CARD;
            uint16_t fg = sel ? CLR_BG     : CLR_TEXT;
            int iy = qrY + 2 + i * 18;
            M5.Display.fillRoundRect(8, iy, W - 16, 16, 4, bg);
            M5.Display.setTextColor(fg, bg);
            int tw2 = strlen(QUICK_REPLIES[i]) * 6;
            M5.Display.setCursor((W - tw2) / 2, iy + 4);
            M5.Display.print(QUICK_REPLIES[i]);
        }
    }

    if (_agentTyping) {
        M5.Display.setTextColor(CLR_ACCENT, CLR_BG);
        M5.Display.setFont(&fonts::Font0);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(4, H - HINT_H - 12);
        M5.Display.print("typing...");
    }

    drawHintBar("[A] Next [B] Reply");
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER SCREEN
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawFlipperScreen() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Flipper");

    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    M5.Display.setCursor(4, BODY_Y + 4);
    M5.Display.print(_flipDevName);

    const char* stateStr;
    uint16_t stateCol;
    if (_flipState == FLIP_CONNECTED) {
        stateStr = "Connected"; stateCol = CLR_SUCCESS;
    } else if (_flipState == FLIP_SCANNING) {
        stateStr = "Scanning..."; stateCol = CLR_WARNING;
    } else {
        stateStr = "Disconnected"; stateCol = CLR_ERROR;
    }
    M5.Display.setTextColor(stateCol, CLR_BG);
    M5.Display.setCursor(4, BODY_Y + 16);
    M5.Display.print(stateStr);

    int y = BODY_Y + 32;
    int maxY = H - HINT_H - 2;
    int startIdx = (_flipLogCount > 8) ? _flipLogCount - 8 : 0;
    for (int i = startIdx; i < _flipLogCount && y < maxY; i++) {
        uint16_t col = _flipLog[i].isCommand ? CLR_ACCENT : CLR_SUCCESS;
        const char* pfx = _flipLog[i].isCommand ? "> " : "< ";
        M5.Display.setTextColor(col, CLR_BG);
        M5.Display.setCursor(4, y);
        M5.Display.print(pfx);
        char truncBuf[24];
        strncpy(truncBuf, _flipLog[i].text, 20);
        truncBuf[20] = '\0';
        M5.Display.print(truncBuf);
        y += 12;
    }

    drawHintBar("[B] Scan  [A] Next");
}

/* ══════════════════════════════════════════════════════════
 *  SETTINGS SCREEN
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawSettingsScreen() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Settings");

    static const char* items[] = {
        "WiFi Setup", "Gateway", "Supabase", "Flipper", "Power Off"
    };
    static const int itemCount = 5;

    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    int y = BODY_Y + 4;
    for (int i = 0; i < itemCount; i++) {
        bool sel = (i == _settingsIdx);
        uint16_t bg = sel ? CLR_ACCENT : CLR_CARD;
        uint16_t fg = sel ? CLR_BG     : CLR_TEXT;
        M5.Display.fillRoundRect(4, y, W - 8, 28, 6, bg);
        M5.Display.setTextColor(fg, bg);
        int tw2 = strlen(items[i]) * 6;
        M5.Display.setCursor((W - tw2) / 2, y + 9);
        M5.Display.print(items[i]);
        y += 32;
    }

    drawHintBar("[B] Select [A] Next");
}

/* ══════════════════════════════════════════════════════════
 *  SETUP SCREENS
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawWifiSetup() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("WiFi Setup");
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    const char* lines[] = { "Connect to:", "Huonyx-Setup", "", "Then visit:", "192.168.4.1" };
    int y = 60;
    for (int i = 0; i < 5; i++) {
        int tw2 = M5.Display.textWidth(lines[i]);
        M5.Display.setCursor((W - tw2) / 2, y);
        M5.Display.print(lines[i]);
        y += 22;
    }
    drawHintBar("[A] Back");
}

void UIManager::drawGatewaySetup() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Gateway Setup");
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    const char* lines[] = { "Use web portal", "at 192.168.4.1", "to configure" };
    int y = 70;
    for (int i = 0; i < 3; i++) {
        int tw2 = M5.Display.textWidth(lines[i]);
        M5.Display.setCursor((W - tw2) / 2, y);
        M5.Display.print(lines[i]);
        y += 22;
    }
    const char* st = (_gwState == GW_AUTHENTICATED) ? "Connected" :
                     (_gwState == GW_CONNECTING)    ? "Connecting..." : "Not connected";
    uint16_t stCol = (_gwState == GW_AUTHENTICATED) ? CLR_SUCCESS :
                     (_gwState == GW_CONNECTING)    ? CLR_WARNING : CLR_ERROR;
    M5.Display.setTextColor(stCol, CLR_BG);
    M5.Display.setFont(&fonts::Font0);
    int tw2 = strlen(st) * 6;
    M5.Display.setCursor((W - tw2) / 2, 160);
    M5.Display.print(st);
    drawHintBar("[A] Back");
}

void UIManager::drawSupabaseSetup() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Supabase Setup");
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    const char* lines[] = { "Use web portal", "at 192.168.4.1", "to configure" };
    int y = 70;
    for (int i = 0; i < 3; i++) {
        int tw2 = M5.Display.textWidth(lines[i]);
        M5.Display.setCursor((W - tw2) / 2, y);
        M5.Display.print(lines[i]);
        y += 22;
    }
    const char* st = (_brState == BRIDGE_JOINED)    ? "Joined" :
                     (_brState == BRIDGE_CONNECTING) ? "Connecting..." : "Not connected";
    uint16_t stCol = (_brState == BRIDGE_JOINED)    ? CLR_SUCCESS :
                     (_brState == BRIDGE_CONNECTING) ? CLR_WARNING : CLR_ERROR;
    M5.Display.setTextColor(stCol, CLR_BG);
    M5.Display.setFont(&fonts::Font0);
    int tw2 = strlen(st) * 6;
    M5.Display.setCursor((W - tw2) / 2, 160);
    M5.Display.print(st);
    drawHintBar("[A] Back");
}

void UIManager::drawFlipperSetup() {
    M5.Display.fillScreen(CLR_BG);
    drawHeader("Flipper Setup");
    M5.Display.setFont(&fonts::Font2);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_TEXT, CLR_BG);
    const char* lines[] = { "BLE disabled", "Use Supabase", "bridge instead" };
    int y = 70;
    for (int i = 0; i < 3; i++) {
        int tw2 = M5.Display.textWidth(lines[i]);
        M5.Display.setCursor((W - tw2) / 2, y);
        M5.Display.print(lines[i]);
        y += 22;
    }
    drawHintBar("[A] Back");
}

/* ══════════════════════════════════════════════════════════
 *  NOTIFICATION OVERLAY
 * ══════════════════════════════════════════════════════════ */
void UIManager::drawNotification() {
    int ny = 26, nh = 50;
    M5.Display.fillRoundRect(4, ny, W - 8, nh, 8, CLR_CARD);
    M5.Display.drawRoundRect(4, ny, W - 8, nh, 8, CLR_ACCENT);
    M5.Display.setFont(&fonts::Font0);
    M5.Display.setTextSize(1);
    M5.Display.setTextColor(CLR_ACCENT, CLR_CARD);
    M5.Display.setCursor(10, ny + 6);
    M5.Display.print(_notifTitle);
    M5.Display.setTextColor(CLR_TEXT, CLR_CARD);
    M5.Display.setCursor(10, ny + 22);
    char truncBody[32];
    strncpy(truncBody, _notifBody, 30);
    truncBody[30] = '\0';
    M5.Display.print(truncBody);
}

/* ══════════════════════════════════════════════════════════
 *  NAVIGATION
 * ══════════════════════════════════════════════════════════ */
void UIManager::showScreen(ScreenId id) {
    _currentScreen = id;
    _dirty = true;
    _quickReplyVisible = false;
}

void UIManager::cycleNextScreen() {
    static const ScreenId mainScreens[] = {
        SCREEN_WATCHFACE, SCREEN_CHAT, SCREEN_FLIPPER, SCREEN_SETTINGS
    };
    static const int mainCount = 4;
    int cur = 0;
    for (int i = 0; i < mainCount; i++) {
        if (mainScreens[i] == _currentScreen) { cur = i; break; }
    }
    cur = (cur + 1) % mainCount;
    showScreen(mainScreens[cur]);
}

/* ══════════════════════════════════════════════════════════
 *  BUTTON HANDLING
 * ══════════════════════════════════════════════════════════ */
void UIManager::handleButton(BtnEvent evt) {
    if (evt == BTN_NONE) return;
    resetSleepTimer();
    if (_isSleeping) { wakeDisplay(); return; }
    if (_notifDismissTime > 0) { _notifDismissTime = 0; _dirty = true; return; }

    switch (_currentScreen) {
        case SCREEN_WATCHFACE:
            if (evt == BTN_A_SHORT) cycleNextScreen();
            else if (evt == BTN_B_SHORT || evt == BTN_B_LONG) showScreen(SCREEN_CHAT);
            break;
        case SCREEN_CHAT:     handleChatButton(evt);     break;
        case SCREEN_FLIPPER:  handleFlipperButton(evt);  break;
        case SCREEN_SETTINGS: handleSettingsButton(evt); break;
        case SCREEN_WIFI_SETUP:
        case SCREEN_GATEWAY_SETUP:
        case SCREEN_SUPABASE_SETUP:
        case SCREEN_FLIPPER_SETUP:
            if (evt == BTN_A_SHORT || evt == BTN_A_LONG) showScreen(SCREEN_SETTINGS);
            break;
        default:
            if (evt == BTN_A_SHORT) cycleNextScreen();
            break;
    }
}

void UIManager::handleChatButton(BtnEvent evt) {
    if (_quickReplyVisible) {
        if (evt == BTN_B_SHORT) {
            _quickReplyIdx = (_quickReplyIdx + 1) % QUICK_REPLY_COUNT;
            _dirty = true;
        } else if (evt == BTN_B_LONG) {
            sendSelectedQuickReply();
            _quickReplyVisible = false;
            _dirty = true;
        } else if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
            _quickReplyVisible = false;
            _dirty = true;
        }
        return;
    }
    if (evt == BTN_A_SHORT) cycleNextScreen();
    else if (evt == BTN_B_SHORT) {
        if (_chatScrollPos < _chatCount - 1) { _chatScrollPos++; _dirty = true; }
    } else if (evt == BTN_B_LONG) {
        _quickReplyVisible = true;
        _quickReplyIdx = 0;
        _dirty = true;
    }
}

void UIManager::handleFlipperButton(BtnEvent evt) {
    if (evt == BTN_A_SHORT) cycleNextScreen();
    else if (evt == BTN_B_SHORT || evt == BTN_B_LONG) {
        if (_bridge) _bridge->sendCommand("flipper", "scan", "");
    }
}

void UIManager::handleSettingsButton(BtnEvent evt) {
    if (evt == BTN_A_SHORT) cycleNextScreen();
    else if (evt == BTN_B_SHORT) { _settingsIdx = (_settingsIdx + 1) % 5; _dirty = true; }
    else if (evt == BTN_B_LONG) {
        switch (_settingsIdx) {
            case 0: showScreen(SCREEN_WIFI_SETUP);     break;
            case 1: showScreen(SCREEN_GATEWAY_SETUP);  break;
            case 2: showScreen(SCREEN_SUPABASE_SETUP); break;
            case 3: showScreen(SCREEN_FLIPPER_SETUP);  break;
            case 4: power_off();                       break;
        }
    }
}

void UIManager::sendSelectedQuickReply() {
    if (_gw && _quickReplyIdx >= 0 && _quickReplyIdx < QUICK_REPLY_COUNT) {
        _gw->sendChat(QUICK_REPLIES[_quickReplyIdx]);
        addChatMessage(QUICK_REPLIES[_quickReplyIdx], true);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SLEEP / WAKE
 * ══════════════════════════════════════════════════════════ */
void UIManager::resetSleepTimer() { _lastInteraction = millis(); }

void UIManager::sleepDisplay() {
    _isSleeping = true;
    M5.Display.setBrightness(0);
    M5.Display.sleep();
}

void UIManager::wakeDisplay() {
    _isSleeping = false;
    _lastInteraction = millis();
    M5.Display.wakeup();
    M5.Display.setBrightness(_targetBacklight);
    _dirty = true;
}

/* ══════════════════════════════════════════════════════════
 *  STATE UPDATE METHODS
 * ══════════════════════════════════════════════════════════ */
void UIManager::updateTime(int hour, int min, int sec) {
    if (_hour != hour || _min != min || _sec != sec) {
        _hour = hour; _min = min; _sec = sec;
        if (_currentScreen == SCREEN_WATCHFACE) _dirty = true;
    }
}

void UIManager::updateDate(const char* dateStr) {
    if (strcmp(_dateStr, dateStr) != 0) {
        strncpy(_dateStr, dateStr, sizeof(_dateStr) - 1);
        if (_currentScreen == SCREEN_WATCHFACE) _dirty = true;
    }
}

void UIManager::updateBattery(int percent, bool charging) {
    if (_battPct != percent || _charging != charging) {
        _battPct = percent; _charging = charging;
        if (_currentScreen == SCREEN_WATCHFACE) _dirty = true;
    }
}

void UIManager::updateWiFiStrength(int rssi) {
    _wifiRssi = rssi;
    if (_currentScreen == SCREEN_WATCHFACE) _dirty = true;
}

void UIManager::updateGatewayStatus(GatewayState state) {
    _gwState = state; _dirty = true;
}

void UIManager::updateFlipperStatus(FlipperState state) {
    _flipState = state; _dirty = true;
}

void UIManager::updateBridgeStatus(BridgeState state) {
    _brState = state; _dirty = true;
}

/* ══════════════════════════════════════════════════════════
 *  CHAT UPDATES
 * ══════════════════════════════════════════════════════════ */
void UIManager::addChatMessage(const char* text, bool isUser) {
    if (_chatCount >= CHAT_MAX_MESSAGES) {
        for (int i = 0; i < CHAT_MAX_MESSAGES - 1; i++)
            _chatMsgs[i] = _chatMsgs[i + 1];
        _chatCount = CHAT_MAX_MESSAGES - 1;
    }
    strncpy(_chatMsgs[_chatCount].text, text, CHAT_MAX_MSG_LEN - 1);
    _chatMsgs[_chatCount].text[CHAT_MAX_MSG_LEN - 1] = '\0';
    _chatMsgs[_chatCount].isUser = isUser;
    _chatCount++;
    _chatScrollPos = (_chatCount > 3) ? _chatCount - 3 : 0;
    if (!isUser) buzzer_notify();
    if (_currentScreen == SCREEN_CHAT) _dirty = true;
}

void UIManager::updateAgentTyping(bool typing) {
    if (_agentTyping != typing) {
        _agentTyping = typing;
        if (_currentScreen == SCREEN_CHAT) _dirty = true;
    }
}

void UIManager::clearChat() {
    _chatCount = 0; _chatScrollPos = 0; _dirty = true;
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER UPDATES
 * ══════════════════════════════════════════════════════════ */
void UIManager::addFlipperLog(const char* text, bool isCommand) {
    if (_flipLogCount >= MAX_FLIPPER_LOG) {
        for (int i = 0; i < MAX_FLIPPER_LOG - 1; i++)
            _flipLog[i] = _flipLog[i + 1];
        _flipLogCount = MAX_FLIPPER_LOG - 1;
    }
    strncpy(_flipLog[_flipLogCount].text, text, 127);
    _flipLog[_flipLogCount].text[127] = '\0';
    _flipLog[_flipLogCount].isCommand = isCommand;
    _flipLogCount++;
    if (_currentScreen == SCREEN_FLIPPER) _dirty = true;
}

void UIManager::updateFlipperInfo(const char* deviceName, int rssi) {
    snprintf(_flipDevName, sizeof(_flipDevName), "%s (%ddBm)", deviceName, rssi);
    _flipRssi = rssi;
    if (_currentScreen == SCREEN_FLIPPER) _dirty = true;
}

void UIManager::updateSettingsValues() { /* configured via web portal */ }

void UIManager::showNotification(const char* title, const char* msg) {
    strncpy(_notifTitle, title, sizeof(_notifTitle) - 1);
    strncpy(_notifBody, msg, sizeof(_notifBody) - 1);
    _notifDismissTime = millis() + NOTIF_DURATION_MS;
    _dirty = true;
    buzzer_notify();
}
