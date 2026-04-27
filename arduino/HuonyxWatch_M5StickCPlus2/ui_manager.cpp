/**
 * ui_manager.cpp
 * UI Manager for M5StickC Plus2
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * All screens redesigned for 135x240 portrait display.
 * Navigation is entirely button-driven.
 */
#include "build_config.h"
#include "ui_manager.h"
#include <lvgl.h>

/* ── Color palette ────────────────────────────────────── */
#define CLR_BG          lv_color_hex(0x0A0A0F)   /* Near-black background */
#define CLR_CARD        lv_color_hex(0x141420)   /* Card background */
#define CLR_ACCENT      lv_color_hex(0x00D4FF)   /* Huonyx cyan */
#define CLR_ACCENT2     lv_color_hex(0x7B2FFF)   /* Huonyx purple */
#define CLR_USER_BUBBLE lv_color_hex(0x1A3A5C)   /* User message bubble */
#define CLR_AGENT_BUBBLE lv_color_hex(0x1A1A2E)  /* Agent message bubble */
#define CLR_TEXT        lv_color_hex(0xE8E8F0)   /* Primary text */
#define CLR_SUBTEXT     lv_color_hex(0x7A7A9A)   /* Secondary text */
#define CLR_SUCCESS     lv_color_hex(0x00C896)   /* Green status */
#define CLR_ERROR       lv_color_hex(0xFF4444)   /* Red status */
#define CLR_WARNING     lv_color_hex(0xFFAA00)   /* Yellow/orange warning */
#define CLR_HEADER      lv_color_hex(0x0D0D1A)   /* Header bar */

/* ── Font shortcuts ───────────────────────────────────── */
#define FONT_TINY       &lv_font_montserrat_10
#define FONT_SMALL      &lv_font_montserrat_12
#define FONT_NORMAL     &lv_font_montserrat_14
#define FONT_LARGE      &lv_font_montserrat_20
#define FONT_TIME       &lv_font_montserrat_36

/* ── Sleep timeout ────────────────────────────────────── */
#define SLEEP_TIMEOUT_MS   30000  /* 30 seconds */
#define NOTIF_DURATION_MS   4000  /* 4 seconds */

/* ── Static instance for callbacks ───────────────────── */
static UIManager* _uiInstance = nullptr;

/* ══════════════════════════════════════════════════════════
 *  INITIALIZATION
 * ══════════════════════════════════════════════════════════ */
void UIManager::begin(GatewayClient* gw, ConfigManager* cfg,
                      FlipperBLE* flipper, SupabaseBridge* bridge) {
    _gw      = gw;
    _cfg     = cfg;
    _flipper = flipper;
    _bridge  = bridge;
    _uiInstance = this;

    _currentScreen    = SCREEN_WATCHFACE;
    _isSleeping       = false;
    _lastInteraction  = millis();
    _targetBacklight  = 180;
    _chatMsgCount     = 0;
    _flipperLogCount  = 0;
    _quickReplyIdx    = 0;
    _settingsIdx      = 0;
    _quickReplyVisible = false;

    createStyles();
    buildWatchface();
    buildChatScreen();
    buildFlipperScreen();
    buildSettingsScreen();
    buildWifiSetup();
    buildGatewaySetup();
    buildSupabaseSetup();
    buildFlipperSetup();
    buildNotificationOverlay();
    buildQuickReplyOverlay();

    lv_screen_load(_scrWatchface);
}

/* ══════════════════════════════════════════════════════════
 *  STYLES
 * ══════════════════════════════════════════════════════════ */
