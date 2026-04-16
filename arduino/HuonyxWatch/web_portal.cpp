/**
 * Web Configuration Portal Implementation
 * Huonyx AI Smartwatch v2.0
 */

#include "web_portal.h"
#include <ArduinoJson.h>

WebPortal::WebPortal(ConfigManager* cfg)
    : _server(80)
    , _cfg(cfg)
    , _running(false)
    , _apMode(false)
{
}

void WebPortal::begin() {
    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/setup", HTTP_GET, [this]() { handleSetup(); });
    _server.on("/save", HTTP_POST, [this]() { handleSave(); });
    _server.on("/status", HTTP_GET, [this]() { handleStatus(); });
    _server.onNotFound([this]() { handleNotFound(); });
    _server.begin();
    _running = true;
    Serial.println("[WEB] Portal started on port 80");
}

void WebPortal::loop() {
    if (_running) {
        _server.handleClient();
    }
}

void WebPortal::stop() {
    _server.stop();
    _running = false;
}

void WebPortal::startAP(const char* ssid, const char* pass) {
    WiFi.softAP(ssid, pass);
    _apMode = true;
    Serial.printf("[WEB] AP started: %s, IP: %s\n", ssid, WiFi.softAPIP().toString().c_str());
    begin();
}

void WebPortal::stopAP() {
    WiFi.softAPdisconnect(true);
    _apMode = false;
}

void WebPortal::startSTA() {
    _apMode = false;
    begin();
}

/* ── HTML Generation ──────────────────────────────────── */

