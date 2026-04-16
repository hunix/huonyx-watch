#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\ui_manager.cpp"
/**
 * UI Manager Implementation - LVGL Smartwatch Interface
 * Huonyx AI Smartwatch v2.0
 *
 * Screens:
 *   - Watch Face (home) with time, date, battery arc, status LEDs
 *   - Chat Interface with message bubbles and quick replies
 *   - Flipper Control with command log and scan/disconnect buttons
 *   - Quick Settings (brightness, WiFi toggle)
 *   - Settings Menu (WiFi, Gateway, Supabase, Flipper, Sessions, About)
 *   - WiFi Setup
 *   - Gateway Setup
 *   - Supabase Setup
 *   - Flipper Setup
 *   - Sessions list
 */

#include "ui_manager.h"

/* ── Color Palette (AI-era dark theme) ────────────────── */
#define COL_BG          lv_color_hex(0x0D0D0F)   /* Deep black */
#define COL_SURFACE     lv_color_hex(0x1A1A2E)   /* Dark navy surface */
#define COL_CARD        lv_color_hex(0x16213E)   /* Card background */
#define COL_PRIMARY     lv_color_hex(0x00D4FF)   /* Cyan accent */
#define COL_SECONDARY   lv_color_hex(0x7B2FFF)   /* Purple accent */
#define COL_ACCENT      lv_color_hex(0x00FF88)   /* Green accent */
#define COL_WARNING     lv_color_hex(0xFF6B35)   /* Orange warning */
#define COL_ERROR       lv_color_hex(0xFF3366)   /* Red error */
#define COL_FLIPPER     lv_color_hex(0xFF8C00)   /* Flipper orange */
#define COL_TEXT        lv_color_hex(0xE8E8F0)   /* Light text */
#define COL_TEXT_DIM    lv_color_hex(0x6B6B8D)   /* Dimmed text */
#define COL_BUBBLE_USER lv_color_hex(0x0A84FF)   /* User bubble blue */
#define COL_BUBBLE_AI   lv_color_hex(0x2A2A3E)   /* AI bubble dark */
#define COL_BTN         lv_color_hex(0x1E1E3A)   /* Button background */
#define COL_BTN_PRESS   lv_color_hex(0x2E2E5A)   /* Button pressed */
#define COL_CMD_BG      lv_color_hex(0x1A0A2E)   /* Command log bg */
#define COL_RESULT_BG   lv_color_hex(0x0A1A1E)   /* Result log bg */

/* Quick reply texts */
static const char* quickReplies[QUICK_REPLY_COUNT] = {
    "Status?",
    "Yes",
    "No",
    "Summarize",
    "Continue",
    "Help"
};

/* ── Constructor ──────────────────────────────────────── */

UIManager::UIManager()
    : _gw(nullptr)
    , _cfg(nullptr)
    , _flipper(nullptr)
    , _bridge(nullptr)
    , _currentScreen(SCREEN_WATCHFACE)
    , _chatMsgCount(0)
    , _flipperLogCount(0)
{
}

/* ── Initialization ───────────────────────────────────── */

void UIManager::begin(GatewayClient* gw, ConfigManager* cfg,
                      FlipperBLE* flipper, SupabaseBridge* bridge) {
    _gw = gw;
    _cfg = cfg;
    _flipper = flipper;
    _bridge = bridge;

    createStyles();
    buildWatchface();
    buildChatScreen();
    buildFlipperScreen();
    buildQuickSettings();
    buildSettingsScreen();
    buildWifiSetup();
    buildGatewaySetup();
    buildSupabaseSetup();
    buildFlipperSetup();
    buildSessionsScreen();

    /* Start with watch face */
    lv_screen_load(_scrWatchface);
}

void UIManager::update() {
    /* Called from main loop - nothing special needed */
}

/* ── Styles ───────────────────────────────────────────── */

void UIManager::createStyles() {
    /* Background style */
    lv_style_init(&_styleBg);
    lv_style_set_bg_color(&_styleBg, COL_BG);
    lv_style_set_bg_opa(&_styleBg, LV_OPA_COVER);
    lv_style_set_border_width(&_styleBg, 0);
    lv_style_set_radius(&_styleBg, LV_RADIUS_CIRCLE);
    lv_style_set_pad_all(&_styleBg, 0);

    /* Card style */
    lv_style_init(&_styleCard);
    lv_style_set_bg_color(&_styleCard, COL_CARD);
    lv_style_set_bg_opa(&_styleCard, LV_OPA_COVER);
    lv_style_set_radius(&_styleCard, 12);
    lv_style_set_pad_all(&_styleCard, 8);
    lv_style_set_border_width(&_styleCard, 0);

    /* User bubble */
    lv_style_init(&_styleBubbleUser);
    lv_style_set_bg_color(&_styleBubbleUser, COL_BUBBLE_USER);
    lv_style_set_bg_opa(&_styleBubbleUser, LV_OPA_COVER);
    lv_style_set_radius(&_styleBubbleUser, 16);
    lv_style_set_pad_all(&_styleBubbleUser, 8);
    lv_style_set_pad_right(&_styleBubbleUser, 10);
    lv_style_set_text_color(&_styleBubbleUser, lv_color_white());
    lv_style_set_max_width(&_styleBubbleUser, 180);

    /* Agent bubble */
    lv_style_init(&_styleBubbleAgent);
    lv_style_set_bg_color(&_styleBubbleAgent, COL_BUBBLE_AI);
    lv_style_set_bg_opa(&_styleBubbleAgent, LV_OPA_COVER);
    lv_style_set_radius(&_styleBubbleAgent, 16);
    lv_style_set_pad_all(&_styleBubbleAgent, 8);
    lv_style_set_pad_left(&_styleBubbleAgent, 10);
    lv_style_set_text_color(&_styleBubbleAgent, COL_TEXT);
    lv_style_set_max_width(&_styleBubbleAgent, 180);

    /* Button style */
    lv_style_init(&_styleBtn);
    lv_style_set_bg_color(&_styleBtn, COL_BTN);
    lv_style_set_bg_opa(&_styleBtn, LV_OPA_COVER);
    lv_style_set_radius(&_styleBtn, 20);
    lv_style_set_text_color(&_styleBtn, COL_PRIMARY);
    lv_style_set_text_font(&_styleBtn, &lv_font_montserrat_12);
    lv_style_set_pad_ver(&_styleBtn, 6);
    lv_style_set_pad_hor(&_styleBtn, 12);
    lv_style_set_border_width(&_styleBtn, 1);
    lv_style_set_border_color(&_styleBtn, lv_color_hex(0x2A2A4E));
    lv_style_set_border_opa(&_styleBtn, LV_OPA_70);

    /* Button pressed */
    lv_style_init(&_styleBtnPressed);
    lv_style_set_bg_color(&_styleBtnPressed, COL_PRIMARY);
    lv_style_set_text_color(&_styleBtnPressed, COL_BG);

    /* Header style */
    lv_style_init(&_styleHeader);
    lv_style_set_bg_color(&_styleHeader, lv_color_hex(0x0D0D1A));
    lv_style_set_bg_opa(&_styleHeader, LV_OPA_90);
    lv_style_set_pad_all(&_styleHeader, 4);
    lv_style_set_border_width(&_styleHeader, 0);

    /* Accent style */
    lv_style_init(&_styleAccent);
    lv_style_set_text_color(&_styleAccent, COL_PRIMARY);

    /* Command log entry */
    lv_style_init(&_styleCmdLog);
    lv_style_set_bg_color(&_styleCmdLog, COL_CMD_BG);
    lv_style_set_bg_opa(&_styleCmdLog, LV_OPA_COVER);
    lv_style_set_radius(&_styleCmdLog, 8);
    lv_style_set_pad_all(&_styleCmdLog, 6);
    lv_style_set_text_color(&_styleCmdLog, COL_FLIPPER);
    lv_style_set_max_width(&_styleCmdLog, 200);

    /* Result log entry */
    lv_style_init(&_styleResultLog);
    lv_style_set_bg_color(&_styleResultLog, COL_RESULT_BG);
    lv_style_set_bg_opa(&_styleResultLog, LV_OPA_COVER);
    lv_style_set_radius(&_styleResultLog, 8);
    lv_style_set_pad_all(&_styleResultLog, 6);
    lv_style_set_text_color(&_styleResultLog, COL_ACCENT);
    lv_style_set_max_width(&_styleResultLog, 200);
}

