#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <DHT.h>
#include <ArduinoJson.h>

// SH1106 Page-Buffer Hardware I2C Driver
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BTN_PIN 15      // Pushbutton to GND
#define LED_GREEN 16    // Host Normal
#define LED_RED 17      // Host Down / Error
#define BUZZER 18       // Audio chirps

const char* ssid = "Monkeyskull2";
const char* password = "6561766707";
const char* serverUrl = "http://192.168.1.40:8000/metrics";

// 0 = Rogue AI HUD View, 1 = Full-Screen 2x Rogue AI View, 2 = Raw Server Telemetry
int currentMode = 0;
unsigned long lastPoll = 0;
unsigned long lastAnim = 0;
int particleY = 0;

// Telemetry state
float cpuVal = 0.0, ramVal = 0.0, prevCpuVal = 0.0;
float diskVal = 0.0, load1m = 0.0;
float roomTemp = 0.0, roomHum = 0.0;
String pollerState = "unknown";
String ollamaState = "unknown";
bool serverOnline = false;
int lastHttpCode = 0;

// Rogue AI Evolution stage enum
enum DigiStage {
  STAGE_SLIME   = 0,   // < 30% CPU (BIT-SLIME: Idle Data-Blob)
  STAGE_PHANTOM = 1,   // 30% - 70% CPU (PHANTOM-NODE: Active Cloaked Wraith)
  STAGE_DAEMON  = 2,   // >= 70% CPU (DAEMON-CORE: Overload Seraphim)
  STAGE_TOMB    = 3    // Host Unreachable / Offline (404-GHOST: System Tombstone)
};
DigiStage currentStage = STAGE_TOMB;
DigiStage prevStage = STAGE_TOMB;

// ============================================================================
// The Rogue AI Evolutionary Arc (24x24 Grid, LSB-first for u8g2.drawXBMP)
// ============================================================================

