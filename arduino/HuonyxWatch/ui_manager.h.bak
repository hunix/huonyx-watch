/**
 * UI Manager - LVGL-based Smartwatch Interface
 * Huonyx AI Smartwatch v2.0
 */

#ifndef UI_MANAGER_H
#define UI_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include "hw_config.h"
#include "gateway_client.h"
#include "config_manager.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"

/* Screen IDs */
enum ScreenId : uint8_t {
    SCREEN_WATCHFACE = 0,
    SCREEN_CHAT,
    SCREEN_FLIPPER,
    SCREEN_QUICK_SETTINGS,
    SCREEN_SETTINGS,
    SCREEN_WIFI_SETUP,
    SCREEN_GATEWAY_SETUP,
    SCREEN_SUPABASE_SETUP,
    SCREEN_FLIPPER_SETUP,
    SCREEN_SESSIONS,
    SCREEN_COUNT
};

/* Quick reply presets */
#define QUICK_REPLY_COUNT 6

class UIManager {
public:
    UIManager();

    void begin(GatewayClient* gw, ConfigManager* cfg,
               FlipperBLE* flipper, SupabaseBridge* bridge);
    void update();

    /* Display Sleep & Wake */
    void resetSleepTimer();
    bool isSleeping() const { return _isSleeping; }
    void wakeDisplay();
    void sleepDisplay();

    /* Push Notifications */
    void showNotification(const char* title, const char* msg);

    /* Screen navigation */
    void showScreen(ScreenId id, lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_MOVE_LEFT);
    ScreenId currentScreen() const { return _currentScreen; }

    /* Watch face updates */
    void updateTime(int hour, int min, int sec);
    void updateDate(const char* dateStr);
    void updateBattery(int percent, bool charging);
    void updateWiFiStrength(int rssi);
    void updateGatewayStatus(GatewayState state);
    void updateFlipperStatus(FlipperState state);
    void updateBridgeStatus(BridgeState state);

    /* Chat updates */
    void addChatMessage(const char* text, bool isUser);
    void updateAgentTyping(bool typing);
    void clearChat();

    /* Flipper screen updates */
    void addFlipperLog(const char* text, bool isCommand);
    void updateFlipperInfo(const char* deviceName, int rssi);

    /* Settings */
    void updateSettingsValues();

private:
    GatewayClient*   _gw;
    ConfigManager*   _cfg;
    FlipperBLE*      _flipper;
    SupabaseBridge*  _bridge;
    ScreenId         _currentScreen;

    /* Sleep State */
    uint32_t         _lastInteraction;
    bool             _isSleeping;
    uint8_t          _targetBacklight;
    uint8_t          _currentBacklight;

    /* Notification Overlay */
    lv_obj_t*        _notifOverlay;
    lv_obj_t*        _lblNotifTitle;
    lv_obj_t*        _lblNotifBody;
    lv_anim_t        _notifAnimIn;
    lv_anim_t        _notifAnimOut;
    uint32_t         _notifDismissTime;

    /* ── Screen objects ───────────────────────────────── */

    /* Watch Face */
    lv_obj_t* _scrWatchface;
    lv_obj_t* _lblTime;
    lv_obj_t* _lblDate;
    lv_obj_t* _lblDay;
    lv_obj_t* _arcBattery;
    lv_obj_t* _lblBatteryPct;
    lv_obj_t* _ledGateway;
    lv_obj_t* _ledFlipper;
    lv_obj_t* _ledBridge;
    lv_obj_t* _imgWifi;
    lv_obj_t* _lblAmPm;

    /* Chat Screen */
    lv_obj_t* _scrChat;
    lv_obj_t* _chatList;
    lv_obj_t* _chatHeader;
    lv_obj_t* _lblChatTitle;
    lv_obj_t* _spinnerTyping;
    lv_obj_t* _btnQuickReply[QUICK_REPLY_COUNT];
    lv_obj_t* _panelQuickReplies;

    /* Flipper Control Screen */
    lv_obj_t* _scrFlipper;
    lv_obj_t* _flipperLogList;
    lv_obj_t* _lblFlipperDevice;
    lv_obj_t* _lblFlipperRssi;
    lv_obj_t* _lblFlipperState;
    lv_obj_t* _btnFlipperScan;
    lv_obj_t* _btnFlipperDisconnect;
    int       _flipperLogCount;

    /* Quick Settings (swipe down) */
    lv_obj_t* _scrQuickSettings;
    lv_obj_t* _sliderBrightness;
    lv_obj_t* _lblBrightnessVal;
    lv_obj_t* _btnWifi;
    lv_obj_t* _btnSettings;

    /* Settings */
    lv_obj_t* _scrSettings;
    lv_obj_t* _settingsList;

    /* WiFi Setup */
    lv_obj_t* _scrWifiSetup;

    /* Gateway Setup */
    lv_obj_t* _scrGatewaySetup;
    lv_obj_t* _lblGwStatus;

    /* Supabase Setup */
    lv_obj_t* _scrSupabaseSetup;
    lv_obj_t* _lblSbStatus;

    /* Flipper Setup */
    lv_obj_t* _scrFlipperSetup;
    lv_obj_t* _lblFlipSetupStatus;

    /* Sessions */
    lv_obj_t* _scrSessions;
    lv_obj_t* _sessionsList;

    /* ── Builders ─────────────────────────────────────── */
    void createStyles();
    void buildWatchface();
    void buildChatScreen();
    void buildFlipperScreen();
    void buildQuickSettings();
    void buildSettingsScreen();
    void buildWifiSetup();
    void buildGatewaySetup();
    void buildSupabaseSetup();
    void buildFlipperSetup();
    void buildSessionsScreen();

    /* ── Helpers ──────────────────────────────────────── */
    lv_obj_t* createRoundScreen();
    void addChatBubble(const char* text, bool isUser);
    void addFlipperLogEntry(const char* text, bool isCommand);

    /* ── Static callbacks ─────────────────────────────── */
    static void onQuickReplyClicked(lv_event_t* e);
    static void onBrightnessChanged(lv_event_t* e);
    static void onSettingsItemClicked(lv_event_t* e);
    static void onGatewaySave(lv_event_t* e);
    static void onBackButton(lv_event_t* e);
    static void onSessionSelected(lv_event_t* e);
    static void onWifiSelected(lv_event_t* e);
    static void onGestureEvent(lv_event_t* e);
    static void onFlipperScan(lv_event_t* e);
    static void onFlipperDisconnect(lv_event_t* e);
    static void onScreenInteracted(lv_event_t* e);

    /* ── Styles ───────────────────────────────────────── */
    lv_style_t _styleBg;
    lv_style_t _styleCard;
    lv_style_t _styleBubbleUser;
    lv_style_t _styleBubbleAgent;
    lv_style_t _styleBtn;
    lv_style_t _styleBtnPressed;
    lv_style_t _styleHeader;
    lv_style_t _styleAccent;
    lv_style_t _styleCmdLog;
    lv_style_t _styleResultLog;

    /* Chat message count */
    int _chatMsgCount;
};

#endif /* UI_MANAGER_H */