/* ── Helper: Create a round screen ────────────────────── */

lv_obj_t* UIManager::createRoundScreen() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &_styleBg, 0);
    lv_obj_set_size(scr, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

/* ══════════════════════════════════════════════════════════
 *  WATCH FACE SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildWatchface() {
    _scrWatchface = createRoundScreen();

    /* ── Outer ring decoration ──────────────────────── */
    lv_obj_t* ringOuter = lv_arc_create(_scrWatchface);
    lv_arc_set_rotation(ringOuter, 0);
    lv_arc_set_range(ringOuter, 0, 360);
    lv_arc_set_value(ringOuter, 360);
    lv_arc_set_bg_angles(ringOuter, 0, 360);
    lv_obj_set_size(ringOuter, 236, 236);
    lv_obj_center(ringOuter);
    lv_obj_set_style_arc_color(ringOuter, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ringOuter, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ringOuter, COL_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ringOuter, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ringOuter, LV_OPA_40, LV_PART_INDICATOR);
    lv_obj_remove_flag(ringOuter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(ringOuter, LV_OPA_TRANSP, LV_PART_KNOB);

    /* ── Time display ───────────────────────────────── */
    _lblTime = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblTime, "12:00");
    lv_obj_set_style_text_font(_lblTime, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(_lblTime, COL_TEXT, 0);
    lv_obj_set_style_text_letter_space(_lblTime, 2, 0);
    lv_obj_align(_lblTime, LV_ALIGN_CENTER, 0, -20);

    /* ── AM/PM indicator ────────────────────────────── */
    _lblAmPm = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblAmPm, "");
    lv_obj_set_style_text_font(_lblAmPm, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblAmPm, COL_TEXT_DIM, 0);
    lv_obj_align_to(_lblAmPm, _lblTime, LV_ALIGN_OUT_RIGHT_TOP, 4, 4);

    /* ── Date display ───────────────────────────────── */
    _lblDate = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblDate, "Mon, Apr 14");
    lv_obj_set_style_text_font(_lblDate, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblDate, COL_TEXT_DIM, 0);
    lv_obj_align(_lblDate, LV_ALIGN_CENTER, 0, 16);

    /* ── Day label ──────────────────────────────────── */
    _lblDay = lv_label_create(_scrWatchface);
    lv_label_set_text(_lblDay, "");
    lv_obj_set_style_text_font(_lblDay, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblDay, COL_PRIMARY, 0);
    lv_obj_align(_lblDay, LV_ALIGN_CENTER, 0, 34);

    /* ── Battery arc (bottom-left) ──────────────────── */
    _arcBattery = lv_arc_create(_scrWatchface);
    lv_arc_set_rotation(_arcBattery, 135);
    lv_arc_set_range(_arcBattery, 0, 100);
    lv_arc_set_value(_arcBattery, 75);
    lv_arc_set_bg_angles(_arcBattery, 0, 90);
    lv_obj_set_size(_arcBattery, 50, 50);
    lv_obj_align(_arcBattery, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    lv_obj_set_style_arc_color(_arcBattery, lv_color_hex(0x1A1A2E), LV_PART_MAIN);
    lv_obj_set_style_arc_width(_arcBattery, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_arcBattery, COL_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_arcBattery, 4, LV_PART_INDICATOR);
    lv_obj_remove_flag(_arcBattery, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(_arcBattery, LV_OPA_TRANSP, LV_PART_KNOB);

    _lblBatteryPct = lv_label_create(_arcBattery);
    lv_label_set_text(_lblBatteryPct, "75%");
    lv_obj_set_style_text_font(_lblBatteryPct, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblBatteryPct, COL_ACCENT, 0);
    lv_obj_center(_lblBatteryPct);

    /* ── Status LEDs (top area) ────────────────────── */
    /* Gateway LED */
    _ledGateway = lv_led_create(_scrWatchface);
    lv_led_set_color(_ledGateway, COL_ERROR);
    lv_obj_set_size(_ledGateway, 7, 7);
    lv_obj_align(_ledGateway, LV_ALIGN_TOP_RIGHT, -40, 42);
    lv_led_on(_ledGateway);

    /* Flipper LED */
    _ledFlipper = lv_led_create(_scrWatchface);
    lv_led_set_color(_ledFlipper, COL_TEXT_DIM);
    lv_obj_set_size(_ledFlipper, 7, 7);
    lv_obj_align(_ledFlipper, LV_ALIGN_TOP_RIGHT, -52, 42);
    lv_led_off(_ledFlipper);

    /* Bridge LED */
    _ledBridge = lv_led_create(_scrWatchface);
    lv_led_set_color(_ledBridge, COL_TEXT_DIM);
    lv_obj_set_size(_ledBridge, 7, 7);
    lv_obj_align(_ledBridge, LV_ALIGN_TOP_RIGHT, -64, 42);
    lv_led_off(_ledBridge);

    /* ── WiFi indicator label (top-left area) ───────── */
    _imgWifi = lv_label_create(_scrWatchface);
    lv_label_set_text(_imgWifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(_imgWifi, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(_imgWifi, &lv_font_montserrat_14, 0);
    lv_obj_align(_imgWifi, LV_ALIGN_TOP_LEFT, 50, 40);

    /* ── Huonyx branding ────────────────────────────── */
    lv_obj_t* lblBrand = lv_label_create(_scrWatchface);
    lv_label_set_text(lblBrand, "HUONYX");
    lv_obj_set_style_text_font(lblBrand, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblBrand, COL_PRIMARY, 0);
    lv_obj_set_style_text_letter_space(lblBrand, 4, 0);
    lv_obj_set_style_text_opa(lblBrand, LV_OPA_60, 0);
    lv_obj_align(lblBrand, LV_ALIGN_CENTER, 0, -50);

    /* ── Navigation hints ──────────────────────────── */
    lv_obj_t* lblHintL = lv_label_create(_scrWatchface);
    lv_label_set_text(lblHintL, LV_SYMBOL_LEFT " Chat");
    lv_obj_set_style_text_font(lblHintL, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHintL, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_opa(lblHintL, LV_OPA_30, 0);
    lv_obj_align(lblHintL, LV_ALIGN_BOTTOM_MID, -30, -22);

    lv_obj_t* lblHintR = lv_label_create(_scrWatchface);
    lv_label_set_text(lblHintR, "Flip " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(lblHintR, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHintR, COL_FLIPPER, 0);
    lv_obj_set_style_text_opa(lblHintR, LV_OPA_30, 0);
    lv_obj_align(lblHintR, LV_ALIGN_BOTTOM_MID, 30, -22);

    /* ── Gesture handling ──────────────────────────── */
    lv_obj_add_event_cb(_scrWatchface, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  CHAT SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildChatScreen() {
    _scrChat = createRoundScreen();

    /* ── Header bar ─────────────────────────────────── */
    _chatHeader = lv_obj_create(_scrChat);
    lv_obj_add_style(_chatHeader, &_styleHeader, 0);
    lv_obj_set_size(_chatHeader, 200, 32);
    lv_obj_align(_chatHeader, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(_chatHeader, 16, 0);
    lv_obj_remove_flag(_chatHeader, LV_OBJ_FLAG_SCROLLABLE);

    _lblChatTitle = lv_label_create(_chatHeader);
    lv_label_set_text(_lblChatTitle, LV_SYMBOL_LEFT "  Huonyx Agent");
    lv_obj_set_style_text_font(_lblChatTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblChatTitle, COL_PRIMARY, 0);
    lv_obj_center(_lblChatTitle);
    lv_obj_add_event_cb(_chatHeader, onBackButton, LV_EVENT_CLICKED, this);

    /* ── Chat message list ──────────────────────────── */
    _chatList = lv_obj_create(_scrChat);
    lv_obj_set_size(_chatList, 210, 120);
    lv_obj_align(_chatList, LV_ALIGN_CENTER, 0, -5);
    lv_obj_set_style_bg_opa(_chatList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_chatList, 0, 0);
    lv_obj_set_style_pad_all(_chatList, 4, 0);
    lv_obj_set_flex_flow(_chatList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_chatList, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(_chatList, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(_chatList, LV_DIR_VER);

    /* ── Typing indicator (spinner) ─────────────────── */
    _spinnerTyping = lv_spinner_create(_scrChat);
    lv_spinner_set_anim_params(_spinnerTyping, 1000, 60);
    lv_obj_set_size(_spinnerTyping, 20, 20);
    lv_obj_align(_spinnerTyping, LV_ALIGN_CENTER, -80, 55);
    lv_obj_set_style_arc_color(_spinnerTyping, COL_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(_spinnerTyping, COL_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_spinnerTyping, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_spinnerTyping, 3, LV_PART_MAIN);
    lv_obj_add_flag(_spinnerTyping, LV_OBJ_FLAG_HIDDEN);

    /* ── Quick reply panel ──────────────────────────── */
    _panelQuickReplies = lv_obj_create(_scrChat);
    lv_obj_set_size(_panelQuickReplies, 220, 60);
    lv_obj_align(_panelQuickReplies, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(_panelQuickReplies, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_panelQuickReplies, 0, 0);
    lv_obj_set_style_pad_all(_panelQuickReplies, 2, 0);
    lv_obj_set_flex_flow(_panelQuickReplies, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(_panelQuickReplies, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(_panelQuickReplies, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_gap(_panelQuickReplies, 4, 0);

    for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
        _btnQuickReply[i] = lv_button_create(_panelQuickReplies);
        lv_obj_add_style(_btnQuickReply[i], &_styleBtn, 0);
        lv_obj_add_style(_btnQuickReply[i], &_styleBtnPressed, LV_STATE_PRESSED);
        lv_obj_set_height(_btnQuickReply[i], 24);

        lv_obj_t* lbl = lv_label_create(_btnQuickReply[i]);
        lv_label_set_text(lbl, quickReplies[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(_btnQuickReply[i], onQuickReplyClicked, LV_EVENT_CLICKED, this);
    }

    lv_obj_add_event_cb(_scrChat, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER CONTROL SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildFlipperScreen() {
    _scrFlipper = createRoundScreen();

    /* ── Header ─────────────────────────────────────── */
    lv_obj_t* header = lv_obj_create(_scrFlipper);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Flipper Zero");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_FLIPPER, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* ── Status bar ─────────────────────────────────── */
    _lblFlipperState = lv_label_create(_scrFlipper);
    lv_label_set_text(_lblFlipperState, "Idle");
    lv_obj_set_style_text_font(_lblFlipperState, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblFlipperState, COL_TEXT_DIM, 0);
    lv_obj_align(_lblFlipperState, LV_ALIGN_TOP_MID, -30, 50);

    _lblFlipperDevice = lv_label_create(_scrFlipper);
    lv_label_set_text(_lblFlipperDevice, "---");
    lv_obj_set_style_text_font(_lblFlipperDevice, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblFlipperDevice, COL_FLIPPER, 0);
    lv_obj_align(_lblFlipperDevice, LV_ALIGN_TOP_MID, 40, 50);

    _lblFlipperRssi = lv_label_create(_scrFlipper);
    lv_label_set_text(_lblFlipperRssi, "");
    lv_obj_set_style_text_font(_lblFlipperRssi, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblFlipperRssi, COL_TEXT_DIM, 0);
    lv_obj_align(_lblFlipperRssi, LV_ALIGN_TOP_RIGHT, -35, 50);

    /* ── Command log list ───────────────────────────── */
    _flipperLogList = lv_obj_create(_scrFlipper);
    lv_obj_set_size(_flipperLogList, 200, 90);
    lv_obj_align(_flipperLogList, LV_ALIGN_CENTER, 0, 5);
    lv_obj_set_style_bg_opa(_flipperLogList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_flipperLogList, 0, 0);
    lv_obj_set_style_pad_all(_flipperLogList, 2, 0);
    lv_obj_set_flex_flow(_flipperLogList, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(_flipperLogList, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(_flipperLogList, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(_flipperLogList, LV_DIR_VER);

    /* ── Action buttons ─────────────────────────────── */
    _btnFlipperScan = lv_button_create(_scrFlipper);
    lv_obj_add_style(_btnFlipperScan, &_styleBtn, 0);
    lv_obj_add_style(_btnFlipperScan, &_styleBtnPressed, LV_STATE_PRESSED);
    lv_obj_set_size(_btnFlipperScan, 80, 30);
    lv_obj_align(_btnFlipperScan, LV_ALIGN_BOTTOM_MID, -45, -25);
    lv_obj_set_style_border_color(_btnFlipperScan, COL_FLIPPER, 0);
    lv_obj_t* lblScan = lv_label_create(_btnFlipperScan);
    lv_label_set_text(lblScan, LV_SYMBOL_REFRESH " Scan");
    lv_obj_set_style_text_font(lblScan, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblScan, COL_FLIPPER, 0);
    lv_obj_center(lblScan);
    lv_obj_add_event_cb(_btnFlipperScan, onFlipperScan, LV_EVENT_CLICKED, this);

    _btnFlipperDisconnect = lv_button_create(_scrFlipper);
    lv_obj_add_style(_btnFlipperDisconnect, &_styleBtn, 0);
    lv_obj_add_style(_btnFlipperDisconnect, &_styleBtnPressed, LV_STATE_PRESSED);
    lv_obj_set_size(_btnFlipperDisconnect, 80, 30);
    lv_obj_align(_btnFlipperDisconnect, LV_ALIGN_BOTTOM_MID, 45, -25);
    lv_obj_set_style_border_color(_btnFlipperDisconnect, COL_ERROR, 0);
    lv_obj_t* lblDisc = lv_label_create(_btnFlipperDisconnect);
    lv_label_set_text(lblDisc, LV_SYMBOL_CLOSE " Disc");
    lv_obj_set_style_text_font(lblDisc, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblDisc, COL_ERROR, 0);
    lv_obj_center(lblDisc);
    lv_obj_add_event_cb(_btnFlipperDisconnect, onFlipperDisconnect, LV_EVENT_CLICKED, this);

    lv_obj_add_event_cb(_scrFlipper, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  QUICK SETTINGS SCREEN (swipe down from watch face)
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildQuickSettings() {
    _scrQuickSettings = createRoundScreen();

    /* Title */
    lv_obj_t* title = lv_label_create(_scrQuickSettings);
    lv_label_set_text(title, "Quick Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, COL_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* Brightness slider */
    lv_obj_t* lblBr = lv_label_create(_scrQuickSettings);
    lv_label_set_text(lblBr, LV_SYMBOL_IMAGE " Brightness");
    lv_obj_set_style_text_font(lblBr, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblBr, COL_TEXT_DIM, 0);
    lv_obj_align(lblBr, LV_ALIGN_CENTER, 0, -30);

    _sliderBrightness = lv_slider_create(_scrQuickSettings);
    lv_obj_set_width(_sliderBrightness, 150);
    lv_obj_align(_sliderBrightness, LV_ALIGN_CENTER, 0, -8);
    lv_slider_set_range(_sliderBrightness, 10, 255);
    lv_slider_set_value(_sliderBrightness, 200, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_sliderBrightness, COL_SURFACE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_sliderBrightness, COL_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_sliderBrightness, COL_PRIMARY, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_sliderBrightness, 3, LV_PART_KNOB);
    lv_obj_add_event_cb(_sliderBrightness, onBrightnessChanged, LV_EVENT_VALUE_CHANGED, this);

    _lblBrightnessVal = lv_label_create(_scrQuickSettings);
    lv_label_set_text(_lblBrightnessVal, "78%");
    lv_obj_set_style_text_font(_lblBrightnessVal, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblBrightnessVal, COL_TEXT_DIM, 0);
    lv_obj_align(_lblBrightnessVal, LV_ALIGN_CENTER, 0, 12);

    /* WiFi button */
    _btnWifi = lv_button_create(_scrQuickSettings);
    lv_obj_add_style(_btnWifi, &_styleBtn, 0);
    lv_obj_add_style(_btnWifi, &_styleBtnPressed, LV_STATE_PRESSED);
    lv_obj_set_size(_btnWifi, 80, 36);
    lv_obj_align(_btnWifi, LV_ALIGN_CENTER, -45, 45);
    lv_obj_t* lblW = lv_label_create(_btnWifi);
    lv_label_set_text(lblW, LV_SYMBOL_WIFI " WiFi");
    lv_obj_set_style_text_font(lblW, &lv_font_montserrat_12, 0);
    lv_obj_center(lblW);

    /* Settings button */
    _btnSettings = lv_button_create(_scrQuickSettings);
    lv_obj_add_style(_btnSettings, &_styleBtn, 0);
    lv_obj_add_style(_btnSettings, &_styleBtnPressed, LV_STATE_PRESSED);
    lv_obj_set_size(_btnSettings, 80, 36);
    lv_obj_align(_btnSettings, LV_ALIGN_CENTER, 45, 45);
    lv_obj_t* lblS = lv_label_create(_btnSettings);
    lv_label_set_text(lblS, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(lblS, &lv_font_montserrat_12, 0);
    lv_obj_center(lblS);
    lv_obj_add_event_cb(_btnSettings, onSettingsItemClicked, LV_EVENT_CLICKED, this);

    /* Back hint */
    lv_obj_t* hint = lv_label_create(_scrQuickSettings);
    lv_label_set_text(hint, LV_SYMBOL_UP " Swipe up");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(hint, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_opa(hint, LV_OPA_40, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -25);

    lv_obj_add_event_cb(_scrQuickSettings, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  SETTINGS SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildSettingsScreen() {
    _scrSettings = createRoundScreen();

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrSettings);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Settings");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* Settings list */
    _settingsList = lv_list_create(_scrSettings);
    lv_obj_set_size(_settingsList, 190, 150);
    lv_obj_align(_settingsList, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(_settingsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_settingsList, 0, 0);
    lv_obj_set_style_pad_row(_settingsList, 3, 0);
    lv_obj_set_scrollbar_mode(_settingsList, LV_SCROLLBAR_MODE_OFF);

    /* Helper lambda-like macro for list items */
    #define ADD_SETTINGS_ITEM(icon, label, screen) do { \
        lv_obj_t* btn = lv_list_add_btn(_settingsList, icon, label); \
        lv_obj_set_style_bg_color(btn, COL_CARD, 0); \
        lv_obj_set_style_text_color(btn, COL_TEXT, 0); \
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_12, 0); \
        lv_obj_set_style_radius(btn, 10, 0); \
        lv_obj_set_style_pad_ver(btn, 7, 0); \
        lv_obj_set_user_data(btn, (void*)(intptr_t)(screen)); \
        lv_obj_add_event_cb(btn, onSettingsItemClicked, LV_EVENT_CLICKED, this); \
    } while(0)

    ADD_SETTINGS_ITEM(LV_SYMBOL_WIFI,     "WiFi Setup",    SCREEN_WIFI_SETUP);
    ADD_SETTINGS_ITEM(LV_SYMBOL_UPLOAD,    "Gateway",       SCREEN_GATEWAY_SETUP);
    ADD_SETTINGS_ITEM(LV_SYMBOL_LOOP,      "Supabase",      SCREEN_SUPABASE_SETUP);
    ADD_SETTINGS_ITEM(LV_SYMBOL_BLUETOOTH, "Flipper BLE",   SCREEN_FLIPPER_SETUP);
    ADD_SETTINGS_ITEM(LV_SYMBOL_LIST,      "Sessions",      SCREEN_SESSIONS);

    #undef ADD_SETTINGS_ITEM

    /* Firmware version at bottom */
    lv_obj_t* lblVer = lv_label_create(_scrSettings);
    lv_label_set_text_fmt(lblVer, "v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_font(lblVer, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblVer, COL_TEXT_DIM, 0);
    lv_obj_align(lblVer, LV_ALIGN_BOTTOM_MID, 0, -22);

    lv_obj_add_event_cb(_scrSettings, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  WIFI SETUP SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildWifiSetup() {
    _scrWifiSetup = createRoundScreen();

    lv_obj_t* header = lv_obj_create(_scrWifiSetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  WiFi");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    lv_obj_t* lblInfo = lv_label_create(_scrWifiSetup);
    lv_label_set_text(lblInfo, "Connect to\n192.168.4.1\nfrom your phone\nto configure WiFi");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblInfo, 180);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -5);

    lv_obj_t* lblStatus = lv_label_create(_scrWifiSetup);
    lv_label_set_text(lblStatus, "AP: HuonyxWatch");
    lv_obj_set_style_text_font(lblStatus, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblStatus, COL_PRIMARY, 0);
    lv_obj_align(lblStatus, LV_ALIGN_CENTER, 0, 50);

    lv_obj_add_event_cb(_scrWifiSetup, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  GATEWAY SETUP SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildGatewaySetup() {
    _scrGatewaySetup = createRoundScreen();

    lv_obj_t* header = lv_obj_create(_scrGatewaySetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Gateway");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    lv_obj_t* lblInfo = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(lblInfo, "Configure via\nWeb Portal");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -25);

    _lblGwStatus = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(_lblGwStatus, "Not configured");
    lv_obj_set_style_text_font(_lblGwStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblGwStatus, COL_WARNING, 0);
    lv_obj_set_style_text_align(_lblGwStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblGwStatus, LV_ALIGN_CENTER, 0, 5);

    lv_obj_t* lblUrl = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(lblUrl, "Open browser:\nhttp://<watch-ip>/setup");
    lv_obj_set_style_text_font(lblUrl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblUrl, COL_PRIMARY, 0);
    lv_obj_set_style_text_align(lblUrl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblUrl, 180);
    lv_obj_align(lblUrl, LV_ALIGN_CENTER, 0, 40);

    lv_obj_add_event_cb(_scrGatewaySetup, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  SUPABASE SETUP SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildSupabaseSetup() {
    _scrSupabaseSetup = createRoundScreen();

    lv_obj_t* header = lv_obj_create(_scrSupabaseSetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Supabase");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    lv_obj_t* lblInfo = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(lblInfo, "Realtime Bridge\nfor Agent " LV_SYMBOL_LOOP " Flipper");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblInfo, 180);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -25);

    _lblSbStatus = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(_lblSbStatus, "Not configured");
    lv_obj_set_style_text_font(_lblSbStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblSbStatus, COL_WARNING, 0);
    lv_obj_set_style_text_align(_lblSbStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblSbStatus, LV_ALIGN_CENTER, 0, 5);

    lv_obj_t* lblUrl = lv_label_create(_scrSupabaseSetup);
    lv_label_set_text(lblUrl, "Configure via\nWeb Portal at\nhttp://<watch-ip>/setup");
    lv_obj_set_style_text_font(lblUrl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblUrl, COL_PRIMARY, 0);
    lv_obj_set_style_text_align(lblUrl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblUrl, 180);
    lv_obj_align(lblUrl, LV_ALIGN_CENTER, 0, 40);

    lv_obj_add_event_cb(_scrSupabaseSetup, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER SETUP SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildFlipperSetup() {
    _scrFlipperSetup = createRoundScreen();

    lv_obj_t* header = lv_obj_create(_scrFlipperSetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Flipper BLE");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_FLIPPER, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    lv_obj_t* lblInfo = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(lblInfo, "BLE Serial to\nFlipper Zero CLI");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblInfo, 180);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -25);

    _lblFlipSetupStatus = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(_lblFlipSetupStatus, "Auto-connect: OFF");
    lv_obj_set_style_text_font(_lblFlipSetupStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblFlipSetupStatus, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_align(_lblFlipSetupStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblFlipSetupStatus, LV_ALIGN_CENTER, 0, 5);

    lv_obj_t* lblUrl = lv_label_create(_scrFlipperSetup);
    lv_label_set_text(lblUrl, "Set device name &\nauto-connect via\nWeb Portal");
    lv_obj_set_style_text_font(lblUrl, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblUrl, COL_PRIMARY, 0);
    lv_obj_set_style_text_align(lblUrl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblUrl, 180);
    lv_obj_align(lblUrl, LV_ALIGN_CENTER, 0, 40);

    lv_obj_add_event_cb(_scrFlipperSetup, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  SESSIONS SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildSessionsScreen() {
    _scrSessions = createRoundScreen();

    lv_obj_t* header = lv_obj_create(_scrSessions);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_remove_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Sessions");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    _sessionsList = lv_list_create(_scrSessions);
    lv_obj_set_size(_sessionsList, 190, 140);
    lv_obj_align(_sessionsList, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(_sessionsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_sessionsList, 0, 0);
    lv_obj_set_style_pad_row(_sessionsList, 4, 0);
    lv_obj_set_scrollbar_mode(_sessionsList, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* btnDefault = lv_list_add_btn(_sessionsList, LV_SYMBOL_EDIT, "esp32-watch");
    lv_obj_set_style_bg_color(btnDefault, COL_CARD, 0);
    lv_obj_set_style_text_color(btnDefault, COL_TEXT, 0);
    lv_obj_set_style_text_font(btnDefault, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(btnDefault, 10, 0);
    lv_obj_set_style_pad_ver(btnDefault, 8, 0);

    lv_obj_add_event_cb(_scrSessions, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  SCREEN NAVIGATION
 * ══════════════════════════════════════════════════════════ */

void UIManager::showScreen(ScreenId id, lv_screen_load_anim_t anim) {
    lv_obj_t* target = nullptr;

    switch (id) {
        case SCREEN_WATCHFACE:       target = _scrWatchface; break;
        case SCREEN_CHAT:            target = _scrChat; break;
        case SCREEN_FLIPPER:         target = _scrFlipper; break;
        case SCREEN_QUICK_SETTINGS:  target = _scrQuickSettings; break;
        case SCREEN_SETTINGS:        target = _scrSettings; break;
        case SCREEN_WIFI_SETUP:      target = _scrWifiSetup; break;
        case SCREEN_GATEWAY_SETUP:   target = _scrGatewaySetup; break;
        case SCREEN_SUPABASE_SETUP:  target = _scrSupabaseSetup; break;
        case SCREEN_FLIPPER_SETUP:   target = _scrFlipperSetup; break;
        case SCREEN_SESSIONS:        target = _scrSessions; break;
        default: return;
    }

    if (target) {
        _currentScreen = id;
        lv_screen_load_anim(target, anim, 300, 0, false);
    }
}

/* ══════════════════════════════════════════════════════════
 *  UPDATE METHODS
 * ══════════════════════════════════════════════════════════ */

void UIManager::updateTime(int hour, int min, int sec) {
    if (!_lblTime) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, min);
    lv_label_set_text(_lblTime, buf);
}

void UIManager::updateDate(const char* dateStr) {
    if (!_lblDate) return;
    lv_label_set_text(_lblDate, dateStr);
}

void UIManager::updateBattery(int percent, bool charging) {
    if (!_arcBattery || !_lblBatteryPct) return;

    lv_arc_set_value(_arcBattery, percent);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", percent);
    lv_label_set_text(_lblBatteryPct, buf);

    lv_color_t col;
    if (charging) {
        col = COL_PRIMARY;
    } else if (percent > 50) {
        col = COL_ACCENT;
    } else if (percent > 20) {
        col = COL_WARNING;
    } else {
        col = COL_ERROR;
    }
    lv_obj_set_style_arc_color(_arcBattery, col, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(_lblBatteryPct, col, 0);
}

void UIManager::updateWiFiStrength(int rssi) {
    if (!_imgWifi) return;
    if (rssi == 0) {
        lv_obj_set_style_text_color(_imgWifi, COL_ERROR, 0);
    } else if (rssi > -50) {
        lv_obj_set_style_text_color(_imgWifi, COL_ACCENT, 0);
    } else if (rssi > -70) {
        lv_obj_set_style_text_color(_imgWifi, COL_PRIMARY, 0);
    } else {
        lv_obj_set_style_text_color(_imgWifi, COL_WARNING, 0);
    }
}

void UIManager::updateGatewayStatus(GatewayState state) {
    if (_ledGateway) {
        switch (state) {
            case GW_AUTHENTICATED:
                lv_led_set_color(_ledGateway, COL_ACCENT);
                lv_led_on(_ledGateway);
                break;
            case GW_CONNECTING:
            case GW_CONNECTED:
                lv_led_set_color(_ledGateway, COL_WARNING);
                lv_led_on(_ledGateway);
                break;
            case GW_ERROR:
                lv_led_set_color(_ledGateway, COL_ERROR);
                lv_led_on(_ledGateway);
                break;
            default:
                lv_led_set_color(_ledGateway, COL_ERROR);
                lv_led_off(_ledGateway);
                break;
        }
    }

    if (_lblGwStatus) {
        switch (state) {
            case GW_AUTHENTICATED:
                lv_label_set_text(_lblGwStatus, "Connected");
                lv_obj_set_style_text_color(_lblGwStatus, COL_ACCENT, 0);
                break;
            case GW_CONNECTING:
                lv_label_set_text(_lblGwStatus, "Connecting...");
                lv_obj_set_style_text_color(_lblGwStatus, COL_WARNING, 0);
                break;
            case GW_ERROR:
                lv_label_set_text(_lblGwStatus, "Error");
                lv_obj_set_style_text_color(_lblGwStatus, COL_ERROR, 0);
                break;
            default:
                lv_label_set_text(_lblGwStatus, "Disconnected");
                lv_obj_set_style_text_color(_lblGwStatus, COL_TEXT_DIM, 0);
                break;
        }
    }
}

void UIManager::updateFlipperStatus(FlipperState state) {
    /* Update watch face LED */
    if (_ledFlipper) {
        switch (state) {
            case FLIP_READY:
                lv_led_set_color(_ledFlipper, COL_FLIPPER);
                lv_led_on(_ledFlipper);
                break;
            case FLIP_SCANNING:
            case FLIP_CONNECTING:
            case FLIP_CONNECTED:
                lv_led_set_color(_ledFlipper, COL_WARNING);
                lv_led_on(_ledFlipper);
                break;
            case FLIP_BUSY:
                lv_led_set_color(_ledFlipper, COL_PRIMARY);
                lv_led_on(_ledFlipper);
                break;
            case FLIP_ERROR:
                lv_led_set_color(_ledFlipper, COL_ERROR);
                lv_led_on(_ledFlipper);
                break;
            default:
                lv_led_set_color(_ledFlipper, COL_TEXT_DIM);
                lv_led_off(_ledFlipper);
                break;
        }
    }

    /* Update Flipper screen state label */
    if (_lblFlipperState) {
        const char* stateStr = "Idle";
        lv_color_t col = COL_TEXT_DIM;
        switch (state) {
            case FLIP_SCANNING:   stateStr = "Scanning..."; col = COL_WARNING; break;
            case FLIP_CONNECTING: stateStr = "Connecting"; col = COL_WARNING; break;
            case FLIP_CONNECTED:  stateStr = "Connected"; col = COL_PRIMARY; break;
            case FLIP_READY:      stateStr = "Ready"; col = COL_FLIPPER; break;
            case FLIP_BUSY:       stateStr = "Busy"; col = COL_PRIMARY; break;
            case FLIP_ERROR:      stateStr = "Error"; col = COL_ERROR; break;
            default: break;
        }
        lv_label_set_text(_lblFlipperState, stateStr);
        lv_obj_set_style_text_color(_lblFlipperState, col, 0);
    }

    /* Update setup screen */
    if (_lblFlipSetupStatus && _cfg) {
        char buf[48];
        snprintf(buf, sizeof(buf), "Auto: %s | %s",
                 _cfg->config().flipperAuto ? "ON" : "OFF",
                 _cfg->config().flipperName[0] ? _cfg->config().flipperName : "Any");
        lv_label_set_text(_lblFlipSetupStatus, buf);
    }
}

void UIManager::updateBridgeStatus(BridgeState state) {
    if (_ledBridge) {
        switch (state) {
            case BRIDGE_JOINED:
                lv_led_set_color(_ledBridge, COL_SECONDARY);
                lv_led_on(_ledBridge);
                break;
            case BRIDGE_CONNECTING:
            case BRIDGE_CONNECTED:
                lv_led_set_color(_ledBridge, COL_WARNING);
                lv_led_on(_ledBridge);
                break;
            case BRIDGE_ERROR:
                lv_led_set_color(_ledBridge, COL_ERROR);
                lv_led_on(_ledBridge);
                break;
            default:
                lv_led_set_color(_ledBridge, COL_TEXT_DIM);
                lv_led_off(_ledBridge);
                break;
        }
    }

    if (_lblSbStatus) {
        switch (state) {
            case BRIDGE_JOINED:
                lv_label_set_text(_lblSbStatus, "Channel Joined");
                lv_obj_set_style_text_color(_lblSbStatus, COL_ACCENT, 0);
                break;
            case BRIDGE_CONNECTING:
                lv_label_set_text(_lblSbStatus, "Connecting...");
                lv_obj_set_style_text_color(_lblSbStatus, COL_WARNING, 0);
                break;
            case BRIDGE_CONNECTED:
                lv_label_set_text(_lblSbStatus, "Joining channel...");
                lv_obj_set_style_text_color(_lblSbStatus, COL_WARNING, 0);
                break;
            case BRIDGE_ERROR:
                lv_label_set_text(_lblSbStatus, "Error");
                lv_obj_set_style_text_color(_lblSbStatus, COL_ERROR, 0);
                break;
            default:
                lv_label_set_text(_lblSbStatus, "Not configured");
                lv_obj_set_style_text_color(_lblSbStatus, COL_TEXT_DIM, 0);
                break;
        }
    }
}

/* ══════════════════════════════════════════════════════════
 *  CHAT METHODS
 * ══════════════════════════════════════════════════════════ */

void UIManager::addChatMessage(const char* text, bool isUser) {
    addChatBubble(text, isUser);
}

void UIManager::addChatBubble(const char* text, bool isUser) {
    if (!_chatList) return;

    if (_chatMsgCount >= CHAT_MAX_MESSAGES) {
        lv_obj_t* first = lv_obj_get_child(_chatList, 0);
        if (first) {
            lv_obj_delete(first);
            _chatMsgCount--;
        }
    }

    lv_obj_t* row = lv_obj_create(_chatList);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* bubble = lv_obj_create(row);
    lv_obj_set_size(bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    if (isUser) {
        lv_obj_add_style(bubble, &_styleBubbleUser, 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_add_style(bubble, &_styleBubbleAgent, 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
    }

    lv_obj_t* lbl = lv_label_create(bubble);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 150);

    _chatMsgCount++;
    lv_obj_scroll_to_y(_chatList, 32767, LV_ANIM_ON);
}

void UIManager::updateAgentTyping(bool typing) {
    if (!_spinnerTyping) return;
    if (typing) {
        lv_obj_remove_flag(_spinnerTyping, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_spinnerTyping, LV_OBJ_FLAG_HIDDEN);
    }
}

void UIManager::clearChat() {
    if (!_chatList) return;
    lv_obj_clean(_chatList);
    _chatMsgCount = 0;
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER LOG METHODS
 * ══════════════════════════════════════════════════════════ */

void UIManager::addFlipperLog(const char* text, bool isCommand) {
    addFlipperLogEntry(text, isCommand);
}

void UIManager::addFlipperLogEntry(const char* text, bool isCommand) {
    if (!_flipperLogList) return;

    /* Limit entries */
    if (_flipperLogCount >= 6) {
        lv_obj_t* first = lv_obj_get_child(_flipperLogList, 0);
        if (first) {
            lv_obj_delete(first);
            _flipperLogCount--;
        }
    }

    lv_obj_t* entry = lv_obj_create(_flipperLogList);
    lv_obj_set_size(entry, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_remove_flag(entry, LV_OBJ_FLAG_SCROLLABLE);

    if (isCommand) {
        lv_obj_add_style(entry, &_styleCmdLog, 0);
    } else {
        lv_obj_add_style(entry, &_styleResultLog, 0);
    }

    lv_obj_t* lbl = lv_label_create(entry);
    /* Prefix with > for commands, < for results */
    char buf[200];
    snprintf(buf, sizeof(buf), "%s %s", isCommand ? ">" : "<", text);
    lv_label_set_text(lbl, buf);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 180);

    _flipperLogCount++;
    lv_obj_scroll_to_y(_flipperLogList, 32767, LV_ANIM_ON);
}

void UIManager::updateFlipperInfo(const char* deviceName, int rssi) {
    if (_lblFlipperDevice && deviceName) {
        lv_label_set_text(_lblFlipperDevice, deviceName);
    }
    if (_lblFlipperRssi) {
        if (rssi != 0) {
            char buf[12];
            snprintf(buf, sizeof(buf), "%ddB", rssi);
            lv_label_set_text(_lblFlipperRssi, buf);
        } else {
            lv_label_set_text(_lblFlipperRssi, "");
        }
    }
}

void UIManager::updateSettingsValues() {
    if (_sliderBrightness && _cfg) {
        lv_slider_set_value(_sliderBrightness, _cfg->config().brightness, LV_ANIM_OFF);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", (_cfg->config().brightness * 100) / 255);
        lv_label_set_text(_lblBrightnessVal, buf);
    }
}

/* ══════════════════════════════════════════════════════════
 *  STATIC CALLBACKS
 * ══════════════════════════════════════════════════════════ */

void UIManager::onQuickReplyClicked(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);

    for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
        if (btn == self->_btnQuickReply[i]) {
            const char* msg = quickReplies[i];
            self->addChatMessage(msg, true);

            if (self->_gw && self->_gw->isConnected()) {
                self->_gw->sendMessage(self->_gw->getSessionKey(), msg);
                self->updateAgentTyping(true);
            }
            break;
        }
    }
}

void UIManager::onBrightnessChanged(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* slider = (lv_obj_t*)lv_event_get_target(e);
    int val = lv_slider_get_value(slider);

    analogWrite(PIN_TFT_BL, val);

    if (self->_cfg) {
        self->_cfg->setBrightness((uint8_t)val);
    }

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (val * 100) / 255);
    lv_label_set_text(self->_lblBrightnessVal, buf);
}

void UIManager::onSettingsItemClicked(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);

    if (btn == self->_btnSettings) {
        self->showScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        return;
    }

    void* data = lv_obj_get_user_data(btn);
    if (data) {
        ScreenId target = (ScreenId)(intptr_t)data;
        self->showScreen(target, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

void UIManager::onGatewaySave(lv_event_t* e) {
    /* Handled by web portal */
}

void UIManager::onBackButton(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);

    switch (self->_currentScreen) {
        case SCREEN_CHAT:
        case SCREEN_FLIPPER:
            self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        case SCREEN_SETTINGS:
            self->showScreen(SCREEN_QUICK_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        case SCREEN_WIFI_SETUP:
        case SCREEN_GATEWAY_SETUP:
        case SCREEN_SUPABASE_SETUP:
        case SCREEN_FLIPPER_SETUP:
        case SCREEN_SESSIONS:
            self->showScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        default:
            self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
    }
}

void UIManager::onSessionSelected(lv_event_t* e) {
    /* TODO: implement session switching */
}

void UIManager::onWifiSelected(lv_event_t* e) {
    /* TODO: implement WiFi network selection */
}

void UIManager::onFlipperScan(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    if (self->_flipper) {
        const char* name = "";
        if (self->_cfg && self->_cfg->config().flipperName[0]) {
            name = self->_cfg->config().flipperName;
        }
        self->_flipper->startScan(name);
        self->addFlipperLog("Scanning for Flipper...", true);
    }
}

void UIManager::onFlipperDisconnect(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    if (self->_flipper) {
        self->_flipper->disconnect();
        self->addFlipperLog("Disconnected", false);
    }
}

void UIManager::onGestureEvent(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());

    switch (self->_currentScreen) {
        case SCREEN_WATCHFACE:
            if (dir == LV_DIR_LEFT) {
                self->showScreen(SCREEN_CHAT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_RIGHT) {
                self->showScreen(SCREEN_FLIPPER, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            } else if (dir == LV_DIR_BOTTOM) {
                self->showScreen(SCREEN_QUICK_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
            break;

        case SCREEN_CHAT:
            if (dir == LV_DIR_RIGHT) {
                self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            }
            break;

        case SCREEN_FLIPPER:
            if (dir == LV_DIR_LEFT) {
                self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            }
            break;

        case SCREEN_QUICK_SETTINGS:
            if (dir == LV_DIR_TOP) {
                self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_TOP);
            }
            break;

        case SCREEN_SETTINGS:
        case SCREEN_WIFI_SETUP:
        case SCREEN_GATEWAY_SETUP:
        case SCREEN_SUPABASE_SETUP:
        case SCREEN_FLIPPER_SETUP:
        case SCREEN_SESSIONS:
            if (dir == LV_DIR_RIGHT) {
                onBackButton(e);
            }
            break;

        default:
            break;
    }
}