// 24x24 Stage 1: BIT-SLIME (Idle < 30%)
const unsigned char ep_ai_slime[] PROGMEM = {
  0x00, 0x18, 0x00, 0x00, 0x3c, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xff, 0x01, 0x80, 0xff, 0x07, 0xc0, 0xff, 0x0f, 0xe0, 0xff, 0x1f,
  0xf0, 0x3c, 0x3c, 0xf0, 0x3c, 0x3c, 0xf0, 0x3c, 0x3c, 0xf0, 0xff, 0x3f,
  0xf0, 0xff, 0x3f, 0xe0, 0xff, 0x1f, 0xc0, 0xdb, 0x0d, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 24x24 Stage 2: PHANTOM-NODE (Active 30% - 70%)
const unsigned char ep_ai_phantom[] PROGMEM = {
  0x00, 0x3c, 0x00, 0x00, 0xff, 0x00, 0x80, 0xff, 0x01, 0xc0, 0xff, 0x03,
  0xe0, 0x00, 0x07, 0x70, 0x66, 0x0e, 0x70, 0x66, 0x0e, 0x70, 0x00, 0x0e,
  0xe0, 0xff, 0x1f, 0xf0, 0xff, 0x3f, 0xf0, 0xff, 0x3f, 0xe0, 0xff, 0x1f,
  0xe0, 0x7e, 0x1c, 0xc0, 0x5a, 0x0c, 0x00, 0x99, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 24x24 Stage 3: DAEMON-CORE (Overload > 70%)
const unsigned char ep_ai_daemon[] PROGMEM = {
  0x04, 0x18, 0x20, 0x0e, 0x3c, 0x70, 0x1f, 0x7e, 0xf8, 0xfe, 0x03, 0x7f,
  0xfc, 0x19, 0x3f, 0xf8, 0x3c, 0x1f, 0xf0, 0x19, 0x0f, 0xfc, 0x03, 0x3f,
  0xde, 0xff, 0x7b, 0x0f, 0xff, 0xf0, 0x06, 0x7e, 0x60, 0x00, 0x66, 0x00,
  0x00, 0xc3, 0x00, 0x80, 0x99, 0x01, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// 24x24 Offline: 404-GHOST (Tombstone)
const unsigned char ep_ai_tomb[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0xff, 0x01, 0x80, 0xff, 0x07, 0xc0, 0xff, 0x0f,
  0xe0, 0xe7, 0x1f, 0xe0, 0xe7, 0x1f, 0xe0, 0x00, 0x1f, 0xe0, 0x00, 0x1f,
  0xe0, 0xe7, 0x1f, 0xe0, 0xe7, 0x1f, 0xe0, 0xe7, 0x1f, 0xe0, 0xff, 0x1f,
  0xe0, 0xff, 0x1f, 0xf0, 0xff, 0x3f, 0xf8, 0xff, 0x7f, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ============================================================================
// Dataholics Official Brand Icono (64x40 XBM, LSB-first for u8g2.drawXBMP)
// ============================================================================
const unsigned char xbm_dataholics_icono[] PROGMEM = {
  0x00, 0x00, 0x00, 0x0f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x0f, 0xf0, 0x3f, 0x00, 0x00,
  0x00, 0xf8, 0xff, 0x0f, 0xf0, 0xff, 0x1f, 0x00, 0xf0, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0x0f,
  0xfc, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0x7f, 0xfe, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0x7f,
  0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x07, 0xff, 0x0f, 0xf0, 0xff, 0xe0, 0xff,
  0xff, 0x01, 0xf8, 0x0f, 0xf0, 0x3f, 0x80, 0xff, 0xff, 0x00, 0xf0, 0x0f, 0xf0, 0x0f, 0x00, 0xff,
  0x7f, 0x00, 0xe0, 0x0f, 0xf0, 0x07, 0x00, 0xfe, 0x3e, 0x00, 0xe0, 0x0f, 0xf0, 0x07, 0x00, 0xfc,
  0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8,
  0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78,
  0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78,
  0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf8,
  0x3e, 0x00, 0xc0, 0x0f, 0xf0, 0x07, 0x00, 0xfc, 0x7e, 0x00, 0xe0, 0x0f, 0xf0, 0x07, 0x00, 0xfe,
  0xff, 0x00, 0xf0, 0x0f, 0xf0, 0x0f, 0x00, 0xfe, 0xff, 0x01, 0xf8, 0x0f, 0xf0, 0x1f, 0x80, 0xff,
  0xff, 0x07, 0xfe, 0x0f, 0xf0, 0x7f, 0xe0, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff,
  0xfe, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0x7f,
  0xf8, 0xff, 0xff, 0x0f, 0xf0, 0xff, 0xff, 0x1f, 0x00, 0xfc, 0xff, 0x0f, 0xf0, 0xff, 0x3f, 0x00,
  0x00, 0x00, 0xfe, 0x0f, 0xf0, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0f, 0xf0, 0x01, 0x00, 0x00
};

// ============================================================================
// Audio & Visual Effects
// ============================================================================

void playDigiviceChirp() {
  tone(BUZZER, 1800, 35);
  delay(45);
  tone(BUZZER, 2500, 55);
}

void playEvolutionArpeggio() {
  const int notes[] = { 1200, 1600, 2000, 2600 };
  for (int i = 0; i < 4; i++) {
    tone(BUZZER, notes[i], 55);
    delay(65);
  }
}

void playAlarmChirp() {
  tone(BUZZER, 2400, 150);
}

const unsigned char* getActiveAI() {
  if (!serverOnline) return ep_ai_tomb;
  if (cpuVal < 30.0) return ep_ai_slime;
  if (cpuVal < 70.0) return ep_ai_phantom;
  return ep_ai_daemon;
}

const char* getStageBanner() {
  if (!serverOnline)   return "ERR: 404 OFFLINE";
  if (cpuVal >= 70.0)  return "AI: DAEMON-CORE";
  if (cpuVal >= 30.0)  return "AI: PHANTOM-NODE";
  return "AI: BIT-SLIME";
}

// Helper to draw a 24x24 PROGMEM XBM scaled by `scale` (e.g., 2x = 48x48 pixels)
void drawScaledXBMP24(int x0, int y0, const unsigned char* progmemBmp, int scale) {
  for (int r = 0; r < 24; r++) {
    for (int c = 0; c < 24; c++) {
      int byteIdx = r * 3 + (c >> 3);
      uint8_t b = pgm_read_byte(&progmemBmp[byteIdx]);
      if ((b >> (c & 7)) & 1) {
        u8g2.drawBox(x0 + c * scale, y0 + r * scale, scale, scale);
      }
    }
  }
}

// Screen-flash + 4-note arpeggio when Rogue AI escalates to a higher stage
void triggerEvolutionSequence() {
  for (int f = 0; f < 2; f++) {
    u8g2.firstPage();
    do {
      u8g2.drawBox(0, 0, 128, 64);
      u8g2.setDrawColor(0);
      u8g2.setFont(u8g2_font_7x14B_tr);
      u8g2.drawStr(16, 14, "ESCALATING!!");
      drawScaledXBMP24(40, 16, getActiveAI(), 2);
      u8g2.setDrawColor(1);
    } while (u8g2.nextPage());
    playEvolutionArpeggio();
    delay(80);
  }
}

void updateMoodAndHardware() {
  prevStage = currentStage;

  if (!serverOnline) {
    currentStage = STAGE_TOMB;
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    playAlarmChirp();
    return;
  }

  if (cpuVal < 30.0) {
    currentStage = STAGE_SLIME;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  } else if (cpuVal < 70.0) {
    currentStage = STAGE_PHANTOM;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
  } else {
    currentStage = STAGE_DAEMON;
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
  }

  if (prevStage != STAGE_TOMB && currentStage > prevStage) {
    triggerEvolutionSequence();
  }
}

void fetchServerMetrics() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected! Attempting reconnect to Monkeyskull2...");
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    serverOnline = false;
    lastHttpCode = -99;
  } else {
    WiFiClient client;
    HTTPClient http;
    http.begin(client, serverUrl);
    http.setTimeout(2500);
    int code = http.GET();
    lastHttpCode = code;
    Serial.printf("[HTTP] GET %s -> code: %d (My IP: %s)\n",
                  serverUrl, code, WiFi.localIP().toString().c_str());
    if (code == 200) {
      String payload = http.getString();
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload);
      if (!err) {
        prevCpuVal = cpuVal;
        cpuVal = doc["cpu"] | 0.0f;
        ramVal = doc["ram"] | 0.0f;
        diskVal = doc["disk"] | 0.0f;
        load1m = doc["load_1m"] | 0.0f;
        pollerState = doc["poller"] | "unknown";
        ollamaState = doc["ollama"] | "unknown";
        serverOnline = true;
      } else {
        Serial.printf("[JSON] Parse error: %s\n", err.c_str());
        serverOnline = false;
      }
    } else {
      serverOnline = false;
    }
    http.end();
  }

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) roomTemp = t;
  if (!isnan(h)) roomHum = h;

  updateMoodAndHardware();
}

