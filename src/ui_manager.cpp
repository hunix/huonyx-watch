/**
 * UI Manager Implementation - LVGL Smartwatch Interface
 * Huonyx AI Smartwatch
 *
 * Screens:
 *   - Watch Face (home) with time, date, battery arc, status indicators
 *   - Chat Interface with message bubbles and quick replies
 *   - Quick Settings (brightness, WiFi toggle)
 *   - Settings Menu (WiFi, Gateway, About)
 *   - WiFi Setup
 *   - Gateway Setup (Host + Token input)
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
#define COL_TEXT        lv_color_hex(0xE8E8F0)   /* Light text */
#define COL_TEXT_DIM    lv_color_hex(0x6B6B8D)   /* Dimmed text */
#define COL_BUBBLE_USER lv_color_hex(0x0A84FF)   /* User bubble blue */
#define COL_BUBBLE_AI   lv_color_hex(0x2A2A3E)   /* AI bubble dark */
#define COL_BTN         lv_color_hex(0x1E1E3A)   /* Button background */
#define COL_BTN_PRESS   lv_color_hex(0x2E2E5A)   /* Button pressed */

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
    , _currentScreen(SCREEN_WATCHFACE)
    , _chatMsgCount(0)
{
}

/* ── Initialization ───────────────────────────────────── */

void UIManager::begin(GatewayClient* gw, ConfigManager* cfg) {
    _gw = gw;
    _cfg = cfg;

    createStyles();
    buildWatchface();
    buildChatScreen();
    buildQuickSettings();
    buildSettingsScreen();
    buildWifiSetup();
    buildGatewaySetup();
    buildSessionsScreen();

    /* Start with watch face */
    lv_scr_load(_scrWatchface);
}

void UIManager::update() {
    /* Called from main loop - nothing special needed, LVGL handles timers */
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
}

/* ── Helper: Create a round screen ────────────────────── */

