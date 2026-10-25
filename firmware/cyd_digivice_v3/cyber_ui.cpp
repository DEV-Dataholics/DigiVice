#include "cyber_ui.h"
#include "sprites.h"
#include "audio_synth.h"
#include <time.h>

extern void triggerGundamCutscene(uint8_t fxType, int fromS, int toS);

CyberUI cyberUI;

// Callback function pointers
static void (*onBacklightChange)(int) = nullptr;
static void (*onManualPollTrigger)() = nullptr;
static void (*onCircuitAckTrigger)() = nullptr;
static void (*onAudioToggleTrigger)(bool) = nullptr;

// Event wrappers
static void slider_event_cb(lv_event_t * e) {
  lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
  int val = (int)lv_slider_get_value(slider);
  if (onBacklightChange) {
    onBacklightChange(val);
  }
}

static void btn_poll_cb(lv_event_t * e) {
  soundFx.playTap();
  if (onManualPollTrigger) {
    onManualPollTrigger();
  }
}

static void btn_ack_cb(lv_event_t * e) {
  soundFx.playChirp();
  if (onCircuitAckTrigger) {
    onCircuitAckTrigger();
  }
}

static void sw_audio_cb(lv_event_t * e) {
  lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
  bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
  soundFx.setMuted(!state);
  if (state) soundFx.playTap();
  if (onAudioToggleTrigger) {
    onAudioToggleTrigger(state);
  }
}

static unsigned long previewUntilMs = 0;

static void pet_touch_cb(lv_event_t * e) {
  soundFx.playChirp();
  // Interactive Showcase: tapping the pet cycles stages for 5 seconds
  previewUntilMs = millis() + 5000;
  cyberUI.triggerEvolutionFanfare(-1);
}

static void btn_transam_cb(lv_event_t * e) {
  soundFx.playEvolutionArpeggio();
  triggerGundamCutscene(0, 1, 2);
}

static void btn_haro_cb(lv_event_t * e) {
  soundFx.playEvolutionArpeggio();
  triggerGundamCutscene(2, 1, 1);
}

static void btn_evo_cb(lv_event_t * e) {
  soundFx.playEvolutionArpeggio();
  triggerGundamCutscene(4, 0, 0); // Replay Dataholics Boot Screen!
}

void CyberUI::setBacklightCallback(void (*cb)(int)) { onBacklightChange = cb; }
void CyberUI::setManualPollCallback(void (*cb)()) { onManualPollTrigger = cb; }
void CyberUI::setResetCircuitBreakerCallback(void (*cb)()) { onCircuitAckTrigger = cb; }
void CyberUI::setAudioToggleCallback(void (*cb)(bool)) { onAudioToggleTrigger = cb; }

void CyberUI::createStyles() {
  // 1. Dark Screen Background
  lv_style_init(&styleDarkBg);
  lv_style_set_bg_color(&styleDarkBg, CYBER_COLOR_BLACK);
  lv_style_set_bg_opa(&styleDarkBg, LV_OPA_COVER);
  lv_style_set_border_width(&styleDarkBg, 0);

  // 2. Cyber Panel Card (Dark Slate with subtle border)
  lv_style_init(&stylePanel);
  lv_style_set_bg_color(&stylePanel, CYBER_COLOR_PANEL);
  lv_style_set_bg_opa(&stylePanel, LV_OPA_COVER);
  lv_style_set_border_color(&stylePanel, CYBER_COLOR_PANEL_ALT);
  lv_style_set_border_width(&stylePanel, 1);
  lv_style_set_radius(&stylePanel, 4);
  lv_style_set_pad_all(&stylePanel, 6);

  // 3. Cyber Accent Panel (Neon Cyan Border)
  lv_style_init(&stylePanelAccent);
  lv_style_set_bg_color(&stylePanelAccent, CYBER_COLOR_PANEL);
  lv_style_set_bg_opa(&stylePanelAccent, LV_OPA_COVER);
  lv_style_set_border_color(&stylePanelAccent, CYBER_COLOR_CYAN);
  lv_style_set_border_width(&stylePanelAccent, 1);
  lv_style_set_radius(&stylePanelAccent, 4);

  // 4. Gate State Styles
  lv_style_init(&styleGatePassed);
  lv_style_set_bg_color(&styleGatePassed, lv_color_hex(0x06331A));
  lv_style_set_border_color(&styleGatePassed, CYBER_COLOR_GREEN);
  lv_style_set_border_width(&styleGatePassed, 1);
  lv_style_set_radius(&styleGatePassed, 4);

  lv_style_init(&styleGateFailed);
  lv_style_set_bg_color(&styleGateFailed, lv_color_hex(0x3B0715));
  lv_style_set_border_color(&styleGateFailed, CYBER_COLOR_RED);
  lv_style_set_border_width(&styleGateFailed, 1);
  lv_style_set_radius(&styleGateFailed, 4);

  lv_style_init(&styleGateIdle);
  lv_style_set_bg_color(&styleGateIdle, CYBER_COLOR_PANEL);
  lv_style_set_border_color(&styleGateIdle, CYBER_COLOR_PANEL_ALT);
  lv_style_set_border_width(&styleGateIdle, 1);
  lv_style_set_radius(&styleGateIdle, 4);

  // 5. Neon Touch Buttons
  lv_style_init(&styleNeonBtn);
  lv_style_set_bg_color(&styleNeonBtn, lv_color_hex(0x0D2A3A));
  lv_style_set_border_color(&styleNeonBtn, CYBER_COLOR_CYAN);
  lv_style_set_border_width(&styleNeonBtn, 1);
  lv_style_set_radius(&styleNeonBtn, 4);
  lv_style_set_text_color(&styleNeonBtn, CYBER_COLOR_CYAN);
}

