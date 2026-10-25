#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>

#include "cyd_display.h"
#include "audio_synth.h"
#include "sprites.h"
#include "cyber_ui.h"

// ============================================================================
// K-2SO / TiRA CYD Digivice v3.0 (ESP32-2432S028 Cyberpunk Edition)
// ============================================================================

// Onboard Hardware Pinout (ESP32-2432S028 CYD)
#define RGB_LED_R       4    // Active LOW
#define RGB_LED_G      16    // Active LOW
#define RGB_LED_B      17    // Active LOW
#define LDR_PIN        34    // Analog Ambient Light Sensor

// Network & Host Settings
const char* WIFI_SSID     = "Monkeyskull2";
const char* WIFI_PASS     = "6561766707";
const char* METRICS_URL   = "http://192.168.1.40:8000/metrics";

// Hardware Instances
LGFX gfx;
CyberSynth soundFx;

// LVGL Buffering (Allocated in heap at runtime)
static const uint32_t screenWidth  = 320;
static const uint32_t screenHeight = 240;
static uint8_t* draw_buf = nullptr;
static lv_display_t* disp = nullptr;
static lv_indev_t* indev = nullptr;

static uint32_t my_tick_get(void) {
  return millis();
}

// Telemetry & Timing State
DigiviceTelemetry telemetry;
unsigned long lastMetricsPoll = 0;
const unsigned long POLL_INTERVAL_MS = 2500;
bool forcePollNow = false;

// Audio & Stage Evolution Tracking
int prevStage = -1;
bool prevMissionActive = false;
bool prevCircuitBreaker = false;

// ============================================================================
// LVGL Driver Callbacks (LVGL 9.x Native)
// ============================================================================

void my_disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  gfx.startWrite();
  gfx.pushImage(area->x1, area->y1, w, h, (lgfx::rgb565_t *)px_map);
  gfx.endWrite();
  lv_display_flush_ready(disp_drv);
}

void my_touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data) {
  uint16_t touchX, touchY;
  bool touched = gfx.getTouch(&touchX, &touchY);
  if (!touched) {
    data->state = LV_INDEV_STATE_RELEASED;
  } else {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = touchX;
    data->point.y = touchY;
  }
}

// ============================================================================
// Hardware Underglow RGB LED Control (Active LOW)
// ============================================================================

void setRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  // Invert because CYD onboard RGB LED is Active LOW
  digitalWrite(RGB_LED_R, r ? LOW : HIGH);
  digitalWrite(RGB_LED_G, g ? LOW : HIGH);
  digitalWrite(RGB_LED_B, b ? LOW : HIGH);
}

void updateRgbUnderglow() {
  if (!telemetry.serverOnline) {
    // Pulsing Red for Offline / Tombstone
    bool pulse = (millis() / 500) % 2;
    setRgbLed(pulse, 0, 0);
    return;
  }

  if (telemetry.circuitBreakerTripped) {
    // High-alert Red/Yellow strobe
    bool strobe = (millis() / 200) % 2;
    if (strobe) setRgbLed(1, 0, 0);
    else setRgbLed(1, 1, 0);
    return;
  }

  if (telemetry.missionActive) {
    // Neon Cyan / Electric Blue for Mission In-Flight
    bool breath = (millis() / 400) % 2;
    setRgbLed(0, breath, 1);
    return;
  }

  // Normal CPU Reactive Underglow
  if (telemetry.cpu < 30.0f) {
    setRgbLed(0, 1, 1); // Cyan (Proto-Gundam)
  } else if (telemetry.cpu < 70.0f) {
    setRgbLed(0, 1, 0); // Green (RX-Cyber Gundam)
  } else {
    setRgbLed(1, 0, 1); // Crimson-Magenta (Exia Trans-Am Overdrive)
  }
}

// ============================================================================
// Full-Screen 320x240 Hardware Gundam Event Cutscene Engine
// fxType: 0 = TRANS-AM OVERDRIVE, 1 = G-FRAME EVOLUTION, 2 = TWIN BUSTER LASER
// ============================================================================

