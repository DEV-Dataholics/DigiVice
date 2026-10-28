#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "cyd_display.h"
#include "audio_synth.h"

// ============================================================================
// Dataholics DigiPet - Mobile Captive Portal & Wi-Fi Network Manager
// Provides non-volatile flash storage (NVS) for credentials, auto-scanning,
// and a zero-install mobile setup portal accessible from any phone or laptop.
// ============================================================================

class DigiPetWiFiManager {
private:
  Preferences _prefs;
  WebServer _server;
  DNSServer _dnsServer;
  
  String _ssid;
  String _pass;
  bool _portalActive;
  bool _credentialsSaved;
  String _savedSsid;
  String _savedPass;

  const char* AP_SSID = "Dataholics-DigiPet";
  const byte DNS_PORT = 53;

public:
  DigiPetWiFiManager() : _server(80), _portalActive(false), _credentialsSaved(false) {}

  void begin() {
    _prefs.begin("digipet_wifi", false);
    _ssid = _prefs.getString("ssid", "Monkeyskull2");
    _pass = _prefs.getString("pass", "6561766707");
  }

  String getSSID() const { return _ssid; }
  String getPass() const { return _pass; }

  void saveCredentials(const String& newSsid, const String& newPass) {
    _ssid = newSsid;
    _pass = newPass;
    _prefs.putString("ssid", _ssid);
    _prefs.putString("pass", _pass);
  }

  void resetToDefaults() {
    saveCredentials("Monkeyskull2", "6561766707");
  }

  // Attempt connection with visual feedback
  bool connectStored(LGFX* gfx, void (*ledCallback)(uint8_t, uint8_t, uint8_t), int timeoutSec = 10) {
    if (_ssid.length() == 0) return false;

    Serial.printf("[WIFI] Connecting to stored SSID: '%s'...\n", _ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _pass.c_str());

    unsigned long startMs = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < (timeoutSec * 1000)) {
      delay(250);
      dots++;
      if (ledCallback) {
        ledCallback((dots % 2) ? 0 : 255, (dots % 2) ? 255 : 0, 180);
      }
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("[WIFI] Uplink OK! IP: %s | RSSI: %d dBm\n", 
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (ledCallback) ledCallback(0, 255, 0); // Green
      return true;
    }

    Serial.println("[WIFI] Connection timeout.");
    if (ledCallback) ledCallback(255, 0, 0); // Red
    return false;
  }

  // Render on-screen setup instructions
  void drawPortalScreen(LGFX* gfx, const String& apName, const String& ipStr) {
    gfx->startWrite();
    gfx->fillScreen(0x0821); // Dark bunker background

    // Outer border
    gfx->drawRect(4, 4, 312, 232, 0x07E0); // Acid green border
    gfx->drawRect(6, 6, 308, 228, 0x18E3);

    // Header bar
    gfx->fillRect(8, 8, 304, 26, 0x10A2);
    gfx->drawFastHLine(8, 34, 304, 0x07E0);
    gfx->setTextColor(0x07E0);
    gfx->setTextSize(1);
    gfx->drawString("DATAHOLICS // WI-FI NETWORK MANAGER", 16, 16);

    // AP Badge Card
    gfx->fillRect(14, 42, 292, 48, 0x18C3);
    gfx->drawRect(14, 42, 292, 48, 0x041F);
    gfx->setTextColor(0xFFFF);
    gfx->drawString("BROADCASTING CONFIG PORTAL (AP MODE)", 22, 50);

    gfx->setTextColor(0x07E0);
    gfx->drawString(String("WI-FI NAME: ") + apName, 22, 64);
    gfx->setTextColor(0x07FF);
    gfx->drawString(String("WEB PORTAL: http://") + ipStr, 22, 76);

    // Steps box
    gfx->fillRect(14, 96, 292, 92, 0x0841);
    gfx->drawRect(14, 96, 292, 92, 0x3186);

    gfx->setTextColor(0xFD20); // Amber
    gfx->drawString("ACTIONABLE 1-2-3 CONNECTION STEPS:", 22, 104);

    gfx->setTextColor(0xFFFF);
    gfx->drawString("1. On phone/laptop, connect to Wi-Fi above.", 22, 120);
    gfx->drawString("2. Open browser or wait for captive portal.", 22, 136);
    gfx->drawString("3. Select your local Wi-Fi, enter password & Save.", 22, 152);
    gfx->setTextColor(0x8410);
    gfx->drawString("Scrap-Hound will save credentials & reboot instantly.", 22, 168);

    // Cancel Button Target
    gfx->fillRect(14, 194, 292, 34, 0x4800);
    gfx->drawRect(14, 194, 292, 34, 0xF800);
    gfx->setTextColor(0xFFFF);
    gfx->drawString("[ TAP SCREEN TO CANCEL / RUN OFFLINE ]", 36, 206);

    gfx->endWrite();
  }

