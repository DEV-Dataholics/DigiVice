# 🐾 Scrap-Hound Digipet (Wasteland Cyber-Gremlin Edition)

Autonomous hardware-bound desk companion for **Dataholics Resolve ServiceDesk** (`resolve.dataholics.com.mx`).

---

## ⚡ Core Concept & Behavior

Scrap-Hound is a **Cute Horror / Mad Max Wasteland Cyber-Gremlin** designed to sit on your desk and react in real-time to service desk lifecycle events:

| Event | Pet Reaction | Acoustic FX | Underglow RGB |
| :--- | :--- | :--- | :--- |
| **Ticket Opened** | Snaps to combat attention, ears perk erect, eyes flare blood red, alert banner drops with Ticket # and title | Two-tone war siren / industrial klaxon | Strobe flashing combat crimson red |
| **Ticket Updated** | Maniacal Gremlin cackle loop, jaw chattering, eyes squinting into wicked crescents with `HEHEHE!` comic bubble | Staccato frequency-stepping 8-bit laugh arpeggio | Flickering fuel-flame amber |
| **Ticket Resolved** | Violent screen-shake **EVIL CHOMP** (snapping severed wires / bug in half with flying pixel sparks), victory dance | Descending metallic crunch bite + victory fanfare | Radioactive toxic green & electric purple pulse |
| **Idle Prowl** | Slumped posture, breathing chest expansion, blinking mismatched eyes (toxic-green & combat-red), spark plug tail twitching | Silent ambient | Gentle green breathing pulse |
| **Touch Screen (Poke)** | Snarls with fangs exposed | Low guttural growl | Warning amber |
| **Long Press (3s)** | Toggles **Silicon Hardware Binding Screen** displaying factory eFuse MAC | Digital UI chime | Deep blue |

---

## 🔒 Silicon Hardware-Bound Security ("Zero-Scrape Protection")

To ensure that **only your physical screen** can query `resolve.dataholics.com.mx/digipet_feed.php`, the system enforces a 3-tier hardware cryptographic handshake on every poll:

1. **Factory Silicon eFuse MAC (`ESP.getEfuseMac()`):** Non-cloneable 48-bit silicon fingerprint burned into the chip at wafer fabrication.
2. **Time-Drift & Nonce Anti-Replay Guard:** The ESP32 signs every poll with a unique 16-hex nonce and UTC Unix timestamp ($\pm 35\text{s}$ server window).
3. **mbedtls HMAC-SHA256:** Every request calculates:
   $$\text{Signature} = \text{HMAC-SHA256}(\text{SecretKey}, \text{"DIGIPET|" + MAC + "|" + Timestamp + "|" + Nonce})$$
   Any request without valid cryptographic hardware credentials receives an immediate `401` or `403` rejection.

---

## 🚀 How to Flash & Enroll Your New Screen

### 1. Flash the Firmware
- Open `cyd_resolve_pet.ino` in **Arduino IDE** or use **PlatformIO**.
- In Arduino IDE:
  - Board: **ESP32 Dev Module**
  - Partition Scheme: **Huge APP (3MB No OTA / 1MB SPIFFS)**
  - Flash Frequency: **80MHz**
  - Upload Speed: **921600**
  - Required Libraries: `LovyanGFX` and `ArduinoJson` (v6.x)
- Hit Upload.

### 2. Discover Your New Screen's Silicon MAC
- When the screen boots up, press and hold the touchscreen for **3 seconds**.
- The screen will flip to the **HARDWARE BINDING** card displaying your exact **FACTORY SILICON eFUSE MAC** (e.g. `24:DC:C3:9A:45:88`).
- (Alternatively, watch the Serial Monitor at `115200` baud on boot).

### 3. Enroll the MAC in Resolve ServiceDesk
Run the enrollment helper script from your terminal:
```bash
python scratch/enroll_digipet.py <YOUR_SCREEN_MAC> "Scrap-Hound Wasteland Gremlin"
```
This script securely generates a 256-bit cryptographically random key and whitelists your screen in the `digipet_hardware` database table on `resolve.dataholics.com.mx`.

### 4. Paste the Generated Secret into Firmware
In `cyd_resolve_pet.ino`, update:
```cpp
String DEVICE_SECRET_KEY = "<YOUR_GENERATED_SECRET_KEY>";
```
Re-flash or save, and Scrap-Hound will instantly link to your live ServiceDesk!