// ============================================================================
// Mode 0: Rogue AI HUD (24x24 Entity + Binary Rain + Glitch Slice + Gauges)
// ============================================================================
void renderPetHUD() {
  // Update rising data particles every 200ms
  if (millis() - lastAnim > 200) {
    lastAnim = millis();
    particleY = (particleY + 2) % 20;

    if (serverOnline && currentStage == STAGE_PHANTOM) {
      digitalWrite(LED_GREEN, (particleY % 4 == 0) ? HIGH : LOW);
    }
  }

  u8g2.firstPage();
  do {
    // --- TOP YELLOW BAND (Rows 0 - 15) ---
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 13, getStageBanner());
    u8g2.drawHLine(0, 15, 128);

    // --- BOTTOM BLUE BAND (Rows 16 - 63) ---
    int floatY = (serverOnline && (particleY % 4 == 0)) ? 1 : 0;
    int glitchX = (serverOnline && cpuVal >= 70.0) ? random(-2, 3) : 0;

    const unsigned char* activeAI = getActiveAI();
    if (!serverOnline) floatY = 0;

    // Draw 24x24 AI Entity
    u8g2.drawXBMP(4 + glitchX, 22 + floatY, 24, 24, activeAI);

    // Procedural Rising Data Bits around the AI
    if (serverOnline) {
      u8g2.drawBox(2, 42 - particleY, 2, 2);
      u8g2.drawBox(28, 42 - ((particleY + 10) % 20), 2, 2);
    }

    // Overload Glitch Scanline Tear
    if (serverOnline && cpuVal >= 70.0 && (particleY % 4 == 0)) {
      u8g2.drawHLine(2, 24 + random(0, 16), 28);
    }

    // Telemetry Readouts on Right Side
    u8g2.setFont(u8g2_font_6x10_tf);
    if (serverOnline) {
      // CPU Thread Load
      u8g2.drawStr(34, 28, "LOAD");
      u8g2.drawFrame(60, 21, 62, 8);
      int cpuW = map((int)constrain(cpuVal, 0, 100), 0, 100, 0, 58);
      if (cpuW > 0) u8g2.drawBox(62, 23, cpuW, 4);

      // RAM Heap Usage
      u8g2.drawStr(34, 40, "HEAP");
      u8g2.drawFrame(60, 33, 62, 8);
      int ramW = map((int)constrain(ramVal, 0, 100), 0, 100, 0, 58);
      if (ramW > 0) u8g2.drawBox(62, 35, ramW, 4);

      // Node Temp
      char tempStr[24];
      snprintf(tempStr, sizeof(tempStr), "NODE TEMP: %.1fC", roomTemp);
      u8g2.drawStr(8, 58, tempStr);
    } else {
      if (WiFi.status() != WL_CONNECTED) {
        char wBuf[24];
        snprintf(wBuf, sizeof(wBuf), "WIFI ERR (ST:%d)", (int)WiFi.status());
        u8g2.drawStr(32, 30, wBuf);
        u8g2.drawStr(32, 42, "CHK 2.4G / WPA2");
      } else {
        char hBuf[24];
        snprintf(hBuf, sizeof(hBuf), "HTTP ERR: %d", lastHttpCode);
        u8g2.drawStr(32, 30, hBuf);
        String ipStr = WiFi.localIP().toString();
        u8g2.drawStr(32, 42, ipStr.c_str());
      }
      char tempStr[24];
      snprintf(tempStr, sizeof(tempStr), "NODE TEMP: %.1fC", roomTemp);
      u8g2.drawStr(8, 58, tempStr);
    }

    u8g2.drawFrame(0, 0, 128, 64);
  } while (u8g2.nextPage());
}

