#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <lvgl.h>

#include "cyd_display.h"
#include "audio_synth.h"
#include "sprites.h"
#include "gundam_hd_sprites.h"
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
String prevMissionStage = "";
String prevTicketStatus = "";
String prevTicketUpdatedAt = "";
bool prevCircuitBreaker = false;

// Conference Lanyard Badge Mode State (240x320 Vertical Portrait, USB Port UP)
bool conferenceBadgeMode = false;
bool badgeHoldCto = false;
uint8_t badgeStyle = 0;      // 0 = Docking Emblem VIP Pass, 1 = Mecha Hatch Split-Reveal
uint16_t badgeTick = 0;
int16_t badgePrevPhase = -1;
unsigned long lastBadgeInputMs = 0;

// Full-Screen 320x240 Interactive Gundam Roaming Pet Mode State
bool petRoamingMode = false;
int16_t petX = 110;
int8_t  petVx = 3;             // Smooth 3px step per frame (~60px/s at 20 FPS)
uint8_t petAction = 0;         // 0=ROAM, 1=BORED, 2=WEAPON, 3=MISSILE, 4=HARO_PLAY, 5=SLEEP_NIGHT
uint16_t petActionTick = 0;
uint16_t petIdleTicks = 0;
bool petNeedsStaticInit = true;
unsigned long lastPetTouchMs = 0;

// Zero-Flicker Dirty Rectangle Differential Tracking
int16_t prevPetX = -1;
int16_t prevDrawY = -1;
int16_t prevHaroX = -1;
int16_t prevHaroY = -1;
int16_t prevExhaustX = -1;
uint8_t prevPetAction = 255;
float   lastPetCpuDisplay = -100.0f;
unsigned long lastBubbleUpdateMs = 0;

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

// High-Definition 16-bit RGB565 PROGMEM Sprite Renderer (RX-78-2, Barbatos, Unicorn NT-D & Haro)
static void drawGfxHdGundam(const uint16_t* progmemData, int w, int h, int ox, int oy, int stage = 0, int frame = 0, int32_t overrideCol = -1, bool flipX = false) {
  bool alt = (frame % 2) == 1;
  for (int y = 0; y < h; y++) {
    int py = oy + y;
    if (py < 0 || py >= 240) continue;
    for (int x = 0; x < w; x++) {
      int px = ox + x;
      if (px < 0 || px >= 320) continue;
      int sx = flipX ? (w - 1 - x) : x;
      uint16_t c565 = pgm_read_word(&progmemData[y * w + sx]);
      if (c565 != GUNDAM_HD_TRANSPARENT) {
        if (overrideCol >= 0) {
          gfx.writePixel(px, py, (uint16_t)overrideCol);
        } else if (alt && stage == 1) {
          uint8_t r8 = ((c565 >> 11) & 0x1F) << 3;
          uint8_t g8 = ((c565 >> 5) & 0x3F) << 2;
          if (g8 > 200 && r8 < 80) gfx.writePixel(px, py, gfx.color565(0x66, 0xFF, 0xBB));
          else gfx.writePixel(px, py, c565);
        } else if (alt && stage == 2) {
          uint8_t r8 = ((c565 >> 11) & 0x1F) << 3;
          uint8_t g8 = ((c565 >> 5) & 0x3F) << 2;
          uint8_t b8 = (c565 & 0x1F) << 3;
          if (r8 > 210 && g8 < 80 && b8 > 60) gfx.writePixel(px, py, gfx.color565(0xFF, 0x88, 0xCC));
          else gfx.writePixel(px, py, c565);
        } else {
          gfx.writePixel(px, py, c565);
        }
      }
    }
  }
}

static inline const uint16_t* getEndlessDuelSuitSprite(int stage, uint8_t action, int tick, int px) {
  int pose = 0; // 0=Shield Walk/Idle, 1=Rocket Thruster Boost, 2=Saber/Rifle Attack
  int frame = 0;
  if (action == 0) {
    // Alternate between Shield Walk (pose 0) and Rocket Thruster Boost (pose 1) during hangar patrol!
    pose = ((tick / 14) % 2 == 1) ? 1 : 0;
    frame = (abs(px) / (pose == 1 ? 3 : 4)) % 8;   // feet locked to ground speed
  } else if (action == 2) {
    // Melee Beam Saber / Iron Mace Slam + Blazing Alaya-Vijnana / NT-D Aura (attack_f0 & attack_f1)
    pose = 2;
    frame = (tick / 4) % 4;                        // melee f0-3
  } else if (action == 3) {
    // Ranged Beam Rifle / Smoothbore / Beam Magnum + Shield/Shoulder Missile Salvo (attack_f2 & attack_f3)
    pose = 2;
    frame = 4 + ((tick / 2) % 4);                  // ranged f4-7
  } else if (action == 4) {
    // Haro Ball Soccer: Rocket Thruster Hover
    pose = 1;
    frame = (tick / 2) % 8;
  } else {
    // Combat Guard / Idle Breathing
    pose = 0;
    frame = (tick / 3) % 8;
  }
  return getEndlessDuelSprite(stage, pose, frame);
}

static const uint16_t* getHdSuit72(int stage) {
  if (stage >= 3) return gundam_404_72x72;
  return getEndlessDuelSprite(stage, 0, 0);
}

