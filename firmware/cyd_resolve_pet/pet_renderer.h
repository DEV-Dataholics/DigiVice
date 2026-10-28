#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================================================
// Kitsune-Bunny Yokai Familiar (Haute Couture Cute-Horror Spirit)
// Porcelain Mask, Crimson Crescent Moon, Gothic Stitched Eye & Will-o'-the-Wisp
// ============================================================================

enum PetMood {
  MOOD_BOOT,
  MOOD_IDLE,
  MOOD_ALARM,       // Ticket Opened
  MOOD_LAUGH,       // Ticket Updated
  MOOD_CHOMP,       // Ticket Resolved (Evil victory bite)
  MOOD_POKED        // User touched screen (Flinch & snarl)
};

// Autonomous Idle Micro-Behaviors
enum IdleSubState {
  IDLE_REST,        // Sitting still, gentle breathing, tail sway
  IDLE_WALK,        // Roaming across the arena floor
  IDLE_SNIFF,       // Curious sniffing ground, twitching whiskers (?)
  IDLE_PLAY_WISP,   // Playfully swatting at Will-o'-the-Wisp
  IDLE_DOZE,        // Drowsy heavy eyelids, nodding head (...)
  IDLE_SLEEP        // Deep slumber, curled up, floating Zzz
};

// Structured Ticket Incident Model for Interactive Dossier Explorer
struct TicketItem {
  int id;
  String title;
  String status;
  String priority;
  String requester;
  String email;
  String description;
};

// Calibrated 16-Bit Color Palette (RGB565)
#define C_BG_DARK      0x0821 // Deep void charcoal
#define C_WHITE        0xFFFF // Pure porcelain fur
#define C_SHADOW_FUR   0xCE59 // Soft lavender-grey contour (#cbd5e1)
#define C_CRIMSON      0xE104 // Kitsune mask crimson (#e11d48)
#define C_CORAL        0xF9EB // Inner ear coral (#f43f5e)
#define C_VOID_BLACK   0x0842 // Hollow eye & seam black
#define C_VIOLET_PATCH 0x4811 // Gothic voodoo patch (#4a044e)
#define C_ACID_GREEN   0x2F4D // Spirit spark lime (#2be86b)
#define C_ACID_GLOW    0x87F2
#define C_COMBAT_RED   0xF800 // Blazing alert crimson
#define C_FUEL_AMBER   0xFD20 // Warm amber flame (#f59e0b)
#define C_SOUL_CYAN    0x3DFE // Cool spirit wisp (#38bdf8)
#define C_METAL_DARK   0x18E3
#define C_METAL_MID    0x2965

class ScrapHoundRenderer {
private:
  LGFX* _gfx;
  LGFX_Sprite _canvas;
  LGFX_Sprite _banner;
  
  uint32_t _animTick;
  int _screenShakeX;
  int _screenShakeY;

  int _petX;
  int _targetX;
  int _facing;
  IdleSubState _idleSubState;
  uint32_t _idleTimer;
  int _startleJump;
  bool _wasAsleep;
  bool _fullWidth;

public:
  ScrapHoundRenderer(LGFX* gfx) 
    : _gfx(gfx), _canvas(gfx), _banner(gfx), _animTick(0), _screenShakeX(0), _screenShakeY(0),
      _petX(160), _targetX(160), _facing(1), _idleSubState(IDLE_REST), _idleTimer(0),
      _startleJump(0), _wasAsleep(false), _fullWidth(false) {}

  void begin() {
    _canvas.setColorDepth(16);
    if (_canvas.createSprite(320, 178)) {
      _fullWidth = true;
      _petX = 160;
      _targetX = 160;
    } else {
      _canvas.createSprite(170, 140);
      _fullWidth = false;
      _petX = 85;
      _targetX = 85;
    }

    _banner.setColorDepth(16);
    _banner.createSprite(320, 36);

    _facing = 1;
    _idleSubState = IDLE_REST;
    _idleTimer = 0;
    _startleJump = 0;
    _wasAsleep = false;
  }

  void setShake(int intensity) {
    if (intensity > 0) {
      _screenShakeX = (random(0, intensity * 2) - intensity);
      _screenShakeY = (random(0, intensity * 2) - intensity);
    } else {
      _screenShakeX = 0;
      _screenShakeY = 0;
    }
  }

  IdleSubState getIdleSubState() const { return _idleSubState; }
  bool isSleeping() const { return (_idleSubState == IDLE_SLEEP); }
  bool isDozing() const { return (_idleSubState == IDLE_DOZE); }

  void wakeUp() {
    if (_wasAsleep || _idleSubState == IDLE_SLEEP || _idleSubState == IDLE_DOZE) {
      _startleJump = 10;
      _wasAsleep = false;
      _idleSubState = IDLE_REST;
      _idleTimer = 0;
    }
  }