lv_obj_t* UIManager::createRoundScreen() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &_styleBg, 0);
    lv_obj_set_size(scr, TFT_WIDTH, TFT_HEIGHT);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_clear_flag(ringOuter, LV_OBJ_FLAG_CLICKABLE);
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
    lv_obj_clear_flag(_arcBattery, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(_arcBattery, LV_OPA_TRANSP, LV_PART_KNOB);

    _lblBatteryPct = lv_label_create(_arcBattery);
    lv_label_set_text(_lblBatteryPct, "75%");
    lv_obj_set_style_text_font(_lblBatteryPct, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(_lblBatteryPct, COL_ACCENT, 0);
    lv_obj_center(_lblBatteryPct);

    /* ── Gateway status LED (top-right area) ────────── */
    _ledGateway = lv_led_create(_scrWatchface);
    lv_led_set_color(_ledGateway, COL_ERROR);
    lv_obj_set_size(_ledGateway, 8, 8);
    lv_obj_align(_ledGateway, LV_ALIGN_TOP_RIGHT, -50, 40);
    lv_led_on(_ledGateway);

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

    /* ── Navigation hint at bottom ──────────────────── */
    lv_obj_t* lblHint = lv_label_create(_scrWatchface);
    lv_label_set_text(lblHint, LV_SYMBOL_LEFT " Chat");
    lv_obj_set_style_text_font(lblHint, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblHint, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_opa(lblHint, LV_OPA_40, 0);
    lv_obj_align(lblHint, LV_ALIGN_BOTTOM_MID, 0, -25);

    /* ── Gesture handling ───────────────────────────── */
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
    lv_obj_clear_flag(_chatHeader, LV_OBJ_FLAG_SCROLLABLE);

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
    _spinnerTyping = lv_spinner_create(_scrChat, 1000, 60);
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
        _btnQuickReply[i] = lv_btn_create(_panelQuickReplies);
        lv_obj_add_style(_btnQuickReply[i], &_styleBtn, 0);
        lv_obj_add_style(_btnQuickReply[i], &_styleBtnPressed, LV_STATE_PRESSED);
        lv_obj_set_height(_btnQuickReply[i], 24);

        lv_obj_t* lbl = lv_label_create(_btnQuickReply[i]);
        lv_label_set_text(lbl, quickReplies[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
        lv_obj_center(lbl);

        lv_obj_add_event_cb(_btnQuickReply[i], onQuickReplyClicked, LV_EVENT_CLICKED, this);
    }

    /* ── Gesture handling ───────────────────────────── */
    lv_obj_add_event_cb(_scrChat, onGestureEvent, LV_EVENT_GESTURE, this);
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
    _btnWifi = lv_btn_create(_scrQuickSettings);
    lv_obj_add_style(_btnWifi, &_styleBtn, 0);
    lv_obj_add_style(_btnWifi, &_styleBtnPressed, LV_STATE_PRESSED);
    lv_obj_set_size(_btnWifi, 80, 36);
    lv_obj_align(_btnWifi, LV_ALIGN_CENTER, -45, 45);
    lv_obj_t* lblW = lv_label_create(_btnWifi);
    lv_label_set_text(lblW, LV_SYMBOL_WIFI " WiFi");
    lv_obj_set_style_text_font(lblW, &lv_font_montserrat_12, 0);
    lv_obj_center(lblW);

    /* Settings button */
    _btnSettings = lv_btn_create(_scrQuickSettings);
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
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Settings");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* Settings list */
    _settingsList = lv_list_create(_scrSettings);
    lv_obj_set_size(_settingsList, 190, 140);
    lv_obj_align(_settingsList, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(_settingsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_settingsList, 0, 0);
    lv_obj_set_style_pad_row(_settingsList, 4, 0);
    lv_obj_set_scrollbar_mode(_settingsList, LV_SCROLLBAR_MODE_OFF);

    /* WiFi item */
    lv_obj_t* btnWifi = lv_list_add_btn(_settingsList, LV_SYMBOL_WIFI, "WiFi Setup");
    lv_obj_set_style_bg_color(btnWifi, COL_CARD, 0);
    lv_obj_set_style_text_color(btnWifi, COL_TEXT, 0);
    lv_obj_set_style_text_font(btnWifi, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(btnWifi, 10, 0);
    lv_obj_set_style_pad_ver(btnWifi, 8, 0);
    lv_obj_set_user_data(btnWifi, (void*)(intptr_t)SCREEN_WIFI_SETUP);
    lv_obj_add_event_cb(btnWifi, onSettingsItemClicked, LV_EVENT_CLICKED, this);

    /* Gateway item */
    lv_obj_t* btnGw = lv_list_add_btn(_settingsList, LV_SYMBOL_UPLOAD, "Gateway");
    lv_obj_set_style_bg_color(btnGw, COL_CARD, 0);
    lv_obj_set_style_text_color(btnGw, COL_TEXT, 0);
    lv_obj_set_style_text_font(btnGw, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(btnGw, 10, 0);
    lv_obj_set_style_pad_ver(btnGw, 8, 0);
    lv_obj_set_user_data(btnGw, (void*)(intptr_t)SCREEN_GATEWAY_SETUP);
    lv_obj_add_event_cb(btnGw, onSettingsItemClicked, LV_EVENT_CLICKED, this);

    /* Sessions item */
    lv_obj_t* btnSess = lv_list_add_btn(_settingsList, LV_SYMBOL_LIST, "Sessions");
    lv_obj_set_style_bg_color(btnSess, COL_CARD, 0);
    lv_obj_set_style_text_color(btnSess, COL_TEXT, 0);
    lv_obj_set_style_text_font(btnSess, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(btnSess, 10, 0);
    lv_obj_set_style_pad_ver(btnSess, 8, 0);
    lv_obj_set_user_data(btnSess, (void*)(intptr_t)SCREEN_SESSIONS);
    lv_obj_add_event_cb(btnSess, onSettingsItemClicked, LV_EVENT_CLICKED, this);

    /* About item */
    lv_obj_t* btnAbout = lv_list_add_btn(_settingsList, LV_SYMBOL_CHARGE, "About");
    lv_obj_set_style_bg_color(btnAbout, COL_CARD, 0);
    lv_obj_set_style_text_color(btnAbout, COL_TEXT, 0);
    lv_obj_set_style_text_font(btnAbout, &lv_font_montserrat_12, 0);
    lv_obj_set_style_radius(btnAbout, 10, 0);
    lv_obj_set_style_pad_ver(btnAbout, 8, 0);

    /* Firmware version at bottom */
    lv_obj_t* lblVer = lv_label_create(_scrSettings);
    lv_label_set_text_fmt(lblVer, "v%s", FIRMWARE_VERSION);
    lv_obj_set_style_text_font(lblVer, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_color(lblVer, COL_TEXT_DIM, 0);
    lv_obj_align(lblVer, LV_ALIGN_BOTTOM_MID, 0, -25);

    lv_obj_add_event_cb(_scrSettings, onGestureEvent, LV_EVENT_GESTURE, this);
}

/* ══════════════════════════════════════════════════════════
 *  WIFI SETUP SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildWifiSetup() {
    _scrWifiSetup = createRoundScreen();

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrWifiSetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  WiFi");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* Info text */
    lv_obj_t* lblInfo = lv_label_create(_scrWifiSetup);
    lv_label_set_text(lblInfo, "Connect to\n192.168.4.1\nfrom your phone\nto configure WiFi");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lblInfo, 180);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -5);

    /* Status */
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

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrGatewaySetup);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Gateway");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* Info - direct user to web config */
    lv_obj_t* lblInfo = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(lblInfo, "Configure via\nWeb Portal");
    lv_obj_set_style_text_font(lblInfo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblInfo, COL_TEXT, 0);
    lv_obj_set_style_text_align(lblInfo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblInfo, LV_ALIGN_CENTER, 0, -25);

    /* Status indicator */
    _lblGwStatus = lv_label_create(_scrGatewaySetup);
    lv_label_set_text(_lblGwStatus, "Not configured");
    lv_obj_set_style_text_font(_lblGwStatus, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(_lblGwStatus, COL_WARNING, 0);
    lv_obj_set_style_text_align(_lblGwStatus, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblGwStatus, LV_ALIGN_CENTER, 0, 5);

    /* Web portal URL */
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
 *  SESSIONS SCREEN
 * ══════════════════════════════════════════════════════════ */

void UIManager::buildSessionsScreen() {
    _scrSessions = createRoundScreen();

    /* Header */
    lv_obj_t* header = lv_obj_create(_scrSessions);
    lv_obj_add_style(header, &_styleHeader, 0);
    lv_obj_set_size(header, 200, 32);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_radius(header, 16, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lblTitle = lv_label_create(header);
    lv_label_set_text(lblTitle, LV_SYMBOL_LEFT "  Sessions");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lblTitle, COL_PRIMARY, 0);
    lv_obj_center(lblTitle);
    lv_obj_add_event_cb(header, onBackButton, LV_EVENT_CLICKED, this);

    /* Sessions list */
    _sessionsList = lv_list_create(_scrSessions);
    lv_obj_set_size(_sessionsList, 190, 140);
    lv_obj_align(_sessionsList, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_opa(_sessionsList, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_sessionsList, 0, 0);
    lv_obj_set_style_pad_row(_sessionsList, 4, 0);
    lv_obj_set_scrollbar_mode(_sessionsList, LV_SCROLLBAR_MODE_OFF);

    /* Default session */
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

void UIManager::showScreen(ScreenId id, lv_scr_load_anim_t anim) {
    lv_obj_t* target = nullptr;

    switch (id) {
        case SCREEN_WATCHFACE:      target = _scrWatchface; break;
        case SCREEN_CHAT:           target = _scrChat; break;
        case SCREEN_QUICK_SETTINGS: target = _scrQuickSettings; break;
        case SCREEN_SETTINGS:       target = _scrSettings; break;
        case SCREEN_WIFI_SETUP:     target = _scrWifiSetup; break;
        case SCREEN_GATEWAY_SETUP:  target = _scrGatewaySetup; break;
        case SCREEN_SESSIONS:       target = _scrSessions; break;
        default: return;
    }

    if (target) {
        _currentScreen = id;
        lv_scr_load_anim(target, anim, 300, 0, false);
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

    /* Blink colon effect - hide colon on odd seconds */
    /* (visual polish handled via timer in main) */
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

    /* Color based on level */
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
    if (!_ledGateway) return;
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

    /* Update gateway setup screen status */
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

/* ══════════════════════════════════════════════════════════
 *  CHAT METHODS
 * ══════════════════════════════════════════════════════════ */

void UIManager::addChatMessage(const char* text, bool isUser) {
    addChatBubble(text, isUser);
}

void UIManager::addChatBubble(const char* text, bool isUser) {
    if (!_chatList) return;

    /* Limit messages to prevent OOM */
    if (_chatMsgCount >= CHAT_MAX_MESSAGES) {
        /* Remove oldest message */
        lv_obj_t* first = lv_obj_get_child(_chatList, 0);
        if (first) {
            lv_obj_del(first);
            _chatMsgCount--;
        }
    }

    /* Create bubble container */
    lv_obj_t* row = lv_obj_create(_chatList);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Create the bubble */
    lv_obj_t* bubble = lv_obj_create(row);
    lv_obj_set_size(bubble, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    if (isUser) {
        lv_obj_add_style(bubble, &_styleBubbleUser, 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
    } else {
        lv_obj_add_style(bubble, &_styleBubbleAgent, 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
    }

    /* Message text */
    lv_obj_t* lbl = lv_label_create(bubble);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 150);

    _chatMsgCount++;

    /* Scroll to bottom */
    lv_obj_scroll_to_y(_chatList, LV_COORD_MAX, LV_ANIM_ON);
}

void UIManager::updateAgentTyping(bool typing) {
    if (!_spinnerTyping) return;
    if (typing) {
        lv_obj_clear_flag(_spinnerTyping, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(_spinnerTyping, LV_OBJ_FLAG_HIDDEN);
    }
}

void UIManager::clearChat() {
    if (!_chatList) return;
    lv_obj_clean(_chatList);
    _chatMsgCount = 0;
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
    lv_obj_t* btn = lv_event_get_target(e);

    /* Find which button was clicked */
    for (int i = 0; i < QUICK_REPLY_COUNT; i++) {
        if (btn == self->_btnQuickReply[i]) {
            const char* msg = quickReplies[i];

            /* Add to chat as user message */
            self->addChatMessage(msg, true);

            /* Send to gateway */
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
    lv_obj_t* slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);

    /* Update backlight */
    analogWrite(PIN_TFT_BL, val);

    /* Save config */
    if (self->_cfg) {
        self->_cfg->setBrightness((uint8_t)val);
    }

    /* Update label */
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", (val * 100) / 255);
    lv_label_set_text(self->_lblBrightnessVal, buf);
}

void UIManager::onSettingsItemClicked(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_target(e);

    /* Check if it's the settings button from quick settings */
    if (btn == self->_btnSettings) {
        self->showScreen(SCREEN_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_LEFT);
        return;
    }

    /* Otherwise it's a list item with screen ID in user data */
    void* data = lv_obj_get_user_data(btn);
    if (data) {
        ScreenId target = (ScreenId)(intptr_t)data;
        self->showScreen(target, LV_SCR_LOAD_ANIM_MOVE_LEFT);
    }
}

void UIManager::onGatewaySave(lv_event_t* e) {
    /* Handled by web portal instead */
}

void UIManager::onBackButton(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);

    switch (self->_currentScreen) {
        case SCREEN_CHAT:
            self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        case SCREEN_SETTINGS:
            self->showScreen(SCREEN_QUICK_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
            break;
        case SCREEN_WIFI_SETUP:
        case SCREEN_GATEWAY_SETUP:
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

void UIManager::onGestureEvent(lv_event_t* e) {
    UIManager* self = (UIManager*)lv_event_get_user_data(e);
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    switch (self->_currentScreen) {
        case SCREEN_WATCHFACE:
            if (dir == LV_DIR_LEFT) {
                self->showScreen(SCREEN_CHAT, LV_SCR_LOAD_ANIM_MOVE_LEFT);
            } else if (dir == LV_DIR_BOTTOM) {
                self->showScreen(SCREEN_QUICK_SETTINGS, LV_SCR_LOAD_ANIM_MOVE_BOTTOM);
            }
            break;

        case SCREEN_CHAT:
            if (dir == LV_DIR_RIGHT) {
                self->showScreen(SCREEN_WATCHFACE, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
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
        case SCREEN_SESSIONS:
            if (dir == LV_DIR_RIGHT) {
                onBackButton(e);
            }
            break;

        default:
            break;
    }
}
