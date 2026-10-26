#pragma once
#include <Arduino.h>
#include <lvgl.h>

// ============================================================================
// Cyberpunk UI Engine for Digivice v3.0 (CYD 320x240)
// ============================================================================

struct DigiviceTelemetry {
  bool serverOnline;
  float cpu;
  float ram;
  float disk;
  float load1m;
  unsigned long uptimeSec;
  String pollerState;
  String ollamaState;

  // Tactical Mission Telemetry
  bool missionActive;
  String missionProtocol;
  int missionTicketId;
  String missionFolio;
  String missionStage;
  int missionAttempt;
  bool circuitBreakerTripped;
  String gate0;
  String gate1;
  String gate2;
  String gate3;
  float elapsedSec;

  // Hardware Status
  int wifiRssi;
  int ldrValue;
  int backlightPercent;
  bool audioMuted;
};

class CyberUI {
public:
  void init();
  void update(const DigiviceTelemetry& data);
  void tickAnimation();
  void triggerEvolutionFanfare(int stage);
  void setBacklightCallback(void (*cb)(int));
  void setManualPollCallback(void (*cb)());
  void setResetCircuitBreakerCallback(void (*cb)());
  void setAudioToggleCallback(void (*cb)(bool));

private:
  void createStyles();
  void createHeader();
  void createTabview();
  void buildPetTab(lv_obj_t* parent);
  void buildTacticalTab(lv_obj_t* parent);
  void buildNodeTab(lv_obj_t* parent);
  void buildDeckTab(lv_obj_t* parent);
  void drawSpriteOnCanvas(int stage, int scale = 3);

  // UI Widgets
  lv_obj_t* tabview;
  lv_obj_t* headerLabelHost;
  lv_obj_t* headerLabelWifi;
  lv_obj_t* headerLabelVitals;

  // Tab 1: Chrono + AI Pet Split-Runway
  lv_obj_t* petCanvas;
  lv_obj_t* haroCanvas;
  lv_obj_t* chronoDateLabel;
  lv_obj_t* chronoTimeLabel;
  lv_obj_t* chronoHaroLabel;
  lv_obj_t* chronoCpuBar;
  lv_obj_t* chronoCpuVal;
  lv_obj_t* chronoRamBar;
  lv_obj_t* chronoRamVal;
  lv_obj_t* petStageLabel;
  lv_obj_t* petStatusLabel;
  lv_obj_t* petEnergyBar;
  lv_obj_t* petEnergyVal;
  lv_color_t* canvasBuffer;
  lv_color_t* haroBuffer;

  // Tab 2: Tactical Mission
  lv_obj_t* tacMissionLabel;
  lv_obj_t* tacStageLabel;
  lv_obj_t* tacTimerLabel;
  lv_obj_t* gateCard0;
  lv_obj_t* gateCard1;
  lv_obj_t* gateCard2;
  lv_obj_t* gateCard3;
  lv_obj_t* gateLabel0;
  lv_obj_t* gateLabel1;
  lv_obj_t* gateLabel2;
  lv_obj_t* gateLabel3;
  lv_obj_t* btnAckCircuit;

  // Tab 3: Node Vitals
  lv_obj_t* barCpu;
  lv_obj_t* barRam;
  lv_obj_t* barDisk;
  lv_obj_t* labelCpuVal;
  lv_obj_t* labelRamVal;
  lv_obj_t* labelDiskVal;
  lv_obj_t* labelLoadVal;
  lv_obj_t* labelPollerVal;
  lv_obj_t* labelOllamaVal;
  lv_obj_t* labelUptimeVal;

  // Tab 4: Deck
  lv_obj_t* sliderBrightness;
  lv_obj_t* labelBrightnessVal;
  lv_obj_t* swAudio;
  lv_obj_t* labelLdrVal;

  // Styles
  lv_style_t styleDarkBg;
  lv_style_t stylePanel;
  lv_style_t stylePanelAccent;
  lv_style_t styleGatePassed;
  lv_style_t styleGateFailed;
  lv_style_t styleGateIdle;
  lv_style_t styleNeonBtn;

  int currentStage;
  int animOffset;
};

extern CyberUI cyberUI;
