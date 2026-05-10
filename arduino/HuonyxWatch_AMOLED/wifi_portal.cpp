/**
 * Huonyx Watch – WiFi Captive Portal Implementation
 */
#include "wifi_portal.h"

WiFiPortal* WiFiPortal::_instance = nullptr;

WiFiPortal::WiFiPortal()
    : _server(80)
    , _active(false)
    , _onSave(nullptr)
{
    strcpy(_apIp, "192.168.4.1");
    _apSsid[0] = '\0';
    _instance = this;
}

void WiFiPortal::start(const char* apSsid, const char* apPass) {
    if (_active) return;

    strncpy(_apSsid, apSsid, sizeof(_apSsid) - 1);
    _apSsid[sizeof(_apSsid) - 1] = '\0';

    Serial.printf("[PORTAL] Starting AP: %s\n", _apSsid);

    /* AP+STA so we can keep scanning for real networks while the portal runs. */
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAPConfig(IPAddress(192, 168, 4, 1),
                      IPAddress(192, 168, 4, 1),
                      IPAddress(255, 255, 255, 0));

    bool ok;
    if (apPass && strlen(apPass) >= 8) {
        ok = WiFi.softAP(_apSsid, apPass);
    } else {
        ok = WiFi.softAP(_apSsid);
    }
    if (!ok) {
        Serial.println("[PORTAL] softAP failed");
        return;
    }

    delay(100);
    IPAddress ip = WiFi.softAPIP();
    snprintf(_apIp, sizeof(_apIp), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    Serial.printf("[PORTAL] AP IP: %s\n", _apIp);

    /* DNS catch-all so iOS/Android trigger captive-portal detection. */
    _dns.start(53, "*", ip);

    _server.on("/", handleRoot);
    _server.on("/scan", handleScan);
    _server.on("/save", HTTP_POST, handleSave);
    _server.onNotFound(handleNotFound);
    _server.begin();

    _active = true;
}

void WiFiPortal::stop() {
    if (!_active) return;
    Serial.println("[PORTAL] Stopping");
    _server.stop();
    _dns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _active = false;
}

void WiFiPortal::loop() {
    if (!_active) return;
    _dns.processNextRequest();
    _server.handleClient();
}

/* ── Static handlers (forward to instance state) ──────── */

void WiFiPortal::handleRoot() {
    if (!_instance) return;
    _instance->_server.send(200, "text/html", _instance->buildIndexHtml());
}

void WiFiPortal::handleScan() {
    if (!_instance) return;
    int n = WiFi.scanNetworks(false, false);
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"";
        json += _instance->escapeHtml(WiFi.SSID(i));
        json += "\",\"rssi\":";
        json += String(WiFi.RSSI(i));
        json += ",\"enc\":";
        json += String((int)WiFi.encryptionType(i));
        json += "}";
    }
    json += "]";
    WiFi.scanDelete();
    _instance->_server.send(200, "application/json", json);
}

void WiFiPortal::handleSave() {
    if (!_instance) return;
    String ssid = _instance->_server.arg("ssid");
    String pass = _instance->_server.arg("pass");

    if (ssid.length() == 0) {
        _instance->_server.send(400, "text/plain", "SSID required");
        return;
    }

    String html =
        "<!doctype html><html><head>"
        "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
        "<style>body{font-family:-apple-system,sans-serif;background:#000;color:#fff;"
        "text-align:center;padding:40px;font-size:18px}"
        "h1{color:#0ff;font-weight:300}b{color:#0ff}</style></head><body>"
        "<h1>Saved</h1><p>Watch is connecting to <b>";
    html += _instance->escapeHtml(ssid);
    html += "</b>.</p><p style=\"color:#888;font-size:14px\">"
            "You can disconnect from <i>";
    html += _instance->_apSsid;
    html += "</i> now.</p></body></html>";
    _instance->_server.send(200, "text/html", html);

    if (_instance->_onSave) {
        _instance->_onSave(ssid.c_str(), pass.c_str());
    }
}

