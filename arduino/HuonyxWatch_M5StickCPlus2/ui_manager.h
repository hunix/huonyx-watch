#ifndef UI_MANAGER_H
#define UI_MANAGER_H
/*  ═══════════════════════════════════════════════════════
 *  UIManager – M5.Display (LovyanGFX) direct rendering
 *  No LVGL dependency — draws directly to the 135x240 LCD
 *
 *  Navigation (button-driven, no touch):
 *    Button A short  -> Cycle to next main screen
 *    Button A long   -> Back / Cancel
 *    Button B short  -> Scroll / Highlight next item
 *    Button B long   -> Select / Confirm / Primary action
 *  ═══════════════════════════════════════════════════════ */
#include <M5Unified.h>
#include "hw_config.h"
#include "gateway_client.h"
#include "config_manager.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "m5_driver.h"

/* ── Color palette (RGB565) ──────────────────────────── */
#define CLR_BG        0x0000
#define CLR_CARD      0x18E3
#define CLR_HEADER    0x1082
#define CLR_TEXT      0xFFFF
#define CLR_SUBTEXT   0x7BEF
#define CLR_ACCENT    0x04FF
#define CLR_SUCCESS   0x07E0
#define CLR_WARNING   0xFD20
#define CLR_ERROR     0xF800
#define CLR_USER_BUB  0x1A3A
#define CLR_AGENT_BUB 0x2104

/* ── Quick reply labels ──────────────────────────────── */
static const char* QUICK_REPLIES[QUICK_REPLY_COUNT] = {
    "Status?",
    "Stop",
    "Continue",
    "Summary"
};

/* ── Chat message storage ────────────────────────────── */
struct ChatMsg {
    char text[CHAT_MAX_MSG_LEN];
    bool isUser;
};

/* ── Flipper log entry ───────────────────────────────── */
struct FlipperLogEntry {
    char text[128];
    bool isCommand;
};

#define MAX_FLIPPER_LOG 12

class UIManager {
public:
    UIManager() = default;

    void begin(GatewayClient* gw, ConfigManager* cfg,
               FlipperBLE* flipper, SupabaseBridge* bridge);
    void handleButton(BtnEvent evt);
    void tick();

    void resetSleepTimer();
    bool isSleeping() const { return _isSleeping; }
    void wakeDisplay();
    void sleepDisplay();

    void showNotification(const char* title, const char* msg);
    void showScreen(ScreenId id);
    ScreenId currentScreen() const { return _currentScreen; }

    void updateTime(int hour, int min, int sec);
    void updateDate(const char* dateStr);
    void updateBattery(int percent, bool charging);
    void updateWiFiStrength(int rssi);
    void updateGatewayStatus(GatewayState state);
    void updateFlipperStatus(FlipperState state);
    void updateBridgeStatus(BridgeState state);

    void addChatMessage(const char* text, bool isUser);
    void updateAgentTyping(bool typing);
    void clearChat();

    void addFlipperLog(const char* text, bool isCommand);
    void updateFlipperInfo(const char* deviceName, int rssi);
    void updateSettingsValues();

private:
    GatewayClient*   _gw      = nullptr;
    ConfigManager*   _cfg     = nullptr;
    FlipperBLE*      _flipper = nullptr;
    SupabaseBridge*  _bridge  = nullptr;

    ScreenId _currentScreen = SCREEN_WATCHFACE;
    bool     _dirty         = true;

    uint32_t _lastInteraction = 0;
    bool     _isSleeping      = false;
    uint8_t  _targetBacklight = 80;

    char     _notifTitle[32]   = {};
    char     _notifBody[64]    = {};
    uint32_t _notifDismissTime = 0;

    /* Watch face state */
    int  _hour = -1, _min = -1, _sec = -1;
    char _dateStr[24]  = "---";
    int  _battPct      = -1;
    bool _charging     = false;
    int  _wifiRssi     = 0;
    GatewayState  _gwState   = GW_DISCONNECTED;
    FlipperState  _flipState = FLIP_IDLE;
    BridgeState   _brState   = BRIDGE_IDLE;

    /* Chat state */
    ChatMsg  _chatMsgs[CHAT_MAX_MESSAGES];
    int      _chatCount         = 0;
    bool     _agentTyping       = false;
    int      _chatScrollPos     = 0;
    int      _quickReplyIdx     = 0;
    bool     _quickReplyVisible = false;

    /* Flipper state */
    FlipperLogEntry _flipLog[MAX_FLIPPER_LOG];
    int      _flipLogCount   = 0;
    char     _flipDevName[32] = "---";
    int      _flipRssi       = 0;

    /* Settings state */
    int      _settingsIdx    = 0;

    /* Drawing methods */
    void drawWatchface();
    void drawChatScreen();
    void drawFlipperScreen();
    void drawSettingsScreen();
    void drawWifiSetup();
    void drawGatewaySetup();
    void drawSupabaseSetup();
    void drawFlipperSetup();
    void drawNotification();

    /* Navigation helpers */
    void cycleNextScreen();
    void handleChatButton(BtnEvent evt);
    void handleFlipperButton(BtnEvent evt);
    void handleSettingsButton(BtnEvent evt);
    void sendSelectedQuickReply();

    /* Drawing helpers */
    void drawHeader(const char* title, uint16_t color = CLR_ACCENT);
    void drawHintBar(const char* text);
    void drawStatusDot(int x, int y, uint16_t color);
};

#endif /* UI_MANAGER_H */