// ============================================================================
// Mode 1: Full-Screen Giant Rogue AI View (48x48 2x Scaled Entity + Binary Rain)
// ============================================================================
void renderFullScreenPetHUD() {
  if (millis() - lastAnim > 200) {
    lastAnim = millis();
    particleY = (particleY + 2) % 20;
  }

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(4, 13, getStageBanner());
    u8g2.drawHLine(0, 15, 128);

    int floatY = (serverOnline && (particleY % 4 == 0)) ? 2 : 0;
    int glitchX = (serverOnline && cpuVal >= 70.0) ? random(-3, 4) : 0;

    // 48x48 (2x scaled 24x24) Rogue AI centered in Blue Zone
    drawScaledXBMP24(40 + glitchX, 18 + floatY, getActiveAI(), 2);

    // Rising Matrix Data Particles flanking the giant AI
    if (serverOnline) {
      u8g2.drawBox(28, 56 - particleY * 2, 3, 3);
      u8g2.drawBox(96, 56 - (((particleY + 10) % 20) * 2), 3, 3);
    }

    // Overload Glitch Scanline Tear across giant sprite
    if (serverOnline && cpuVal >= 70.0 && (particleY % 4 == 0)) {
      u8g2.drawHLine(32, 22 + random(0, 32), 64);
    }

    // Vertical LOAD and HEAP pillars on left/right edges
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(4, 26, "L");
    u8g2.drawFrame(4, 30, 8, 30);
    int cH = map((int)constrain(cpuVal, 0, 100), 0, 100, 0, 26);
    if (cH > 0) u8g2.drawBox(6, 58 - cH, 4, cH);

    u8g2.drawStr(116, 26, "H");
    u8g2.drawFrame(116, 30, 8, 30);
    int rH = map((int)constrain(ramVal, 0, 100), 0, 100, 0, 26);
    if (rH > 0) u8g2.drawBox(118, 58 - rH, 4, rH);

    u8g2.drawFrame(0, 0, 128, 64);
  } while (u8g2.nextPage());
}

