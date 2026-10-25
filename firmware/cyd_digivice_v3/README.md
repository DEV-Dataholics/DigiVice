# K-2SO CYD Digivice v3.0 (ESP32-2432S028 Cyberpunk Edition)

Autonomous Hardware Companion & Rogue AI Tactical Telemetry Terminal for **K-2SO** (`192.168.1.40`) and the **Nemik / Andor Autonomous Engineering Pipeline**.

Designed specifically for the **ESP32-2432S028** (CYD - "Cheap Yellow Display") featuring a **2.8" 320x240 ILI9341 Color TFT LCD** and **XPT2046 Resistive Touchscreen**.

---

## ⚡ Architectural Features

1. **Cyberpunk UI Engine (LVGL 8.3 + LovyanGFX):**
   - High-contrast terminal dark aesthetic (`#070A10`), cyber cards, glowing neon borders (`#00F0FF`, `#FF007F`, `#00FF88`).
   - Hardware-accelerated SPI DMA transfers via LovyanGFX.
2. **Interactive 4-Deck Touch Architecture:**
   - **`[AI-PET]`**: 72x72 scaled Rogue AI sanctuary (Bit-Slime, Phantom-Node, Daemon-Core, 404-Tombstone). Includes interactive touch poke/pet reactions and evolution fanfares.
   - **`[GATES]`**: Live Tactical Mission & Nemik/Andor Gate Deck. Displays active Ticket ID, Stage, Elapsed Timer, and real-time states for Gate 0 (Integrity), Gate 1 (Linters), Gate 2 (Assertions), and Gate 3 (Circuit Breaker) with on-screen reset/acknowledge.
   - **`[NODE]`**: Sovereign vitals gauges for CPU%, RAM%, Disk%, 1-min Load, and service statuses (`resolve-poller.service` & `ollama.service`).
   - **`[DECK]`**: Hardware control deck with live Backlight PWM slider, ambient LDR light sensor telemetry, audio mute toggle, and manual HTTP re-poll trigger.
3. **Reactive RGB Underglow:**
   - Uses the CYD onboard RGB LED (Pins 4, 16, 17 - Active LOW).
   - Dynamic colors: Matrix Green (Idle) -> Electric Cyan (Active) -> Hot Amber (Overload) -> Pulsing Red (Offline) -> Crimson Strobe (Circuit Breaker).
4. **Onboard Cyber Synth Audio:**
   - Uses the CYD onboard audio DAC / speaker header on **GPIO 26**.
   - Polyphonic evolution fanfares, gate chime passes, alarm sirens, and tactile tap feedback.

---

## 🛠️ Required Libraries

Install via **Arduino IDE Library Manager** (or automatically through `platformio.ini`):
- `LovyanGFX` by lovyan03 (>= v1.1.16)
- `lvgl` by lvgl (v8.3.11 recommended)
- `ArduinoJson` by Benoît Blanchon (>= v6.21.4)

---

## 📋 Flashing Instructions

### Option A: Arduino IDE 2.x
1. Open [`cyd_digivice_v3.ino`](file:///c:/Users/luisc/Documents/Dataholics/Dataholics%20Guidelines/Tinker%20Hardware/firmware/cyd_digivice_v3/cyd_digivice_v3.ino).
2. Select Board: **ESP32 Dev Module**.
3. Settings:
   - Flash Frequency: **80MHz**
   - Flash Mode: **QIO**
   - Partition Scheme: **Huge APP (3MB No OTA / 1MB SPIFFS)** or **Default 4MB with spiffs**
   - Upload Speed: **921600**
4. Connect the CYD board via USB-C and hit **Upload**.

### Option B: PlatformIO
```bash
cd "firmware/cyd_digivice_v3"
pio run --target upload
```

---

## 🔌 Hardware Pin Mapping Reference

| Component | Pin | Notes |
| :--- | :--- | :--- |
| **TFT MOSI** | `GPIO 13` | SPI Bus (VSPI) |
| **TFT MISO** | `GPIO 12` | SPI Bus (VSPI) |
| **TFT SCLK** | `GPIO 14` | SPI Bus (VSPI) |
| **TFT CS** | `GPIO 15` | Chip Select |
| **TFT DC** | `GPIO 2` | Data/Command |
| **TFT Backlight** | `GPIO 21` | PWM Brightness |
| **Touch MOSI** | `GPIO 32` | Touch SPI Bus (HSPI) |
| **Touch MISO** | `GPIO 39` | Touch SPI Bus (HSPI) |
| **Touch CLK** | `GPIO 25` | Touch SPI Bus (HSPI) |
| **Touch CS** | `GPIO 33` | Touch Chip Select |
| **Touch IRQ** | `GPIO 36` | Touch Interrupt |
| **RGB LED Red** | `GPIO 4` | Active LOW |
| **RGB LED Green** | `GPIO 16` | Active LOW |
| **RGB LED Blue** | `GPIO 17` | Active LOW |
| **Audio DAC** | `GPIO 26` | Speaker Header |
| **LDR Sensor** | `GPIO 34` | Analog Light Sensor |