  void updateIdleLife(PetMood mood) {
    if (mood != MOOD_IDLE) {
      wakeUp();
      return;
    }

    _idleTimer++;

    if (_startleJump > 0) {
      _startleJump -= 2;
      if (_startleJump < 0) _startleJump = 0;
    }

    if (_idleSubState == IDLE_WALK) {
      int dist = _targetX - _petX;
      if (abs(dist) < 3) {
        _petX = _targetX;
        int r = random(0, 100);
        if (r < 35) _idleSubState = IDLE_SNIFF;
        else if (r < 65) _idleSubState = IDLE_PLAY_WISP;
        else _idleSubState = IDLE_REST;
        _idleTimer = 0;
      } else {
        _facing = (dist > 0) ? 1 : -1;
        _petX += _facing;
      }
      return;
    }

    if (_idleSubState == IDLE_REST) {
      if (_idleTimer > 200 && random(0, 100) < 4) {
        if (_fullWidth) {
          int targets[] = {60, 100, 140, 160, 200, 230, 260};
          _targetX = targets[random(0, 7)];
        } else {
          int targets[] = {45, 65, 85, 105, 125};
          _targetX = targets[random(0, 5)];
        }
        _idleSubState = IDLE_WALK;
        _idleTimer = 0;
      } else if (_idleTimer > 380) {
        _idleSubState = IDLE_DOZE;
        _idleTimer = 0;
      }
    } else if (_idleSubState == IDLE_SNIFF) {
      if (_idleTimer > 80) {
        if (random(0, 2) == 0) {
          if (_fullWidth) {
            int targets[] = {70, 120, 160, 210, 250};
            _targetX = targets[random(0, 5)];
          } else {
            int targets[] = {50, 85, 120};
            _targetX = targets[random(0, 3)];
          }
          _idleSubState = IDLE_WALK;
        } else {
          _idleSubState = IDLE_REST;
        }
        _idleTimer = 0;
      }
    } else if (_idleSubState == IDLE_PLAY_WISP) {
      if (_idleTimer > 90) {
        _idleSubState = IDLE_REST;
        _idleTimer = 0;
      }
    } else if (_idleSubState == IDLE_DOZE) {
      if (_idleTimer > 130) {
        _idleSubState = IDLE_SLEEP;
        _wasAsleep = true;
        _idleTimer = 0;
      }
    } else if (_idleSubState == IDLE_SLEEP) {
      _wasAsleep = true;
      if (_idleTimer > 600 && random(0, 100) < 4) {
        _idleSubState = IDLE_REST;
        _wasAsleep = false;
        _idleTimer = 0;
      }
    }
  }