  // Start Captive Portal Web Server and DNS
  bool startConfigPortal(LGFX* gfx, ScrapHoundAudio* soundFx, void (*ledCallback)(uint8_t, uint8_t, uint8_t), int timeoutSec = 180) {
    _portalActive = true;
    _credentialsSaved = false;

    Serial.println("[PORTAL] Initializing SoftAP...");
    WiFi.disconnect();
    delay(100);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID);

    IPAddress apIP = WiFi.softAPIP();
    Serial.printf("[PORTAL] AP Started: '%s' | IP: %s\n", AP_SSID, apIP.toString().c_str());

    drawPortalScreen(gfx, AP_SSID, apIP.toString());
    if (soundFx) soundFx->playTone(880, 80);

    // Start Captive Portal DNS on port 53 (redirect all queries to softAP IP)
    _dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    _dnsServer.start(DNS_PORT, "*", apIP);

    // Setup Web Routes
    _server.on("/", [this]() {
      this->handleRoot();
    });

    _server.on("/save", HTTP_POST, [this]() {
      this->handleSave();
    });

    _server.on("/cancel", [this]() {
      this->_server.send(200, "text/html", "<html><body style='background:#111;color:#fff;text-align:center;padding:40px;font-family:sans-serif;'><h2>Cancelled</h2><p>DigiPet returning to offline mode.</p></body></html>");
      this->_portalActive = false;
    });

    // Captive Portal Redirect Catch-All
    _server.onNotFound([this]() {
      this->handleRoot();
    });

    _server.begin();
    Serial.println("[PORTAL] HTTP WebServer active on port 80.");

    unsigned long startMs = millis();
    unsigned long lastLedMs = 0;
    bool ledToggle = false;

    // Portal loop
    while (_portalActive && (millis() - startMs) < ((unsigned long)timeoutSec * 1000)) {
      _dnsServer.processNextRequest();
      _server.handleClient();

      // Check on-screen cancel touch (tap lower half)
      uint16_t tx, ty;
      if (gfx->getTouch(&tx, &ty)) {
        if (ty >= 180) {
          Serial.println("[PORTAL] User tapped Cancel on screen.");
          if (soundFx) soundFx->playTone(400, 60);
          _portalActive = false;
          break;
        }
      }

      // Blink Amber Underglow during portal mode
      if (millis() - lastLedMs > 350) {
        lastLedMs = millis();
        ledToggle = !ledToggle;
        if (ledCallback) {
          ledCallback(ledToggle ? 255 : 0, ledToggle ? 140 : 0, 0);
        }
      }

      delay(10);
    }

    // Cleanup AP
    _server.close();
    _dnsServer.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);

    if (_credentialsSaved) {
      Serial.printf("[PORTAL] New credentials saved: SSID '%s'. Connecting...\n", _savedSsid.c_str());
      saveCredentials(_savedSsid, _savedPass);
      if (soundFx) soundFx->playEvilChompFanfare();
      return true;
    }

    Serial.println("[PORTAL] Exited without new credentials.");
    return false;
  }

