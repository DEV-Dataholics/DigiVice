#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "cyd_display.h"
#include "audio_synth.h"
#include "auth_crypto.h"
#include "pet_renderer.h"
#include "wifi_manager.h"

// ============================================================================
// Dataholics Resolve ServiceDesk - Scrap-Hound Digipet
// Cute Horror / Mad Max Edition (Hardware Cryptographic Silicon Binding)
// Target Board: ESP32-2432S028 (CYD 2.8" ST7789 320x240)
// ============================================================================

// Onboard Hardware Pinout (ESP32-2432S028 CYD)
#define RGB_LED_R       4    // Active LOW
#define RGB_LED_G      16    // Active LOW
#define RGB_LED_B      17    // Active LOW
#define LDR_PIN        34    // Ambient Light Sensor
#define BOOT_BTN_PIN    0    // Physical BOOT Button on back (Active LOW)

// Resolve ServiceDesk Secure Feed URL
const char* RESOLVE_FEED_URL = "https://resolve.dataholics.com.mx/digipet_feed.php";

// Pre-Shared Hardware Cryptographic Secret Key
// (Matched in Resolve DB `digipet_hardware` table via scratch/enroll_digipet.py)
String DEVICE_SECRET_KEY = "757618edd61e1f81993949992d11b46eb10d62db902c5e8d674229f52e48cc85";

// Hardware & Peripheral Instances
LGFX gfx;
ScrapHoundAudio soundFx;
ScrapHoundRenderer petRenderer(&gfx);
DigiPetWiFiManager wifiManager;

// State Tracking
String siliconMac = "";
PetMood currentMood = MOOD_BOOT;
unsigned long moodExpiryMs = 0;

int prevTicketId = 0;
int prevCommentId = 0;
int prevUpdatedTs = 0;
int prevResolvedTs = 0;
String prevStatus = "";

int activeTicketCount = 0;
int resolvedTodayCount = 0;
String latestTicketTitle = "Awaiting Uplink...";
int latestTicketId = 0;
String latestTicketStatus = "idle";

// Interactive Screen Modes
enum ScreenMode {
  SCREEN_PET,           // Living Yokai Familiar companion
  SCREEN_BADGE,         // Alonso Ruiz CEO Conference Badge
  SCREEN_TICKET_LIST,   // ServiceDesk Recent Incidents Stream
  SCREEN_TICKET_DETAIL, // Deep Ticket Forensic Dossier
  SCREEN_ENROLLMENT     // Hardware Silicon MAC Enclave
};

ScreenMode currentScreen = SCREEN_PET;
int selectedTicketIdx = 0;
TicketItem cachedTickets[4];
int cachedTicketCount = 0;
unsigned long lastScreenInputMs = 0;

// Timing & Modes
unsigned long lastPollMs = 0;
const unsigned long POLL_INTERVAL_MS = 3500;
unsigned long touchStartTime = 0;
bool prevBootBtn = HIGH;
unsigned long bootBtnPressTime = 0;

void audioPlayToneWrapper(int freq, int durationMs) {
  soundFx.playTone(freq, durationMs);
}

// RGB Underglow Control (Active LOW)
void setRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  digitalWrite(RGB_LED_R, r ? LOW : HIGH);
  digitalWrite(RGB_LED_G, g ? LOW : HIGH);
  digitalWrite(RGB_LED_B, b ? LOW : HIGH);
}

void updateUnderglow() {
  switch (currentMood) {
    case MOOD_ALARM:
      // Flashing Crimson Red Strobe
      if ((millis() / 150) % 2 == 0) {
        setRgbLed(255, 0, 0);
      } else {
        setRgbLed(0, 0, 0);
      }
      break;

    case MOOD_LAUGH:
      // Flickering Mad Max Fuel Amber
      if ((millis() / 80) % 2 == 0) {
        setRgbLed(255, 120, 0);
      } else {
        setRgbLed(180, 40, 0);
      }
      break;

    case MOOD_CHOMP:
      // Radioactive Toxic Green / Electric Purple Pulse
      if ((millis() / 200) % 2 == 0) {
        setRgbLed(0, 255, 60);
      } else {
        setRgbLed(180, 0, 255);
      }
      break;

    case MOOD_POKED:
      setRgbLed(255, 0, 60);
      break;

    case MOOD_IDLE:
    default:
      if (petRenderer.isSleeping()) {
        // Deep slumber gentle breathing nightlight (Ocean Blue)
        uint8_t breath = ((millis() / 80) % 2 == 0) ? 40 : 10;
        setRgbLed(0, 0, breath);
      } else if (petRenderer.isDozing()) {
        // Drowsy soft teal
        setRgbLed(0, 20, 20);
      } else if (WiFi.status() == WL_CONNECTED) {
        setRgbLed(0, 30, 0); // Active Green Heartbeat
      } else {
        setRgbLed(30, 0, 0); // Warning red when WiFi down
      }
      break;
  }
}

