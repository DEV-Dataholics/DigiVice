# Hardware Reference & Pinout Guide

## Target Board: ESP32-2432S028 ("Cheap Yellow Display" / CYD)

The **ESP32-2432S028** is an all-in-one IoT terminal board integrating an ESP32-D0WD-V3 dual-core microcontroller with a 2.8-inch SPI TFT display, resistive touchscreen, microSD slot, ambient light sensor (LDR), active-low RGB LED, and audio amplifier.

```
       +---------------------------------------------------+
       |                    2.8" TFT                       |
       |               320 x 240 Landscape                 |
       |                (ST7789 / ILI9341)                 |
       |                                                   |
       |  +-------------+  +----------------------------+  |
       |  |   AI-PET    |  |  TACTICAL GATES / METRICS  |  |
       |  |  72x72 HUD  |  |  G0 [PASS] G1 [LINT] ...   |  |
       |  +-------------+  +----------------------------+  |
       |                                                   |
       |   [AI-PET]      [GATES]      [NODE]      [DECK]   |
       +---------------------------------------------------+
           |                     |                     |
      RGB Underglow        Audio Synth (26)       LDR (34)
```

---

## ⚠️ The ST7789 vs ILI9341 "Batch Silicon" Divergence

Older documentation states all CYD boards use the `ILI9341` controller. However, newer manufacturing revisions labeled **`TPM408-2.8`** on the display flex ribbon ship with an **ST7789** controller.

### Forensic Diagnostic Comparison

| Symptom / Feature | Legacy ILI9341 Setup | TPM408 ST7789 Revision |
| :--- | :--- | :--- |
| **Driver Class** | `lgfx::Panel_ILI9341` | `lgfx::Panel_ST7789` |
| **Offset Rotation** | Must be `offset_rotation = 2` | Default `offset_rotation = 0` |
| **Inversion Polarity** | `cfg.invert = false` | `cfg.invert = false` (TN mode) |
| **Wrong Driver Result** | Sideways tilt, 25% noise block | Razor-sharp 320x240 landscape |
| **SPI Write Freq** | 40 MHz max | 27 MHz sweet spot (eliminates ribbon bounce) |

> [!IMPORTANT]
> If an ILI9341 driver is loaded onto an ST7789 panel, the right 25% of the display will render static memory noise and columns will misalign into vertical scanline stripes. Digivice v3.0 uses the calibrated `Panel_ST7789` profile in `cyd_display.h`.

---

## 📌 Complete Pin Mapping Matrix

### Display (SPI2 / HSPI)
| Function | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **TFT MOSI** | `GPIO 13` | Master Out Slave In |
| **TFT MISO** | `GPIO 12` | Master In Slave Out (dummy read) |
| **TFT SCLK** | `GPIO 14` | SPI Clock (27 MHz) |
| **TFT CS** | `GPIO 15` | Chip Select (Active LOW) |
| **TFT DC** | `GPIO 2` | Data / Command control |
| **TFT RST** | `-1` | Tied to hardware reset line |
| **TFT Backlight** | `GPIO 21` | Backlight LED control (PWM / High-side) |

### Resistive Touch Controller (XPT2046)
To avoid SPI bus contention on the hardware bus during high-speed display flushes, touch is driven via dedicated software SPI:
| Function | ESP32 GPIO | Description |
| :--- | :--- | :--- |
| **Touch CLK** | `GPIO 25` | Software SPI Clock |
| **Touch MOSI** | `GPIO 32` | Software SPI Data In |
| **Touch MISO** | `GPIO 39` | Software SPI Data Out |
| **Touch CS** | `GPIO 33` | Touch Chip Select |
| **Touch IRQ** | `GPIO 36` | Touch Interrupt (optional, polled) |

### Peripherals & Feedback
| Peripheral | ESP32 GPIO | Electrical Logic | Description |
| :--- | :--- | :--- | :--- |
| **RGB LED (Red)** | `GPIO 4` | Active **LOW** (`digitalWrite(4, LOW)` = ON) | Status underglow |
| **RGB LED (Green)** | `GPIO 16` | Active **LOW** (`digitalWrite(16, LOW)` = ON) | Status underglow |
| **RGB LED (Blue)** | `GPIO 17` | Active **LOW** (`digitalWrite(17, LOW)` = ON) | Status underglow |
| **Audio DAC / Speaker** | `GPIO 26` | Square wave / PWM (`tone()`) | Acoustic feedback |
| **LDR Sensor** | `GPIO 34` | Analog Input (`0` dark, `4095` bright) | Ambient lux telemetry |
| **MicroSD Slot** | `GPIO 5` (CS) | Shared SPI Bus (HSPI) | Local logging / storage |

---

## ⚡ Memory Partition Scheme Requirement

LVGL 9.x compiled with full fonts, animations, and LovyanGFX requires approximately **1.35 MB – 1.40 MB** of flash storage. The default ESP32 flash partition table (1.31 MB application) will throw:
```
region `dram0_0_seg' overflowed by XX bytes / text section exceeds available space
```

To flash successfully:
- In **Arduino IDE**: Set **Tools → Partition Scheme** to **"Huge APP (3MB No OTA / 1MB SPIFFS)"**.
- In **PlatformIO**: Included in `platformio.ini` via `board_build.partitions = huge_app.csv`.