// ============================================================================
// Mode 2: Raw K-2SO Server Telemetry View
// ============================================================================
void renderTelemetryHUD() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(6, 11, "K-2SO RAW TELEMETRY");
    u8g2.drawHLine(0, 15, 128);

    char buf[32];
    snprintf(buf, sizeof(buf), "CPU: %4.1f%%  RAM: %4.1f%%", cpuVal, ramVal);
    u8g2.drawStr(6, 27, buf);

    snprintf(buf, sizeof(buf), "DSK: %4.1f%%  1M:  %4.2f", diskVal, load1m);
    u8g2.drawStr(6, 39, buf);

    snprintf(buf, sizeof(buf), "ENV: %4.1fC  HUM: %3.0f%%", roomTemp, roomHum);
    u8g2.drawStr(6, 51, buf);

    snprintf(buf, sizeof(buf), "POLLER:%s OLLAMA:%s",
             (pollerState == "active") ? "ON" : "OFF",
             (ollamaState == "active") ? "ON" : "OFF");
    u8g2.drawStr(6, 61, buf);

    u8g2.drawFrame(0, 0, 128, 64);
  } while (u8g2.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  Wire.begin(21, 22);
  Wire.setClock(100000);

  u8g2.begin();
  u8g2.setContrast(128);

  dht.begin();

  // Phase 1 Boot Screen: Dataholics Official Brand Icono Splash
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_7x14B_tr);
    u8g2.drawStr(24, 13, "DATAHOLICS");
    u8g2.drawHLine(0, 15, 128);
    // Draw 64x40 Dataholics Icono centered in the Blue Zone (x=32, y=20)
    u8g2.drawXBMP(32, 20, 64, 40, xbm_dataholics_icono);
  } while (u8g2.nextPage());

  // Play startup chime while holding the Dataholics Icono on screen
  tone(BUZZER, 1400, 60);
  delay(80);
  tone(BUZZER, 2100, 90);
  delay(1800);

  // Phase 2 Boot Screen: Linking to K-2SO Droid while Wi-Fi associates
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(14, 11, "DATAHOLICS // TiRA");
    u8g2.drawHLine(0, 15, 128);
    u8g2.drawXBMP(32, 18, 64, 40, xbm_dataholics_icono);
    u8g2.drawStr(16, 62, "LINKING K-2SO...");
  } while (u8g2.nextPage());

  // Force Station Mode, disable Wi-Fi sleep, and allow WPA2/WPA3 compatibility
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    playDigiviceChirp();
    Serial.println("\nWiFi Connected!");
    fetchServerMetrics();
  } else {
    Serial.println("\nWiFi Offline - booting in standalone mode");
  }
}

void loop() {
  // Mode switch button check (active LOW): cycles 0 (HUD) -> 1 (Full-Screen AI) -> 2 (Raw Telemetry)
  if (digitalRead(BTN_PIN) == LOW) {
    playDigiviceChirp();
    currentMode = (currentMode + 1) % 3;
    delay(250); // Debounce
  }

  // Poll server every 5 seconds
  if (millis() - lastPoll > 5000) {
    lastPoll = millis();
    fetchServerMetrics();
  }

  // Render active view
  if (currentMode == 0) {
    renderPetHUD();
  } else if (currentMode == 1) {
    renderFullScreenPetHUD();
  } else {
    renderTelemetryHUD();
  }

  delay(50);
}