// Secure Hardware-Bound Polling
void pollResolveFeed() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Rely on hardware HMAC-SHA256 signature for authentication
  client.setTimeout(2);

  HTTPClient http;
  http.setTimeout(2000);
  http.setReuse(false);
  if (!http.begin(client, RESOLVE_FEED_URL)) {
    Serial.println("[HTTP] Begin failed");
    return;
  }

  // 1. Prepare Cryptographic Headers
  time_t now = time(nullptr);
  String ts = String((unsigned long)now);
  String nonce = HardwareAuth::generateNonce();
  String payload = "DIGIPET|" + siliconMac + "|" + ts + "|" + nonce;
  String signature = HardwareAuth::computeHmacSha256(DEVICE_SECRET_KEY, payload);

  http.addHeader("User-Agent", "ESP32-ScrapHound/1.0");
  http.addHeader("X-Device-Mac", siliconMac);
  http.addHeader("X-Timestamp", ts);
  http.addHeader("X-Nonce", nonce);
  http.addHeader("X-Signature", signature);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String resp = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);

    if (!err) {
      int tktId = doc["latest_ticket"]["id"];
      const char* title = doc["latest_ticket"]["title"];
      const char* status = doc["latest_ticket"]["status"];
      int updTs = doc["latest_ticket"]["updated_ts"];
      int resTs = doc["latest_ticket"]["resolved_ts"];
      int cmtId = doc["latest_comment"]["id"];

      activeTicketCount = doc["metrics"]["active"];
      resolvedTodayCount = doc["metrics"]["resolved"];
      latestTicketId = tktId;
      latestTicketTitle = String(title);
      latestTicketStatus = String(status);

      // EVENT DISPATCH LOGIC:
      // 1. New Ticket Opened (intake alert)
      if (prevTicketId > 0 && tktId > prevTicketId) {
        currentMood = MOOD_ALARM;
        moodExpiryMs = millis() + 9000;
        soundFx.playWarKlaxon();
      }
      // 2. Ticket Resolved (evil chomp celebration!)
      else if (String(status) == "resolved" && (prevStatus != "resolved" || resTs > prevResolvedTs)) {
        if (prevTicketId > 0) { // Only trigger on transition
          currentMood = MOOD_CHOMP;
          moodExpiryMs = millis() + 8000;
          petRenderer.setShake(6);
          soundFx.playEvilChompFanfare();
        }
      }
      // 3. Ticket Updated (new comment or state modification -> maniacal laugh!)
      else if (prevCommentId > 0 && (cmtId > prevCommentId || (updTs > prevUpdatedTs && String(status) != "resolved"))) {
        currentMood = MOOD_LAUGH;
        moodExpiryMs = millis() + 5000;
        soundFx.playManiacalLaugh();
      }

      // Update baseline state
      prevTicketId = tktId;
      prevCommentId = cmtId;
      prevUpdatedTs = updTs;
      prevResolvedTs = resTs;
      prevStatus = String(status);

      // Cache recent tickets for interactive explorer
      JsonArray recArr = doc["recent_tickets"];
      if (recArr) {
        cachedTicketCount = 0;
        for (JsonObject item : recArr) {
          if (cachedTicketCount >= 4) break;
          cachedTickets[cachedTicketCount].id = item["id"] | 0;
          cachedTickets[cachedTicketCount].title = item["title"] | "No Title";
          cachedTickets[cachedTicketCount].status = item["status"] | "open";
          cachedTickets[cachedTicketCount].priority = item["priority"] | "normal";
          cachedTickets[cachedTicketCount].requester = item["requester_name"] | "User";
          cachedTickets[cachedTicketCount].email = item["requester_email"] | "";
          cachedTickets[cachedTicketCount].description = item["description"] | "";
          cachedTicketCount++;
        }
      }
    }
  } else {
    Serial.printf("[HTTP] Error code: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  // Initialize RGB LED Pins
  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  setRgbLed(0, 0, 0);

  // Initialize Buzzer & LDR
  soundFx.begin();
  pinMode(LDR_PIN, INPUT);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  // Initialize Screen
  gfx.begin();
  gfx.setRotation(1); // 320x240 Landscape
  gfx.setBrightness(240);

  petRenderer.begin();

  // Extract Silicon MAC
  siliconMac = HardwareAuth::getSiliconMac();
  Serial.println("==========================================");
  Serial.println(" DATAHOLICS SCRAP-HOUND DIGIPET v1.0");
  Serial.printf (" SILICON MAC: %s\n", siliconMac.c_str());
  Serial.println("==========================================");

  // Play Dataholics Logo Boot Animation
  petRenderer.playBootAnimation(audioPlayToneWrapper);

  // Initialize Wi-Fi Network Manager
  wifiManager.begin();

  // If BOOT button is held down on power-on, launch Wi-Fi Config Portal immediately
  if (digitalRead(BOOT_BTN_PIN) == LOW) {
    Serial.println("[BOOT] Forced Wi-Fi Config Portal requested on power-on.");
    soundFx.playTap();
    wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
  }

  // Attempt connection with stored credentials (defaults to Monkeyskull2)
  bool connected = wifiManager.connectStored(&gfx, setRgbLed, 8);
  if (!connected) {
    Serial.println("[WIFI] Stored connection failed. Auto-launching Config Portal...");
    bool newCreds = wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
    if (newCreds) {
      connected = wifiManager.connectStored(&gfx, setRgbLed, 10);
    }
  }

  if (connected) {
    Serial.println("[WIFI] Connected! Syncing NTP...");
    setRgbLed(0, 255, 0);
    HardwareAuth::syncNtp();
    currentMood = MOOD_IDLE;
  } else {
    Serial.println("[WIFI] Connection unavailable. Running in offline/demo mode.");
    currentMood = MOOD_IDLE;
  }

  soundFx.playTap();
  petRenderer.clearScreen();
  pollResolveFeed();
}

void loop() {
  unsigned long now = millis();

  // 0. Physical BOOT Button: Short Press = Badge Mode | Long Press (>= 2.5s) = Wi-Fi Setup Portal
  bool curBootBtn = digitalRead(BOOT_BTN_PIN);
  if (curBootBtn == LOW && prevBootBtn == HIGH) {
    bootBtnPressTime = now;
  } else if (curBootBtn == HIGH && prevBootBtn == LOW && now > lastScreenInputMs) {
    unsigned long holdMs = now - bootBtnPressTime;
    lastScreenInputMs = now + 400;
    if (holdMs >= 2500) {
      Serial.println("[BOOT] Long press: Launching Wi-Fi Config Portal...");
      soundFx.playTap();
      wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
      currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
    } else {
      if (currentScreen == SCREEN_BADGE) currentScreen = SCREEN_PET;
      else currentScreen = SCREEN_BADGE;
      petRenderer.clearScreen();
      soundFx.playTap();
    }
  }
  prevBootBtn = curBootBtn;

  // Poll Resolve ServiceDesk (background polling)
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    pollResolveFeed();
  }

  // 1. SCREEN_BADGE Mode (Alonso Ruiz / CEO)
  if (currentScreen == SCREEN_BADGE) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 400;
      currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
      soundFx.playTap();
      return;
    }
    petRenderer.drawConferenceBadge("ALONSO", "RUIZ", "CEO", siliconMac);
    bool ledStep = (now / 400) % 2;
    if (ledStep) setRgbLed(255, 120, 0); // Gold
    else setRgbLed(0, 180, 255);        // Electric Cyan
    delay(60);
    return;
  }

  // 2. SCREEN_TICKET_LIST Mode (Recent Incidents Stream)
  if (currentScreen == SCREEN_TICKET_LIST) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 350;
      // Top right "< PET" or bottom bar -> Return to Pet
      if ((touchY < 28 && touchX > 220) || touchY >= 204) {
        currentScreen = SCREEN_PET;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      // Check Row clicks
      for (int i = 0; i < 4; i++) {
        int rowY = 32 + (i * 42);
        if (touchY >= rowY && touchY < (rowY + 40) && i < cachedTicketCount) {
          selectedTicketIdx = i;
          currentScreen = SCREEN_TICKET_DETAIL;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
      }
    }
    petRenderer.drawTicketListScreen(cachedTickets, cachedTicketCount);
    setRgbLed(0, 120, 255); // Cyan inspection underglow
    delay(50);
    return;
  }

  // 3. SCREEN_TICKET_DETAIL Mode (Deep Ticket Dossier)
  if (currentScreen == SCREEN_TICKET_DETAIL) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 350;
      // "< LIST" button (top left or bottom left)
      if ((touchY < 28 && touchX < 100) || (touchY >= 204 && touchX < 160)) {
        currentScreen = SCREEN_TICKET_LIST;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      // "PET" button (top right or bottom right)
      if ((touchY < 28 && touchX > 220) || (touchY >= 204 && touchX >= 160)) {
        currentScreen = SCREEN_PET;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
    }
    if (selectedTicketIdx < cachedTicketCount) {
      petRenderer.drawTicketDetailScreen(cachedTickets[selectedTicketIdx]);
    } else {
      currentScreen = SCREEN_TICKET_LIST;
    }
    setRgbLed(255, 120, 0); // Amber dossier underglow
    delay(50);
    return;
  }

  // 4. SCREEN_ENROLLMENT Mode (Hardware Silicon Enclave)
  if (currentScreen == SCREEN_ENROLLMENT) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 400;
      // Check if user tapped the "[ 📡 TAP FOR WI-FI SETUP PORTAL ]" button
      if (touchY >= 165 && touchY <= 204 && touchX >= 25 && touchX <= 295) {
        Serial.println("[TOUCH] Wi-Fi Portal requested from Enrollment Enclave.");
        soundFx.playTap();
        wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
        currentScreen = SCREEN_PET;
        petRenderer.clearScreen();
        return;
      }
      currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
      soundFx.playTap();
      return;
    }
    petRenderer.drawEnrollmentScreen(siliconMac, (WiFi.status() == WL_CONNECTED) ? "ONLINE" : "OFFLINE");
    delay(80);
    return;
  }

  // 5. SCREEN_PET (Main Yokai Familiar Arena)
  // Reset mood after expiry
  if (moodExpiryMs > 0 && now >= moodExpiryMs) {
    currentMood = MOOD_IDLE;
    moodExpiryMs = 0;
    petRenderer.setShake(0);
  }

  // Handle Touch Inputs
  uint16_t touchX, touchY;
  bool touched = gfx.getTouch(&touchX, &touchY);

  if (touched) {
    if (touchStartTime == 0) {
      touchStartTime = now;
      if (now > lastScreenInputMs) {
        // Direct Touch Zone 1: Top-Left "🪪 ALONSO (CEO)" pill -> Open Badge Mode!
        if (touchY < 26 && touchX < 125) {
          lastScreenInputMs = now + 400;
          currentScreen = SCREEN_BADGE;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
        // Direct Touch Zone 2: Ticket Ticker (Bottom) or Counter Pills (Top Right) -> Open Ticket Explorer!
        else if (touchY >= 200 || (touchY < 26 && touchX >= 125)) {
          lastScreenInputMs = now + 400;
          currentScreen = SCREEN_TICKET_LIST;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
        // Direct Touch Zone 3: Poke the Pet!
        else {
          petRenderer.wakeUp(); // Wake up with a startle if asleep!
          currentMood = MOOD_POKED;
          moodExpiryMs = now + 1400;
          soundFx.playGrowl();
        }
      }
    } else if (now - touchStartTime > 2000) {
      // 2-second hold in center enters Silicon Enrollment Enclave
      currentScreen = SCREEN_ENROLLMENT;
      petRenderer.clearScreen();
      touchStartTime = now + 1500;
      soundFx.playTap();
      return;
    }
  } else {
    touchStartTime = 0;
  }

  // Render Scrap-Hound frame
  petRenderer.renderFrame(currentMood, latestTicketTitle, latestTicketId, latestTicketStatus, activeTicketCount, resolvedTodayCount, siliconMac);
  updateUnderglow();

  delay(35); // ~28-30 FPS smooth animation loop
}
