# 📁 Dataholics DigiVice: Projects Technical Directory

This document serves as the comprehensive engineering breakdown for every project, module, firmware, and tool inside the **[DEV-Dataholics/DigiVice](https://github.com/DEV-Dataholics/DigiVice)** repository.

---

## 📑 Table of Contents
1. [Project 1: Scrap-Hound Resolve Digipet (`firmware/cyd_resolve_pet/`)](#1-scrap-hound-resolve-digipet)
2. [Project 2: Mecha OS / Gundam Tactical HUD (`firmware/cyd_digivice_v3/`)](#2-mecha-os--gundam-tactical-hud)
3. [Project 3: Pocket OLED Cyber Badge (`firmware/oled_digivice_v1/`)](#3-pocket-oled-cyber-badge)
4. [Project 4: Host Telemetry Daemon Suite (`server/`)](#4-host-telemetry-daemon-suite)
5. [Project 5: Interactive Web Studios (`tools/` & `viewer/`)](#5-interactive-web-studios)

---

## 1. Scrap-Hound Resolve Digipet

### Directory: `firmware/cyd_resolve_pet/`
* **Entry Point:** [`cyd_resolve_pet.ino`](../firmware/cyd_resolve_pet/cyd_resolve_pet.ino)
* **Target Board:** ESP32-2432S028 ("CYD" 2.8" TFT 320x240, ST7789, XPT2046 Touch)
* **Framework:** Arduino / ESP32 Core 2.0.x–3.0.x

### Purpose
An ambient physical companion for support engineers, store managers, and executives that monitors the live pulse of **Dataholics Resolve ServiceDesk** (`resolve.dataholics.com.mx`), while also doubling as an **Executive Conference Badge** for **Alonso Ruiz (CEO)**.

### Subsystem Files
* **`cyd_resolve_pet.ino`:**
  * Handles Wi-Fi connection with auto-reconnect logic.
  * Synchronizes high-precision UTC time from NTP pool (`pool.ntp.org`).
  * Executes cryptographic HMAC polling loop against `https://resolve.dataholics.com.mx/digipet_feed.php` every 4 seconds.
  * Routes all touch interrupts into screen state transitions (`SCREEN_PET`, `SCREEN_BADGE`, `SCREEN_TICKET_LIST`, `SCREEN_TICKET_DETAIL`, `SCREEN_ENROLLMENT`).
  * Drives RGB LED underglow states and physical `BOOT` button debounce.
* **`pet_renderer.h`:**
  * **Procedural Pixel Renderer:** Draws the Yokai/Gremlin with customizable horns, glowing ocular pulses, eye patch with stitched cross, razor smile with vampire fangs, and orbiting will-o'-the-wisp flame.
  * **Autonomous Life Simulation:** Tracks sub-states (`IDLE_REST`, `IDLE_WALK`, `IDLE_SNIFF`, `IDLE_PLAY_WISP`, `IDLE_DOZE`, `IDLE_SLEEP`). Manages smooth floating `Zzz` particles and startle-jump reactions.
  * **Top HUD & On-Screen Badge Target:** Renders the persistent `[ 🪪 ALONSO (CEO) ]` button and live incident counters (`ACT: X`, `SLAIN: Y`).
  * **Bottom Hazard Ticker:** Displays real-time incident titles, priorities, and status indicators.
  * **Incident Deck (`drawTicketListScreen`):** 4-row incident summary with status accent strips.
  * **Incident Dossier (`drawTicketDetailScreen`):** Full multi-line problem description, requester contact information, and touch return buttons.
  * **Executive Conference Pass (`drawBadgeScreen`):** Vertical portrait pass with Dataholics wings logo, "ALONSO RUIZ", "CEO", and silicon cryptographic token.
* **`auth_crypto.h`:**
  * Computes hardware-bound HMAC-SHA256 signatures via hardware-accelerated `mbedtls/md.h`.
  * Generates 16-hex cryptographically random nonces to protect against replay attacks.
* **`cyd_display.h`:**
  * Pinned LovyanGFX configuration calibrated for the `TPM408-2.8` ST7789 panel (27MHz SPI, `RGB565`, rotation 1, non-inverted colors).
* **`audio_synth.h`:**
  * Software polyphonic synthesizer generating acoustic square/sawtooth tones on GPIO 26:
    * Two-tone war klaxon (Ticket Opened)
    * Gremlin giggle arpeggio (Ticket Updated)
    * Descending crunch snap + victory fanfare (Ticket Resolved)
    * Guttural snarl (Touch poke)

### Compilation & Flash Settings
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = huge_app.csv  # 3MB App / 1MB SPIFFS
upload_speed = 921600
monitor_speed = 115200
lib_deps = 
    lovyan03/LovyanGFX@^1.2.0
    bblanchon/ArduinoJson@^6.21.3
```

---

## 2. Mecha OS / Gundam Tactical HUD

### Directory: `firmware/cyd_digivice_v3/`
* **Entry Point:** [`cyd_digivice_v3.ino`](../firmware/cyd_digivice_v3/cyd_digivice_v3.ino)
* **Target Board:** ESP32-2432S028 ("CYD" 2.8" ST7789 320x240)
* **Framework:** Arduino + LVGL 9.6.0 + LovyanGFX

### Purpose
Mission-control terminal engineered for high-density compute clusters. Monitors autonomous AI engineering droids (K-2SO), parsing crucible test runs, build verifications, and system resource vitals.

### Subsystem Files
* **`cyd_digivice_v3.ino`:**
  * Runs the LVGL tick scheduler alongside the Wi-Fi telemetry loop polling `http://192.168.1.40:8000/metrics`.
  * Randomly triggers one of three boot sequences on cold boot using hardware RNG:
    * *Option A:* Mecha OS boot terminal.
    * *Option B:* Dark Editorial Split-Emblem pass.
    * *Option C:* Haro Hangar launch sequence.
  * Evaluates system load to trigger Gundam frame evolutions and Haro comms alerts.
* **`cyber_ui.h` & `cyber_ui.cpp`:**
  * 4-Tab Haute Couture Cyberpunk UI built on LVGL 9:
    * `[AI-PET]`: Gundam pet arena with animated scanlines and touch poke responses.
    * `[GATES]`: Deterministic verification gate monitors (`G0`, `G1`, `G2`, `G3`) with crucible progress bars and elapsed timers.
    * `[NODE]`: Circular CPU, RAM, and Disk resource meters.
    * `[DECK]`: Hardware PWM backlight slider and audio mute controls.
* **`sprites.h`:**
  * 16-color shaded pixel art tables (24x24 scaled to 72x72) for:
    * `sprite_slime`: Proto-Gundam
    * `sprite_phantom`: RX-Cyber Gundam
    * `sprite_daemon`: Gundam Exia [Trans-Am Overdrive]
    * `sprite_tomb`: 404 Unit Wreck
    * `sprite_haro`: Haro AI mascot with flapping ear-wings
* **`gundam_hd_sprites.h`:**
  * 16-bit 1:1 scale combat sprites extracted from *Shin Kidou Senki Gundam: Endless Duel* (4-frame idle, 4-frame stride, 2-frame melee strikes).
* **`lv_conf.h`:**
  * Tuned configuration for LVGL v9.6.0 with custom color depth (`LV_COLOR_DEPTH 16`), memory pool allocation (48KB RAM), and widget feature flags.

---

## 3. Pocket OLED Cyber Badge

### Directory: `firmware/oled_digivice_v1/`
* **Entry Point:** [`Digivice.ino`](../firmware/oled_digivice_v1/Digivice.ino)
* **Target Board:** ESP32 DevKit v1 + 128x64 SH1106 I2C OLED
* **Framework:** Arduino + U8g2

### Purpose
Ultra-low-power wearable badge and pocket monitor providing unobtrusive glances at server vitals, room temperature, and humidity.

### Key Capabilities
* **Environmental Sensors:** Reads DHT11 on GPIO 4 for ambient room heat and moisture.
* **Single-Button Mode Shifter:** Physical button on GPIO 15 cycles between:
  * *Mode 0:* Rogue AI HUD (Compact 24x24 mascot, IP, CPU gauge, active ticket count).
  * *Mode 1:* Full-Screen 2x Zoom Pet View (48x48 upscaled sprite with animated cyber-particles).
  * *Mode 2:* Raw Server Telemetry Table (CPU%, RAM%, Disk%, 1m load average, poller status).
* **Buzzer & LEDs:** Dual status LEDs (Green for online, Red for alert) and piezo frequency chirps.

---

## 4. Host Telemetry Daemon Suite

### Directory: `server/`
* **Host Platform:** Ubuntu/Debian Linux on Sovereign Node (`192.168.1.40`)
* **Service:** Systemd daemon `k2so-digibadge.service`

### Purpose
Gathers system metrics and agent crucible state from the operating system and serves them over LAN to all connected DigiVice units.

### Files
* **`k2so_metrics_daemon.py`:**
  * Standalone HTTP micro-server running on port `8000`.
  * Queries `psutil` for CPU%, RAM%, and disk usage.
  * Inspects `systemctl is-active` for `resolve-poller.service` and `ollama.service`.
  * Reads the latest ticket log from `/tmp/k2so_current_ticket.json`.
* **`k2so_mission_state.py`:**
  * Real-time regex parser extracting active ticket numbers, gate execution durations, and pass/fail states from TiRA logstreams.
* **`nemik_hitm_bridge.py`:**
  * Human-In-The-Middle supervisory socket allowing an engineer to review and approve gate verdicts directly from physical hardware.
* **`k2so-digibadge.service`:**
  * Standard systemd unit definition ensuring automatic daemon restarts on failure and boot-time launch.

---

## 5. Interactive Web Studios

### Directory: `tools/` and `viewer/`
* **Platform:** Browser-native (HTML5 Canvas, Vanilla ES6, Web Audio API, KaTeX)
* **Zero External Dependencies:** Runs 100% locally from `file://` or static web servers.

### Files
* **`tools/scraphound_studio.html` & `viewer/index.html`:**
  * High-precision emulator for the Scrap-Hound Digipet.
  * Allows developers and artists to preview:
    * Yokohama Yokai sprite adjustments and color palettes (SNES vs PS1 dither).
    * Autonomous wandering, sniffing, dozing, and sleep routines.
    * Interactive touch targets matching the physical ESP32 CYD glass coordinates:
      * Top HUD badge pill `[ 🪪 ALONSO (CEO) ]`
      * Bottom ticker banner opening the Incident Deck
      * Incident Dossier inspection with return navigation
    * Acoustic synthesizer preview using the Web Audio API.
    * CRT scanline overlay emulation.
* **`tools/digivice_simulator.html`:**
  * Full web simulator for the LVGL 9 Mecha OS interface. Simulates node compute surges, gate failures, and Haro comms alerts.
* **`tools/conference_badge_simulator.html`:**
  * Dedicated simulator for the vertical portrait conference badge with split-emblem animations and silicon ID preview.