  void renderFrame(PetMood mood, const String &ticketTitle, int ticketId, const String &ticketStatus, int activeCount, int resolvedCount, const String &deviceMac) {
    _animTick++;
    updateIdleLife(mood);

    // 1. Character Canvas Arena
    _canvas.fillScreen(C_BG_DARK);

    if (_fullWidth) {
      _canvas.drawFastHLine(0, 142, 320, 0x18E3);
      _canvas.drawFastHLine(0, 144, 320, 0x2965);
      for (int x = 0; x < 320; x += 24) {
        _canvas.drawLine(x, 144, x + (x - 160) / 4, 178, 0x10A2);
      }
    } else {
      _canvas.drawFastHLine(0, 126, 170, 0x18E3);
      _canvas.drawFastHLine(0, 128, 170, 0x2965);
    }

    int cx = _petX;
    int cy = _fullWidth ? 104 : 72;

    bool isWalking = (mood == MOOD_IDLE && _idleSubState == IDLE_WALK);
    bool isSleeping = (mood == MOOD_IDLE && _idleSubState == IDLE_SLEEP);
    bool isDozing = (mood == MOOD_IDLE && _idleSubState == IDLE_DOZE);
    bool isSniffing = (mood == MOOD_IDLE && _idleSubState == IDLE_SNIFF);
    bool isPlayingWisp = (mood == MOOD_IDLE && _idleSubState == IDLE_PLAY_WISP);

    int walkBob = isWalking ? (((_animTick / 3) % 2 == 0) ? 2 : 0) : 0;
    int breatheY = (mood == MOOD_IDLE) ? (isSleeping ? ((_animTick / 16) % 2) : ((_animTick / 8) % 3)) : 0;
    int bounceY = (mood == MOOD_LAUGH) ? ((_animTick / 3) % 4) : 0;
    int flinchX = (mood == MOOD_POKED) ? ((_animTick % 2 == 0) ? -3 : 3) : 0;
    int lungeX = (mood == MOOD_CHOMP) ? ((_animTick % 6 < 3) ? 6 : -2) : 0;

    int dozeDrop = isDozing ? ((_animTick / 8) % 4) : 0;
    int sleepLower = isSleeping ? 4 : 0;

    int petY = cy + breatheY - bounceY - walkBob + dozeDrop + sleepLower - _startleJump;
    int petX = cx + flinchX + lungeX;

    // 1. Bushy Curling Kitsune Tail (Wrapped around flank)
    int tailWiggle = isWalking ? (((_animTick / 2) % 3) * 3) : (((_animTick / 5) % 3) * 2);
    _canvas.fillEllipse(petX - 28, petY + 16, 14, 20, C_WHITE);
    _canvas.fillEllipse(petX - 26, petY + 18, 10, 16, C_SHADOW_FUR);
    _canvas.fillTriangle(petX - 32, petY + 4 - tailWiggle, petX - 44, petY - 6 - tailWiggle, petX - 22, petY - 2 - tailWiggle, C_CRIMSON);
    _canvas.fillCircle(petX - 44, petY - 6 - tailWiggle, 3, C_CRIMSON);

    // 2. Seated Porcelain White Body
    _canvas.fillEllipse(petX, petY + 22, 22, 17, C_SHADOW_FUR);
    _canvas.fillEllipse(petX, petY + 20, 20, 15, C_WHITE);

    // Fluffy Chest Ruff
    _canvas.fillTriangle(petX - 12, petY + 12, petX + 12, petY + 12, petX, petY + 24, C_WHITE);

    // Crimson Leg Swirls
    _canvas.drawCircle(petX - 12, petY + 20, 5, C_CRIMSON);
    _canvas.drawCircle(petX + 12, petY + 20, 5, C_CRIMSON);

    // Cute Paws with coral pads (Animated for walking / swatting)
    int leftPawY = petY + 28;
    int rightPawY = petY + 28;
    if (isWalking) {
      int step = ((_animTick / 3) % 2 == 0) ? 2 : -2;
      leftPawY += step;
      rightPawY -= step;
    } else if (isPlayingWisp) {
      rightPawY -= 12; // Swat up at wisp!
    } else if (isSniffing) {
      leftPawY += 2;
      rightPawY += 2;
    }
    _canvas.fillCircle(petX - 6, leftPawY, 4, C_WHITE);
    _canvas.fillCircle(petX + 6, rightPawY, 4, C_WHITE);
    _canvas.fillRect(petX - 7, leftPawY + 1, 2, 2, C_CORAL);
    _canvas.fillRect(petX + 5, rightPawY + 1, 2, 2, C_CORAL);

    // 3. Tall Kitsune-Bunny Hybrid Ears
    int earAngle = (mood == MOOD_ALARM) ? 8 : (isSleeping ? 12 : (isSniffing ? 2 : 4));
    int earDrop = isSleeping ? 4 : 0;
    _canvas.fillEllipse(petX - 18, petY - 28 - earAngle + earDrop, 8, 22, C_WHITE);
    _canvas.fillEllipse(petX - 18, petY - 28 - earAngle + earDrop, 4, 16, C_CRIMSON);
    _canvas.fillEllipse(petX + 18, petY - 28 - earAngle + earDrop, 8, 22, C_WHITE);
    _canvas.fillEllipse(petX + 18, petY - 28 - earAngle + earDrop, 4, 16, C_CRIMSON);

    // 4. Porcelain Chibi Head
    _canvas.fillEllipse(petX, petY - 4, 30, 24, C_SHADOW_FUR);
    _canvas.fillEllipse(petX, petY - 6, 28, 22, C_WHITE);

    // Fluffy Cheek Tufts
    _canvas.fillTriangle(petX - 26, petY - 6, petX - 34, petY - 2, petX - 24, petY + 4, C_WHITE);
    _canvas.fillTriangle(petX + 26, petY - 6, petX + 34, petY - 2, petX + 24, petY + 4, C_WHITE);

    // 5. Forehead Crescent Moon & Kitsune Markings
    uint16_t moonCol = (mood == MOOD_ALARM) ? C_COMBAT_RED : C_CRIMSON;
    _canvas.fillCircle(petX, petY - 18, 6, moonCol);
    _canvas.fillCircle(petX, petY - 16, 4, C_WHITE);
    _canvas.fillRect(petX - 6, petY - 14, 2, 4, moonCol);
    _canvas.fillRect(petX + 4, petY - 14, 2, 4, moonCol);

    // Cheek Whisker Flames
    _canvas.drawLine(petX - 20, petY - 2, petX - 13, petY + 1, C_CRIMSON);
    _canvas.drawLine(petX - 22, petY + 3, petX - 15, petY + 5, C_CRIMSON);
    _canvas.drawLine(petX + 20, petY - 2, petX + 13, petY + 1, C_CRIMSON);
    _canvas.drawLine(petX + 22, petY + 3, petX + 15, petY + 5, C_CRIMSON);

    // 6. EYES: Gothic Voodoo Asymmetry
    if (isSleeping) {
      _canvas.drawCircle(petX - 13, petY - 3, 4, C_VOID_BLACK);
      _canvas.fillRect(petX - 18, petY - 8, 12, 5, C_WHITE);
    } else if (isDozing) {
      _canvas.fillEllipse(petX - 13, petY - 6, 7, 9, C_VOID_BLACK);
      _canvas.fillRect(petX - 17, petY - 10, 10, 5, C_WHITE);
    } else if (mood == MOOD_LAUGH) {
      _canvas.drawLine(petX - 17, petY - 8, petX - 11, petY - 4, C_ACID_GREEN);
      _canvas.drawLine(petX - 17, petY, petX - 11, petY - 4, C_ACID_GREEN);
    } else {
      uint16_t pupCol = (mood == MOOD_ALARM) ? C_COMBAT_RED : C_ACID_GREEN;
      _canvas.fillCircle(petX - 13, petY - 6, (mood == MOOD_ALARM ? 4 : 3), pupCol);
      _canvas.drawPixel(petX - 14, petY - 7, C_WHITE);
    }

    // RIGHT EYE: Gothic Violet Stitched Patch (+)
    _canvas.fillRoundRect(petX + 7, petY - 14, 14, 16, 3, C_VIOLET_PATCH);
    _canvas.drawRoundRect(petX + 7, petY - 14, 14, 16, 3, 0x7811);
    if (isSleeping) {
      _canvas.drawLine(petX + 9, petY - 6, petX + 19, petY - 6, C_WHITE);
    } else {
      _canvas.drawLine(petX + 14, petY - 12, petX + 14, petY, C_WHITE);
      _canvas.drawLine(petX + 9, petY - 6, petX + 19, petY - 6, C_WHITE);
    }

    // Little Nose
    _canvas.fillCircle(petX, petY - 1, 2, C_VOID_BLACK);

    // 7. Stitched Razor Smile with Vampire Fangs
    int mouthOpen = 2;
    if (mood == MOOD_LAUGH) mouthOpen = 8 + ((_animTick / 2) % 6);
    else if (mood == MOOD_CHOMP) mouthOpen = ((_animTick / 2) % 2 == 0) ? 14 : 2;
    else if (mood == MOOD_ALARM || mood == MOOD_POKED) mouthOpen = 6;
    else if (isSleeping) mouthOpen = 0;

    _canvas.drawCircle(petX, petY + 2, 14, C_VOID_BLACK);
    for (int s = -10; s <= 10; s += 5) {
      _canvas.drawLine(petX + s, petY + 6, petX + s, petY + 11, C_VOID_BLACK);
    }

    _canvas.fillTriangle(petX - 8, petY + 7, petX - 5, petY + 7, petX - 6, petY + 12 + (mouthOpen > 4 ? 2 : 0), C_WHITE);
    _canvas.fillTriangle(petX + 5, petY + 7, petX + 8, petY + 7, petX + 6, petY + 12 + (mouthOpen > 4 ? 2 : 0), C_WHITE);

    if (mouthOpen > 3) {
      _canvas.fillEllipse(petX, petY + 10, 8, mouthOpen, C_VOID_BLACK);
      _canvas.fillCircle(petX, petY + 10 + mouthOpen / 2, 3, C_CORAL);
    }

    // 8. Hovering Will-o'-the-Wisp Spirit Flame
    int flameY = petY + 2 + ((_animTick / 4) % 3) * 2;
    int flameX = petX + 34;
    if (isPlayingWisp) {
      flameX = petX + 22 + ((_animTick / 3) % 4) * 2;
      flameY = petY + 12;
    } else if (isSleeping) {
      flameX = petX + 28;
      flameY = petY + 16;
    }
    uint16_t flCol = (mood == MOOD_ALARM) ? C_COMBAT_RED : ((mood == MOOD_CHOMP) ? C_ACID_GREEN : ((mood == MOOD_LAUGH) ? C_FUEL_AMBER : (isSleeping ? 0x035F : C_SOUL_CYAN)));
    uint16_t flCore = (mood == MOOD_ALARM) ? C_WHITE : ((mood == MOOD_CHOMP) ? C_ACID_GLOW : (isSleeping ? 0x3DFE : C_WHITE));

    _canvas.fillCircle(flameX, flameY + 2, 6, flCol);
    _canvas.fillTriangle(flameX - 6, flameY + 2, flameX + 6, flameY + 2, flameX, flameY - 12, flCol);
    _canvas.fillCircle(flameX, flameY + 2, 3, flCore);

    if (_animTick % 3 == 0 && !isSleeping) {
      _canvas.drawPixel(flameX + (random(0, 8) - 4), flameY - 14, flCore);
    }

    // 9. Comic Popups & Thought Bubbles
    if (_startleJump > 0) {
      _canvas.fillCircle(petX, petY - 34, 7, C_COMBAT_RED);
      _canvas.setTextColor(C_WHITE);
      _canvas.setTextSize(1);
      _canvas.drawString("!", petX - 2, petY - 38);
    } else if (isSniffing) {
      _canvas.setTextColor(C_FUEL_AMBER);
      _canvas.setTextSize(1);
      _canvas.drawString("?", petX + 14, petY - 32);
    } else if (isDozing) {
      _canvas.setTextColor(0x9CF3);
      _canvas.setTextSize(1);
      _canvas.drawString("...", petX + 14, petY - 32);
    } else if (isSleeping) {
      int zStep = (_animTick / 8) % 3;
      _canvas.setTextColor(0x87F2);
      _canvas.setTextSize(1);
      if (zStep == 0) _canvas.drawString("z", petX + 16, petY - 30);
      else if (zStep == 1) _canvas.drawString("Z", petX + 20, petY - 36);
      else _canvas.drawString("z", petX + 24, petY - 42);
    } else if (mood == MOOD_LAUGH) {
      _canvas.fillRoundRect(petX + 16, petY - 36, 60, 20, 4, C_FUEL_AMBER);
      _canvas.drawRoundRect(petX + 16, petY - 36, 60, 20, 4, C_WHITE);
      _canvas.setTextColor(0x0000);
      _canvas.setTextSize(1);
      _canvas.drawString((_animTick % 4 < 2) ? "HEHEHE!" : "HAHAHA!", petX + 20, petY - 30);
    } else if (mood == MOOD_CHOMP) {
      _canvas.drawLine(petX - 26, petY + 12, petX + 26, petY + 12, C_SOUL_CYAN);
      _canvas.setTextColor(C_ACID_GREEN);
      _canvas.setTextSize(1);
      _canvas.drawString("CRUNCH!", petX - 24, petY - 30);
    } else if (mood == MOOD_ALARM) {
      if (_animTick % 4 < 2) {
        _canvas.setTextColor(C_COMBAT_RED);
        _canvas.setTextSize(2);
        _canvas.drawString("!", petX - 4, petY - 36);
      }
    } else if (mood == MOOD_POKED) {
      _canvas.setTextColor(0xA01F);
      _canvas.setTextSize(1);
      _canvas.drawString("GRRR!", petX - 14, petY - 32);
    }

    // Push Sprite
    if (_fullWidth) {
      int finalX = _screenShakeX;
      int finalY = 25 + _screenShakeY;
      _canvas.pushSprite(_gfx, finalX, finalY);
    } else {
      int finalX = 75 + _screenShakeX;
      int finalY = 44 + _screenShakeY;
      _canvas.pushSprite(_gfx, finalX, finalY);
    }

    // 2. Top HUD Bar
    _gfx->startWrite();
    _gfx->fillRect(0, 0, 320, 24, C_METAL_DARK);
    _gfx->drawFastHLine(0, 24, 320, C_CRIMSON);

    // On-Screen Direct Touch Pill for Executive Badge Mode
    _gfx->fillRect(4, 3, 114, 18, 0x2104);
    _gfx->drawRect(4, 3, 114, 18, C_CRIMSON);
    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(1);
    _gfx->drawString("🪪 ALONSO (CEO)", 8, 8);

    // Active Ticket Metric Button
    _gfx->fillRect(126, 3, 90, 18, C_METAL_MID);
    _gfx->drawRect(126, 3, 90, 18, 0x3D9E);
    _gfx->setTextColor(C_WHITE);
    _gfx->drawString(String("ACT: ") + String(activeCount), 132, 8);

    // Slain Ticket Metric Button
    _gfx->fillRect(222, 3, 94, 18, 0x0280);
    _gfx->drawRect(222, 3, 94, 18, C_ACID_GREEN);
    _gfx->setTextColor(C_ACID_GREEN);
    _gfx->drawString(String("SLAIN: ") + String(resolvedCount), 228, 8);

    // 3. Bottom Ticker Banner
    _banner.fillScreen(0x0821);
    uint16_t bannerBorder = (mood == MOOD_ALARM) ? C_COMBAT_RED : ((mood == MOOD_CHOMP) ? C_ACID_GREEN : C_CRIMSON);
    _banner.drawFastHLine(0, 0, 320, bannerBorder);

    for (int hz = 0; hz < 24; hz += 8) {
      _banner.drawLine(hz, 0, hz + 6, 36, C_CRIMSON);
      _banner.drawLine(290 + hz, 0, 290 + hz + 6, 36, C_CRIMSON);
    }

    _banner.setTextSize(1);
    if (mood == MOOD_ALARM) {
      _banner.fillRect(28, 4, 260, 28, C_CRIMSON);
      _banner.setTextColor(C_WHITE);
      _banner.drawString(String("!! NEW INTAKE [TKT-") + String(ticketId) + "] !!", 36, 8);
      _banner.drawString(ticketTitle.substring(0, 38), 36, 20);
    } else if (mood == MOOD_CHOMP) {
      _banner.fillRect(28, 4, 260, 28, 0x0360);
      _banner.setTextColor(C_ACID_GREEN);
      _banner.drawString(String("TICKET SLAIN & RESOLVED [TKT-") + String(ticketId) + "]", 34, 8);
      _banner.setTextColor(C_WHITE);
      _banner.drawString("OPERATIONAL HARMONY RESTORED", 34, 20);
    } else if (mood == MOOD_LAUGH) {
      _banner.fillRect(28, 4, 260, 28, 0x6180);
      _banner.setTextColor(C_FUEL_AMBER);
      _banner.drawString(String("TICKET UPDATED [TKT-") + String(ticketId) + "]", 36, 8);
      _banner.setTextColor(C_WHITE);
      _banner.drawString(ticketTitle.substring(0, 38), 36, 20);
    } else {
      _banner.setTextColor(C_SOUL_CYAN);
      _banner.drawString(String("LATEST: [TKT-") + String(ticketId) + "] " + ticketStatus, 32, 8);
      _banner.setTextColor(0x9CF3);
      _banner.drawString(ticketTitle.substring(0, 34) + " [TAP >]", 32, 20);
    }

    _banner.pushSprite(_gfx, 0, 204);
    _gfx->endWrite();
  }