void UIManager::createStyles() {
    /* Background */
    lv_style_init(&_styleBg);
    lv_style_set_bg_color(&_styleBg, CLR_BG);
    lv_style_set_bg_opa(&_styleBg, LV_OPA_COVER);
    lv_style_set_border_width(&_styleBg, 0);
    lv_style_set_pad_all(&_styleBg, 0);
    lv_style_set_radius(&_styleBg, 0);

    /* Card */
    lv_style_init(&_styleCard);
    lv_style_set_bg_color(&_styleCard, CLR_CARD);
    lv_style_set_bg_opa(&_styleCard, LV_OPA_COVER);
    lv_style_set_border_width(&_styleCard, 0);
    lv_style_set_radius(&_styleCard, 6);
    lv_style_set_pad_all(&_styleCard, 6);

    /* User chat bubble */
    lv_style_init(&_styleBubbleUser);
    lv_style_set_bg_color(&_styleBubbleUser, CLR_USER_BUBBLE);
    lv_style_set_bg_opa(&_styleBubbleUser, LV_OPA_COVER);
    lv_style_set_border_color(&_styleBubbleUser, CLR_ACCENT);
    lv_style_set_border_width(&_styleBubbleUser, 1);
    lv_style_set_radius(&_styleBubbleUser, 8);
    lv_style_set_pad_all(&_styleBubbleUser, 5);
    lv_style_set_text_color(&_styleBubbleUser, CLR_TEXT);
    lv_style_set_text_font(&_styleBubbleUser, FONT_SMALL);

    /* Agent chat bubble */
    lv_style_init(&_styleBubbleAgent);
    lv_style_set_bg_color(&_styleBubbleAgent, CLR_AGENT_BUBBLE);
    lv_style_set_bg_opa(&_styleBubbleAgent, LV_OPA_COVER);
    lv_style_set_border_color(&_styleBubbleAgent, CLR_ACCENT2);
    lv_style_set_border_width(&_styleBubbleAgent, 1);
    lv_style_set_radius(&_styleBubbleAgent, 8);
    lv_style_set_pad_all(&_styleBubbleAgent, 5);
    lv_style_set_text_color(&_styleBubbleAgent, CLR_TEXT);
    lv_style_set_text_font(&_styleBubbleAgent, FONT_SMALL);

    /* Button */
    lv_style_init(&_styleBtn);
    lv_style_set_bg_color(&_styleBtn, CLR_CARD);
    lv_style_set_bg_opa(&_styleBtn, LV_OPA_COVER);
    lv_style_set_border_color(&_styleBtn, CLR_ACCENT);
    lv_style_set_border_width(&_styleBtn, 1);
    lv_style_set_radius(&_styleBtn, 6);
    lv_style_set_text_color(&_styleBtn, CLR_ACCENT);
    lv_style_set_text_font(&_styleBtn, FONT_SMALL);
    lv_style_set_pad_ver(&_styleBtn, 4);
    lv_style_set_pad_hor(&_styleBtn, 6);

    /* Selected button */
    lv_style_init(&_styleBtnSelected);
    lv_style_set_bg_color(&_styleBtnSelected, CLR_ACCENT);
    lv_style_set_bg_opa(&_styleBtnSelected, LV_OPA_COVER);
    lv_style_set_border_width(&_styleBtnSelected, 0);
    lv_style_set_radius(&_styleBtnSelected, 6);
    lv_style_set_text_color(&_styleBtnSelected, CLR_BG);
    lv_style_set_text_font(&_styleBtnSelected, FONT_SMALL);
    lv_style_set_pad_ver(&_styleBtnSelected, 4);
    lv_style_set_pad_hor(&_styleBtnSelected, 6);

    /* Header */
    lv_style_init(&_styleHeader);
    lv_style_set_bg_color(&_styleHeader, CLR_HEADER);
    lv_style_set_bg_opa(&_styleHeader, LV_OPA_COVER);
    lv_style_set_border_width(&_styleHeader, 0);
    lv_style_set_radius(&_styleHeader, 0);
    lv_style_set_pad_all(&_styleHeader, 4);

    /* Small text */
    lv_style_init(&_styleSmallText);
    lv_style_set_text_color(&_styleSmallText, CLR_SUBTEXT);
    lv_style_set_text_font(&_styleSmallText, FONT_TINY);

    /* Command log */
    lv_style_init(&_styleCmdLog);
    lv_style_set_text_color(&_styleCmdLog, CLR_ACCENT);
    lv_style_set_text_font(&_styleCmdLog, FONT_TINY);
    lv_style_set_bg_color(&_styleCmdLog, CLR_CARD);
    lv_style_set_bg_opa(&_styleCmdLog, LV_OPA_COVER);
    lv_style_set_pad_all(&_styleCmdLog, 3);
    lv_style_set_radius(&_styleCmdLog, 3);

    /* Result log */
    lv_style_init(&_styleResultLog);
    lv_style_set_text_color(&_styleResultLog, CLR_SUCCESS);
    lv_style_set_text_font(&_styleResultLog, FONT_TINY);
    lv_style_set_bg_color(&_styleResultLog, CLR_CARD);
    lv_style_set_bg_opa(&_styleResultLog, LV_OPA_COVER);
    lv_style_set_pad_all(&_styleResultLog, 3);
    lv_style_set_radius(&_styleResultLog, 3);
}