String WebPortal::generateHTML() {
    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Huonyx Watch Setup</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
background:#0D0D0F;color:#E8E8F0;min-height:100vh;display:flex;
flex-direction:column;align-items:center;padding:20px}
.logo{font-size:24px;font-weight:700;color:#00D4FF;letter-spacing:4px;margin:20px 0}
.subtitle{color:#6B6B8D;font-size:14px;margin-bottom:30px}
.card{background:#16213E;border-radius:16px;padding:24px;width:100%;max-width:400px;margin-bottom:16px;
border:1px solid #2A2A4E;transition:border-color 0.3s}
.card:hover{border-color:#00D4FF30}
.card h2{color:#00D4FF;font-size:16px;margin-bottom:16px;display:flex;align-items:center;gap:8px}
.card h2.orange{color:#FF8C00}
.card h2.purple{color:#7B2FFF}
.field{margin-bottom:16px}
.field label{display:block;color:#6B6B8D;font-size:12px;margin-bottom:6px;text-transform:uppercase;letter-spacing:1px}
.field input,.field select{width:100%;padding:12px;background:#1A1A2E;border:1px solid #2A2A4E;
border-radius:10px;color:#E8E8F0;font-size:14px;outline:none;transition:border-color 0.3s}
.field input:focus{border-color:#00D4FF}
.field input::placeholder{color:#4A4A6A}
.field .hint{color:#4A4A6A;font-size:11px;margin-top:4px}
.btn{width:100%;padding:14px;background:linear-gradient(135deg,#00D4FF,#7B2FFF);
border:none;border-radius:12px;color:#fff;font-size:16px;font-weight:600;
cursor:pointer;transition:opacity 0.3s}
.btn:hover{opacity:0.9}
.btn:active{transform:scale(0.98)}
.status{display:flex;justify-content:space-between;align-items:center;padding:8px 0}
.status .dot{width:8px;height:8px;border-radius:50%;display:inline-block}
.dot.green{background:#00FF88}
.dot.red{background:#FF3366}
.dot.yellow{background:#FF6B35}
.dot.orange{background:#FF8C00}
.dot.purple{background:#7B2FFF}
.info{color:#6B6B8D;font-size:12px;text-align:center;margin-top:20px}
.toggle{display:flex;align-items:center;gap:10px;margin-bottom:12px}
.toggle input[type=checkbox]{width:20px;height:20px;accent-color:#00D4FF}
.section-divider{width:100%;max-width:400px;border:none;border-top:1px solid #2A2A4E;margin:8px 0}
.badge{display:inline-block;padding:2px 8px;border-radius:8px;font-size:10px;font-weight:600;
text-transform:uppercase;letter-spacing:1px;margin-left:8px}
.badge.optional{background:#2A2A4E;color:#6B6B8D}
.badge.required{background:#FF336630;color:#FF3366}
</style>
</head>
<body>
<div class="logo">HUONYX</div>
<div class="subtitle">AI Smartwatch Configuration Portal</div>
)rawliteral";
    return html;
}

static String escapeHTML(const char* input) {
    String out;
    out.reserve(strlen(input));
    while (*input) {
        if (*input == '&') out += "&amp;";
        else if (*input == '<') out += "&lt;";
        else if (*input == '>') out += "&gt;";
        else if (*input == '"') out += "&quot;";
        else if (*input == '\'') out += "&#39;";
        else out += *input;
        input++;
    }
    return out;
}


/* ── Request Handlers ─────────────────────────────────── */

void WebPortal::handleRoot() {
    _server.sendHeader("Location", "/setup", true);
    _server.send(302, "text/plain", "");
}

void WebPortal::handleSetup() {
    /* Use chunked transfer to avoid building one massive String in heap */
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "text/html", "");

    /* Send static header + CSS (raw literal already in flash) */
    _server.sendContent(generateHTML());

    WatchConfig& c = _cfg->config();
    char buf[300];  /* Reusable small buffer for dynamic parts */

    _server.sendContent(F("<form method='POST' action='/save'>"));

    /* WiFi Card */
    _server.sendContent(F("<div class='card'><h2>&#x1F4F6; WiFi Network <span class='badge required'>Required</span></h2>"));
    _server.sendContent(F("<div class='field'><label>SSID</label>"));
    snprintf(buf, sizeof(buf), "<input type='text' name='ssid' placeholder='Network name' value='%s'></div>", escapeHTML(c.wifiSSID).c_str());
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='field'><label>Password</label><input type='password' name='wifi_pass' placeholder='WiFi password' value='%s'></div></div>", escapeHTML(c.wifiPass).c_str());
    _server.sendContent(buf);

    /* Gateway Card */
    _server.sendContent(F("<div class='card'><h2>&#x1F916; HoC Gateway <span class='badge required'>Required</span></h2>"));
    snprintf(buf, sizeof(buf), "<div class='field'><label>Host / IP</label><input type='text' name='gw_host' placeholder='192.168.1.100' value='%s'><div class='hint'>Your Huonyx/OpenClaw gateway host</div></div>", escapeHTML(c.gwHost).c_str());
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='field'><label>Port</label><input type='number' name='gw_port' placeholder='18789' value='%u'></div>", c.gwPort);
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='field'><label>Gateway Token</label><input type='password' name='gw_token' placeholder='OPENCLAW_GATEWAY_TOKEN' value='%s'><div class='hint'>Token to authenticate with the gateway</div></div>", escapeHTML(c.gwToken).c_str());
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='toggle'><input type='checkbox' name='gw_ssl' id='ssl'%s><label for='ssl' style='color:#E8E8F0;font-size:14px'>Use SSL (wss://)</label></div></div>", c.gwUseSSL ? " checked" : "");
    _server.sendContent(buf);

    _server.sendContent(F("<hr class='section-divider'>"));

    /* Supabase Card */
    _server.sendContent(F("<div class='card'><h2 class='purple'>&#x1F504; Supabase Realtime Bridge <span class='badge optional'>Optional</span></h2>"));
    snprintf(buf, sizeof(buf), "<div class='field'><label>Project URL</label><input type='text' name='sb_url' placeholder='abcdefg.supabase.co' value='%s'><div class='hint'>Supabase project URL (without https://)</div></div>", escapeHTML(c.sbUrl).c_str());
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='field'><label>API Key (anon)</label><input type='password' name='sb_key' placeholder='eyJhbGciOi...' value='%s'><div class='hint'>Supabase anon key for Realtime channel access</div></div>", escapeHTML(c.sbKey).c_str());
    _server.sendContent(buf);
    _server.sendContent(F("<div style='color:#6B6B8D;font-size:12px;padding:8px;background:#1A1A2E;border-radius:8px'>&#x1F4A1; Agent &#x2192; Supabase &#x2192; Watch &#x2192; Flipper</div></div>"));

    /* Flipper Card */
    _server.sendContent(F("<div class='card'><h2 class='orange'>&#x1F42C; Flipper Zero BLE <span class='badge optional'>Optional</span></h2>"));
    snprintf(buf, sizeof(buf), "<div class='field'><label>Device Name</label><input type='text' name='flip_name' placeholder='Flipper (leave empty for any)' value='%s'><div class='hint'>Specific device name or empty for any</div></div>", escapeHTML(c.flipperName).c_str());
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='toggle'><input type='checkbox' name='flip_auto' id='flipauto'%s><label for='flipauto' style='color:#E8E8F0;font-size:14px'>Auto-connect on boot</label></div>", c.flipperAuto ? " checked" : "");
    _server.sendContent(buf);
    _server.sendContent(F("<div style='color:#6B6B8D;font-size:12px;padding:8px;background:#1A1A2E;border-radius:8px'>&#x1F4A1; BLE Serial to Flipper Zero CLI (~10m range)</div></div>"));

    _server.sendContent(F("<hr class='section-divider'>"));

    /* Display Card */
    _server.sendContent(F("<div class='card'><h2>&#x1F4A1; Display</h2>"));
    snprintf(buf, sizeof(buf), "<div class='field'><label>Brightness (10-255)</label><input type='range' name='brightness' min='10' max='255' value='%u' style='accent-color:#00D4FF'></div>", c.brightness);
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='field'><label>Timezone (UTC offset)</label><input type='number' name='timezone' min='-12' max='14' value='%d'></div></div>", c.timezone);
    _server.sendContent(buf);

    _server.sendContent(F("<button type='submit' class='btn'>&#x1F680; Save & Restart</button></form>"));
    snprintf(buf, sizeof(buf), "<div class='info'>Huonyx Watch v" FIRMWARE_VERSION " | ESP32-C3 | %lu bytes free</div></body></html>", (unsigned long)ESP.getFreeHeap());
    _server.sendContent(buf);
    _server.sendContent("");
}

void WebPortal::handleSave() {
    /* WiFi */
    if (_server.hasArg("ssid")) {
        _cfg->setWiFi(_server.arg("ssid").c_str(), _server.arg("wifi_pass").c_str());
    }

    /* Gateway */
    if (_server.hasArg("gw_host")) {
        _cfg->setGateway(
            _server.arg("gw_host").c_str(),
            _server.arg("gw_port").toInt(),
            _server.arg("gw_token").c_str(),
            _server.hasArg("gw_ssl")
        );
    }

    /* Supabase */
    if (_server.hasArg("sb_url") || _server.hasArg("sb_key")) {
        _cfg->setSupabase(_server.arg("sb_url").c_str(), _server.arg("sb_key").c_str());
    }

    /* Flipper */
    if (_server.hasArg("flip_name") || _server.hasArg("flip_auto")) {
        _cfg->setFlipper(_server.arg("flip_name").c_str(), _server.hasArg("flip_auto"));
    }

    /* Display */
    if (_server.hasArg("brightness")) {
        _cfg->setBrightness(_server.arg("brightness").toInt());
    }
    if (_server.hasArg("timezone")) {
        _cfg->setTimezone(_server.arg("timezone").toInt());
    }

    /* Send response using chunked transfer */
    _server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    _server.send(200, "text/html", "");
    _server.sendContent(generateHTML());
    _server.sendContent(F("<div class='card' style='text-align:center'>"));
    _server.sendContent(F("<h2 style='color:#00FF88'>&#x2705; Settings Saved!</h2>"));
    _server.sendContent(F("<p style='color:#E8E8F0;margin:16px 0'>The watch will restart in 3 seconds...</p><div style='margin-top:12px'>"));

    char buf[128];
    snprintf(buf, sizeof(buf), "<div class='status'><span>WiFi</span><span class='dot %s'></span></div>",
             strlen(_cfg->config().wifiSSID) > 0 ? "green" : "red");
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='status'><span>Gateway</span><span class='dot %s'></span></div>",
             _cfg->hasGatewayConfig() ? "green" : "red");
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='status'><span>Supabase</span><span class='dot %s'></span></div>",
             _cfg->hasSupabaseConfig() ? "purple" : "yellow");
    _server.sendContent(buf);
    snprintf(buf, sizeof(buf), "<div class='status'><span>Flipper</span><span class='dot %s'></span></div>",
             _cfg->config().flipperAuto ? "orange" : "yellow");
    _server.sendContent(buf);
    _server.sendContent(F("</div></div><script>setTimeout(()=>window.location='/setup',5000)</script></body></html>"));
    _server.sendContent("");

    delay(2000);
    ESP.restart();
}

void WebPortal::handleStatus() {
    /* Use ArduinoJson onto a stack buffer instead of direct snprintf */
    /* Mitigates JSON injection vulnerabilities from unescaped wifi_pass, wifi_ssid etc */
    JsonDocument doc;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["wifi_connected"] = WiFi.isConnected();
    doc["wifi_ssid"] = _cfg->config().wifiSSID;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["gateway_host"] = _cfg->config().gwHost;
    doc["gateway_port"] = _cfg->config().gwPort;
    doc["supabase_url"] = _cfg->config().sbUrl;
    doc["supabase_configured"] = _cfg->hasSupabaseConfig();
    doc["flipper_name"] = _cfg->config().flipperName;
    doc["flipper_auto"] = _cfg->config().flipperAuto;
    doc["free_heap"] = ESP.getFreeHeap();

    char jsonBuffer[512];
    serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));
    _server.send(200, "application/json", jsonBuffer);
}

void WebPortal::handleNotFound() {
    _server.sendHeader("Location", "/setup", true);
    _server.send(302, "text/plain", "");
}