static const uint16_t* getHdSuit120(int stage) {
  if (stage == 1) return barbatos_sd_120x154;
  if (stage == 2) return unicorn_sd_120x154;
  return gundam_sd_120x154;
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

  if (conferenceBadgeMode) {
    gfx.setRotation(1); // Temporarily switch to 320x240 landscape for cutscene
  }

  gfx.startWrite();
  // Differential Frame Buffer Init: clear background ONCE per cutscene (eliminates full-screen flash/flicker!)
  uint16_t initBg = colBg;
  if (fxType == 2) initBg = gfx.color565(0x05, 0x13, 0x0E);
  else if (fxType == 4) initBg = gfx.color565(0x07, 0x09, 0x0E);
  else if (fxType == 5) initBg = gfx.color565(0x05, 0x07, 0x0B);
  else if (fxType == 6) initBg = gfx.color565(0x06, 0x0A, 0x12);
  else if (fxType == 7) initBg = gfx.color565(0x06, 0x0D, 0x1B);
  else if (fxType == 8) initBg = gfx.color565(0x05, 0x16, 0x0F);
  gfx.fillScreen(initBg);

  for (int t = 0; t < 18; t++) {
    if (fxType < 2) {
      gfx.fillScreen(colBg);
      // Anime horizontal speedlines
      for (int i = 0; i < 20; i++) {
        int ly = (i * 11 + t * 9) % 188 + 26;
        int lx = ((i * 53 - t * 40) % 320 + 320) % 320;
        gfx.fillRect(lx, ly, 36, 2, colSpeed);
      }
    }

    if (fxType == 0) {
      // UNICORN GUNDAM [NT-D DESTROY MODE]: PSYCHO-FRAME OVERDRIVE BURST CUTSCENE
      gfx.fillRect(0, 0, 320, 24, gfx.color565(0x3B, 0x05, 0x1C));
      gfx.fillRect(0, 216, 320, 24, gfx.color565(0x3B, 0x05, 0x1C));
      gfx.setTextColor(colMint);
      gfx.setCursor(8, 7);
      gfx.print("NT-D SYSTEM // UNICORN PSYCHO-FRAME BURST!");
      gfx.setTextColor(colGold);
      gfx.setCursor(12, 223);
      gfx.print("BEAM MAGNUM + I-FIELD SHIELD FUNNELS!!");

      // Floating Psycho-Frame Crimson/Mint Particles
      for (int p = 0; p < 16; p++) {
        int px = (p * 47 + t * 23) % 310 + 4;
        int py = (p * 31 - t * 11 + 400) % 180 + 28;
        gfx.fillRect(px, py, 3, 3, (p % 2 == 0) ? colMagenta : colMint);
      }

      // 3 High-Speed NT-D After-Image Clones + 120x154 HD SD UNICORN GUNDAM!
      drawGfxHdGundam(unicorn_sd_120x154, 120, 154, 4 + (t % 3) * 4, 44, 2, t, colRed);
      drawGfxHdGundam(unicorn_sd_120x154, 120, 154, 16 + (t % 2) * 4, 40, 2, t, colViolet);

      int shakeX = (t >= 5 && (t % 2 == 0)) ? 4 : 0;
      drawGfxHdGundam(unicorn_sd_120x154, 120, 154, 28 + shakeX, 38, 2, t);

      int muzzleX = 136;
      int muzzleY = 118;
      if (t < 6) {
        int rad = 8 + t * 6;
        gfx.drawCircle(muzzleX, muzzleY, rad, colMint);
        gfx.fillCircle(muzzleX, muzzleY, rad - 3, (t % 2 == 0) ? colMagenta : colGold);
      } else {
        int beamEnd = min(320, muzzleX + (t - 5) * 56);
        int beamW = beamEnd - muzzleX;
        int jit = (t % 2 == 0) ? 3 : -3;

        gfx.fillRect(muzzleX, muzzleY - 34 + jit, beamW, 68, (t % 2 == 0) ? colMagenta : colViolet);
        gfx.fillRect(muzzleX, muzzleY - 22 - jit, beamW, 44, (t % 2 == 0) ? colCyan : colMint);
        gfx.fillRect(muzzleX, muzzleY - 10, beamW, 20, colWhite);

        for (int s = 0; s < 5; s++) {
          gfx.drawLine(180 + s, 32, 310 + s, 204, colMint);
          gfx.drawLine(310 + s, 32, 180 + s, 204, colWhite);
        }
      }
    } else if (fxType == 1) {
      // G-FRAME EVOLUTION CUT-IN (RX-78-2 -> BARBATOS -> UNICORN)
      gfx.fillRect(0, 0, 320, 24, gfx.color565(0x0A, 0x19, 0x2F));
      gfx.fillRect(0, 216, 320, 24, gfx.color565(0x0A, 0x19, 0x2F));
      gfx.setTextColor(colCyan);
      gfx.setCursor(18, 7);
      gfx.print("/// G-FRAME EVOLUTION // MOBILE SUIT ///");
      gfx.drawRect(160 - t * 7, 120 - t * 4, t * 14, t * 8, (t % 2 == 0) ? colCyan : colGold);
      int curS = (t > 9 || (t % 2 == 0)) ? toS : fromS;
      int32_t flash = (t >= 6 && t <= 10 && (t % 2 == 1)) ? (int32_t)colWhite : -1;
      drawGfxHdGundam(getHdSuit120(curS), 120, 154, 100, 40, curS, t, flash);
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

      // Clear only HARO's bounce box (no full screen wipe!) and render 90x90 HD Vector-Style HARO!
      gfx.fillRect(8, 46, 100, 124, gfx.color565(0x05, 0x13, 0x0E));
      int bounceY = (t % 4 == 1 || t % 4 == 2) ? 18 : 0;
      const uint16_t* haroBig = (bounceY > 0) ? haro_hd_90x90_f1 : haro_hd_90x90_f0;
      drawGfxHdGundam(haroBig, 90, 90, 12, 66 - bounceY, 0, t);

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

      // Update Only Right-Side Bouncing HD HARO + Waking HD SD RX-78-2 Gundam (Restore Grid for 100% Transparent Background!)
      gfx.fillRect(204, 4, 96, 78, colDark);
      for (int gx = 220; gx <= 280; gx += 20) gfx.drawFastVLine(gx, 4, 78, colGrid);
      for (int gy = 20; gy <= 80; gy += 20) gfx.drawFastHLine(204, gy, 96, colGrid);
      int bounce = (t % 4 == 1 || t % 4 == 2) ? 10 : 0;
      const uint16_t* haroBoot = (bounce > 0) ? haro_hd_48x48_f1 : haro_hd_48x48_f0;
      drawGfxHdGundam(haroBoot, 48, 48, 224, 20 - bounce, 0, t);

      gfx.fillRect(194, 84, 116, 20, gfx.color565(0x06, 0x33, 0x1A));
      gfx.drawRect(194, 84, 116, 20, colGreen);
      gfx.setTextSize(1);
      gfx.setTextColor(colMint);
      gfx.setCursor(204, 90);
      gfx.print((t % 2 == 0) ? "\"HELLO MY CTO!\"" : "\"TiRA ONLINE!\"");

      gfx.fillRect(208, 110, 84, 84, colDark);
      for (int gx = 220; gx <= 280; gx += 20) gfx.drawFastVLine(gx, 110, 84, colGrid);
      for (int gy = 120; gy <= 180; gy += 20) gfx.drawFastHLine(208, gy, 84, colGrid);
      drawGfxHdGundam(gundam_sd_72x72, 72, 72, 214, 114, 1, t, (t < 6 && t % 2 == 0) ? (int32_t)colCyan : -1);
    } else if (fxType == 7) {
      // FX 7: LIVE TICKET STATE UPDATE CUTSCENE (IN_PROGRESS / BLUEPRINT / GATES)
      uint16_t colNavy = gfx.color565(0x06, 0x0D, 0x1B);
      if (t == 0) {
        gfx.fillRect(0, 0, 320, 24, gfx.color565(0x0B, 0x1E, 0x38));
        gfx.fillRect(0, 216, 320, 24, gfx.color565(0x0B, 0x1E, 0x38));
        gfx.fillRect(0, 23, 320, 2, colCyan);
        gfx.fillRect(0, 215, 320, 2, colCyan);
        gfx.setTextSize(1);
        gfx.setTextColor(colGold);
        gfx.setCursor(16, 223);
        gfx.printf("STAGE: %s // ATTEMPT #%d", telemetry.missionStage.c_str(), max(1, telemetry.missionAttempt));

        // Right Tactical Update Card
        gfx.fillRect(132, 36, 178, 164, gfx.color565(0x0C, 0x19, 0x2E));
        gfx.setTextColor(colWhite);
        gfx.setCursor(142, 50);
        gfx.printf("TICKET #%d UPDATED!", telemetry.missionTicketId);
        gfx.setTextColor(colCyan);
        gfx.setCursor(142, 72);
        gfx.printf("STATUS: %s", telemetry.ticketStatus.c_str());
        gfx.setTextColor(colMint);
        gfx.setCursor(142, 94);
        gfx.printf("STAGE:  %s", telemetry.missionStage.c_str());

        gfx.fillRect(142, 116, 158, 22, gfx.color565(0x06, 0x33, 0x1A));
        gfx.drawRect(142, 116, 158, 22, colGreen);
        gfx.setTextColor(colGreen);
        gfx.setCursor(148, 123);
        gfx.print("G0:PASS  G1:PASS  G2:RUN");

        gfx.fillRect(142, 148, 158, 40, gfx.color565(0x16, 0x10, 0x28));
        gfx.drawRect(142, 148, 158, 40, colGold);
        gfx.setTextColor(colGold);
        gfx.setCursor(148, 156);
        gfx.print("MOBILE SUIT ENGAGED:");
        gfx.setTextColor(colWhite);
        gfx.setCursor(148, 172);
        int st = cyberUI.getCurrentStage();
        gfx.print(st == 1 ? "ASW-G-08 BARBATOS" : (st == 2 ? "RX-0 UNICORN [NT-D]" : "RX-78-2 GUNDAM"));
      }
      gfx.fillRect(8, 5, 304, 14, gfx.color565(0x0B, 0x1E, 0x38));
      gfx.setTextColor((t % 2 == 0) ? colCyan : colGold);
      gfx.setCursor(10, 7);
      gfx.print(">>> TACTICAL UPLINK: TICKET STATE UPDATED! <<<");
      gfx.drawRect(132, 36, 178, 164, (t % 2 == 0) ? colCyan : colGold);

      // Left Active Gundam Suit performing Combat Strike!
      int st = cyberUI.getCurrentStage();
      if (st < 0 || st > 2) st = 0;
      gfx.fillRect(4, 34, 124, 170, colNavy);
      int lungeX = (t % 4 == 1 || t % 4 == 2) ? 8 : 2;
      drawGfxHdGundam(getHdSuit120(st), 120, 154, lungeX, 40, st, t);
      // Weapon slash flash over suit
      if (t % 2 == 1) {
        uint16_t wCol = (st == 1) ? colGold : ((st == 2) ? colMagenta : colCyan);
        gfx.drawLine(14, 52, 122, 176, wCol);
        gfx.drawLine(15, 52, 123, 176, colWhite);
      }
    } else if (fxType == 8) {
      // FX 8: TICKET RESOLVED // MISSION ACCOMPLISHED VICTORY FANFARE!
      uint16_t colVicBg = gfx.color565(0x05, 0x16, 0x0F);
      if (t == 0) {
        gfx.fillRect(0, 0, 320, 24, gfx.color565(0x09, 0x36, 0x1E));
        gfx.fillRect(0, 216, 320, 24, gfx.color565(0x09, 0x36, 0x1E));
        gfx.fillRect(0, 23, 320, 2, colGold);
        gfx.fillRect(0, 215, 320, 2, colGold);

        gfx.setTextSize(1);
        gfx.setTextColor(colMint);
        gfx.setCursor(18, 223);
        gfx.print("ALL GATES PASSED // ZERO FALSE POSITIVES!");

        // Right Victory Banner Card
        gfx.fillRect(132, 36, 178, 164, gfx.color565(0x08, 0x24, 0x18));
        gfx.fillRect(140, 46, 162, 34, gfx.color565(0x28, 0x1E, 0x05));
        gfx.drawRect(140, 46, 162, 34, colGold);
        gfx.setTextSize(2);
        gfx.setTextColor(colGold);
        gfx.setCursor(154, 55);
        gfx.print("RESOLVED!");

        gfx.setTextSize(1);
        gfx.setTextColor(colWhite);
        gfx.setCursor(142, 92);
        gfx.printf("TICKET: #%d", telemetry.missionTicketId);
        gfx.setTextColor(colGreen);
        gfx.setCursor(142, 110);
        gfx.print("G0:PASS  G1:PASS  G2:PASS");
        gfx.setTextColor(colCyan);
        gfx.setCursor(142, 126);
        gfx.print("STARDUST PING: HTTP 200");
      }
      gfx.fillRect(8, 5, 304, 14, gfx.color565(0x09, 0x36, 0x1E));
      gfx.setTextSize(1);
      gfx.setTextColor((t % 2 == 0) ? colGold : colGreen);
      gfx.setCursor(12, 7);
      gfx.print("*** MISSION ACCOMPLISHED // TICKET CLOSED ***");
      gfx.drawRect(132, 36, 178, 164, (t % 2 == 0) ? colGold : colGreen);

      // Bouncing HD Vector HARO inside right victory card + Active HD Gundam on left!
      gfx.fillRect(142, 140, 158, 54, gfx.color565(0x08, 0x24, 0x18));
      int hb = (t % 2) * 4;
      drawGfxHdGundam((t % 2) ? haro_hd_48x48_f1 : haro_hd_48x48_f0, 48, 48, 146, 144 - hb, 0, t);
      gfx.setTextColor(colGold);
      gfx.setCursor(202, 156);
      gfx.print((t % 2 == 0) ? "\"VICTORY!!\"" : "\"GREAT JOB!\"");
      gfx.setTextColor(colMint);
      gfx.setCursor(202, 174);
      gfx.print("MY CTO // K-2SO");

      int st = cyberUI.getCurrentStage();
      if (st < 0 || st > 2) st = 0;
      gfx.fillRect(4, 34, 124, 170, colVicBg);
      int jumpY = (t % 2 == 0) ? 36 : 42;
      drawGfxHdGundam(getHdSuit120(st), 120, 154, 6, jumpY, st, t);
    } else {
      // RED ALERT / CIRCUIT BREAKER EMP (120x154 HD SD Gundam in Crimson Alert!)
      gfx.fillRect(0, 0, 320, 24, colRed);
      gfx.fillRect(0, 216, 320, 24, colRed);
      gfx.setTextColor(colWhite);
      gfx.setCursor(20, 7);
      gfx.print("!!! TACTICAL INTERCEPT // ALERT !!!");
      drawGfxHdGundam(unicorn_sd_120x154, 120, 154, 100, 40, 2, t, (t % 2 == 0) ? (int32_t)colRed : -1);
    }
    delay(65);
  }
  gfx.endWrite();

  if (conferenceBadgeMode) {
    gfx.setRotation(2); // Restore Vertical Portrait 240x320 Badge Mode
    badgePrevPhase = -1;
  } else if (petRoamingMode) {
    petNeedsStaticInit = true; // Repaint Full-Screen Pet Hangar cleanly
  } else if (lv_screen_active()) {
    // Force LVGL to repaint the full 320x240 HUD after the cutscene completes
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
      // Style 1: Mecha Hatch Blast Doors split open vertically to reveal Giant HD SD RX-78-2 Gundam!
      drawGfxHdGundam(gundam_sd_120x154, 120, 154, 60, 88, 1, t);
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

    // Dynamic Update 2: Animated HD Haro (Style 0) or HD SD RX-78-2 Gundam (Style 1) in Bottom-Left Box!
    gfx.fillRect(14, 252, 54, 52, gfx.color565(0x0A, 0x16, 0x24));
    if (badgeStyle == 0) {
      int hb = (t % 2) * 2;
      const uint16_t* haroBadge = (t % 2) ? haro_hd_48x48_f1 : haro_hd_48x48_f0;
      drawGfxHdGundam(haroBadge, 48, 48, 17, 254 - hb, 0, t); // Native 48x48 HD Vector-Style Haro!
    } else {
      int gb = (t % 2) * 2;
      drawGfxHdGundam(gundam_sd_48x48, 48, 48, 17, 254 - gb, 1, t); // Native 48x48 HD SD RX-78-2 Gundam!
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
// 320x240 Full-Screen Interactive Gundam Playground / Roaming Pet Mode
// Autonomous Roaming, Boredom AI, Signature Weapons, Haro Soccer & LDR Night Mode
// ============================================================================

// Restores the static Cyber Hangar perspective floor & grid over a specific dirty rectangle (rx, ry, rw, rh)
static void restoreHangarRect(int rx, int ry, int rw, int rh, uint16_t colBg, uint16_t colGrid, uint16_t colCyan) {
  if (rw <= 0 || rh <= 0) return;
  // Arena vertical bounds: y = 28 to 211
  if (rx < 0) { rw += rx; rx = 0; }
  if (ry < 28) { rh -= (28 - ry); ry = 28; }
  if (rx + rw > 320) rw = 320 - rx;
  if (ry + rh > 211) rh = 211 - ry;
  if (rw <= 0 || rh <= 0) return;

  // Clear dirty rectangle to solid arena background
  gfx.fillRect(rx, ry, rw, rh, colBg);

  // Restore vertical grid lines: gx = 16, 48, 80, 112, 144, 176, 208, 240, 272, 304 (step 32)
  int firstGx = ((rx - 16) / 32) * 32 + 16;
  if (firstGx < 16) firstGx = 16;
  for (int gx = firstGx; gx < rx + rw && gx < 320; gx += 32) {
    if (gx >= rx) {
      int lineTop = max(28, ry);
      int lineBot = min(179, ry + rh);
      if (lineBot > lineTop) {
        gfx.drawFastVLine(gx, lineTop, lineBot - lineTop, colGrid);
      }
    }
  }

  // Restore Cyan Floor Horizon Line at y = 179
  if (ry <= 179 && (ry + rh) >= 179) {
    gfx.drawFastHLine(rx, 179, rw, colCyan);
  }

  // Restore Perspective Floor Diagonals: fx = 0..320 step 28, from (fx, 179) to (fx - 18, 210)
  if (ry + rh >= 179 && ry <= 210) {
    for (int fx = 0; fx < 320; fx += 28) {
      int minX = min(fx, fx - 18);
      int maxX = max(fx, fx - 18);
      if (maxX >= rx && minX <= rx + rw) {
        gfx.drawLine(fx, 179, fx - 18, 210, colGrid);
      }
    }
  }
}

void enterPetRoamingMode() {
  petRoamingMode = true;
  petX = 116;
  petVx = 3;
  petAction = 0; // ROAM
  petActionTick = 0;
  petIdleTicks = 0;
  petNeedsStaticInit = true;
  lastPetTouchMs = millis() + 350;

  prevPetX = -1;
  prevDrawY = -1;
  prevHaroX = -1;
  prevHaroY = -1;
  prevExhaustX = -1;
  prevPetAction = 255;
  lastPetCpuDisplay = -100.0f;
  lastBubbleUpdateMs = 0;

  gfx.setRotation(1); // 320x240 Landscape
  gfx.fillScreen(gfx.color565(0x06, 0x09, 0x12));
}

void exitPetRoamingMode() {
  petRoamingMode = false;
  lastPetTouchMs = millis() + 350;
  soundFx.playChirp();
  gfx.fillScreen(TFT_BLACK);
  if (lv_screen_active()) {
    lv_obj_invalidate(lv_screen_active());
  }
}

void renderPetRoamingTick() {
  int st = cyberUI.getCurrentStage();
  if (st < 0 || st > 2) st = 0; // Default to RX-78-2 in playground if offline

  // Check LDR ambient light sensor for Night Hangar Mode
  bool isNight = (telemetry.ldrValue > 900);

  uint16_t colBg      = isNight ? gfx.color565(0x02, 0x04, 0x09) : gfx.color565(0x07, 0x0B, 0x16);
  uint16_t colHdr     = gfx.color565(0x0E, 0x16, 0x28);
  uint16_t colGrid    = isNight ? gfx.color565(0x09, 0x12, 0x22) : gfx.color565(0x13, 0x1E, 0x35);
  uint16_t colCyan    = gfx.color565(0x00, 0xF0, 0xFF);
  uint16_t colGold    = gfx.color565(0xFF, 0xDD, 0x00);
  uint16_t colGreen   = gfx.color565(0x00, 0xFF, 0x88);
  uint16_t colMint    = gfx.color565(0x66, 0xFF, 0xBB);
  uint16_t colMagenta = gfx.color565(0xFF, 0x22, 0x88);
  uint16_t colRed     = gfx.color565(0xFF, 0x2A, 0x55);
  uint16_t colOrange  = gfx.color565(0xFF, 0x73, 0x00);
  uint16_t colWhite   = 0xFFFF;

  gfx.startWrite();

  // 1. Draw Top & Bottom Control Bars + Initial Hangar Floor ONCE on entry or suit switch
  if (petNeedsStaticInit) {
    petNeedsStaticInit = false;
    prevPetX = -1;
    prevDrawY = -1;
    prevHaroX = -1;
    prevHaroY = -1;
    prevExhaustX = -1;
    prevPetAction = 255;
    lastPetCpuDisplay = -100.0f;
    lastBubbleUpdateMs = 0;

    gfx.fillScreen(colBg);

    // Top Header Bar (y = 0..27)
    gfx.fillRect(0, 0, 320, 28, colHdr);
    gfx.fillRect(0, 27, 320, 2, colCyan);

    // [ < BACK ] Button (Top-Left x=4..68)
    gfx.fillRect(4, 3, 64, 21, gfx.color565(0x28, 0x0A, 0x16));
    gfx.drawRect(4, 3, 64, 21, colRed);
    gfx.setTextSize(1);
    gfx.setTextColor(colWhite);
    gfx.setCursor(10, 10);
    gfx.print("< BACK");

    // Active Suit Title in Center
    gfx.setTextColor(st == 1 ? colGold : (st == 2 ? colMagenta : colCyan));
    gfx.setCursor(76, 10);
    gfx.print(st == 1 ? "ASW-G-08 BARBATOS" : (st == 2 ? "UNICORN [NT-D]" : "RX-78-2 GUNDAM"));

    // [ SUIT >> ] Switch Button (Top-Right x=236..316)
    gfx.fillRect(236, 3, 80, 21, gfx.color565(0x0D, 0x28, 0x3A));
    gfx.drawRect(236, 3, 80, 21, colCyan);
    gfx.setTextColor(colCyan);
    gfx.setCursor(244, 10);
    gfx.print("[SUIT >>]");

    // Bottom Touch Action Bar (y = 212..239)
    gfx.fillRect(0, 212, 320, 28, colHdr);
    gfx.fillRect(0, 211, 320, 2, colCyan);

    // 3 Bottom Action Pills: [SABER/MACE], [MISSILES], [HARO PLAY]
    gfx.fillRect(6, 215, 98, 21, gfx.color565(0x2A, 0x09, 0x1E));
    gfx.drawRect(6, 215, 98, 21, colMagenta);
    gfx.setTextColor(colMint);
    gfx.setCursor(14, 222);
    gfx.print(st == 1 ? "[IRON MACE]" : (st == 2 ? "[TONFA NT-D]" : "[BEAM SABER]"));

    gfx.fillRect(111, 215, 98, 21, gfx.color565(0x26, 0x16, 0x05));
    gfx.drawRect(111, 215, 98, 21, colGold);
    gfx.setTextColor(colGold);
    gfx.setCursor(120, 222);
    gfx.print("[MISSILE FX]");

    gfx.fillRect(216, 215, 98, 21, gfx.color565(0x06, 0x2E, 0x1A));
    gfx.drawRect(216, 215, 98, 21, colGreen);
    gfx.setTextColor(colGreen);
    gfx.setCursor(226, 222);
    gfx.print("[HARO BALL]");

    // Draw Static Hangar Floor & Grid ONCE
    gfx.fillRect(0, 28, 320, 183, colBg);
    for (int gx = 16; gx < 320; gx += 32) {
      gfx.drawFastVLine(gx, 28, 151, colGrid);
    }
    gfx.drawFastHLine(0, 179, 320, colCyan);
    for (int fx = 0; fx < 320; fx += 28) {
      gfx.drawLine(fx, 179, fx - 18, 210, colGrid);
    }
  }

  // 2. Autonomous AI State Machine Transition Logic
  petActionTick++;
  if (isNight && petAction == 0 && petIdleTicks > 40) {
    petAction = 5; // Enter Night Sleep Guard Mode
  } else if (!isNight && petAction == 5) {
    petAction = 0;
  }

  // Mode Transition Cleanup: Clear residual action debris (saber slashes, missile trails) ONCE
  if (petAction != prevPetAction) {
    if (prevPetAction >= 2 && prevPetAction <= 4) {
      gfx.fillRect(0, 52, 320, 159, colBg);
      for (int gx = 16; gx < 320; gx += 32) {
        gfx.drawFastVLine(gx, 52, 127, colGrid);
      }
      gfx.drawFastHLine(0, 179, 320, colCyan);
      for (int fx = 0; fx < 320; fx += 28) {
        gfx.drawLine(fx, 179, fx - 18, 210, colGrid);
      }
    }
    prevPetAction = petAction;
    prevPetX = -1;
    prevDrawY = -1;
    prevHaroX = -1;
    prevHaroY = -1;
    prevExhaustX = -1;
  }

  // 3. AI Behavior Execution
  if (petAction == 0) {
    // STATE 0: ROAMING ACROSS THE HANGAR FLOOR
    petX += petVx;
    if (petX <= 14) { petX = 14; petVx = 3; }
    if (petX >= 234) { petX = 234; petVx = -3; }
    petIdleTicks++;

    // Erase previous thruster exhaust puff
    if (prevExhaustX >= 0) {
      restoreHangarRect(prevExhaustX - 6, 162, 22, 14, colBg, colGrid, colCyan);
      prevExhaustX = -1;
    }

    // Draw new thruster exhaust puffs behind feet
    int exhaustX = (petVx > 0) ? (petX - 8) : (petX + 74);
    gfx.fillCircle(exhaustX, 168 - (petActionTick % 3) * 2, 3, colCyan);
    gfx.fillCircle(exhaustX - petVx, 171, 2, colMint);
    prevExhaustX = exhaustX;

    // After roaming for ~70 ticks (~3.5s at 50ms), get BORED or spontaneously perform a trick!
    if (petIdleTicks >= 70) {
      petIdleTicks = 0;
      petActionTick = 0;
      uint8_t r = esp_random() % 4;
      petAction = (r == 0) ? 1 : ((r == 1) ? 2 : ((r == 2) ? 3 : 4));
      if (petAction == 2 || petAction == 3) soundFx.playTap();
    }
  } else if (petAction == 1) {
    // STATE 1: BORED / YAWNING / WAITING FOR BUGS (~1.7s)
    if (petActionTick > 34) {
      petAction = 2 + (esp_random() % 3); // Spontaneously launch weapon or Haro trick!
      petActionTick = 0;
      soundFx.playChirp();
    }
  } else if (petAction >= 2 && petAction <= 4) {
    // STATES 2..4: Action duration (~2.2s)
    if (petActionTick > 44) {
      petAction = 0; // Return to roaming
      petActionTick = 0;
    }
  }

  // 4. Live Thought Bubble / Status Banner (Redrawn ONLY when action or CPU text changes)
  unsigned long nowMs = millis();
  bool updateBubble = (nowMs - lastBubbleUpdateMs > 1800) || (abs(telemetry.cpu - lastPetCpuDisplay) >= 3.0f);
  if (updateBubble) {
    lastBubbleUpdateMs = nowMs;
    lastPetCpuDisplay = telemetry.cpu;
    gfx.fillRect(10, 33, 300, 18, gfx.color565(0x0D, 0x18, 0x2B));
    gfx.drawRect(10, 33, 300, 18, (petAction == 2 || petAction == 3) ? colGold : colCyan);
    gfx.setTextSize(1);
    gfx.setCursor(18, 38);
    if (petAction == 0) {
      gfx.setTextColor(colMint);
      gfx.printf("PATROLLING HANGAR // CPU: %.1f%% // TAP ME!", telemetry.cpu);
    } else if (petAction == 1) {
      gfx.setTextColor(colGold);
      gfx.print("Zzz... BORED // NO BUGS TO SLAY... HMMM!");
    } else if (petAction == 2) {
      gfx.setTextColor(colMagenta);
      gfx.print(st == 1 ? "*** BARBATOS IRON MACE GROUND SLAM!! ***" : (st == 2 ? "*** UNICORN NT-D BEAM TONFA X-SLASH! ***" : "*** RX-78-2 PINK BEAM SABER X-SLASH! ***"));
    } else if (petAction == 3) {
      gfx.setTextColor(colGold);
      gfx.print(st == 1 ? ">>> 200mm SMOOTHBORE CANNON BARRAGE! <<<" : (st == 2 ? ">>> BEAM MAGNUM + SHIELD FUNNEL SALVO! <<<" : ">>> HEAD VULCANS + MICRO-MISSILE SALVO! <<<"));
    } else if (petAction == 4) {
      gfx.setTextColor(colGreen);
      gfx.print("PLAYING SOCCER WITH HARO // BOING! BOING!");
    } else {
      gfx.setTextColor(colCyan);
      gfx.print("[NIGHT SENSOR ACTIVE] LOW-POWER GUARD SLEEP...");
    }
  }

  // 5. Calculate Smooth Walking Bob & Vertical Position
  int stepCycle = (abs(petX) / 8) % 4;
  int bobY = (petAction == 0) ? ((stepCycle == 1 || stepCycle == 3) ? -2 : 0) : 0;
  if (petAction == 2 && ((petActionTick / 3) % 2 == 1)) bobY = -6; // Jump during melee attack!
  int drawY = 104 + bobY;

  // 6. Differential Erasing of Dirty Rectangles:
  // Erase previous Gundam position
  if (prevPetX >= 0 && prevDrawY >= 0) {
    restoreHangarRect(prevPetX - 4, prevDrawY - 4, 80, 80, colBg, colGrid, colCyan);
  }

  // Erase previous Haro position
  if (prevHaroX >= 0 && prevHaroY >= 0) {
    restoreHangarRect(prevHaroX - 2, prevHaroY - 2, 52, 52, colBg, colGrid, colCyan);
    prevHaroX = -1;
    prevHaroY = -1;
  }

  // If Melee or Missiles active, restore their action dirty regions
  if (petAction == 2) {
    restoreHangarRect(petX + 42, drawY - 46, 96, 110, colBg, colGrid, colCyan);
  } else if (petAction == 3) {
    restoreHangarRect(0, drawY - 20, 320, 72, colBg, colGrid, colCyan);
  }

  // 7. Render 72x72 Shin Kidou Senki Gundam: Endless Duel Animated Mobile Suit
  const uint16_t* activeSpr = getEndlessDuelSuitSprite(st, petAction, petActionTick, petX);
  bool flipSuit = (petAction == 0 && petVx < 0);
  drawGfxHdGundam(activeSpr, 72, 72, petX, drawY, st, petActionTick, -1, flipSuit);
  prevPetX = petX;
  prevDrawY = drawY;

  // 8. Render Action Visual Effects
  if (petAction == 1 || petAction == 5) {
    // Floating "Zzz..." or "?!" above Gundam head
    gfx.setTextSize(1);
    gfx.setTextColor(colGold);
    gfx.setCursor(petX + 68, drawY - 4 - (petActionTick % 3) * 2);
    gfx.print(petAction == 5 ? "Zzz.." : "?! ..bored");
  } else if (petAction == 2) {
    // MELEE WEAPON PRACTICE (Suit-Specific Signature Weapon!)
    int wx = petX + 58;
    int wy = drawY + 26;
    if (st == 0) {
      // RX-78-2 Glowing Pink/White Beam Saber Arc!
      int tipX = wx + 56;
      int tipY = ((petActionTick / 2) % 2 == 0) ? (wy - 42) : (wy + 38);
      for (int w = -3; w <= 3; w++) {
        gfx.drawLine(wx, wy + w, tipX, tipY + w, (abs(w) <= 1) ? colWhite : colMagenta);
      }
      gfx.drawCircle(wx + 28, wy, 24 + (petActionTick % 3) * 6, colMagenta);
    } else if (st == 1) {
      // Barbatos Giant Iron Mace Ground Slam + Golden Shockwave Cracks!
      int slamY = ((petActionTick / 2) % 2 == 0) ? (drawY - 18) : 168;
      gfx.fillRect(wx, wy, 48, 6, gfx.color565(0x55, 0x60, 0x75));
      gfx.fillRect(wx + 42, slamY - 14, 24, 28, gfx.color565(0x30, 0x38, 0x46));
      gfx.drawRect(wx + 42, slamY - 14, 24, 28, colGold);
      if (slamY >= 160) {
        gfx.drawCircle(wx + 54, 174, (petActionTick % 4) * 12 + 8, colGold);
        gfx.drawLine(wx + 30, 178, wx + 85, 160, colOrange);
      }
    } else {
      // Unicorn NT-D Dual Beam Tonfas + Expanding Psycho-Frame Aura!
      gfx.drawRect(petX - 6, drawY - 4, 84, 80, (petActionTick % 2 == 0) ? colMagenta : colCyan);
      for (int s = -2; s <= 2; s++) {
        gfx.drawLine(petX + 10, drawY + 10 + s, petX + 115, drawY + 70 + s, colMagenta);
        gfx.drawLine(petX + 10, drawY + 70 + s, petX + 115, drawY + 10 + s, colCyan);
      }
    }
  } else if (petAction == 3) {
    // MISSILE BARRAGE / BEAM MAGNUM / SHIELD FUNNELS!
    for (int m = 0; m < 3; m++) {
      int prog = (petActionTick * 12 + m * 36) % 140;
      int mx = (petX < 140) ? (petX + 70 + prog) : (petX - prog);
      int my = drawY + 18 + m * 16 - (int)(sin(prog * 0.04f) * 14.0f);
      if (mx > 12 && mx < 308) {
        if (st == 2) {
          // Unicorn Green Shield Funnel Lasers + Pink Beam Magnum Orbs
          gfx.fillCircle(mx, my, 5, colMagenta);
          gfx.fillCircle(mx, my, 2, colWhite);
          gfx.drawLine(petX + 36, drawY + 24, mx, my, colGreen);
        } else {
          // Micro-Missiles with smoke trails + Detonation rings!
          gfx.drawLine(petX + 48, drawY + 28, mx, my, gfx.color565(0x55, 0x66, 0x77));
          gfx.fillRect(mx - 5, my - 3, 10, 6, colWhite);
          gfx.fillRect(mx + 3, my - 3, 4, 6, colRed);
          if (prog > 105) {
            gfx.fillCircle(mx, my, 12, (petActionTick % 2 == 0) ? colOrange : colGold);
            gfx.drawCircle(mx, my, 18, colRed);
          }
        }
      }
    }
  } else if (petAction == 4) {
    // HARO SOCCER PLAYTIME: Bouncing 48x48 HD Vector Haro back and forth!
    int hx = (petX < 150) ? (petX + 82 + (petActionTick % 6) * 12) : (petX - 58 - (petActionTick % 6) * 12);
    hx = constrain(hx, 14, 258);
    int hy = 128 - abs((int)(petActionTick % 6) - 3) * 14;
    const uint16_t* hSpr = (petActionTick % 2 == 0) ? haro_hd_48x48_f0 : haro_hd_48x48_f1;
    drawGfxHdGundam(hSpr, 48, 48, hx, hy, 0, petActionTick);
    prevHaroX = hx;
    prevHaroY = hy;
  }

  gfx.endWrite();
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
        telemetry.ticketStatus = mission["ticket_status"] | "idle";
        telemetry.ticketUpdatedAt = mission["ticket_updated_at"] | "";
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

      // Check Acoustic & Full-Screen Cutscene Events (Incoming, State Update, Resolved, or Circuit Breaker!)
      bool isFirstBaseline = (prevStage == -1);
      bool newTicketArrived = !isFirstBaseline && telemetry.missionActive && (!prevMissionActive || (telemetry.missionTicketId > 0 && telemetry.missionTicketId != prevTicketId));
      bool ticketResolvedNow = !isFirstBaseline && (
        (telemetry.ticketStatus.equalsIgnoreCase("resolved") && !prevTicketStatus.equalsIgnoreCase("resolved")) ||
        (telemetry.missionStage.equalsIgnoreCase("RESOLVED") && !prevMissionStage.equalsIgnoreCase("RESOLVED"))
      );
      bool ticketUpdatedNow = !isFirstBaseline && !newTicketArrived && !ticketResolvedNow && (
        (telemetry.missionActive && telemetry.missionStage != "IDLE" && telemetry.missionStage != prevMissionStage) ||
        (prevTicketUpdatedAt.length() > 0 && telemetry.ticketUpdatedAt.length() > 0 && telemetry.ticketUpdatedAt != prevTicketUpdatedAt)
      );

      if (telemetry.circuitBreakerTripped && !prevCircuitBreaker) {
        soundFx.playCircuitBreakerAlert();
        triggerGundamCutscene(3, 2, 2);
      } else if (ticketResolvedNow) {
        soundFx.playVictoryFanfare();
        triggerGundamCutscene(8, prevStage, prevStage); // FX 8: Full-Screen Victory Resolved Cutscene!
      } else if (newTicketArrived) {
        soundFx.playEvolutionArpeggio();
        triggerGundamCutscene(2, prevStage, prevStage); // FX 2: Full-Screen HARO Incoming Ticket Alert!
      } else if (ticketUpdatedNow) {
        soundFx.playTicketUpdateRadar();
        triggerGundamCutscene(7, prevStage, prevStage); // FX 7: Full-Screen Ticket State Update Cutscene!
      }

      // Track Stage Evolutions & Fire Full-Screen Gundam Cutscenes
      int curStage = (telemetry.cpu < 30.0f) ? 0 : ((telemetry.cpu < 70.0f) ? 1 : 2);
      if (prevStage != -1 && curStage > prevStage) {
        soundFx.playEvolutionArpeggio();
        if (curStage == 2) {
          triggerGundamCutscene(0, prevStage, curStage); // Full-Screen Unicorn NT-D Burst!
        } else {
          triggerGundamCutscene(1, prevStage, curStage); // Full-Screen G-Frame Evolution to Barbatos!
        }
      }
      prevStage = curStage;
      prevMissionActive = telemetry.missionActive;
      prevTicketId = telemetry.missionTicketId;
      prevMissionStage = telemetry.missionStage;
      prevTicketStatus = telemetry.ticketStatus;
      prevTicketUpdatedAt = telemetry.ticketUpdatedAt;
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
    // Keep polling K-2SO so incoming tickets or resolved tickets still fire live cutscenes!
    if (now - lastMetricsPoll >= POLL_INTERVAL_MS || forcePollNow) {
      lastMetricsPoll = now;
      forcePollNow = false;
      pollK2soMetrics();
    }
    delay(5);
    return;
  }

  // 0.7 If in Full-Screen 320x240 Interactive Gundam Roaming Pet Mode:
  if (petRoamingMode) {
    uint16_t tx, ty;
    static bool wasPetTouched = false;
    bool isPetTouched = gfx.getTouch(&tx, &ty);
    if (isPetTouched && !wasPetTouched && now > lastPetTouchMs) {
      lastPetTouchMs = now + 260;
      if (ty <= 34 && tx <= 84) {
        // Top-Left [ < BACK ] Button -> Return to 4-Tab Dashboard
        exitPetRoamingMode();
        wasPetTouched = isPetTouched;
        return;
      } else if (ty <= 34 && tx >= 220) {
        // Top-Right [ SUIT >> ] Button -> Cycle RX-78-2 -> BARBATOS -> UNICORN NT-D!
        soundFx.playEvolutionArpeggio();
        int curS = cyberUI.getCurrentStage();
        if (curS < 0 || curS > 2) curS = 0;
        int nextS = (curS + 1) % 3;
        cyberUI.setCurrentStage(nextS);
        petNeedsStaticInit = true;
        petAction = 2; // Perform signature weapon pose on suit switch!
        petActionTick = 0;
      } else if (ty >= 204) {
        // Bottom 3 Action Buttons: [BEAM SABER/MACE], [MISSILE FX], [HARO BALL]
        soundFx.playChirp();
        if (tx < 108) petAction = 2;
        else if (tx < 212) petAction = 3;
        else petAction = 4;
        petActionTick = 0;
        petIdleTicks = 0;
      } else {
        // Tapped the Hangar Arena or Gundam directly -> Random Signature Trick!
        soundFx.playChirp();
        petAction = 2 + (esp_random() % 3); // 2=Weapon, 3=Missiles, 4=Haro Ball
        petActionTick = 0;
        petIdleTicks = 0;
      }
    }
    wasPetTouched = isPetTouched;

    // Render 320x240 Roaming Pet Frame at 50ms cadence (~20 FPS buttery smooth zero-flicker differential)
    static unsigned long lastPetFrame = 0;
    if (now - lastPetFrame >= 50) {
      lastPetFrame = now;
      renderPetRoamingTick();
    }

    // Keep polling K-2SO so live Ticket Arrivals / Updates / Resolutions interrupt Pet Mode!
    if (now - lastMetricsPoll >= POLL_INTERVAL_MS || forcePollNow) {
      lastMetricsPoll = now;
      forcePollNow = false;
      pollK2soMetrics();
    }
    updateRgbUnderglow();
    delay(5);
    return;
  }

  // 1. Tick LVGL Engine
  lv_timer_handler();

  // 2. Tick Multi-Color Pet Sprite & Sanctuary Animation (350ms cadence)
  static unsigned long lastAnimTick = 0;
  if (now - lastAnimTick >= 125) {
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
