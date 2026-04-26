/**
 * ui_manager.h
 * UI Manager for M5StickC Plus2
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * Manages all LVGL screens adapted for the 135x240 portrait display.
 * Navigation is entirely button-driven (no touch screen).
 *
 * Navigation:
 *   Button A short  → Cycle to next main screen
 *   Button A long   → Back / Cancel
 *   Button B short  → Scroll down / Highlight next item
 *   Button B long   → Select / Confirm / Primary action
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
#include "m5_driver.h"

/* ── Quick reply labels ───────────────────────────────── */
static const char* QUICK_REPLIES[QUICK_REPLY_COUNT] = {
    "Status?",
    "Stop",
    "Continue",
    "Summary"
};

class UIManager {
public:
    UIManager() = default;

    /* Lifecycle */
    void begin(GatewayClient* gw, ConfigManager* cfg,
               FlipperBLE* flipper, SupabaseBridge* bridge);

    /* Must be called from loop() with the latest button event */
    void handleButton(BtnEvent evt);

    /* Sleep & Wake */
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

    /* Notification Overlay */
    lv_obj_t*        _notifOverlay;
    lv_obj_t*        _lblNotifTitle;
    lv_obj_t*        _lblNotifBody;
    uint32_t         _notifDismissTime;

    /* ── Screen objects ───────────────────────────────── */
    /* Watch Face */
    lv_obj_t* _scrWatchface;
    lv_obj_t* _lblTime;
    lv_obj_t* _lblDate;
    lv_obj_t* _lblDay;
    lv_obj_t* _lblBatteryPct;
    lv_obj_t* _lblBatteryIcon;
    lv_obj_t* _lblWifiIcon;
    lv_obj_t* _lblGwDot;
    lv_obj_t* _lblFlipDot;
    lv_obj_t* _lblBridgeDot;
    lv_obj_t* _lblScreenHint;

    /* Chat Screen */
    lv_obj_t* _scrChat;
    lv_obj_t* _chatList;
    lv_obj_t* _lblChatTitle;
    lv_obj_t* _lblTyping;
    int        _chatMsgCount;
    int        _quickReplyIdx;  /* Currently highlighted quick reply */

    /* Quick Reply Overlay (shown on Button B long press in chat) */
    lv_obj_t* _quickReplyOverlay;
    lv_obj_t* _btnQuickReply[QUICK_REPLY_COUNT];
    bool       _quickReplyVisible;

    /* Flipper Control Screen */
    lv_obj_t* _scrFlipper;
    lv_obj_t* _flipperLogList;
    lv_obj_t* _lblFlipperDevice;
    lv_obj_t* _lblFlipperState;
    int        _flipperLogCount;

    /* Settings Screen */
    lv_obj_t* _scrSettings;
    lv_obj_t* _settingsList;
    int        _settingsIdx;    /* Currently highlighted settings item */

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

    /* ── Screen builders ──────────────────────────────── */
    void createStyles();
    void buildWatchface();
    void buildChatScreen();
    void buildFlipperScreen();
    void buildSettingsScreen();
    void buildWifiSetup();
    void buildGatewaySetup();
    void buildSupabaseSetup();
    void buildFlipperSetup();
    void buildNotificationOverlay();
    void buildQuickReplyOverlay();

    /* ── Navigation helpers ───────────────────────────── */
    void cycleNextScreen();
    void handleChatButton(BtnEvent evt);
    void handleFlipperButton(BtnEvent evt);
    void handleSettingsButton(BtnEvent evt);
    void showQuickReplies();
    void hideQuickReplies();
    void sendSelectedQuickReply();

    /* ── Helpers ──────────────────────────────────────── */
    void addChatBubble(const char* text, bool isUser);
    void addFlipperLogEntry(const char* text, bool isCommand);

    /* ── Styles ───────────────────────────────────────── */
    lv_style_t _styleBg;
    lv_style_t _styleCard;
    lv_style_t _styleBubbleUser;
    lv_style_t _styleBubbleAgent;
    lv_style_t _styleBtn;
    lv_style_t _styleBtnSelected;
    lv_style_t _styleHeader;
    lv_style_t _styleSmallText;
    lv_style_t _styleCmdLog;
    lv_style_t _styleResultLog;
};

#endif /* UI_MANAGER_H */