static uint16_t resolveGfxColor(char code) {
  switch (code) {
    case '1': return gfx.color565(0x05, 0x07, 0x0B);
    case '2': return gfx.color565(0x00, 0x55, 0x33);
    case '3': return gfx.color565(0x00, 0xCC, 0x66);
    case '4': return gfx.color565(0x66, 0xFF, 0xBB);
    case '5': return gfx.color565(0xFF, 0xFF, 0xFF);
    case '6': return gfx.color565(0x00, 0x38, 0x88);
    case '7': return gfx.color565(0x00, 0xBB, 0xFF);
    case '8': return gfx.color565(0x88, 0xFF, 0xFF);
    case '9': return gfx.color565(0xCC, 0x22, 0xFF);
    case 'A': return gfx.color565(0x4A, 0x12, 0x08);
    case 'B': return gfx.color565(0xEE, 0x11, 0x44);
    case 'C': return gfx.color565(0xFF, 0x66, 0x00);
    case 'D': return gfx.color565(0xFF, 0xDD, 0x00);
    case 'E': return gfx.color565(0x3A, 0x46, 0x59);
    case 'F': return gfx.color565(0x8FA3B8 >> 16, (0x8FA3B8 >> 8) & 0xFF, 0x8FA3B8 & 0xFF);
    default:  return gfx.color565(0x05, 0x08, 0x11);
  }
}

static void drawGfxSpriteScaled(int stage, int frame, int ox, int oy, int scale, int32_t overrideCol = -1) {
  const char* const* spriteMap = sprite_slime_f0;
  bool alt = (frame % 2) == 1;
  switch (stage) {
    case 0: spriteMap = alt ? sprite_slime_f1   : sprite_slime_f0;   break;
    case 1: spriteMap = alt ? sprite_phantom_f1 : sprite_phantom_f0; break;
    case 2: spriteMap = alt ? sprite_daemon_f1  : sprite_daemon_f0;  break;
    default: spriteMap = alt ? sprite_tomb_f1   : sprite_tomb_f0;    break;
  }
  for (int r = 0; r < 24; r++) {
    const char* row = spriteMap[r];
    for (int c = 0; c < 24; c++) {
      char ch = row[c];
      if (ch != '.' && ch != '\0') {
        uint16_t col = (overrideCol >= 0) ? (uint16_t)overrideCol : resolveGfxColor(ch);
        gfx.fillRect(ox + c * scale, oy + r * scale, scale, scale, col);
      }
    }
  }
}