  void drawTicketListScreen(const TicketItem tickets[], int count) {
    _gfx->startWrite();
    _gfx->fillScreen(0x0821);

    // Top Header Bar
    _gfx->fillRect(0, 0, 320, 26, C_METAL_DARK);
    _gfx->drawFastHLine(0, 26, 320, C_CRIMSON);

    _gfx->setTextSize(1);
    _gfx->setTextColor(C_CRIMSON);
    _gfx->drawString("RESOLVE // INCIDENTS", 10, 8);

    // Return to Pet Touch Pill in Top Right
    _gfx->fillRect(230, 3, 86, 20, 0x10A2);
    _gfx->drawRect(230, 3, 86, 20, C_SOUL_CYAN);
    _gfx->setTextColor(C_SOUL_CYAN);
    _gfx->drawString("< PET 🐾", 244, 8);

    // 4 Ticket Rows
    for (int i = 0; i < 4; i++) {
      int rowY = 32 + (i * 42);
      if (i < count) {
        const TicketItem &t = tickets[i];
        uint16_t statCol = (t.status == "resolved") ? C_ACID_GREEN : ((t.status == "in_progress") ? C_FUEL_AMBER : C_COMBAT_RED);
        
        // Card Background
        _gfx->fillRect(8, rowY, 304, 38, 0x10A2);
        _gfx->drawRect(8, rowY, 304, 38, 0x2965);

        // Status indicator accent strip
        _gfx->fillRect(8, rowY, 4, 38, statCol);

        // Ticket ID & Status
        _gfx->setTextSize(1);
        _gfx->setTextColor(statCol);
        String idStr = String("#") + String(t.id) + " " + t.status;
        idStr.toUpperCase();
        _gfx->drawString(idStr, 18, rowY + 5);

        // Priority Badge
        _gfx->setTextColor(0x9CF3);
        String prioStr = t.priority;
        prioStr.toUpperCase();
        _gfx->drawString(String("PRIO: ") + prioStr, 130, rowY + 5);

        // Arrow indicator
        _gfx->setTextColor(C_SOUL_CYAN);
        _gfx->drawString("TAP >", 268, rowY + 5);

        // Title
        _gfx->setTextColor(C_WHITE);
        _gfx->drawString(t.title.substring(0, 44), 18, rowY + 20);
      } else {
        // Empty Slot
        _gfx->fillRect(8, rowY, 304, 38, 0x0821);
        _gfx->drawRect(8, rowY, 304, 38, 0x18E3);
        _gfx->setTextColor(0x528A);
        _gfx->drawString("-- NO ADDITIONAL INCIDENTS --", 55, rowY + 14);
      }
    }

    // Bottom Navigation Bar
    _gfx->fillRect(0, 206, 320, 34, C_METAL_DARK);
    _gfx->drawFastHLine(0, 206, 320, 0x2104);
    _gfx->setTextSize(1);
    _gfx->setTextColor(C_ACID_GREEN);
    _gfx->drawString("SELECT ANY INCIDENT TO OPEN DETAILED DOSSIER", 16, 218);

    _gfx->endWrite();
  }