void CyberUI::createHeader() {
  lv_obj_t* header = lv_obj_create(lv_screen_active());
  lv_obj_set_size(header, 320, 24);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_add_style(header, &styleDarkBg, 0);
  lv_obj_set_style_border_color(header, CYBER_COLOR_CYAN, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  headerLabelHost = lv_label_create(header);
  lv_label_set_text(headerLabelHost, "[*] K-2SO: CONNECTING");
  lv_obj_set_style_text_color(headerLabelHost, CYBER_COLOR_AMBER, 0);
  lv_obj_align(headerLabelHost, LV_ALIGN_LEFT_MID, -6, 0);

  headerLabelWifi = lv_label_create(header);
  lv_label_set_text(headerLabelWifi, "WIFI: --");
  lv_obj_set_style_text_color(headerLabelWifi, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_align(headerLabelWifi, LV_ALIGN_RIGHT_MID, 6, 0);
}

void CyberUI::createTabview() {
  // Tabview positioned below header
  tabview = lv_tabview_create(lv_screen_active());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tabview, 32);
  lv_obj_set_pos(tabview, 0, 24);
  lv_obj_set_size(tabview, 320, 216);
  lv_obj_add_style(tabview, &styleDarkBg, 0);

  // Style bottom tab button bar
  lv_obj_t* tab_btns = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x05070B), 0);
  lv_obj_set_style_border_color(tab_btns, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_width(tab_btns, 1, 0);
  lv_obj_set_style_text_color(tab_btns, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_set_style_text_color(tab_btns, CYBER_COLOR_CYAN, LV_STATE_CHECKED);

  // Create 4 Fabrizzio Haute Couture Cyber Tabs
  lv_obj_t* tab1 = lv_tabview_add_tab(tabview, "CHRONO");
  lv_obj_t* tab2 = lv_tabview_add_tab(tabview, "MECHA");
  lv_obj_t* tab3 = lv_tabview_add_tab(tabview, "GATES");
  lv_obj_t* tab4 = lv_tabview_add_tab(tabview, "SYSTEM");

  lv_obj_add_style(tab1, &styleDarkBg, 0);
  lv_obj_add_style(tab2, &styleDarkBg, 0);
  lv_obj_add_style(tab3, &styleDarkBg, 0);
  lv_obj_add_style(tab4, &styleDarkBg, 0);
  lv_obj_clear_flag(tab1, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(tab2, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(tab3, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(tab4, LV_OBJ_FLAG_SCROLLABLE);

  buildPetTab(tab1);
  buildDeckTab(tab2);
  buildTacticalTab(tab3);
  buildNodeTab(tab4);
}

void CyberUI::buildPetTab(lv_obj_t* parent) {
  // Left Runway: Gundam Hero Pod (72x72 Sanctuary + Stage Badge)
  lv_obj_t* petCard = lv_obj_create(parent);
  lv_obj_set_size(petCard, 92, 166);
  lv_obj_align(petCard, LV_ALIGN_TOP_LEFT, 4, 4);
  lv_obj_add_style(petCard, &stylePanelAccent, 0);
  lv_obj_clear_flag(petCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(petCard, pet_touch_cb, LV_EVENT_CLICKED, NULL);

  petCanvas = lv_canvas_create(petCard);
  if (!canvasBuffer) {
    canvasBuffer = (lv_color_t*)malloc(72 * 72 * sizeof(lv_color_t));
  }
  lv_canvas_set_buffer(petCanvas, canvasBuffer, 72, 72, LV_COLOR_FORMAT_RGB565);
  lv_obj_align(petCanvas, LV_ALIGN_TOP_MID, 0, 2);

  petStageLabel = lv_label_create(petCard);
  lv_label_set_text(petStageLabel, "PROTO-GUNDAM");
  lv_obj_set_style_text_color(petStageLabel, CYBER_COLOR_CYAN, 0);
  lv_obj_align(petStageLabel, LV_ALIGN_BOTTOM_MID, 0, -18);

  lv_obj_t* tapHint = lv_label_create(petCard);
  lv_label_set_text(tapHint, "[TAP EVOLVE]");
  lv_obj_set_style_text_color(tapHint, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_align(tapHint, LV_ALIGN_BOTTOM_MID, 0, -2);

  // Right Top: Editorial Chrono Card (Local Time + Date)
  lv_obj_t* clockCard = lv_obj_create(parent);
  lv_obj_set_size(clockCard, 208, 66);
  lv_obj_align(clockCard, LV_ALIGN_TOP_LEFT, 102, 4);
  lv_obj_add_style(clockCard, &stylePanelAccent, 0);
  lv_obj_clear_flag(clockCard, LV_OBJ_FLAG_SCROLLABLE);

  chronoDateLabel = lv_label_create(clockCard);
  lv_label_set_text(chronoDateLabel, "LOCAL TIME // CST SYNC");
  lv_obj_set_style_text_color(chronoDateLabel, CYBER_COLOR_AMBER, 0);
  lv_obj_align(chronoDateLabel, LV_ALIGN_TOP_LEFT, 2, -2);

  chronoTimeLabel = lv_label_create(clockCard);
  lv_label_set_text(chronoTimeLabel, "13 : 52 : 00");
  lv_obj_set_style_text_color(chronoTimeLabel, CYBER_COLOR_CYAN, 0);
  lv_obj_set_style_transform_scale(chronoTimeLabel, 360, 0); // ~1.4x crisp scale
  lv_obj_align(chronoTimeLabel, LV_ALIGN_BOTTOM_LEFT, 6, -4);

  // Right Middle: Sculpted Twin Vitals Card (CPU & RAM)
  lv_obj_t* vitalsCard = lv_obj_create(parent);
  lv_obj_set_size(vitalsCard, 208, 60);
  lv_obj_align(vitalsCard, LV_ALIGN_TOP_LEFT, 102, 74);
  lv_obj_add_style(vitalsCard, &stylePanel, 0);
  lv_obj_clear_flag(vitalsCard, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* lblC = lv_label_create(vitalsCard);
  lv_label_set_text(lblC, "CPU");
  lv_obj_set_style_text_color(lblC, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblC, LV_ALIGN_TOP_LEFT, 2, 2);

  chronoCpuBar = lv_bar_create(vitalsCard);
  lv_obj_set_size(chronoCpuBar, 112, 10);
  lv_obj_align(chronoCpuBar, LV_ALIGN_TOP_LEFT, 38, 5);
  lv_obj_set_style_bg_color(chronoCpuBar, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(chronoCpuBar, CYBER_COLOR_CYAN, LV_PART_INDICATOR);

  chronoCpuVal = lv_label_create(vitalsCard);
  lv_label_set_text(chronoCpuVal, "0.0%");
  lv_obj_set_style_text_color(chronoCpuVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(chronoCpuVal, LV_ALIGN_TOP_RIGHT, -2, 2);

  lv_obj_t* lblR = lv_label_create(vitalsCard);
  lv_label_set_text(lblR, "RAM");
  lv_obj_set_style_text_color(lblR, CYBER_COLOR_AMBER, 0);
  lv_obj_align(lblR, LV_ALIGN_BOTTOM_LEFT, 2, -2);

  chronoRamBar = lv_bar_create(vitalsCard);
  lv_obj_set_size(chronoRamBar, 112, 10);
  lv_obj_align(chronoRamBar, LV_ALIGN_BOTTOM_LEFT, 38, -4);
  lv_obj_set_style_bg_color(chronoRamBar, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(chronoRamBar, CYBER_COLOR_AMBER, LV_PART_INDICATOR);

  chronoRamVal = lv_label_create(vitalsCard);
  lv_label_set_text(chronoRamVal, "0.0%");
  lv_obj_set_style_text_color(chronoRamVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(chronoRamVal, LV_ALIGN_BOTTOM_RIGHT, -2, -2);

  // Right Bottom: Live Mission / Haro Status Pill
  lv_obj_t* statusCard = lv_obj_create(parent);
  lv_obj_set_size(statusCard, 208, 32);
  lv_obj_align(statusCard, LV_ALIGN_TOP_LEFT, 102, 138);
  lv_obj_add_style(statusCard, &stylePanel, 0);
  lv_obj_clear_flag(statusCard, LV_OBJ_FLAG_SCROLLABLE);

  petStatusLabel = lv_label_create(statusCard);
  lv_label_set_text(petStatusLabel, "HARO: STANDBY // POLLER OK");
  lv_obj_set_style_text_color(petStatusLabel, CYBER_COLOR_GREEN, 0);
  lv_obj_center(petStatusLabel);

  // Keep hidden pointers valid for update()
  petEnergyBar = chronoCpuBar;
  petEnergyVal = chronoCpuVal;
}

void CyberUI::buildTacticalTab(lv_obj_t* parent) {
  // Mission Header Banner
  tacMissionLabel = lv_label_create(parent);
  lv_label_set_text(tacMissionLabel, "[ MISSION: STANDBY | TICKET #291 ]");
  lv_obj_set_style_text_color(tacMissionLabel, CYBER_COLOR_CYAN, 0);
  lv_obj_align(tacMissionLabel, LV_ALIGN_TOP_LEFT, 8, 4);

  tacStageLabel = lv_label_create(parent);
  lv_label_set_text(tacStageLabel, "STAGE: IDLE  |  ATTEMPT: 0");
  lv_obj_set_style_text_color(tacStageLabel, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_align(tacStageLabel, LV_ALIGN_TOP_LEFT, 8, 20);

  tacTimerLabel = lv_label_create(parent);
  lv_label_set_text(tacTimerLabel, "TIME: 00:00.0");
  lv_obj_set_style_text_color(tacTimerLabel, CYBER_COLOR_AMBER, 0);
  lv_obj_align(tacTimerLabel, LV_ALIGN_TOP_RIGHT, -8, 20);

  // 4 Gate Status Blocks (2x2 Grid)
  // Gate 0: Integrity
  gateCard0 = lv_obj_create(parent);
  lv_obj_set_size(gateCard0, 145, 36);
  lv_obj_align(gateCard0, LV_ALIGN_TOP_LEFT, 8, 40);
  lv_obj_add_style(gateCard0, &styleGateIdle, 0);
  lv_obj_clear_flag(gateCard0, LV_OBJ_FLAG_SCROLLABLE);
  gateLabel0 = lv_label_create(gateCard0);
  lv_label_set_text(gateLabel0, "G0: INTEGRITY [IDLE]");
  lv_obj_center(gateLabel0);

  // Gate 1: Linters
  gateCard1 = lv_obj_create(parent);
  lv_obj_set_size(gateCard1, 145, 36);
  lv_obj_align(gateCard1, LV_ALIGN_TOP_RIGHT, -8, 40);
  lv_obj_add_style(gateCard1, &styleGateIdle, 0);
  lv_obj_clear_flag(gateCard1, LV_OBJ_FLAG_SCROLLABLE);
  gateLabel1 = lv_label_create(gateCard1);
  lv_label_set_text(gateLabel1, "G1: LINTERS   [IDLE]");
  lv_obj_center(gateLabel1);

  // Gate 2: Assertions
  gateCard2 = lv_obj_create(parent);
  lv_obj_set_size(gateCard2, 145, 36);
  lv_obj_align(gateCard2, LV_ALIGN_TOP_LEFT, 8, 80);
  lv_obj_add_style(gateCard2, &styleGateIdle, 0);
  lv_obj_clear_flag(gateCard2, LV_OBJ_FLAG_SCROLLABLE);
  gateLabel2 = lv_label_create(gateCard2);
  lv_label_set_text(gateLabel2, "G2: ASSERTION [IDLE]");
  lv_obj_center(gateLabel2);

  // Gate 3: Circuit Breaker
  gateCard3 = lv_obj_create(parent);
  lv_obj_set_size(gateCard3, 145, 36);
  lv_obj_align(gateCard3, LV_ALIGN_TOP_RIGHT, -8, 80);
  lv_obj_add_style(gateCard3, &styleGateIdle, 0);
  lv_obj_clear_flag(gateCard3, LV_OBJ_FLAG_SCROLLABLE);
  gateLabel3 = lv_label_create(gateCard3);
  lv_label_set_text(gateLabel3, "G3: CIRCUIT   [CLEAR]");
  lv_obj_center(gateLabel3);

  // Circuit Breaker Reset Action Button
  btnAckCircuit = lv_btn_create(parent);
  lv_obj_set_size(btnAckCircuit, 298, 28);
  lv_obj_align(btnAckCircuit, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_add_style(btnAckCircuit, &styleNeonBtn, 0);
  lv_obj_add_event_cb(btnAckCircuit, btn_ack_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t* lblAck = lv_label_create(btnAckCircuit);
  lv_label_set_text(lblAck, "[ RESET / ACKNOWLEDGE CIRCUIT BREAKER ]");
  lv_obj_center(lblAck);
}

void CyberUI::buildNodeTab(lv_obj_t* parent) {
  // CPU Bar
  lv_obj_t* lblCpu = lv_label_create(parent);
  lv_label_set_text(lblCpu, "CPU LOAD:");
  lv_obj_set_style_text_color(lblCpu, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblCpu, LV_ALIGN_TOP_LEFT, 8, 8);

  barCpu = lv_bar_create(parent);
  lv_obj_set_size(barCpu, 190, 10);
  lv_obj_align(barCpu, LV_ALIGN_TOP_LEFT, 78, 12);
  lv_obj_set_style_bg_color(barCpu, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(barCpu, CYBER_COLOR_CYAN, LV_PART_INDICATOR);

  labelCpuVal = lv_label_create(parent);
  lv_label_set_text(labelCpuVal, "0.0%");
  lv_obj_set_style_text_color(labelCpuVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(labelCpuVal, LV_ALIGN_TOP_RIGHT, -8, 8);

  // RAM Bar
  lv_obj_t* lblRam = lv_label_create(parent);
  lv_label_set_text(lblRam, "RAM LOAD:");
  lv_obj_set_style_text_color(lblRam, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblRam, LV_ALIGN_TOP_LEFT, 8, 28);

  barRam = lv_bar_create(parent);
  lv_obj_set_size(barRam, 190, 10);
  lv_obj_align(barRam, LV_ALIGN_TOP_LEFT, 78, 32);
  lv_obj_set_style_bg_color(barRam, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(barRam, CYBER_COLOR_AMBER, LV_PART_INDICATOR);

  labelRamVal = lv_label_create(parent);
  lv_label_set_text(labelRamVal, "0.0%");
  lv_obj_set_style_text_color(labelRamVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(labelRamVal, LV_ALIGN_TOP_RIGHT, -8, 28);

  // Disk Bar
  lv_obj_t* lblDisk = lv_label_create(parent);
  lv_label_set_text(lblDisk, "DISK SSD:");
  lv_obj_set_style_text_color(lblDisk, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblDisk, LV_ALIGN_TOP_LEFT, 8, 48);

  barDisk = lv_bar_create(parent);
  lv_obj_set_size(barDisk, 190, 10);
  lv_obj_align(barDisk, LV_ALIGN_TOP_LEFT, 78, 52);
  lv_obj_set_style_bg_color(barDisk, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(barDisk, CYBER_COLOR_GREEN, LV_PART_INDICATOR);

  labelDiskVal = lv_label_create(parent);
  lv_label_set_text(labelDiskVal, "0.0%");
  lv_obj_set_style_text_color(labelDiskVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(labelDiskVal, LV_ALIGN_TOP_RIGHT, -8, 48);

  // Daemons Matrix Frame
  lv_obj_t* srvCard = lv_obj_create(parent);
  lv_obj_set_size(srvCard, 298, 64);
  lv_obj_align(srvCard, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_add_style(srvCard, &stylePanel, 0);
  lv_obj_clear_flag(srvCard, LV_OBJ_FLAG_SCROLLABLE);

  labelPollerVal = lv_label_create(srvCard);
  lv_label_set_text(labelPollerVal, "POLLER: CHECKING...");
  lv_obj_set_style_text_color(labelPollerVal, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_align(labelPollerVal, LV_ALIGN_TOP_LEFT, 4, 2);

  labelOllamaVal = lv_label_create(srvCard);
  lv_label_set_text(labelOllamaVal, "OLLAMA: CHECKING...");
  lv_obj_set_style_text_color(labelOllamaVal, CYBER_COLOR_TEXT_DIM, 0);
  lv_obj_align(labelOllamaVal, LV_ALIGN_TOP_RIGHT, -4, 2);

  labelLoadVal = lv_label_create(srvCard);
  lv_label_set_text(labelLoadVal, "LOAD 1M: 0.00");
  lv_obj_set_style_text_color(labelLoadVal, CYBER_COLOR_AMBER, 0);
  lv_obj_align(labelLoadVal, LV_ALIGN_BOTTOM_LEFT, 4, -2);

  labelUptimeVal = lv_label_create(srvCard);
  lv_label_set_text(labelUptimeVal, "UP: 0h 00m");
  lv_obj_set_style_text_color(labelUptimeVal, CYBER_COLOR_CYAN, 0);
  lv_obj_align(labelUptimeVal, LV_ALIGN_BOTTOM_RIGHT, -4, -2);
}

void CyberUI::buildDeckTab(lv_obj_t* parent) {
  // Tab 2 ([MECHA]): Interactive Cutscene Launchpad + Cockpit Controls
  lv_obj_t* lblTitle = lv_label_create(parent);
  lv_label_set_text(lblTitle, "CELESTIAL BEING // CUTSCENE LAUNCHPAD:");
  lv_obj_set_style_text_color(lblTitle, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblTitle, LV_ALIGN_TOP_LEFT, 8, 4);

  // 3 Touchscreen Cutscene Launcher Buttons
  lv_obj_t* btnTransAm = lv_btn_create(parent);
  lv_obj_set_size(btnTransAm, 96, 38);
  lv_obj_align(btnTransAm, LV_ALIGN_TOP_LEFT, 6, 24);
  lv_obj_add_style(btnTransAm, &styleGateFailed, 0);
  lv_obj_add_event_cb(btnTransAm, btn_transam_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* l1 = lv_label_create(btnTransAm);
  lv_label_set_text(l1, "[TRANS-AM]");
  lv_obj_center(l1);

  lv_obj_t* btnHaro = lv_btn_create(parent);
  lv_obj_set_size(btnHaro, 96, 38);
  lv_obj_align(btnHaro, LV_ALIGN_TOP_MID, 0, 24);
  lv_obj_add_style(btnHaro, &styleGatePassed, 0);
  lv_obj_add_event_cb(btnHaro, btn_haro_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* l2 = lv_label_create(btnHaro);
  lv_label_set_text(l2, "[HARO COMMS]");
  lv_obj_center(l2);

  lv_obj_t* btnEvo = lv_btn_create(parent);
  lv_obj_set_size(btnEvo, 96, 38);
  lv_obj_align(btnEvo, LV_ALIGN_TOP_RIGHT, -6, 24);
  lv_obj_add_style(btnEvo, &styleNeonBtn, 0);
  lv_obj_add_event_cb(btnEvo, btn_evo_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t* l3 = lv_label_create(btnEvo);
  lv_label_set_text(l3, "[BOOT LOGO]");
  lv_obj_center(l3);

  // Brightness Slider
  lv_obj_t* lblBright = lv_label_create(parent);
  lv_label_set_text(lblBright, "PWM BACKLIGHT:");
  lv_obj_set_style_text_color(lblBright, CYBER_COLOR_AMBER, 0);
  lv_obj_align(lblBright, LV_ALIGN_TOP_LEFT, 8, 72);

  sliderBrightness = lv_slider_create(parent);
  lv_slider_set_range(sliderBrightness, 10, 100);
  lv_slider_set_value(sliderBrightness, 90, LV_ANIM_OFF);
  lv_obj_set_size(sliderBrightness, 130, 10);
  lv_obj_align(sliderBrightness, LV_ALIGN_TOP_LEFT, 128, 75);
  lv_obj_set_style_bg_color(sliderBrightness, CYBER_COLOR_PANEL_ALT, 0);
  lv_obj_set_style_bg_color(sliderBrightness, CYBER_COLOR_CYAN, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(sliderBrightness, CYBER_COLOR_CYAN, LV_PART_KNOB);
  lv_obj_add_event_cb(sliderBrightness, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

  labelBrightnessVal = lv_label_create(parent);
  lv_label_set_text(labelBrightnessVal, "90%");
  lv_obj_set_style_text_color(labelBrightnessVal, CYBER_COLOR_TEXT_BRIGHT, 0);
  lv_obj_align(labelBrightnessVal, LV_ALIGN_TOP_RIGHT, -8, 72);

  // Audio Mute Switch & LDR on same row
  lv_obj_t* lblAud = lv_label_create(parent);
  lv_label_set_text(lblAud, "SYNTH AUDIO:");
  lv_obj_set_style_text_color(lblAud, CYBER_COLOR_CYAN, 0);
  lv_obj_align(lblAud, LV_ALIGN_TOP_LEFT, 8, 100);

  swAudio = lv_switch_create(parent);
  lv_obj_set_size(swAudio, 40, 20);
  lv_obj_align(swAudio, LV_ALIGN_TOP_LEFT, 108, 98);
  lv_obj_add_state(swAudio, LV_STATE_CHECKED);
  lv_obj_add_event_cb(swAudio, sw_audio_cb, LV_EVENT_VALUE_CHANGED, NULL);

  labelLdrVal = lv_label_create(parent);
  lv_label_set_text(labelLdrVal, "LDR: 0");
  lv_obj_set_style_text_color(labelLdrVal, CYBER_COLOR_GREEN, 0);
  lv_obj_align(labelLdrVal, LV_ALIGN_TOP_RIGHT, -8, 100);

  // Manual HTTP Poll Button
  lv_obj_t* btnPoll = lv_btn_create(parent);
  lv_obj_set_size(btnPoll, 298, 28);
  lv_obj_align(btnPoll, LV_ALIGN_BOTTOM_MID, 0, -4);
  lv_obj_add_style(btnPoll, &styleNeonBtn, 0);
  lv_obj_add_event_cb(btnPoll, btn_poll_cb, LV_EVENT_CLICKED, NULL);

  lv_obj_t* lblPoll = lv_label_create(btnPoll);
  lv_label_set_text(lblPoll, "[ RE-POLL K-2SO TELEMETRY NOW ]");
  lv_obj_center(lblPoll);
}

static lv_color_t resolveSpriteColor(char code) {
  switch (code) {
    case '1': return lv_color_hex(0x05070B); // Obsidian Outline
    case '2': return lv_color_hex(0x005533); // Deep Emerald Shadow
    case '3': return lv_color_hex(0x00CC66); // Matrix Green Midtone
    case '4': return lv_color_hex(0x66FFBB); // Neon Mint Highlight
    case '5': return lv_color_hex(0xFFFFFF); // Pure Specular White
    case '6': return lv_color_hex(0x003888); // Deep Cobalt Shadow
    case '7': return lv_color_hex(0x00BBFF); // Electric Cyan Midtone
    case '8': return lv_color_hex(0x88FFFF); // Laser Ice Cyan
    case '9': return lv_color_hex(0xCC22FF); // Neon Violet Aura
    case 'A': return lv_color_hex(0x4A1208); // Dark Magma / Rust
    case 'B': return lv_color_hex(0xEE1144); // Molten Crimson
    case 'C': return lv_color_hex(0xFF6600); // Blazing Orange
    case 'D': return lv_color_hex(0xFFDD00); // Solar Gold / Yellow
    case 'E': return lv_color_hex(0x3A4659); // Dark Stone / Gunmetal
    case 'F': return lv_color_hex(0x8FA3B8); // Light Stone / Bone
    default:  return CYBER_COLOR_PANEL;
  }
}

void CyberUI::drawSpriteOnCanvas(int stage, int scale) {
  if (!petCanvas) return;

  // 1. Render Cyber-Sanctuary Background & Subtle Grid (72x72)
  lv_color_t bgBase = (stage == 2) ? lv_color_hex(0x180612) : lv_color_hex(0x090E1A);
  lv_color_t bgGrid = (stage == 2) ? lv_color_hex(0x2F0A22) : lv_color_hex(0x131D33);
  lv_color_t pedColor = CYBER_COLOR_CYAN;
  if (stage == 1) pedColor = CYBER_COLOR_GREEN;
  else if (stage == 2) pedColor = CYBER_COLOR_MAGENTA;
  else if (stage >= 3) pedColor = CYBER_COLOR_RED;

  for (int y = 0; y < 72; y++) {
    for (int x = 0; x < 72; x++) {
      bool isGrid = (x % 12 == 0) || (y % 12 == 0);
      lv_canvas_set_px(petCanvas, x, y, isGrid ? bgGrid : bgBase, LV_OPA_COVER);
    }
  }

  // 2. Render Holographic Projection Pedestal Ring at bottom (y = 62..67)
  int ringWidth = (animOffset % 2 == 0) ? 24 : 21;
  for (int dx = -ringWidth; dx <= ringWidth; dx++) {
    int px = 36 + dx;
    if (px >= 2 && px < 70) {
      lv_canvas_set_px(petCanvas, px, 64, pedColor, LV_OPA_COVER);
      if (abs(dx) < ringWidth - 5) {
        lv_canvas_set_px(petCanvas, px, 65, pedColor, LV_OPA_COVER);
      }
    }
  }

  // 3. Select Dual-Frame Multi-Color Sprite Matrix
  bool altFrame = (animOffset % 2) == 1;
  const char* const* spriteMap = sprite_slime_f0;
  switch (stage) {
    case 0: spriteMap = altFrame ? sprite_slime_f1   : sprite_slime_f0;   break;
    case 1: spriteMap = altFrame ? sprite_phantom_f1 : sprite_phantom_f0; break;
    case 2: spriteMap = altFrame ? sprite_daemon_f1  : sprite_daemon_f0;  break;
    default: spriteMap = altFrame ? sprite_tomb_f1   : sprite_tomb_f0;    break;
  }

  // 4. Draw 24x24 Shaded Pixel Art at 3x Scale (72x72)
  for (int r = 0; r < 24; r++) {
    const char* rowStr = spriteMap[r];
    for (int c = 0; c < 24; c++) {
      char code = rowStr[c];
      if (code != '.' && code != '\0') {
        lv_color_t pixColor = resolveSpriteColor(code);
        for (int dy = 0; dy < scale; dy++) {
          for (int dx = 0; dx < scale; dx++) {
            lv_canvas_set_px(petCanvas, c * scale + dx, r * scale + dy, pixColor, LV_OPA_COVER);
          }
        }
      }
    }
  }

  // 5. Render Floating Cyber-Particles Rising in Sanctuary
  static const uint8_t pCols[4] = { 10, 58, 16, 62 };
  for (int i = 0; i < 4; i++) {
    int py = 60 - ((animOffset * 5 + i * 15) % 54);
    int px = pCols[i] + ((animOffset + i) % 3) - 1;
    if (px >= 2 && px < 70 && py >= 2 && py < 68) {
      lv_canvas_set_px(petCanvas, px, py, pedColor, LV_OPA_COVER);
      lv_canvas_set_px(petCanvas, px + 1, py, pedColor, LV_OPA_COVER);
    }
  }
}

extern void triggerGundamCutscene(uint8_t fxType, int fromS, int toS);

void CyberUI::triggerEvolutionFanfare(int stage) {
  int prevS = currentStage < 0 ? 0 : currentStage;
  bool manualTap = (stage < 0);
  if (manualTap) {
    currentStage = (currentStage + 1) % 4;
    if (currentStage == 1) {
      triggerGundamCutscene(1, prevS, 1); // Full-Screen G-Frame Evolution!
    } else if (currentStage == 2) {
      soundFx.playEvolutionArpeggio();
      triggerGundamCutscene(0, prevS, 2); // Full-Screen Twin Buster Laser!
    }
  } else {
    currentStage = stage;
  }
  drawSpriteOnCanvas(currentStage);
  switch (currentStage) {
    case 0:
      lv_label_set_text(petStageLabel, "AI: PROTO-GUNDAM");
      lv_obj_set_style_text_color(petStageLabel, CYBER_COLOR_CYAN, 0);
      break;
    case 1:
      lv_label_set_text(petStageLabel, "AI: RX-CYBER GUNDAM");
      lv_obj_set_style_text_color(petStageLabel, CYBER_COLOR_GREEN, 0);
      break;
    case 2:
      lv_label_set_text(petStageLabel, "AI: EXIA [TRANS-AM]");
      lv_obj_set_style_text_color(petStageLabel, CYBER_COLOR_MAGENTA, 0);
      break;
    default:
      lv_label_set_text(petStageLabel, "AI: 404 UNIT WRECK");
      lv_obj_set_style_text_color(petStageLabel, CYBER_COLOR_RED, 0);
      break;
  }
}

void CyberUI::tickAnimation() {
  animOffset++;
  drawSpriteOnCanvas(currentStage < 0 ? 3 : currentStage);

  // Update Local NTP Clock on Tab 1 ([CHRONO])
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    static const char* days[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char* months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    char dBuf[36];
    snprintf(dBuf, sizeof(dBuf), "%s · %s %02d // CST", days[timeinfo.tm_wday], months[timeinfo.tm_mon], timeinfo.tm_mday);
    lv_label_set_text(chronoDateLabel, dBuf);

    char tBuf[24];
    char sep = (animOffset % 2 == 0) ? ':' : ' ';
    snprintf(tBuf, sizeof(tBuf), "%02d %c %02d : %02d", timeinfo.tm_hour, sep, timeinfo.tm_min, timeinfo.tm_sec);
    lv_label_set_text(chronoTimeLabel, tBuf);
  } else {
    unsigned long s = millis() / 1000;
    char tBuf[24];
    snprintf(tBuf, sizeof(tBuf), "%02lu : %02lu : %02lu", (s / 3600) % 24, (s / 60) % 60, s % 60);
    lv_label_set_text(chronoTimeLabel, tBuf);
  }
}

void CyberUI::init() {
  currentStage = 3;
  animOffset = 0;
  canvasBuffer = nullptr;
  createStyles();
  createHeader();
  createTabview();
  drawSpriteOnCanvas(3); // Start as 404 Tombstone until polled
}

void CyberUI::update(const DigiviceTelemetry& data) {
  // 1. Update Header
  if (data.serverOnline) {
    lv_label_set_text(headerLabelHost, "[*] K-2SO: ONLINE");
    lv_obj_set_style_text_color(headerLabelHost, CYBER_COLOR_GREEN, 0);
  } else {
    lv_label_set_text(headerLabelHost, "[!] K-2SO: 404 OFFLINE");
    lv_obj_set_style_text_color(headerLabelHost, CYBER_COLOR_RED, 0);
  }

  char buf[32];
  snprintf(buf, sizeof(buf), "WIFI: %ddBm", data.wifiRssi);
  lv_label_set_text(headerLabelWifi, buf);

  // 2. Stage calculation (respect 5-second interactive touch showcase if active)
  if (millis() >= previewUntilMs) {
    int newStage = 3; // Offline
    if (data.serverOnline) {
      if (data.cpu < 30.0f) newStage = 0;
      else if (data.cpu < 70.0f) newStage = 1;
      else newStage = 2;
    }

    if (newStage != currentStage) {
      triggerEvolutionFanfare(newStage);
    }
  }

  // Chrono Vitals (CPU & RAM on Tab 1)
  lv_bar_set_value(chronoCpuBar, (int)data.cpu, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "%.1f%%", data.cpu);
  lv_label_set_text(chronoCpuVal, buf);

  lv_bar_set_value(chronoRamBar, (int)data.ram, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "%.1f%%", data.ram);
  lv_label_set_text(chronoRamVal, buf);

  if (data.circuitBreakerTripped) {
    lv_label_set_text(petStatusLabel, "BREAKER TRIPPED // HALTED");
    lv_obj_set_style_text_color(petStatusLabel, CYBER_COLOR_RED, 0);
  } else if (data.missionActive) {
    snprintf(buf, sizeof(buf), "HARO: TICKET #%d ACTIVE!", data.missionTicketId);
    lv_label_set_text(petStatusLabel, buf);
    lv_obj_set_style_text_color(petStatusLabel, CYBER_COLOR_GREEN, 0);
  } else {
    lv_label_set_text(petStatusLabel, data.serverOnline ? "HARO: STANDBY // POLLER OK" : "HARO: NODE UNREACHABLE");
    lv_obj_set_style_text_color(petStatusLabel, data.serverOnline ? CYBER_COLOR_CYAN : CYBER_COLOR_RED, 0);
  }

  // 3. Tactical Tab
  if (data.missionActive) {
    snprintf(buf, sizeof(buf), "[ MISSION: #%d (%s) ]", data.missionTicketId, data.missionProtocol.c_str());
    lv_label_set_text(tacMissionLabel, buf);
    lv_obj_set_style_text_color(tacMissionLabel, CYBER_COLOR_GREEN, 0);
  } else {
    lv_label_set_text(tacMissionLabel, "[ MISSION: STANDBY ]");
    lv_obj_set_style_text_color(tacMissionLabel, CYBER_COLOR_CYAN, 0);
  }

  snprintf(buf, sizeof(buf), "STAGE: %s | ATTEMPT: %d", data.missionStage.c_str(), data.missionAttempt);
  lv_label_set_text(tacStageLabel, buf);

  int mins = (int)data.elapsedSec / 60;
  int secs = (int)data.elapsedSec % 60;
  snprintf(buf, sizeof(buf), "TIME: %02d:%02d", mins, secs);
  lv_label_set_text(tacTimerLabel, buf);

  // Gate styles
  auto updateGateCard = [](lv_obj_t* card, lv_obj_t* label, const char* name, const String& val, lv_style_t* stPass, lv_style_t* stFail, lv_style_t* stIdle) {
    char gbuf[32];
    snprintf(gbuf, sizeof(gbuf), "%s: [%s]", name, val.c_str());
    lv_label_set_text(label, gbuf);
    if (val.equalsIgnoreCase("PASS") || val.equalsIgnoreCase("CLEAR") || val.equalsIgnoreCase("TRUE")) {
      lv_obj_add_style(card, stPass, 0);
    } else if (val.equalsIgnoreCase("FAIL") || val.equalsIgnoreCase("TRIPPED")) {
      lv_obj_add_style(card, stFail, 0);
    } else {
      lv_obj_add_style(card, stIdle, 0);
    }
  };

  updateGateCard(gateCard0, gateLabel0, "G0 INTEGRITY", data.gate0, &styleGatePassed, &styleGateFailed, &styleGateIdle);
  updateGateCard(gateCard1, gateLabel1, "G1 LINTERS  ", data.gate1, &styleGatePassed, &styleGateFailed, &styleGateIdle);
  updateGateCard(gateCard2, gateLabel2, "G2 ASSERTION", data.gate2, &styleGatePassed, &styleGateFailed, &styleGateIdle);
  updateGateCard(gateCard3, gateLabel3, "G3 CIRCUIT  ", data.circuitBreakerTripped ? "TRIPPED" : "CLEAR", &styleGatePassed, &styleGateFailed, &styleGateIdle);

  // 4. Node Vitals Tab
  lv_bar_set_value(barCpu, (int)data.cpu, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "%.1f%%", data.cpu);
  lv_label_set_text(labelCpuVal, buf);

  lv_bar_set_value(barRam, (int)data.ram, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "%.1f%%", data.ram);
  lv_label_set_text(labelRamVal, buf);

  lv_bar_set_value(barDisk, (int)data.disk, LV_ANIM_ON);
  snprintf(buf, sizeof(buf), "%.1f%%", data.disk);
  lv_label_set_text(labelDiskVal, buf);

  snprintf(buf, sizeof(buf), "POLLER: %s", data.pollerState.c_str());
  lv_label_set_text(labelPollerVal, buf);
  lv_obj_set_style_text_color(labelPollerVal, data.pollerState == "active" ? CYBER_COLOR_GREEN : CYBER_COLOR_RED, 0);

  snprintf(buf, sizeof(buf), "OLLAMA: %s", data.ollamaState.c_str());
  lv_label_set_text(labelOllamaVal, buf);
  lv_obj_set_style_text_color(labelOllamaVal, data.ollamaState == "active" ? CYBER_COLOR_CYAN : CYBER_COLOR_AMBER, 0);

  snprintf(buf, sizeof(buf), "LOAD 1M: %.2f", data.load1m);
  lv_label_set_text(labelLoadVal, buf);

  int upH = data.uptimeSec / 3600;
  int upM = (data.uptimeSec % 3600) / 60;
  snprintf(buf, sizeof(buf), "UP: %dh %02dm", upH, upM);
  lv_label_set_text(labelUptimeVal, buf);

  // 5. Deck Tab
  snprintf(buf, sizeof(buf), "LDR SENSOR: %d", data.ldrValue);
  lv_label_set_text(labelLdrVal, buf);
}