void triggerGundamCutscene(uint8_t fxType, int fromS, int toS) {
  uint16_t colBg       = (fxType == 0) ? gfx.color565(0x16, 0x04, 0x0D) : gfx.color565(0x05, 0x08, 0x11);
  uint16_t colSpeed    = (fxType == 0) ? gfx.color565(0x55, 0x0A, 0x2E) : gfx.color565(0x11, 0x22, 0x38);
  uint16_t colCyan     = gfx.color565(0x00, 0xF0, 0xFF);
  uint16_t colGold     = gfx.color565(0xFF, 0xDD, 0x00);
  uint16_t colMint     = gfx.color565(0x66, 0xFF, 0xBB);
  uint16_t colGreen    = gfx.color565(0x00, 0xFF, 0x88);
  uint16_t colMagenta  = gfx.color565(0xFF, 0x00, 0x7F);
  uint16_t colViolet   = gfx.color565(0xCC, 0x22, 0xFF);
  uint16_t colRed      = gfx.color565(0xFF, 0x2A, 0x55);
  uint16_t colWhite    = 0xFFFF;

  gfx.startWrite();
  for (int t = 0; t < 18; t++) {
    gfx.fillScreen(colBg);

    // Anime horizontal speedlines
    for (int i = 0; i < 20; i++) {
      int ly = (i * 11 + t * 9) % 188 + 26;
      int lx = ((i * 53 - t * 40) % 320 + 320) % 320;
      gfx.fillRect(lx, ly, 36, 2, colSpeed);
    }

    if (fxType == 0) {
      // GUNDAM EXIA: TRANS-AM OVERDRIVE BURST CUTSCENE
      gfx.fillRect(0, 0, 320, 24, gfx.color565(0x3B, 0x05, 0x1C));
      gfx.fillRect(0, 216, 320, 24, gfx.color565(0x3B, 0x05, 0x1C));
      gfx.setTextColor(colMint);
      gfx.setCursor(10, 7);
      gfx.print("GN-DRIVE OVERLOAD // TRANS-AM SYSTEM!!");
      gfx.setTextColor(colGold);
      gfx.setCursor(12, 223);
      gfx.print("OUTPUT: 300% // GN SWORD FULL-BURST");

      // Floating Emerald GN Particles
      for (int p = 0; p < 16; p++) {
        int px = (p * 47 + t * 23) % 310 + 4;
        int py = (p * 31 - t * 11 + 400) % 180 + 28;
        gfx.fillRect(px, py, 3, 3, (p % 2 == 0) ? colMint : colGreen);
      }

      // 3 High-Speed Trans-Am After-Image Clones
      drawGfxSpriteScaled(2, t % 2, 4 + (t % 3) * 4, 62, 5, colRed);
      drawGfxSpriteScaled(2, t % 2, 18 + (t % 2) * 4, 58, 5, colViolet);

      int shakeX = (t >= 5 && (t % 2 == 0)) ? 4 : 0;
      drawGfxSpriteScaled(2, t % 2, 34 + shakeX, 54, 5);

      int muzzleX = 136;
      int muzzleY = 118;
      if (t < 6) {
        int rad = 8 + t * 6;
        gfx.drawCircle(muzzleX, muzzleY, rad, colMint);
        gfx.fillCircle(muzzleX, muzzleY, rad - 3, (t % 2 == 0) ? colGreen : colMagenta);
      } else {
        // X-Shaped GN Sword Slashes + GN Full-Burst Beam
        int beamEnd = min(320, muzzleX + (t - 5) * 56);
        int beamW = beamEnd - muzzleX;
        int jit = (t % 2 == 0) ? 3 : -3;

        gfx.fillRect(muzzleX, muzzleY - 34 + jit, beamW, 68, (t % 2 == 0) ? colMagenta : colRed);
        gfx.fillRect(muzzleX, muzzleY - 22 - jit, beamW, 44, (t % 2 == 0) ? colGreen : colMint);
        gfx.fillRect(muzzleX, muzzleY - 10, beamW, 20, colWhite);

        // Diagonal GN Sword Slashes across right side
        for (int s = 0; s < 5; s++) {
          gfx.drawLine(180 + s, 32, 310 + s, 204, colMint);
          gfx.drawLine(310 + s, 32, 180 + s, 204, colWhite);
        }
      }
    } else if (fxType == 1) {
      // G-FRAME EVOLUTION CUT-IN (Proto -> RX-Cyber -> Exia)
      gfx.fillRect(0, 0, 320, 24, gfx.color565(0x0A, 0x19, 0x2F));
      gfx.fillRect(0, 216, 320, 24, gfx.color565(0x0A, 0x19, 0x2F));
      gfx.setTextColor(colCyan);
      gfx.setCursor(18, 7);
      gfx.print("/// G-FRAME EVOLUTION // MOBILE SUIT ///");
      gfx.drawRect(160 - t * 7, 120 - t * 4, t * 14, t * 8, (t % 2 == 0) ? colCyan : colGold);
      int curS = (t > 9 || (t % 2 == 0)) ? toS : fromS;
      int32_t flash = (t >= 6 && t <= 10 && (t % 2 == 1)) ? (int32_t)colWhite : -1;
      drawGfxSpriteScaled(curS, t % 2, 100, 56, 5, flash);
    } else {
      // RED ALERT / CIRCUIT BREAKER EMP
      gfx.fillRect(0, 0, 320, 24, colRed);
      gfx.fillRect(0, 216, 320, 24, colRed);
      gfx.setTextColor(colWhite);
      gfx.setCursor(20, 7);
      gfx.print("!!! TACTICAL INTERCEPT // ALERT !!!");
      drawGfxSpriteScaled(2, t % 2, 100, 56, 5, (t % 2 == 0) ? (int32_t)colRed : -1);
    }
    delay(65);
  }
  gfx.endWrite();

  // Force LVGL to repaint the full 320x240 HUD after the cutscene completes
  lv_obj_invalidate(lv_screen_active());
}