  void drawTicketDetailScreen(const TicketItem &t) {
    _gfx->startWrite();
    _gfx->fillScreen(0x0821);

    // Top Header
    _gfx->fillRect(0, 0, 320, 26, C_METAL_DARK);
    _gfx->drawFastHLine(0, 26, 320, C_CRIMSON);

    // Back to list button (top left)
    _gfx->fillRect(6, 3, 70, 20, 0x2104);
    _gfx->drawRect(6, 3, 70, 20, C_FUEL_AMBER);
    _gfx->setTextColor(C_FUEL_AMBER);
    _gfx->setTextSize(1);
    _gfx->drawString("< LIST", 18, 8);

    _gfx->setTextColor(C_WHITE);
    _gfx->drawString(String("INCIDENT #") + String(t.id), 105, 8);

    // Return to pet button (top right)
    _gfx->fillRect(244, 3, 70, 20, 0x10A2);
    _gfx->drawRect(244, 3, 70, 20, C_SOUL_CYAN);
    _gfx->setTextColor(C_SOUL_CYAN);
    _gfx->drawString("PET 🐾", 258, 8);

    // Card Body
    _gfx->fillRect(8, 32, 304, 166, 0x10A2);
    _gfx->drawRect(8, 32, 304, 166, 0x3D9E);

    // Status pill
    uint16_t statCol = (t.status == "resolved") ? C_ACID_GREEN : ((t.status == "in_progress") ? C_FUEL_AMBER : C_COMBAT_RED);
    _gfx->fillRect(16, 40, 80, 18, 0x0821);
    _gfx->drawRect(16, 40, 80, 18, statCol);
    _gfx->setTextColor(statCol);
    String statText = t.status;
    statText.toUpperCase();
    _gfx->drawString(statText, 22, 45);

    // Priority pill
    _gfx->fillRect(104, 40, 80, 18, 0x0821);
    _gfx->drawRect(104, 40, 80, 18, C_CRIMSON);
    _gfx->setTextColor(C_CRIMSON);
    String prioText = t.priority;
    prioText.toUpperCase();
    _gfx->drawString(String("PRIO: ") + prioText, 110, 45);

    // Requester info
    _gfx->setTextColor(0x9CF3);
    _gfx->drawString(String("BY: ") + t.requester + " (" + t.email.substring(0, 24) + ")", 16, 66);

    // Title Box
    _gfx->fillRect(14, 80, 292, 30, 0x0821);
    _gfx->drawRect(14, 80, 292, 30, 0x2965);
    _gfx->setTextColor(C_WHITE);
    _gfx->drawString(t.title.substring(0, 46), 20, 86);
    if (t.title.length() > 46) {
      _gfx->drawString(t.title.substring(46, 92), 20, 98);
    }

    // Description Box
    _gfx->fillRect(14, 116, 292, 74, 0x0821);
    _gfx->drawRect(14, 116, 292, 74, 0x2104);
    _gfx->setTextColor(C_SHADOW_FUR);
    
    // Draw wrapped description lines
    String desc = t.description;
    if (desc.length() == 0) desc = "Operational incident recorded in Resolve ServiceDesk. All forensic checks passing.";
    int lineY = 122;
    for (int p = 0; p < desc.length() && lineY < 180; p += 44) {
      _gfx->drawString(desc.substring(p, p + 44), 20, lineY);
      lineY += 12;
    }

    // Bottom Navigation Bar
    _gfx->fillRect(0, 204, 320, 36, C_METAL_DARK);
    _gfx->drawFastHLine(0, 204, 320, C_CRIMSON);

    _gfx->fillRect(12, 208, 140, 26, 0x2104);
    _gfx->drawRect(12, 208, 140, 26, C_FUEL_AMBER);
    _gfx->setTextColor(C_FUEL_AMBER);
    _gfx->drawString("< BACK TO LIST", 32, 216);

    _gfx->fillRect(168, 208, 140, 26, 0x10A2);
    _gfx->drawRect(168, 208, 140, 26, C_SOUL_CYAN);
    _gfx->setTextColor(C_SOUL_CYAN);
    _gfx->drawString("RETURN TO PET 🐾", 184, 216);

    _gfx->endWrite();
  }

