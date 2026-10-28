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
#define BOOT_BTN_PIN    0    // Physical BOOT Button on back (Active LOW)

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
int prevTicketId = 0;
bool prevCircuitBreaker = false;

// Conference Lanyard Badge Mode State (240x320 Vertical Portrait, USB Port UP)
bool conferenceBadgeMode = false;
bool badgeHoldCto = false;
uint8_t badgeStyle = 0;      // 0 = Docking Emblem VIP Pass, 1 = Mecha Hatch Split-Reveal
uint16_t badgeTick = 0;
int16_t badgePrevPhase = -1;
unsigned long lastBadgeInputMs = 0;

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
    case 4: spriteMap = alt ? sprite_haro_f1    : sprite_haro_f0;    break;
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
  // Differential Frame Buffer Init: clear background ONCE per cutscene (eliminates full-screen flash/flicker!)
  uint16_t initBg = colBg;
  if (fxType == 2) initBg = gfx.color565(0x05, 0x13, 0x0E);
  else if (fxType == 4) initBg = gfx.color565(0x07, 0x09, 0x0E);
  else if (fxType == 5) initBg = gfx.color565(0x05, 0x07, 0x0B);
  else if (fxType == 6) initBg = gfx.color565(0x06, 0x0A, 0x12);
  gfx.fillScreen(initBg);

  for (int t = 0; t < 18; t++) {
    if (fxType < 4 && fxType != 2) {
      gfx.fillScreen(colBg);
      // Anime horizontal speedlines
      for (int i = 0; i < 20; i++) {
        int ly = (i * 11 + t * 9) % 188 + 26;
        int lx = ((i * 53 - t * 40) % 320 + 320) % 320;
        gfx.fillRect(lx, ly, 36, 2, colSpeed);
      }
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
    } else if (fxType == 2) {
      // HARO INCOMING TICKET ALERT CUTSCENE ("TICKET IN! TICKET IN!")
      if (t == 0) {
        gfx.fillRect(0, 0, 320, 24, gfx.color565(0x0A, 0x2E, 0x1F));
        gfx.fillRect(0, 216, 320, 24, gfx.color565(0x0A, 0x2E, 0x1F));
        gfx.setTextColor(colCyan);
        gfx.setCursor(18, 223);
        gfx.print("CATAPULT DECK READY // K-2SO SCRAMBLE");
      }
      gfx.fillRect(8, 5, 304, 14, gfx.color565(0x0A, 0x2E, 0x1F));
      gfx.setTextColor((t % 2 == 0) ? colMint : colGold);
      gfx.setCursor(10, 7);
      gfx.print(">>> HARO COMMS: INCOMING TICKET ALERT! <<<");

      // Clear only HARO's bounce box (no full screen wipe!)
      gfx.fillRect(8, 46, 100, 124, gfx.color565(0x05, 0x13, 0x0E));
      int bounceY = (t % 4 == 1 || t % 4 == 2) ? 18 : 0;
      int haroFrame = (bounceY > 0) ? 1 : 0;
      drawGfxSpriteScaled(4, haroFrame, 10, 68 - bounceY, 4);

      // Hologram Telemetry Box on Right
      if (t == 0) {
        gfx.fillRect(114, 42, 194, 148, gfx.color565(0x0B, 0x22, 0x30));
        gfx.setTextColor(colGold);
        gfx.setCursor(124, 86);
        gfx.printf("TICKET: #%d (%s)", telemetry.missionTicketId, telemetry.missionFolio.c_str());

        gfx.setTextColor(colCyan);
        gfx.setCursor(124, 112);
        gfx.printf("PROTOCOL: %s", telemetry.missionProtocol.c_str());

        gfx.setTextColor(colWhite);
        gfx.setCursor(124, 136);
        gfx.print("GATES: G0 -> G1 -> G2");

        gfx.fillRect(124, 156, 174, 22, gfx.color565(0x06, 0x33, 0x1A));
        gfx.drawRect(124, 156, 174, 22, colGreen);
        gfx.setTextColor(colGreen);
        gfx.setCursor(132, 163);
        gfx.print("STATUS: LAUNCHING NOW");
      }
      gfx.drawRect(114, 42, 194, 148, (t % 2 == 0) ? colGreen : colCyan);
      gfx.fillRect(122, 52, 176, 16, gfx.color565(0x0B, 0x22, 0x30));
      gfx.setTextColor(colMint);
      gfx.setCursor(124, 56);
      gfx.print((t % 2 == 0) ? "\"TICKET IN! TICKET IN!\"" : "\"SCRAMBLE! K-2SO GO!\"");
    } else if (fxType == 4) {
      // OPTION B: CELESTIAL BEING SPLIT-EMBLEM REVEAL (ZERO-FLICKER DIFFERENTIAL RENDER)
      uint16_t colDark   = gfx.color565(0x07, 0x09, 0x0E);
      uint16_t colOrange = gfx.color565(0xFF, 0x73, 0x00);
      uint16_t colHair   = gfx.color565(0x1E, 0x26, 0x36);
      uint16_t colDim    = gfx.color565(0x73, 0x8A, 0x9C);

      if (t == 0) {
        // Static Architectural Blueprint Hairlines & Bottom Signature
        gfx.drawFastHLine(0, 26, 320, colHair);
        gfx.drawFastHLine(0, 206, 320, colHair);
        gfx.drawFastVLine(160, 0, 26, colHair);

        gfx.setTextSize(1);
        gfx.setTextColor(colDim);
        gfx.setCursor(10, 9);
        gfx.print("[01] SOVEREIGN ARCHITECTURE");
        gfx.setTextColor(colOrange);
        gfx.setCursor(224, 9);
        gfx.print("BOOT // SEQ-00");

        gfx.setTextColor(colGold);
        gfx.setCursor(10, 216);
        gfx.print("LUIS MORALES // MY CTO");
        gfx.setTextColor(colGreen);
        gfx.setCursor(184, 216);
        gfx.print("[ CLEARANCE: LEVEL 0 ]");
      }

      // Update Emblem Region Only while sliding (ticks 0..7)
      if (t <= 7) {
        gfx.fillRect(60, 38, 200, 68, colDark);
        int slide = (t < 6) ? (6 - t) * 7 : 0;
        int cx = 160;
        int cy = 72;

        int lxOuter = cx - 46 - slide;
        int lxInner = cx - 6 - slide;
        gfx.fillTriangle(lxOuter, cy - 23, lxInner, cy - 30, lxInner, cy + 30, colOrange);
        gfx.fillTriangle(lxOuter, cy - 23, lxInner, cy + 30, lxOuter, cy + 23, colOrange);

        int rxInner = cx + 6 + slide;
        int rxOuter = cx + 46 + slide;
        gfx.fillTriangle(rxInner, cy - 30, rxOuter, cy - 23, rxOuter, cy + 23, colOrange);
        gfx.fillTriangle(rxInner, cy - 30, rxOuter, cy + 23, rxInner, cy + 30, colOrange);

        gfx.fillCircle(cx - 25 - slide, cy, 13, colDark);
        gfx.fillCircle(cx + 25 + slide, cy, 13, colDark);
        gfx.fillRect(cx - 25 - slide, cy - 5, 50 + slide * 2, 10, colDark);
      }

      // Editorial Typography Stack (Drawn once at t == 4)
      if (t == 4) {
        gfx.setTextSize(2);
        gfx.setTextColor(colWhite);
        gfx.setCursor(52, 118);
        gfx.print("D A T A H O L I C S");

        gfx.setTextSize(1);
        gfx.setTextColor(colCyan);
        gfx.setCursor(44, 146);
        gfx.print("[ TiRA SYSTEM // SOVEREIGN AI ENGINE ]");
      }

      // Executive Owner Credential Pill (Drawn once at t == 7)
      if (t == 7) {
        gfx.fillRect(46, 164, 228, 24, gfx.color565(0x0E, 0x15, 0x24));
        gfx.drawRect(46, 164, 228, 24, colOrange);
        gfx.setTextSize(1);
        gfx.setTextColor(colGold);
        gfx.setCursor(102, 172);
        gfx.print("LUIS MORALES DEVICE");
      }

      // Smooth Bottom Orange Progress Hairline
      int progW = min(320, (t + 1) * 18);
      gfx.fillRect(0, 237, progW, 3, colOrange);
    } else if (fxType == 5) {
      // OPTION A: THE SOVEREIGN MECHA OS BOOT (ZERO-FLICKER DIFFERENTIAL RENDER)
      uint16_t colDark   = gfx.color565(0x05, 0x07, 0x0B);
      uint16_t colOrange = gfx.color565(0xFF, 0x73, 0x00);
      uint16_t colHazard = gfx.color565(0x1C, 0x0D, 0x04);

      if (t == 0) {
        gfx.fillRect(0, 0, 320, 22, colHazard);
        gfx.fillRect(0, 218, 320, 22, colHazard);
        gfx.fillRect(0, 21, 320, 2, colOrange);
        gfx.fillRect(0, 217, 320, 2, colOrange);

        gfx.setTextSize(1);
        gfx.setTextColor(colOrange);
        gfx.setCursor(16, 7);
        gfx.print("/// G-FRAME BIOS v3.0 // SOVEREIGN IGNITION ///");
        gfx.setTextColor(colGreen);
        gfx.setCursor(44, 225);
        gfx.print("NODE: K-2SO [192.168.1.40] // ONLINE");

        gfx.setTextSize(2);
        gfx.setTextColor(colWhite);
        gfx.setCursor(52, 104);
        gfx.print("D A T A H O L I C S");

        gfx.setTextSize(1);
        gfx.setTextColor(colCyan);
        gfx.setCursor(58, 126);
        gfx.print("TiRA AUTONOMOUS SYSTEM // CORE");

        gfx.fillRect(18, 140, 284, 38, gfx.color565(0x14, 0x0C, 0x08));
        gfx.drawRect(18, 140, 284, 38, colGold);
        gfx.setTextColor(colGold);
        gfx.setCursor(52, 148);
        gfx.print("* LUIS MORALES DEVICE [MY CTO] *");
        gfx.setTextColor(colMint);
        gfx.setCursor(48, 163);
        gfx.print("BIOMETRIC AUTH: VERIFIED // LEVEL 0");
      }

      // Update Emblem only on ticks 0, 3, 5 (Flash effect without clearing screen)
      if (t == 0 || t == 3 || t == 5) {
        gfx.fillRect(98, 28, 124, 68, colDark);
        gfx.drawRect(98, 28, 124, 68, colCyan);
        int cx = 160;
        int cy = 62;
        uint16_t embCol = (t == 3) ? colWhite : colOrange;
        gfx.fillTriangle(cx - 44, cy - 21, cx - 5, cy - 28, cx - 5, cy + 28, embCol);
        gfx.fillTriangle(cx - 44, cy - 21, cx - 5, cy + 28, cx - 44, cy + 21, embCol);
        gfx.fillTriangle(cx + 5, cy - 28, cx + 44, cy - 21, cx + 44, cy + 21, embCol);
        gfx.fillTriangle(cx + 5, cy - 28, cx + 44, cy + 21, cx + 5, cy + 28, embCol);
        gfx.fillCircle(cx - 24, cy, 12, colDark);
        gfx.fillCircle(cx + 24, cy, 12, colDark);
        gfx.fillRect(cx - 24, cy - 5, 48, 10, colDark);
      }

      // Update 14-Segment Progress Bar only
      gfx.fillRect(18, 188, 106, 14, colDark);
      int pct = min(100, ((t + 1) * 100) / 18);
      gfx.setTextSize(1);
      gfx.setTextColor(colOrange);
      gfx.setCursor(18, 192);
      gfx.printf("BOOTING OS: %d%%", pct);
      int activeSegs = ((t + 1) * 14) / 18;
      for (int s = 0; s < 14; s++) {
        uint16_t sCol = (s < activeSegs) ? colOrange : gfx.color565(0x1B, 0x24, 0x38);
        gfx.fillRect(128 + s * 12, 189, 9, 11, sCol);
      }
    } else if (fxType == 6) {
      // OPTION C: THE HARO & GUNDAM HANGAR WAKE-UP (ZERO-FLICKER DIFFERENTIAL RENDER)
      uint16_t colDark   = gfx.color565(0x06, 0x0A, 0x12);
      uint16_t colCard   = gfx.color565(0x0C, 0x13, 0x22);
      uint16_t colGrid   = gfx.color565(0x11, 0x1C, 0x2D);
      uint16_t colOrange = gfx.color565(0xFF, 0x73, 0x00);

      if (t == 0) {
        for (int gx = 0; gx < 320; gx += 20) gfx.drawFastVLine(gx, 0, 240, colGrid);
        for (int gy = 0; gy < 240; gy += 20) gfx.drawFastHLine(0, gy, 320, colGrid);

        // Static Left Card: Dataholics Emblem + TiRA System + Luis Morales Badge
        gfx.fillRect(10, 12, 176, 186, colCard);
        gfx.drawRect(10, 12, 176, 186, colOrange);

        int cx = 98;
        int cy = 54;
        gfx.fillTriangle(cx - 40, cy - 19, cx - 5, cy - 25, cx - 5, cy + 25, colOrange);
        gfx.fillTriangle(cx - 40, cy - 19, cx - 5, cy + 25, cx - 40, cy + 19, colOrange);
        gfx.fillTriangle(cx + 5, cy - 25, cx + 40, cy - 19, cx + 40, cy + 19, colOrange);
        gfx.fillTriangle(cx + 5, cy - 25, cx + 40, cy + 19, cx + 5, cy + 25, colOrange);
        gfx.fillCircle(cx - 22, cy, 11, colCard);
        gfx.fillCircle(cx + 22, cy, 11, colCard);
        gfx.fillRect(cx - 22, cy - 4, 44, 8, colCard);

        gfx.setTextSize(1);
        gfx.setTextColor(colWhite);
        gfx.setCursor(40, 96);
        gfx.print("D A T A H O L I C S");

        gfx.setTextColor(colCyan);
        gfx.setCursor(48, 116);
        gfx.print("TiRA SYSTEM v3.0");

        gfx.fillRect(20, 134, 156, 48, gfx.color565(0x1A, 0x12, 0x0B));
        gfx.drawRect(20, 134, 156, 48, colGold);
        gfx.setTextColor(colGold);
        gfx.setCursor(60, 145);
        gfx.print("LUIS MORALES");
        gfx.setTextColor(colGreen);
        gfx.setCursor(32, 164);
        gfx.print("EXECUTIVE DEVICE [CTO]");

        // Static Bottom Strip
        gfx.fillRect(10, 206, 300, 24, gfx.color565(0x13, 0x1A, 0x29));
        gfx.drawRect(10, 206, 300, 24, colCyan);
        gfx.setTextColor(colOrange);
        gfx.setCursor(38, 214);
        gfx.print(">>> HANGAR READY // K-2SO TETHERED <<<");
      }

      // Update Only Right-Side Bouncing HARO + Waking Gundam!
      gfx.fillRect(204, 4, 96, 78, colDark);
      int bounce = (t % 4 == 1 || t % 4 == 2) ? 10 : 0;
      drawGfxSpriteScaled(4, (bounce > 0) ? 1 : 0, 212, 14 - bounce, 3);

      gfx.fillRect(194, 84, 116, 20, gfx.color565(0x06, 0x33, 0x1A));
      gfx.drawRect(194, 84, 116, 20, colGreen);
      gfx.setTextSize(1);
      gfx.setTextColor(colMint);
      gfx.setCursor(204, 90);
      gfx.print((t % 2 == 0) ? "\"HELLO MY CTO!\"" : "\"TiRA ONLINE!\"");

      gfx.fillRect(208, 110, 84, 84, colDark);
      drawGfxSpriteScaled(1, t % 2, 214, 112, 3, (t < 6 && t % 2 == 0) ? (int32_t)colCyan : -1);
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
  if (lv_screen_active()) {
    lv_obj_invalidate(lv_screen_active());
  }
}

// ============================================================================
// 240x320 Vertical Portrait Conference Badge Engine (USB Port Pointing UP)
// Shuffles Style 0 (Docking Emblem VIP Pass) & Style 1 (Mecha Hatch Split)
// ============================================================================

static void drawGfxDataholicsEmblem(int cx, int cy, float s, uint16_t bgCol, uint16_t wingCol, int splitX = 0) {
  int wOut = (int)(46.0f * s);
  int wIn  = (int)(6.0f * s);
  int hOut = (int)(23.0f * s);
  int hIn  = (int)(30.0f * s);
  int cOff = (int)(25.0f * s);
  int cRad = (int)(13.0f * s);
  int bHalf = max(2, (int)(5.0f * s));

  // Left Wing Polygon
  int lxO = cx - wOut - splitX;
  int lxI = cx - wIn - splitX;
  gfx.fillTriangle(lxO, cy - hOut, lxI, cy - hIn, lxI, cy + hIn, wingCol);
  gfx.fillTriangle(lxO, cy - hOut, lxI, cy + hIn, lxO, cy + hOut, wingCol);

  // Right Wing Polygon
  int rxI = cx + wIn + splitX;
  int rxO = cx + wOut + splitX;
  gfx.fillTriangle(rxI, cy - hIn, rxO, cy - hOut, rxO, cy + hOut, wingCol);
  gfx.fillTriangle(rxI, cy - hIn, rxO, cy + hOut, rxI, cy + hIn, wingCol);

  // Negative-Space Inner Circles & Bridge Cutout
  gfx.fillCircle(cx - cOff - splitX, cy, cRad, bgCol);
  gfx.fillCircle(cx + cOff + splitX, cy, cRad, bgCol);
  gfx.fillRect(cx - cOff - splitX, cy - bHalf, (cOff + splitX) * 2, bHalf * 2, bgCol);
}

void enterConferenceBadgeMode() {
  conferenceBadgeMode = true;
  badgeHoldCto = false;
  badgeStyle = esp_random() % 2; // Start with random style (0 or 1), then shuffle each loop!
  badgeTick = 0;
  badgePrevPhase = -1;
  lastBadgeInputMs = millis() + 400; // Debounce entry touch

  gfx.setRotation(2); // Vertical Portrait 240x320 with USB Port Pointing UP!
  gfx.fillScreen(gfx.color565(0x06, 0x08, 0x0E));
}

void exitConferenceBadgeMode() {
  conferenceBadgeMode = false;
  badgeHoldCto = false;
  lastBadgeInputMs = millis() + 400;

  soundFx.playChirp();
  gfx.setRotation(1); // Restore Horizontal 320x240 Desk Digivice Mode
  gfx.fillScreen(TFT_BLACK);
  if (lv_screen_active()) {
    lv_obj_invalidate(lv_screen_active());
  }
}

void renderConferenceBadgeTick() {
  uint16_t colBg     = gfx.color565(0x06, 0x08, 0x0E);
  uint16_t colHdr    = gfx.color565(0x11, 0x17, 0x26);
  uint16_t colCard   = gfx.color565(0x0D, 0x15, 0x26);
  uint16_t colOrange = gfx.color565(0xFF, 0x73, 0x00);
  uint16_t colCyan   = gfx.color565(0x00, 0xF0, 0xFF);
  uint16_t colGold   = gfx.color565(0xFF, 0xDD, 0x00);
  uint16_t colGreen  = gfx.color565(0x00, 0xFF, 0x88);
  uint16_t colMint   = gfx.color565(0x66, 0xFF, 0xBB);
  uint16_t colRed    = gfx.color565(0xFF, 0x2A, 0x55);
  uint16_t colDim    = gfx.color565(0x73, 0x8A, 0x9C);
  uint16_t colWhite  = 0xFFFF;

  // 80-tick cycle (~7.2 seconds per full cycle at 90ms/tick):
  //   0..29  = Act I: Giant Full-Screen Dataholics Emblem + TiRA System
  //   30..39 = Smooth Transition (Style 0: Emblem glides & docks to top | Style 1: Mecha Hatch splits)
  //   40..79 = Act II: Giant LUIS MORALES // CTO Executive Card + Animated Haro/Gundam
  uint16_t t = badgeHoldCto ? (50 + (badgeTick % 20)) : (badgeTick % 80);

  // Shuffle between Style 0 and Style 1 at the start of every cycle!
  if (!badgeHoldCto && t == 0 && badgeTick > 0) {
    badgeStyle = (badgeStyle + 1) % 2;
    badgePrevPhase = -1;
  }

  int16_t curPhase = (t < 30) ? 0 : ((t < 40) ? 1 : 2);

  gfx.startWrite();

  // Draw Top Lanyard Header Bar once at start of cycle or phase change
  if (badgePrevPhase == -1) {
    gfx.fillScreen(colBg);
    gfx.fillRect(0, 0, 240, 22, colHdr);
    gfx.fillRect(0, 21, 240, 2, colOrange);
    gfx.setTextSize(1);
    gfx.setTextColor(colOrange);
    gfx.setCursor(8, 7);
    gfx.print("DATAHOLICS // SOVEREIGN AI");
    gfx.setTextColor(colGreen);
    gfx.setCursor(194, 7);
    gfx.print("VIP-01");
  }

  if (curPhase == 0) {
    // =========================================================================
    // ACT I: FULL-SCREEN DATAHOLICS EMBLEM IGNITION (Ticks 0..29)
    // =========================================================================
    if (badgePrevPhase != 0) {
      gfx.fillRect(0, 24, 240, 290, colBg);

      // Static Typography below Giant Emblem
      gfx.setTextSize(3);
      gfx.setTextColor(colWhite);
      gfx.setCursor(30, 206);
      gfx.print("DATAHOLICS");

      gfx.fillRect(16, 242, 208, 28, gfx.color565(0x0D, 0x1E, 0x2E));
      gfx.drawRect(16, 242, 208, 28, colCyan);
      gfx.setTextSize(1);
      gfx.setTextColor(colCyan);
      gfx.setCursor(38, 252);
      gfx.print("TiRA AUTONOMOUS SYSTEM");

      gfx.setTextColor(colDim);
      gfx.setCursor(22, 284);
      gfx.print(">>> EXECUTIVE ID // CLEARANCE 0 <<<");
    }

    // Update Emblem Zone only during wing slide-in (t <= 8) or flash (t == 8, 9)
    if (t <= 10 || badgePrevPhase != 0) {
      gfx.fillRect(14, 58, 212, 134, colBg);
      gfx.drawRect(18, 62, 204, 126, (t % 2 == 0) ? colOrange : colCyan);
      int splitX = (t < 7) ? (7 - t) * 7 : 0;
      uint16_t wCol = (t == 7 || t == 8) ? colWhite : colOrange;
      drawGfxDataholicsEmblem(120, 125, 1.75f, colBg, wCol, splitX);
    }

  } else if (curPhase == 1) {
    // =========================================================================
    // TRANSITION PHASE (Ticks 30..39): DOCKING EMBLEM OR MECHA HATCH SPLIT
    // =========================================================================
    int step = t - 30; // 0..9
    gfx.fillRect(0, 24, 240, 290, colBg);

    if (badgeStyle == 0) {
      // Style 0: Dataholics Emblem smoothly glides upward from y=125 (scale 1.75) to y=58 (scale 1.0)
      float p = (float)(step + 1) / 10.0f;
      int curY = 125 - (int)(p * 67.0f);
      float curS = 1.75f - p * 0.75f;
      drawGfxDataholicsEmblem(120, curY, curS, colBg, colOrange, 0);

      // Slide up the LUIS MORALES card outline from the bottom
      int cardY = 310 - (int)(p * 186.0f);
      gfx.drawRect(10, cardY, 220, 76, colCyan);
    } else {
      // Style 1: Mecha Hatch Blast Doors split open vertically!
      int splitY = (step + 1) * 12;
      int topDoorH = max(0, 140 - splitY);
      if (topDoorH > 0) {
        gfx.fillRect(0, 24, 240, topDoorH, colHdr);
        gfx.fillRect(0, 24 + topDoorH - 3, 240, 3, colOrange);
      }
      int botDoorY = min(312, 168 + splitY);
      int botDoorH = max(0, 312 - botDoorY);
      if (botDoorH > 0) {
        gfx.fillRect(0, botDoorY, 240, botDoorH, colHdr);
        gfx.fillRect(0, botDoorY, 240, 3, colOrange);
      }
      gfx.drawRect(14, 160 - splitY / 2, 212, splitY, colRed);
    }

  } else {
    // =========================================================================
    // ACT II: FULL-SCREEN LUIS MORALES // CTO MONOLITH (Ticks 40..79)
    // =========================================================================
    if (badgePrevPhase != 2) {
      gfx.fillRect(0, 24, 240, 290, colBg);

      // 1. Top Docked Dataholics Emblem & Title
      drawGfxDataholicsEmblem(120, 58, 1.0f, colBg, colOrange, 0);
      gfx.setTextSize(1);
      gfx.setTextColor(colWhite);
      gfx.setCursor(54, 96);
      gfx.print("D A T A H O L I C S");

      // Divider with glowing cyan nodes
      gfx.fillRect(14, 108, 212, 2, gfx.color565(0x1E, 0x2D, 0x4A));
      gfx.fillRect(14, 107, 14, 4, colCyan);
      gfx.fillRect(212, 107, 14, 4, colCyan);

      // 2. Giant Executive Name Monolith (Readable from 4 meters away!)
      gfx.fillRect(10, 116, 220, 82, colCard);
      gfx.drawRect(10, 116, 220, 82, colCyan);
      gfx.drawRect(11, 117, 218, 80, colCyan);
      gfx.fillRect(10, 116, 18, 4, colOrange);
      gfx.fillRect(212, 116, 18, 4, colOrange);

      gfx.setTextSize(3);
      gfx.setTextColor(colWhite);
      gfx.setCursor(84, 126);
      gfx.print("LUIS");

      gfx.setTextSize(3);
      gfx.setTextColor(colCyan);
      gfx.setCursor(57, 162);
      gfx.print("MORALES");

      // 3. Bottom TiRA System + Animated Mascot Frame
      gfx.fillRect(10, 248, 220, 60, gfx.color565(0x0A, 0x16, 0x24));
      gfx.drawRect(10, 248, 220, 60, colGreen);

      gfx.setTextSize(1);
      gfx.setTextColor(colGreen);
      gfx.setCursor(74, 260);
      gfx.print("TiRA AI ARCHITECT");
      gfx.setTextColor(colMint);
      gfx.setCursor(74, 276);
      gfx.print("SOVEREIGN // K-2SO");
      gfx.setTextColor(colGold);
      gfx.setCursor(74, 292);
      gfx.print(badgeHoldCto ? "[LOCKED: CTO CARD]" : "[TAP: LOCK / EXIT]");
    }

    // Dynamic Update 1: Pulsing Orange/Gold CTO Banner (y = 204..240)
    bool pulse = ((t / 3) % 2 == 0);
    if (t == 40 || (t % 3 == 0)) {
      uint16_t ctoBg  = pulse ? colOrange : gfx.color565(0x26, 0x11, 0x05);
      uint16_t ctoTxt = pulse ? colBg : colGold;
      gfx.fillRect(10, 204, 220, 38, ctoBg);
      gfx.drawRect(10, 204, 220, 38, colGold);
      gfx.drawRect(11, 205, 218, 36, colGold);
      gfx.setTextSize(3);
      gfx.setTextColor(ctoTxt);
      gfx.setCursor(45, 212);
      gfx.print("* CTO *");
    }

    // Dynamic Update 2: Animated Haro (Style 0) or Exia Gundam (Style 1) in Bottom-Left Box!
    gfx.fillRect(14, 252, 54, 52, gfx.color565(0x0A, 0x16, 0x24));
    if (badgeStyle == 0) {
      int hb = (t % 2) * 3;
      drawGfxSpriteScaled(4, t % 2, 16, 256 - hb, 2); // Bouncing Haro!
    } else {
      drawGfxSpriteScaled(2, t % 2, 16, 254, 2);      // Gundam Exia Trans-Am!
    }
  }

  // Bottom 3px Progress Bar
  int progW = ((t % 40) * 240) / 39;
  if (t == 0 || t == 40) gfx.fillRect(0, 316, 240, 4, colBg);
  gfx.fillRect(0, 316, progW, 4, badgeHoldCto ? colCyan : colOrange);

  gfx.endWrite();
  badgePrevPhase = curPhase;
  badgeTick++;

  // Cycle RGB LED Underglow in Orange/Cyan while in Badge Mode
  bool ledStep = (badgeTick / 6) % 2;
  if (ledStep) setRgbLed(1, 1, 0); // Orange/Gold
  else setRgbLed(0, 1, 1);         // Electric Cyan
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
      bool newTicketArrived = telemetry.missionActive && (!prevMissionActive || (telemetry.missionTicketId > 0 && telemetry.missionTicketId != prevTicketId));
      if (telemetry.circuitBreakerTripped && !prevCircuitBreaker) {
        soundFx.playCircuitBreakerAlert();
        triggerGundamCutscene(3, 2, 2);
      } else if (newTicketArrived) {
        soundFx.playEvolutionArpeggio();
        triggerGundamCutscene(2, prevStage, prevStage); // Full-Screen HARO Incoming Ticket Alert!
      }

      // Track Stage Evolutions & Fire Full-Screen Gundam Cutscenes
      int curStage = (telemetry.cpu < 30.0f) ? 0 : ((telemetry.cpu < 70.0f) ? 1 : 2);
      if (prevStage != -1 && curStage > prevStage) {
        soundFx.playEvolutionArpeggio();
        if (curStage == 2) {
          triggerGundamCutscene(0, prevStage, curStage); // Full-Screen Exia Trans-Am Burst!
        } else {
          triggerGundamCutscene(1, prevStage, curStage); // Full-Screen G-Frame Evolution!
        }
      }
      prevStage = curStage;
      prevMissionActive = telemetry.missionActive;
      prevTicketId = telemetry.missionTicketId;
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

  // 1. Setup Onboard RGB LED Pins & Physical BOOT Button (GPIO 0)
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);
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

  // Play Shuffled Dataholics // TiRA System // Luis Morales Device Boot Sequence (A, B, or C!)
  soundFx.playEvolutionArpeggio();
  uint8_t randomBootFx = 4 + (esp_random() % 3); // 4 = Editorial B, 5 = Mecha A, 6 = Haro C
  Serial.printf("[BOOT] Selected Shuffled Boot Sequence FX #%d\n", randomBootFx);
  triggerGundamCutscene(randomBootFx, 0, 0);

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

  // 6. Connect Wi-Fi & Configure Local NTP Clock (CST UTC-6)
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTime(-6 * 3600, 0, "pool.ntp.org", "time.nist.gov");
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
  unsigned long now = millis();

  // 0. Check Physical BOOT Button on Back (GPIO 0) to Toggle Conference Badge Mode!
  static bool prevBootBtn = HIGH;
  bool curBootBtn = digitalRead(BOOT_BTN_PIN);
  if (curBootBtn == LOW && prevBootBtn == HIGH && now > lastBadgeInputMs) {
    lastBadgeInputMs = now + 350;
    if (!conferenceBadgeMode) {
      soundFx.playEvolutionArpeggio();
      enterConferenceBadgeMode();
    } else {
      exitConferenceBadgeMode();
    }
  }
  prevBootBtn = curBootBtn;

  // 0.5 If in Vertical Conference Lanyard Badge Mode (240x320, USB Port UP):
  if (conferenceBadgeMode) {
    // Check touchscreen tap: 1st tap = Lock/Hold on LUIS MORALES CTO card; 2nd tap = Exit to Desk Mode!
    uint16_t tx, ty;
    static bool wasTouched = false;
    bool isTouched = gfx.getTouch(&tx, &ty);
    if (isTouched && !wasTouched && now > lastBadgeInputMs) {
      lastBadgeInputMs = now + 400;
      if (!badgeHoldCto) {
        soundFx.playTap();
        badgeHoldCto = true;
        badgePrevPhase = -1; // Force clean redraw of locked CTO card
      } else {
        exitConferenceBadgeMode();
        wasTouched = isTouched;
        return;
      }
    }
    wasTouched = isTouched;

    // Render 240x320 Vertical Badge Frame at 90ms cadence (~11 FPS zero-flicker)
    static unsigned long lastBadgeFrame = 0;
    if (now - lastBadgeFrame >= 90) {
      lastBadgeFrame = now;
      renderConferenceBadgeTick();
    }
    delay(5);
    return;
  }

  // 1. Tick LVGL Engine
  lv_timer_handler();

  // 2. Tick Multi-Color Pet Sprite & Sanctuary Animation (350ms cadence)
  static unsigned long lastAnimTick = 0;
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