// ============================================================================
// Network & Telemetry Poller
// ============================================================================

void pollK2soMetrics() {
  if (WiFi.status() != WL_CONNECTED) {
    telemetry.serverOnline = false;
    telemetry.wifiRssi = 0;
    return;
  }

  telemetry.wifiRssi = WiFi.RSSI();

  HTTPClient http;
  http.begin(METRICS_URL);
  http.setTimeout(1800); // Fast non-blocking timeout

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (!err) {
      telemetry.serverOnline = true;
      telemetry.cpu = doc["cpu"] | 0.0f;
      telemetry.ram = doc["ram"] | 0.0f;
      telemetry.disk = doc["disk"] | 0.0f;
      telemetry.load1m = doc["load_1m"] | 0.0f;
      telemetry.uptimeSec = doc["uptime_sec"] | 0;
      telemetry.pollerState = doc["poller"].as<String>();
      telemetry.ollamaState = doc["ollama"].as<String>();

      // Parse Mission Sub-tree
      JsonObject mission = doc["mission"];
      if (!mission.isNull()) {
        telemetry.missionActive = mission["active"] | false;
        telemetry.missionProtocol = mission["protocol"] | "STANDBY";
        telemetry.missionTicketId = mission["ticket_id"] | 0;
        telemetry.missionFolio = mission["ticket_folio"] | "STANDBY";
        telemetry.missionStage = mission["stage"] | "IDLE";
        telemetry.missionAttempt = mission["attempt"] | 0;
        telemetry.circuitBreakerTripped = mission["circuit_breaker"] | false;
        telemetry.elapsedSec = mission["elapsed_sec"] | 0.0f;

        JsonObject gates = mission["gates"];
        if (!gates.isNull()) {
          telemetry.gate0 = gates["g0"] | "IDLE";
          telemetry.gate1 = gates["g1"] | "IDLE";
          telemetry.gate2 = gates["g2"] | "IDLE";
          telemetry.gate3 = gates["g3"] | "IDLE";
        }
      }

      // Check Acoustic & Full-Screen Cutscene Events
      if (telemetry.circuitBreakerTripped && !prevCircuitBreaker) {
        soundFx.playCircuitBreakerAlert();
        triggerGundamCutscene(3, 2, 2);
      } else if (telemetry.missionActive && !prevMissionActive) {
        soundFx.playGatePass();
      }

      // Track Stage Evolutions & Fire Full-Screen Gundam Cutscenes
      int curStage = (telemetry.cpu < 30.0f) ? 0 : ((telemetry.cpu < 70.0f) ? 1 : 2);
      if (prevStage != -1 && curStage > prevStage) {
        soundFx.playEvolutionArpeggio();
        if (curStage == 2) {
          triggerGundamCutscene(0, prevStage, curStage); // Full-Screen Twin Buster Laser!
        } else {
          triggerGundamCutscene(1, prevStage, curStage); // Full-Screen G-Frame Evolution!
        }
      }
      prevStage = curStage;
      prevMissionActive = telemetry.missionActive;
      prevCircuitBreaker = telemetry.circuitBreakerTripped;
    } else {
      telemetry.serverOnline = false;
    }
  } else {
    telemetry.serverOnline = false;
  }
  http.end();

  // Read ambient light
  telemetry.ldrValue = analogRead(LDR_PIN);

  // Push to LVGL UI
  cyberUI.update(telemetry);
}