  void drawDataholicsEmblem(int cx, int cy, float s, uint16_t bgCol, uint16_t wingCol, int splitX = 0) {
    int wOut = (int)(46.0f * s);
    int wIn  = (int)(6.0f * s);
    int hOut = (int)(23.0f * s);
    int hIn  = (int)(30.0f * s);
    int cOff = (int)(25.0f * s);
    int cRad = (int)(13.0f * s);
    int bHalf = max(2, (int)(5.0f * s));

    int lxO = cx - wOut - splitX;
    int lxI = cx - wIn - splitX;
    _gfx->fillTriangle(lxO, cy - hOut, lxI, cy - hIn, lxI, cy + hIn, wingCol);
    _gfx->fillTriangle(lxO, cy - hOut, lxI, cy + hIn, lxO, cy + hOut, wingCol);

    int rxI = cx + wIn + splitX;
    int rxO = cx + wOut + splitX;
    _gfx->fillTriangle(rxI, cy - hIn, rxO, cy - hOut, rxO, cy + hOut, wingCol);
    _gfx->fillTriangle(rxI, cy - hIn, rxO, cy + hOut, rxI, cy + hIn, wingCol);

    _gfx->fillCircle(cx - cOff - splitX, cy, cRad, bgCol);
    _gfx->fillCircle(cx + cOff + splitX, cy, cRad, bgCol);
    _gfx->fillRect(cx - cOff - splitX, cy - bHalf, (cOff + splitX) * 2, bHalf * 2, bgCol);
  }