/* ══════════════════════════════════════════════════════════
 *  WATCHFACE SCREEN (135x240)
 *
 *  Layout (portrait):
 *  ┌─────────────────┐  y=0
 *  │  ●  ●  ●  [BAT] │  y=0..18  (status bar)
 *  ├─────────────────┤  y=18
 *  │                 │
 *  │    HH:MM:SS     │  y=50..110 (time, large)
 *  │   Mon, Apr 26   │  y=112..130 (date)
 *  │                 │
 *  ├─────────────────┤  y=200
 *  │  [A]=next [B]=? │  y=220..240 (hint)
 *  └─────────────────┘  y=240
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildWatchface() {
    _scrWatchface = lv_obj_create(NULL);
    lv_obj_add_style(_scrWatchface, &_styleBg, 0);

    /* Status bar */
    lv_obj_t* statusBar = lv_obj_create(_scrWatchface);
    lv_obj_add_style(statusBar, &_styleHeader, 0);
    lv_obj_set_size(statusBar, TFT_WIDTH, 20);
    lv_obj_align(statusBar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(statusBar, LV_OBJ_FLAG_SCROLLABLE);

    /* Gateway dot */
    _lblGwDot = lv_label_create(statusBar);
    lv_label_set_text(_lblGwDot, LV_SYMBOL_BULLET);
    lv_obj_set_style_text_color(_lblGwDot, CLR_ERROR, 0);
    lv_obj_set_style_text_font(_lblGwDot, FONT_TINY, 0);
    lv_obj_align(_lblGwDot, LV_ALIGN_LEFT_MID, 2, 0);

    /* Flipper dot */
    _lblFlipDot = lv_label_create(statusBar);
    lv_label_set_text(_lblFlipDot, LV_SYMBOL_BULLET);
    lv_obj_set_style_text_color(_lblFlipDot, CLR_ERROR, 0);
    lv_obj_set_style_text_font(_lblFlipDot, FONT_TINY, 0);
    lv_obj_align(_lblFlipDot, LV_ALIGN_LEFT_MID, 16, 0);

    /* Bridge dot */
    _lblBridgeDot = lv_label_create(statusBar);
    lv_label_set_text(_lblBridgeDot, LV_SYMBOL_BULLET);
    lv_obj_set_style_text_color(_lblBridgeDot, CLR_ERROR, 0);
    lv_obj_set_style_text_font(_lblBridgeDot, FONT_TINY, 0);
    lv_obj_align(_lblBridgeDot, LV_ALIGN_LEFT_MID, 30, 0);

    /* WiFi icon */
    _lblWifiIcon = lv_label_create(statusBar);
    lv_label_set_text(_lblWifiIcon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(_lblWifiIcon, CLR_SUBTEXT, 0);
    lv_obj_set_style_text_font(_lblWifiIcon, FONT_TINY, 0);
    lv_obj_align(_lblWifiIcon, LV_ALIGN_RIGHT_MID, -28, 0);

    /* Battery */
    _lblBatteryIcon = lv_label_create(statusBar);
    lv_label_set_text(_lblBatteryIcon, LV_SYMBOL_BATTERY_3);
    lv_obj_set_style_text_color(_lblBatteryIcon, CLR_SUCCESS, 0);
    lv_obj_set_style_text_font(_lblBatteryIcon, FONT_TINY, 0);
    lv_obj_align(_lblBatteryIcon, LV_ALIGN_RIGHT_MID, -14, 0);

    _lblBatteryPct = lv_label_create(statusBar);
    lv_label_set_text(_lblBatteryPct, "?%");
    lv_obj_set_style_text_color(_lblBatteryPct, CLR_SUBTEXT, 0);
    lv_obj_set_style_text_font(_lblBatteryPct, FONT_TINY, 0);
    lv_obj_align(_lblBatteryPct, LV_ALIGN_RIGHT_MID, 0, 0);

    /* Time label — large, centered */
    _lblTime = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblTime, "--:--");
    lv_obj_set_style_text_font(_lblTime, FONT_TIME, 0);
    lv_obj_set_style_text_color(_lblTime, CLR_ACCENT, 0);
    lv_obj_align(_lblTime, LV_ALIGN_CENTER, 0, -30);

    /* Day label */
    _lblDay = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblDay, "---");
    lv_obj_set_style_text_font(_lblDay, FONT_NORMAL, 0);
    lv_obj_set_style_text_color(_lblDay, CLR_TEXT, 0);
    lv_obj_align(_lblDay, LV_ALIGN_CENTER, 0, 20);

    /* Date label */
    _lblDate = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblDate, "-- --- ----");
    lv_obj_set_style_text_font(_lblDate, FONT_SMALL, 0);
    lv_obj_set_style_text_color(_lblDate, CLR_SUBTEXT, 0);
    lv_obj_align(_lblDate, LV_ALIGN_CENTER, 0, 42);

    /* Divider line */
    lv_obj_t* divider = lv_obj_create(_scrWatchface);
    lv_obj_set_size(divider, TFT_WIDTH - 20, 1);
    lv_obj_set_style_bg_color(divider, CLR_ACCENT2, 0);
    lv_obj_set_style_bg_opa(divider, LV_OPA_40, 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_align(divider, LV_ALIGN_CENTER, 0, 65);

    /* Hint label at bottom */
    _lblScreenHint = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblScreenHint, "[A] Chat  [B] Flipper");
    lv_obj_add_style(_lblScreenHint, &_styleSmallText, 0);
    lv_obj_align(_lblScreenHint, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/* ══════════════════════════════════════════════════════════
 *  CHAT SCREEN (135x240)
 *
 *  Layout:
 *  ┌─────────────────┐  y=0
 *  │ Huonyx  [typing]│  y=0..20 (header)
 *  ├─────────────────┤  y=20
 *  │ [Agent bubble]  │
 *  │  [User bubble]  │
 *  │ [Agent bubble]  │  y=20..210 (scrollable chat list)
 *  ├─────────────────┤  y=210
 *  │ [B]=Reply [A]=← │  y=210..240 (hint bar)
 *  └─────────────────┘  y=240
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildChatScreen() {
    _scrChat = lv_obj_create(NULL);
    lv_obj_add_style(_scrChat, &_styleBg, 0);

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrChat);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, TFT_WIDTH, 22);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    _lblChatTitle = lv_label_create(header);
    lv_label_set_text(_lblChatTitle, LV_SYMBOL_BELL "  Huonyx");
    lv_obj_set_style_text_color(_lblChatTitle, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(_lblChatTitle, FONT_SMALL, 0);
    lv_obj_align(_lblChatTitle, LV_ALIGN_LEFT_MID, 2, 0);

    _lblTyping = lv_label_create(header);
    lv_label_set_text(_lblTyping, "");
    lv_obj_set_style_text_color(_lblTyping, CLR_ACCENT2, 0);
    lv_obj_set_style_text_font(_lblTyping, FONT_TINY, 0);
    lv_obj_align(_lblTyping, LV_ALIGN_RIGHT_MID, -2, 0);

    /* Chat list (scrollable) */
    _chatList = lv_list_create(_scrChat);
    lv_obj_set_size(_chatList, TFT_WIDTH, 196);
    lv_obj_align(_chatList, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_bg_color(_chatList, CLR_BG, 0);
    lv_obj_set_style_bg_opa(_chatList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_chatList, 0, 0);
    lv_obj_set_style_pad_all(_chatList, 4, 0);
    lv_obj_set_style_pad_row(_chatList, 4, 0);

    /* Hint bar */
    lv_obj_t* hintBar = lv_obj_create(_scrChat);
    lv_obj_add_style(hintBar, &_styleHeader, 0);
    lv_obj_set_size(hintBar, TFT_WIDTH, 22);
    lv_obj_align(hintBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_clear_flag(hintBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hintLbl = lv_label_create(hintBar);
    lv_label_set_text(hintLbl, "[B] Reply  [A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_CENTER, 0, 0);
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER SCREEN (135x240)
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildFlipperScreen() {
    _scrFlipper = lv_obj_create(NULL);
    lv_obj_add_style(_scrFlipper, &_styleBg, 0);

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrFlipper);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, TFT_WIDTH, 22);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* titleLbl = lv_label_create(header);
    lv_label_set_text(titleLbl, LV_SYMBOL_BLUETOOTH "  Flipper");
    lv_obj_set_style_text_color(titleLbl, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(titleLbl, FONT_SMALL, 0);
    lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, 2, 0);

    /* Device info */
    _lblFlipperDevice = lv_label_create(_scrFlipper);
    lv_label_set_text(_lblFlipperDevice, "Not connected");
    lv_obj_set_style_text_color(_lblFlipperDevice, CLR_SUBTEXT, 0);
    lv_obj_set_style_text_font(_lblFlipperDevice, FONT_TINY, 0);
    lv_obj_align(_lblFlipperDevice, LV_ALIGN_TOP_LEFT, 4, 26);

    _lblFlipperState = lv_label_create(_scrFlipper);
    lv_label_set_text(_lblFlipperState, "");
    lv_obj_set_style_text_color(_lblFlipperState, CLR_ACCENT2, 0);
    lv_obj_set_style_text_font(_lblFlipperState, FONT_TINY, 0);
    lv_obj_align(_lblFlipperState, LV_ALIGN_TOP_RIGHT, -4, 26);

    /* Log list */
    _flipperLogList = lv_list_create(_scrFlipper);
    lv_obj_set_size(_flipperLogList, TFT_WIDTH, 178);
    lv_obj_align(_flipperLogList, LV_ALIGN_TOP_LEFT, 0, 38);
    lv_obj_set_style_bg_color(_flipperLogList, CLR_BG, 0);
    lv_obj_set_style_bg_opa(_flipperLogList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_flipperLogList, 0, 0);
    lv_obj_set_style_pad_all(_flipperLogList, 3, 0);
    lv_obj_set_style_pad_row(_flipperLogList, 2, 0);

    /* Hint bar */
    lv_obj_t* hintBar = lv_obj_create(_scrFlipper);
    lv_obj_add_style(hintBar, &_styleHeader, 0);
    lv_obj_set_size(hintBar, TFT_WIDTH, 22);
    lv_obj_align(hintBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_clear_flag(hintBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hintLbl = lv_label_create(hintBar);
    lv_label_set_text(hintLbl, "[B] Scan  [A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_CENTER, 0, 0);
}

/* ══════════════════════════════════════════════════════════
 *  SETTINGS SCREEN (135x240)
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildSettingsScreen() {
    _scrSettings = lv_obj_create(NULL);
    lv_obj_add_style(_scrSettings, &_styleBg, 0);

    lv_obj_t* header = lv_obj_create(_scrSettings);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, TFT_WIDTH, 22);
    lv_obj_align(header, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* titleLbl = lv_label_create(header);
    lv_label_set_text(titleLbl, LV_SYMBOL_SETTINGS "  Settings");
    lv_obj_set_style_text_color(titleLbl, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(titleLbl, FONT_SMALL, 0);
    lv_obj_align(titleLbl, LV_ALIGN_LEFT_MID, 2, 0);

    _settingsList = lv_list_create(_scrSettings);
    lv_obj_set_size(_settingsList, TFT_WIDTH, 196);
    lv_obj_align(_settingsList, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_set_style_bg_color(_settingsList, CLR_BG, 0);
    lv_obj_set_style_bg_opa(_settingsList, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_settingsList, 0, 0);
    lv_obj_set_style_pad_all(_settingsList, 4, 0);
    lv_obj_set_style_pad_row(_settingsList, 3, 0);

    /* Settings items */
    const char* items[] = {
        LV_SYMBOL_WIFI "  WiFi Setup",
        LV_SYMBOL_UPLOAD "  Gateway",
        LV_SYMBOL_UPLOAD "  Supabase",
        LV_SYMBOL_BLUETOOTH "  Flipper",
        LV_SYMBOL_POWER "  Power Off"
    };
    for (int i = 0; i < 5; i++) {
        lv_obj_t* btn = lv_list_add_button(_settingsList, NULL, items[i]);
        lv_obj_set_style_bg_color(btn, (i == 0) ? CLR_ACCENT : CLR_CARD, 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(btn, (i == 0) ? CLR_BG : CLR_TEXT, 0);
        lv_obj_set_style_text_font(btn, FONT_SMALL, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_radius(btn, 6, 0);
    }

    lv_obj_t* hintBar = lv_obj_create(_scrSettings);
    lv_obj_add_style(hintBar, &_styleHeader, 0);
    lv_obj_set_size(hintBar, TFT_WIDTH, 22);
    lv_obj_align(hintBar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_clear_flag(hintBar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* hintLbl = lv_label_create(hintBar);
    lv_label_set_text(hintLbl, "[B] Select  [A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_CENTER, 0, 0);
}

/* ══════════════════════════════════════════════════════════
 *  SETUP SCREENS (WiFi, Gateway, Supabase, Flipper)
 *  These are minimal — they show a QR code / URL to use
 *  the web portal for configuration (same as original watch)
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildWifiSetup() {
    _scrWifiSetup = lv_obj_create(NULL);
    lv_obj_add_style(_scrWifiSetup, &_styleBg, 0);

    lv_obj_t* lbl = lv_label_create(_scrWifiSetup);
    lv_label_set_text(lbl, LV_SYMBOL_WIFI "\nWiFi Setup\n\nConnect to:\nHuonyx-Setup\n\nThen visit:\n192.168.4.1");
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t* hintLbl = lv_label_create(_scrWifiSetup);
    lv_label_set_text(hintLbl, "[A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void UIManager::buildGatewaySetup() {
    _scrGatewaySetup = lv_obj_create(NULL);
    lv_obj_add_style(_scrGatewaySetup, &_styleBg, 0);

    lv_obj_t* lbl = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(lbl, LV_SYMBOL_UPLOAD "\nGateway Setup\n\nUse web portal\nat 192.168.4.1\nto configure");
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

    _lblGwStatus = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(_lblGwStatus, "");
    lv_obj_set_style_text_color(_lblGwStatus, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(_lblGwStatus, FONT_TINY, 0);
    lv_obj_align(_lblGwStatus, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t* hintLbl = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(hintLbl, "[A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void UIManager::buildSupabaseSetup() {
    _scrSupabaseSetup = lv_obj_create(NULL);
    lv_obj_add_style(_scrSupabaseSetup, &_styleBg, 0);

    lv_obj_t* lbl = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(lbl, LV_SYMBOL_UPLOAD "\nSupabase Setup\n\nUse web portal\nat 192.168.4.1\nto configure");
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

    _lblSbStatus = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(_lblSbStatus, "");
    lv_obj_set_style_text_color(_lblSbStatus, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(_lblSbStatus, FONT_TINY, 0);
    lv_obj_align(_lblSbStatus, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t* hintLbl = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(hintLbl, "[A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

void UIManager::buildFlipperSetup() {
    _scrFlipperSetup = lv_obj_create(NULL);
    lv_obj_add_style(_scrFlipperSetup, &_styleBg, 0);

    lv_obj_t* lbl = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(lbl, LV_SYMBOL_BLUETOOTH "\nFlipper Setup\n\nEnable BLE in\nFlipper settings\nthen press [B]\nto scan");
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -10);

    _lblFlipSetupStatus = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(_lblFlipSetupStatus, "");
    lv_obj_set_style_text_color(_lblFlipSetupStatus, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(_lblFlipSetupStatus, FONT_TINY, 0);
    lv_obj_align(_lblFlipSetupStatus, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_t* hintLbl = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(hintLbl, "[B] Scan  [A] Back");
    lv_obj_add_style(hintLbl, &_styleSmallText, 0);
    lv_obj_align(hintLbl, LV_ALIGN_BOTTOM_MID, 0, -6);
}

/* ══════════════════════════════════════════════════════════
 *  NOTIFICATION OVERLAY
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildNotificationOverlay() {
    _notifOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_notifOverlay, TFT_WIDTH - 10, 52);
    lv_obj_align(_notifOverlay, LV_ALIGN_TOP_MID, 0, -60);  /* Start off-screen */
    lv_obj_set_style_bg_color(_notifOverlay, CLR_CARD, 0);
    lv_obj_set_style_bg_opa(_notifOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_notifOverlay, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(_notifOverlay, 1, 0);
    lv_obj_set_style_radius(_notifOverlay, 8, 0);
    lv_obj_set_style_pad_all(_notifOverlay, 6, 0);
    lv_obj_clear_flag(_notifOverlay, LV_OBJ_FLAG_SCROLLABLE);

    _lblNotifTitle = lv_label_create(_notifOverlay);
    lv_label_set_text(_lblNotifTitle, "");
    lv_obj_set_style_text_color(_lblNotifTitle, CLR_ACCENT, 0);
    lv_obj_set_style_text_font(_lblNotifTitle, FONT_SMALL, 0);
    lv_obj_align(_lblNotifTitle, LV_ALIGN_TOP_LEFT, 0, 0);

    _lblNotifBody = lv_label_create(_notifOverlay);
    lv_label_set_text(_lblNotifBody, "");
    lv_obj_set_style_text_color(_lblNotifBody, CLR_TEXT, 0);
    lv_obj_set_style_text_font(_lblNotifBody, FONT_TINY, 0);
    lv_label_set_long_mode(_lblNotifBody, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(_lblNotifBody, TFT_WIDTH - 22);
    lv_obj_align(_lblNotifBody, LV_ALIGN_TOP_LEFT, 0, 16);
}

/* ══════════════════════════════════════════════════════════
 *  QUICK REPLY OVERLAY
 * ══════════════════════════════════════════════════════════ */
void UIManager::buildQuickReplyOverlay() {
    _quickReplyOverlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(_quickReplyOverlay, TFT_WIDTH, 110);
    lv_obj_align(_quickReplyOverlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(_quickReplyOverlay, CLR_HEADER, 0);
    lv_obj_set_style_bg_opa(_quickReplyOverlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_quickReplyOverlay, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(_quickReplyOverlay, 1, 0);
    lv_obj_set_style_border_side(_quickReplyOverlay, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_radius(_quickReplyOverlay, 0, 0);
    lv_obj_set_style_pad_all(_quickReplyOverlay, 6, 0);
    lv_obj_set_style_pad_row(_quickReplyOverlay, 4, 0);
    lv_obj_set_layout(_quickReplyOverlay, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(_quickReplyOverlay, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(_quickReplyOverlay, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
        _btnQuickReply[i] = lv_button_create(_quickReplyOverlay);
        lv_obj_set_size(_btnQuickReply[i], TFT_WIDTH - 12, 20);
        lv_obj_add_style(_btnQuickReply[i], (i == 0) ? &_styleBtnSelected : &_styleBtn, 0);
        lv_obj_t* lbl = lv_label_create(_btnQuickReply[i]);
        lv_label_set_text(lbl, QUICK_REPLIES[i]);
        lv_obj_center(lbl);
    }
}

/* ══════════════════════════════════════════════════════════
 *  BUTTON HANDLING
 * ══════════════════════════════════════════════════════════ */
void UIManager::handleButton(BtnEvent evt) {
    if (evt == BTN_NONE) return;

    /* Wake on any button press */
    if (_isSleeping) {
        wakeDisplay();
        return;
    }
    resetSleepTimer();

    /* Dispatch to screen-specific handler */
    switch (_currentScreen) {
        case SCREEN_WATCHFACE:
            if (evt == BTN_A_SHORT) {
                showScreen(SCREEN_CHAT);
            } else if (evt == BTN_B_SHORT) {
                showScreen(SCREEN_FLIPPER);
            } else if (evt == BTN_A_LONG) {
                showScreen(SCREEN_SETTINGS);
            }
            break;

        case SCREEN_CHAT:
            handleChatButton(evt);
            break;

        case SCREEN_FLIPPER:
            handleFlipperButton(evt);
            break;

        case SCREEN_SETTINGS:
            handleSettingsButton(evt);
            break;

        case SCREEN_WIFI_SETUP:
        case SCREEN_GATEWAY_SETUP:
        case SCREEN_SUPABASE_SETUP:
        case SCREEN_FLIPPER_SETUP:
            if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
                showScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (evt == BTN_B_SHORT || evt == BTN_B_LONG) {
                if (_currentScreen == SCREEN_FLIPPER_SETUP) {
                    _flipper->startScan();
                    lv_label_set_text(_lblFlipSetupStatus, "Scanning...");
                }
            }
            break;

        default:
            break;
    }
}

void UIManager::handleChatButton(BtnEvent evt) {
    if (_quickReplyVisible) {
        if (evt == BTN_B_SHORT) {
            /* Cycle through quick replies */
            _quickReplyIdx = (_quickReplyIdx + 1) % QUICK_REPLY_COUNT;
            for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
                lv_obj_remove_style(_btnQuickReply[i], &_styleBtnSelected, 0);
                lv_obj_remove_style(_btnQuickReply[i], &_styleBtn, 0);
                lv_obj_add_style(_btnQuickReply[i],
                    (i == _quickReplyIdx) ? &_styleBtnSelected : &_styleBtn, 0);
            }
        } else if (evt == BTN_B_LONG) {
            sendSelectedQuickReply();
            hideQuickReplies();
        } else if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
            hideQuickReplies();
        }
    } else {
        if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
            showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        } else if (evt == BTN_B_SHORT) {
            /* Scroll chat list */
            lv_obj_scroll_by(_chatList, 0, 30, LV_ANIM_ON);
        } else if (evt == BTN_B_LONG) {
            showQuickReplies();
        }
    }
}

void UIManager::handleFlipperButton(BtnEvent evt) {
    if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
        showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    } else if (evt == BTN_B_SHORT) {
        /* Scroll log */
        lv_obj_scroll_by(_flipperLogList, 0, 30, LV_ANIM_ON);
    } else if (evt == BTN_B_LONG) {
        /* Scan for Flipper */
        if (_flipper) {
            _flipper->startScan();
            addFlipperLog("Scanning for Flipper...", true);
        }
    }
}

void UIManager::handleSettingsButton(BtnEvent evt) {
    if (evt == BTN_A_SHORT || evt == BTN_A_LONG) {
        showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
        return;
    }

    uint32_t childCount = lv_obj_get_child_count(_settingsList);
    if (evt == BTN_B_SHORT) {
        /* Highlight next item */
        int prevIdx = _settingsIdx;
        _settingsIdx = (_settingsIdx + 1) % (int)childCount;
        lv_obj_t* prev = lv_obj_get_child(_settingsList, prevIdx);
        lv_obj_t* curr = lv_obj_get_child(_settingsList, _settingsIdx);
        if (prev) {
            lv_obj_set_style_bg_color(prev, CLR_CARD, 0);
            lv_obj_set_style_text_color(prev, CLR_TEXT, 0);
        }
        if (curr) {
            lv_obj_set_style_bg_color(curr, CLR_ACCENT, 0);
            lv_obj_set_style_text_color(curr, CLR_BG, 0);
        }
    } else if (evt == BTN_B_LONG) {
        /* Select item */
        switch (_settingsIdx) {
            case 0: showScreen(SCREEN_WIFI_SETUP);     break;
            case 1: showScreen(SCREEN_GATEWAY_SETUP);  break;
            case 2: showScreen(SCREEN_SUPABASE_SETUP); break;
            case 3: showScreen(SCREEN_FLIPPER_SETUP);  break;
            case 4: power_off();                        break;
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  QUICK REPLIES
 * ══════════════════════════════════════════════════════════ */
void UIManager::showQuickReplies() {
    _quickReplyVisible = true;
    _quickReplyIdx = 0;
    for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
        lv_obj_remove_style(_btnQuickReply[i], &_styleBtnSelected, 0);
        lv_obj_remove_style(_btnQuickReply[i], &_styleBtn, 0);
        lv_obj_add_style(_btnQuickReply[i],
            (i == 0) ? &_styleBtnSelected : &_styleBtn, 0);
    }
    lv_obj_clear_flag(_quickReplyOverlay, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::hideQuickReplies() {
    _quickReplyVisible = false;
    lv_obj_add_flag(_quickReplyOverlay, LV_OBJ_FLAG_HIDDEN);
}

void UIManager::sendSelectedQuickReply() {
    if (!_gw || !_gw->isConnected()) return;
    const char* reply = QUICK_REPLIES[_quickReplyIdx];
    _gw->sendMessage(_gw->getSessionKey(), reply);
    addChatBubble(reply, true);
}

/* ══════════════════════════════════════════════════════════
 *  SCREEN NAVIGATION
 * ══════════════════════════════════════════════════════════ */
void UIManager::showScreen(ScreenId id, lv_screen_load_anim_t anim) {
    lv_obj_t* scr = nullptr;
    switch (id) {
        case SCREEN_WATCHFACE:      scr = _scrWatchface;     break;
        case SCREEN_CHAT:           scr = _scrChat;          break;
        case SCREEN_FLIPPER:        scr = _scrFlipper;       break;
        case SCREEN_SETTINGS:       scr = _scrSettings;      break;
        case SCREEN_WIFI_SETUP:     scr = _scrWifiSetup;     break;
        case SCREEN_GATEWAY_SETUP:  scr = _scrGatewaySetup;  break;
        case SCREEN_SUPABASE_SETUP: scr = _scrSupabaseSetup; break;
        case SCREEN_FLIPPER_SETUP:  scr = _scrFlipperSetup;  break;
        default: return;
    }
    _currentScreen = id;
    lv_screen_load_anim(scr, anim, 200, 0, false);
}

/* ══════════════════════════════════════════════════════════
 *  SLEEP / WAKE
 * ══════════════════════════════════════════════════════════ */
void UIManager::resetSleepTimer() {
    _lastInteraction = millis();
    if (_isSleeping) wakeDisplay();
}

void UIManager::wakeDisplay() {
    _isSleeping = false;
    m5_set_backlight(_targetBacklight);
}

void UIManager::sleepDisplay() {
    _isSleeping = true;
    m5_set_backlight(0);
}

/* ══════════════════════════════════════════════════════════
 *  WATCHFACE UPDATES
 * ══════════════════════════════════════════════════════════ */
void UIManager::updateTime(int hour, int min, int sec) {
    char buf[9];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, min);
    lv_label_set_text(_lblTime, buf);
}

void UIManager::updateDate(const char* dateStr) {
    lv_label_set_text(_lblDate, dateStr);
}

void UIManager::updateBattery(int percent, bool charging) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(_lblBatteryPct, buf);

    const char* icon;
    lv_color_t color;
    if (charging) {
        icon = LV_SYMBOL_CHARGE;
        color = CLR_ACCENT;
    } else if (percent > 60) {
        icon = LV_SYMBOL_BATTERY_FULL;
        color = CLR_SUCCESS;
    } else if (percent > 30) {
        icon = LV_SYMBOL_BATTERY_3;
        color = CLR_WARNING;
    } else if (percent > 10) {
        icon = LV_SYMBOL_BATTERY_2;
        color = CLR_WARNING;
    } else {
        icon = LV_SYMBOL_BATTERY_1;
        color = CLR_ERROR;
    }
    lv_label_set_text(_lblBatteryIcon, icon);
    lv_obj_set_style_text_color(_lblBatteryIcon, color, 0);
}

void UIManager::updateWiFiStrength(int rssi) {
    lv_color_t color = (rssi == 0) ? CLR_SUBTEXT :
                       (rssi > -60) ? CLR_SUCCESS :
                       (rssi > -75) ? CLR_WARNING : CLR_ERROR;
    lv_obj_set_style_text_color(_lblWifiIcon, color, 0);
}

void UIManager::updateGatewayStatus(GatewayState state) {
    lv_color_t color = (state == GW_AUTHENTICATED) ? CLR_SUCCESS :
                       (state == GW_CONNECTING)    ? CLR_WARNING : CLR_ERROR;
    lv_obj_set_style_text_color(_lblGwDot, color, 0);
}

void UIManager::updateFlipperStatus(FlipperState state) {
    lv_color_t color = (state == FLIP_CONNECTED) ? CLR_SUCCESS :
                       (state == FLIP_SCANNING)  ? CLR_WARNING : CLR_ERROR;
    lv_obj_set_style_text_color(_lblFlipDot, color, 0);

    /* Update Flipper screen info */
    if (state == FLIP_CONNECTED && _flipper) {
        lv_label_set_text(_lblFlipperState, "Connected");
        lv_obj_set_style_text_color(_lblFlipperState, CLR_SUCCESS, 0);
    } else if (state == FLIP_SCANNING) {
        lv_label_set_text(_lblFlipperState, "Scanning...");
        lv_obj_set_style_text_color(_lblFlipperState, CLR_WARNING, 0);
    } else {
        lv_label_set_text(_lblFlipperState, "Disconnected");
        lv_obj_set_style_text_color(_lblFlipperState, CLR_ERROR, 0);
    }
}

void UIManager::updateBridgeStatus(BridgeState state) {
    lv_color_t color = (state == BRIDGE_JOINED)    ? CLR_SUCCESS :
                       (state == BRIDGE_CONNECTING) ? CLR_WARNING : CLR_ERROR;
    lv_obj_set_style_text_color(_lblBridgeDot, color, 0);
}

/* ══════════════════════════════════════════════════════════
 *  CHAT UPDATES
 * ══════════════════════════════════════════════════════════ */
void UIManager::addChatMessage(const char* text, bool isUser) {
    addChatBubble(text, isUser);
    if (!isUser) buzzer_notify();
}

void UIManager::addChatBubble(const char* text, bool isUser) {
    if (_chatMsgCount >= CHAT_MAX_MESSAGES) {
        /* Remove oldest message */
        lv_obj_t* first = lv_obj_get_child(_chatList, 0);
        if (first) lv_obj_delete(first);
    } else {
        _chatMsgCount++;
    }

    lv_obj_t* bubble = lv_obj_create(_chatList);
    lv_obj_set_width(bubble, TFT_WIDTH - 16);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_add_style(bubble, isUser ? &_styleBubbleUser : &_styleBubbleAgent, 0);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(bubble);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, TFT_WIDTH - 28);
    lv_obj_set_style_text_color(lbl, CLR_TEXT, 0);
    lv_obj_set_style_text_font(lbl, FONT_SMALL, 0);

    /* Scroll to bottom */
    lv_obj_scroll_to_view(bubble, LV_ANIM_ON);
}

void UIManager::updateAgentTyping(bool typing) {
    lv_label_set_text(_lblTyping, typing ? "typing..." : "");
}

void UIManager::clearChat() {
    lv_obj_clean(_chatList);
    _chatMsgCount = 0;
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER UPDATES
 * ══════════════════════════════════════════════════════════ */
void UIManager::addFlipperLog(const char* text, bool isCommand) {
    addFlipperLogEntry(text, isCommand);
}

void UIManager::addFlipperLogEntry(const char* text, bool isCommand) {
    if (_flipperLogCount >= 20) {
        lv_obj_t* first = lv_obj_get_child(_flipperLogList, 0);
        if (first) lv_obj_delete(first);
    } else {
        _flipperLogCount++;
    }

    lv_obj_t* entry = lv_obj_create(_flipperLogList);
    lv_obj_set_width(entry, TFT_WIDTH - 10);
    lv_obj_set_height(entry, LV_SIZE_CONTENT);
    lv_obj_add_style(entry, isCommand ? &_styleCmdLog : &_styleResultLog, 0);
    lv_obj_clear_flag(entry, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(entry);
    lv_label_set_text(lbl, text);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, TFT_WIDTH - 16);

    lv_obj_scroll_to_view(entry, LV_ANIM_ON);
}

void UIManager::updateFlipperInfo(const char* deviceName, int rssi) {
    char buf[48];
    snprintf(buf, sizeof(buf), "%s (%ddBm)", deviceName, rssi);
    lv_label_set_text(_lblFlipperDevice, buf);
}

/* ══════════════════════════════════════════════════════════
 *  SETTINGS UPDATE
 * ══════════════════════════════════════════════════════════ */
void UIManager::updateSettingsValues() {
    /* No dynamic values needed — settings are configured via web portal */
}

/* ══════════════════════════════════════════════════════════
 *  NOTIFICATIONS
 * ══════════════════════════════════════════════════════════ */
void UIManager::showNotification(const char* title, const char* msg) {
    lv_label_set_text(_lblNotifTitle, title);
    lv_label_set_text(_lblNotifBody, msg);

    /* Slide in from top */
    lv_obj_align(_notifOverlay, LV_ALIGN_TOP_MID, 0, 24);
    _notifDismissTime = millis() + NOTIF_DURATION_MS;

    buzzer_notify();
}