private:
  void handleRoot() {
    Serial.println("[PORTAL] Serving Captive Portal Root Page...");

    // Scan available networks
    int n = WiFi.scanNetworks();
    String options = "";
    if (n == 0) {
      options += "<option value=''>No networks found - Enter manually</option>";
    } else {
      for (int i = 0; i < n; ++i) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        int quality = min(max(2 * (rssi + 100), 0), 100);
        String enc = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "🔓" : "🔒";
        options += "<option value='" + ssid + "'>" + enc + " " + ssid + " (" + String(quality) + "%)</option>";
      }
    }

    String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Dataholics DigiPet Setup</title>
  <style>
    * { box-sizing: border-box; }
    body {
      background: #080a0f;
      color: #e2e8f0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace;
      margin: 0;
      padding: 16px;
      display: flex;
      justify-content: center;
      align-items: center;
      min-height: 100vh;
    }
    .card {
      background: #11141c;
      border: 1px solid #1e293b;
      border-top: 3px solid #2be86b;
      border-radius: 12px;
      padding: 24px;
      max-width: 400px;
      width: 100%;
      box-shadow: 0 10px 30px rgba(0,0,0,0.8);
    }
    h1 {
      font-size: 18px;
      margin: 0 0 6px 0;
      color: #2be86b;
      letter-spacing: 1px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .subtitle {
      font-size: 12px;
      color: #94a3b8;
      margin-bottom: 20px;
    }
    label {
      display: block;
      font-size: 11px;
      color: #38bdf8;
      text-transform: uppercase;
      margin-bottom: 6px;
      font-weight: bold;
    }
    select, input[type="text"], input[type="password"] {
      width: 100%;
      background: #080a0f;
      border: 1px solid #334155;
      color: #ffffff;
      padding: 12px;
      border-radius: 6px;
      font-size: 14px;
      margin-bottom: 16px;
      outline: none;
    }
    select:focus, input:focus {
      border-color: #2be86b;
    }
    .btn-submit {
      width: 100%;
      background: #2be86b;
      color: #080a0f;
      border: none;
      padding: 14px;
      border-radius: 6px;
      font-size: 14px;
      font-weight: bold;
      cursor: pointer;
      text-transform: uppercase;
      letter-spacing: 0.5px;
      transition: background 0.2s;
    }
    .btn-submit:hover {
      background: #22c55e;
    }
    .show-pass {
      display: flex;
      align-items: center;
      gap: 8px;
      font-size: 12px;
      color: #94a3b8;
      margin-top: -10px;
      margin-bottom: 16px;
      cursor: pointer;
    }
    .footer {
      text-align: center;
      font-size: 11px;
      color: #64748b;
      margin-top: 20px;
    }
  </style>
</head>
<body>
  <div class="card">
    <h1>🐾 DATAHOLICS DIGIPET</h1>
    <div class="subtitle">Autonomous Tactical Hardware Wi-Fi Manager</div>

    <form method="POST" action="/save">
      <label for="ssid_select">Nearby Networks Found</label>
      <select id="ssid_select" onchange="syncSsid(this.value)">
        <option value="">-- Tap to Select Scanned Network --</option>
)rawhtml";

    html += options;

    html += R"rawhtml(
      </select>

      <label for="ssid">Network SSID (Name)</label>
      <input type="text" id="ssid" name="ssid" placeholder="Enter or select Wi-Fi name" required>

      <label for="pass">Wi-Fi Password</label>
      <input type="password" id="pass" name="pass" placeholder="Network password">

      <div class="show-pass">
        <input type="checkbox" id="toggleP" onclick="togglePass()">
        <label for="toggleP" style="margin:0;cursor:pointer;color:#94a3b8;text-transform:none;font-weight:normal;">Show Password</label>
      </div>

      <button type="submit" class="btn-submit">💾 Save & Connect DigiPet</button>
    </form>

    <div class="footer">Dataholics Resolve ServiceDesk • ESP32 Hardware</div>
  </div>

  <script>
    function syncSsid(val) {
      if (val) document.getElementById('ssid').value = val;
    }
    function togglePass() {
      var x = document.getElementById("pass");
      x.type = (x.type === "password") ? "text" : "password";
    }
  </script>
</body>
</html>
)rawhtml";

    _server.send(200, "text/html", html);
  }

  void handleSave() {
    Serial.println("[PORTAL] Received Wi-Fi Save Request...");

    if (_server.hasArg("ssid") && _server.arg("ssid").length() > 0) {
      _savedSsid = _server.arg("ssid");
      _savedPass = _server.hasArg("pass") ? _server.arg("pass") : "";
      _credentialsSaved = true;

      String html = R"rawhtml(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Credentials Saved</title>
  <style>
    body { background: #080a0f; color: #2be86b; font-family: sans-serif; text-align: center; padding: 40px 20px; }
    .box { background: #11141c; border: 1px solid #2be86b; border-radius: 12px; padding: 30px; max-width: 380px; margin: 0 auto; }
    h2 { margin-top: 0; }
    p { color: #cbd5e1; font-size: 14px; line-height: 1.6; }
  </style>
</head>
<body>
  <div class="box">
    <h2>✅ Credentials Saved!</h2>
    <p>DigiPet is reconnecting to <strong>)rawhtml";
      html += _savedSsid;
      html += R"rawhtml(</strong>.</p>
    <p>You can now disconnect your phone from <em>Dataholics-DigiPet</em> and watch your screen.</p>
  </div>
</body>
</html>
)rawhtml";

      _server.send(200, "text/html", html);
      delay(800);
      _portalActive = false;
    } else {
      _server.send(400, "text/html", "<html><body><h3>Error: SSID cannot be empty!</h3><a href='/'>Go Back</a></body></html>");
    }
  }
};