  void playBootAnimation(void (*audioCb)(int, int)) {
    _gfx->fillScreen(0x0000);
    for (int y = 0; y < 240; y += 20) {
      _gfx->drawFastHLine(0, y, 320, 0x10A2);
    }
    for (int t = 0; t <= 18; t++) {
      int splitX = (18 - t) * 4;
      uint16_t wingCol = (t == 18) ? C_WHITE : C_CRIMSON;
      drawDataholicsEmblem(160, 90, 1.4f, 0x0000, wingCol, splitX);
      if (t % 3 == 0 && audioCb) {
        audioCb(440 + (t * 40), 20);
      }
      delay(25);
    }
    _gfx->drawRect(60, 36, 200, 110, 0x3D9E);
    _gfx->drawRect(58, 34, 204, 114, C_CRIMSON);

    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(3);
    _gfx->drawString("DATAHOLICS", 64, 160);

    _gfx->setTextColor(0x3D9E);
    _gfx->setTextSize(1);
    _gfx->drawString("SOVEREIGN AI // YOKAI FAMILIAR", 55, 192);

    _gfx->setTextColor(C_ACID_GREEN);
    _gfx->drawString("HARDWARE ENCLAVE MOUNTED [COM6]", 62, 212);

    if (audioCb) {
      audioCb(880, 60);
      audioCb(1174, 120);
    }
    delay(1200);

    // CRITICAL: Wipe all boot logo vectors & text from the display
    _gfx->fillScreen(C_BG_DARK);
  }