void WiFiPortal::handleNotFound() {
    if (!_instance) return;
    _instance->_server.sendHeader("Location",
        String("http://") + _instance->_apIp + "/", true);
    _instance->_server.send(302, "text/plain", "");
}

/* ── HTML ──────────────────────────────────────────────── */

String WiFiPortal::buildIndexHtml() {
    String html = F(
"<!doctype html><html><head>"
"<meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Huonyx Watch Setup</title>"
"<style>"
"*{box-sizing:border-box}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:#000;color:#fff;margin:0;padding:24px;max-width:480px;margin:auto;font-size:16px}"
"h1{color:#0ff;font-weight:300;margin:0 0 4px;font-size:28px;letter-spacing:1px}"
".sub{color:#888;font-size:13px;margin-bottom:24px;letter-spacing:.5px}"
"label{display:block;color:#aaa;font-size:13px;margin:18px 0 6px}"
"input,select{width:100%;padding:14px;background:#111;border:1px solid #333;"
"color:#fff;border-radius:10px;font-size:16px;-webkit-appearance:none}"
"input:focus,select:focus{outline:none;border-color:#0ff}"
"button{width:100%;padding:14px;background:#0ff;color:#000;border:none;"
"border-radius:10px;font-size:16px;font-weight:600;margin-top:24px;cursor:pointer}"
"button:disabled{background:#333;color:#666}"
".row{display:flex;gap:8px}"
".row select{flex:1}"
".row .scan-btn{flex:0 0 auto;width:auto;padding:14px 18px;background:#222;"
"color:#0ff;margin-top:0;font-size:14px}"
"</style></head><body>"
"<h1>Huonyx Watch</h1><div class=\"sub\">WiFi setup</div>"
"<form id=f onsubmit=\"return save(event)\">"
"<label>Network</label>"
"<div class=row>"
"<select id=ssid name=ssid><option value=\"\">Scanning...</option></select>"
"<button type=button class=scan-btn onclick=\"scan()\">Scan</button>"
"</div>"
"<label>Password</label>"
"<input type=password id=pass name=pass placeholder=\"Leave blank for open networks\" autocomplete=off>"
"<button type=submit id=ok>Save &amp; Connect</button>"
"</form>"
"<script>"
"async function scan(){"
"const s=document.getElementById('ssid');"
"s.innerHTML='<option>Scanning...</option>';"
"try{const r=await fetch('/scan');const n=await r.json();"
"n.sort((a,b)=>b.rssi-a.rssi);s.innerHTML='';"
"if(!n.length){s.innerHTML='<option value=\"\">(none found)</option>';return}"
"n.forEach(x=>{const o=document.createElement('option');o.value=x.ssid;"
"o.textContent=x.ssid+(x.enc==0?'  (open)':'')+'  '+x.rssi+'dBm';s.appendChild(o)})"
"}catch(e){s.innerHTML='<option value=\"\">(scan failed)</option>'}}"
"async function save(e){e.preventDefault();const b=document.getElementById('ok');"
"b.disabled=true;b.textContent='Saving...';"
"const fd=new FormData(document.getElementById('f'));"
"try{const r=await fetch('/save',{method:'POST',body:new URLSearchParams(fd)});"
"document.body.innerHTML=await r.text()"
"}catch(err){b.disabled=false;b.textContent='Save & Connect';alert('Save failed')}"
"return false}"
"scan();"
"</script>"
"</body></html>"
    );
    return html;
}

String WiFiPortal::escapeHtml(const String& s) {
    String r;
    r.reserve(s.length());
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if      (c == '<')  r += "&lt;";
        else if (c == '>')  r += "&gt;";
        else if (c == '&')  r += "&amp;";
        else if (c == '"')  r += "&quot;";
        else if (c == '\'') r += "&#39;";
        else r += c;
    }
    return r;
}