// ============================================================================
// UI Callback Handlers
// ============================================================================

void handleBacklight(int percent) {
  uint8_t bright = map(percent, 0, 100, 0, 255);
  gfx.setBrightness(bright);
}

void handleManualPoll() {
  forcePollNow = true;
}

void handleCircuitAck() {
  telemetry.circuitBreakerTripped = false;
  prevCircuitBreaker = false;
  forcePollNow = true;
}

void handleAudioToggle(bool enabled) {
  soundFx.setMuted(!enabled);
}

// ============================================================================
// Setup & Loop
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println(F("\n[K-2SO] Booting CYD Digivice v3.0 Cyberpunk Edition..."));

  // 1. Setup Onboard RGB LED Pins
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  setRgbLed(0, 0, 1); // Booting Blue

  // 2. Setup Audio Synth
  soundFx.begin();
  soundFx.playChirp();

  // 3. Setup Display & Hardware Backlight (GPIO 21)
  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH); // Explicit backlight power failsafe

  gfx.init();
  gfx.setRotation(1); // Landscape 320x240 (or 3 if upside down)
  gfx.setBrightness(255);

  Serial.printf("[GFX] Display initialized: width=%d height=%d\n", gfx.width(), gfx.height());

  // Quick boot pattern to clear any old vendor display data
  gfx.fillScreen(TFT_BLACK);

  // 4. Setup LVGL (v9.x)
  lv_init();
  lv_tick_set_cb(my_tick_get);

  const size_t bufBytes = screenWidth * 20 * sizeof(lv_color16_t);
  draw_buf = (uint8_t*)malloc(bufBytes);
  if (!draw_buf) {
    Serial.println(F("[LVGL] FATAL: draw buffer alloc failed"));
    while (true) delay(1000);
  }
  disp = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(disp, draw_buf, NULL, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, my_disp_flush);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  // 5. Initialize Cyberpunk UI & Callbacks
  cyberUI.init();
  cyberUI.setBacklightCallback(handleBacklight);
  cyberUI.setManualPollCallback(handleManualPoll);
  cyberUI.setResetCircuitBreakerCallback(handleCircuitAck);
  cyberUI.setAudioToggleCallback(handleAudioToggle);

  // 6. Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print(F("[WiFi] Connecting to: "));
  Serial.println(WIFI_SSID);

  telemetry.serverOnline = false;
  telemetry.cpu = 0;
  telemetry.ram = 0;
  telemetry.disk = 0;
  telemetry.load1m = 0;
  telemetry.uptimeSec = 0;
  telemetry.missionActive = false;
  telemetry.missionProtocol = "STANDBY";
  telemetry.gate0 = "IDLE";
  telemetry.gate1 = "IDLE";
  telemetry.gate2 = "IDLE";
  telemetry.gate3 = "IDLE";
  telemetry.wifiRssi = -100;
  cyberUI.update(telemetry);
}

void loop() {
  // 1. Tick LVGL Engine
  lv_timer_handler();

  // 2. Tick Multi-Color Pet Sprite & Sanctuary Animation (350ms cadence)
  static unsigned long lastAnimTick = 0;
  unsigned long now = millis();
  if (now - lastAnimTick >= 350) {
    lastAnimTick = now;
    cyberUI.tickAnimation();
  }

  // 3. Periodically or On-Demand Poll K-2SO Node
  if (now - lastMetricsPoll >= POLL_INTERVAL_MS || forcePollNow) {
    lastMetricsPoll = now;
    forcePollNow = false;
    pollK2soMetrics();
  }

  // 4. Update Hardware RGB reactive Underglow
  updateRgbUnderglow();

  delay(5);
}