  void clearScreen() {
    _gfx->fillScreen(C_BG_DARK);
  }

  void drawConferenceBadge(const String &nameFirst, const String &nameLast, const String &role, const String &mac) {
    _gfx->startWrite();
    _gfx->fillScreen(0x0821);

    _gfx->fillRect(0, 0, 320, 24, C_METAL_DARK);
    _gfx->drawFastHLine(0, 24, 320, C_CRIMSON);

    _gfx->setTextSize(1);
    _gfx->setTextColor(C_CRIMSON);
    _gfx->drawString("DATAHOLICS // SOVEREIGN AI", 12, 7);
    _gfx->setTextColor(0x3D9E);
    _gfx->drawString("VIP CLEARANCE 0", 215, 7);

    drawDataholicsEmblem(160, 48, 0.75f, 0x0821, C_CRIMSON, 0);

    _gfx->fillRect(16, 74, 288, 116, 0x10A2);
    _gfx->drawRect(16, 74, 288, 116, 0x3D9E);
    _gfx->drawRect(17, 75, 286, 114, 0x3D9E);

    _gfx->fillRect(16, 74, 18, 4, C_CRIMSON);
    _gfx->fillRect(16, 74, 4, 18, C_CRIMSON);
    _gfx->fillRect(286, 74, 18, 4, C_CRIMSON);
    _gfx->fillRect(300, 74, 4, 18, C_CRIMSON);
    _gfx->fillRect(16, 186, 18, 4, C_CRIMSON);
    _gfx->fillRect(16, 172, 4, 18, C_CRIMSON);
    _gfx->fillRect(286, 186, 18, 4, C_CRIMSON);
    _gfx->fillRect(300, 172, 4, 18, C_CRIMSON);

    _gfx->setTextSize(3);
    _gfx->setTextColor(C_WHITE);
    _gfx->drawString(nameFirst, 40, 88);

    _gfx->setTextColor(0x3D9E);
    _gfx->drawString(nameLast, 40, 118);

    _gfx->fillRect(36, 150, 248, 26, 0x2104);
    _gfx->drawRect(36, 150, 248, 26, C_CRIMSON);
    _gfx->setTextColor(C_CRIMSON);
    _gfx->setTextSize(1);
    _gfx->drawString("CEO • CHIEF EXECUTIVE OFFICER", 46, 158);

    _gfx->fillRect(0, 204, 320, 36, C_METAL_DARK);
    _gfx->drawFastHLine(0, 204, 320, 0x2104);

    _gfx->setTextSize(1);
    _gfx->setTextColor(0x9CF3);
    _gfx->drawString(String("SILICON ID: ") + mac + " • COM6", 16, 210);
    _gfx->setTextColor(C_ACID_GREEN);
    _gfx->drawString("STATUS: EXECUTIVE PASS ACTIVE • TAP TO RETURN", 16, 224);

    _gfx->endWrite();
  }

  void drawEnrollmentScreen(const String &siliconMac, const String &statusText) {
    _gfx->fillScreen(0x0000);
    _gfx->drawRect(10, 10, 300, 220, C_ACID_GREEN);
    _gfx->drawRect(12, 12, 296, 216, C_ACID_GREEN);

    _gfx->setTextColor(C_ACID_GREEN);
    _gfx->setTextSize(2);
    _gfx->drawString("HARDWARE BINDING", 34, 25);

    _gfx->setTextColor(C_WHITE);
    _gfx->setTextSize(1);
    _gfx->drawString("FACTORY SILICON eFUSE MAC:", 34, 60);

    _gfx->fillRect(30, 75, 260, 30, C_METAL_DARK);
    _gfx->drawRect(30, 75, 260, 30, C_CRIMSON);
    _gfx->setTextColor(0x07FF);
    _gfx->setTextSize(2);
    _gfx->drawString(siliconMac, 42, 82);

    _gfx->setTextColor(0x9CF3);
    _gfx->setTextSize(1);
    _gfx->drawString("Status:", 34, 120);
    _gfx->setTextColor(C_CRIMSON);
    _gfx->drawString(statusText, 90, 120);

    _gfx->drawString("Enroll this MAC in Resolve ServiceDesk", 34, 140);
    _gfx->drawString("python scratch/enroll_digipet.py <MAC>", 34, 154);

    // Wi-Fi Setup Button
    _gfx->fillRect(30, 172, 260, 26, 0x10A2);
    _gfx->drawRect(30, 172, 260, 26, C_SOUL_CYAN);
    _gfx->setTextColor(C_SOUL_CYAN);
    _gfx->drawString("[ 📡 TAP FOR WI-FI SETUP PORTAL ]", 44, 180);

    _gfx->setTextColor(C_SHADOW_FUR);
    _gfx->drawString("Tap outside button to return to Yokai Familiar", 36, 208);
  }
};
