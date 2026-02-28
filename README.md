# PC IR Remote

A custom PCB that lets you control your PC's power and reset buttons using any IR remote.
Built around an ESP32-C3 Super Mini. Supports Home Assistant integration via ESPHome,
or runs fully standalone with the included Arduino sketch — no smart home required.

## Features

- Power on/off and reset your PC with any IR remote
- IR codes stored in flash — survive power cycles
- Two firmware options: ESPHome (Home Assistant) or standalone Arduino sketch
- Physical learn/trigger buttons on the board (GPIO5, GPIO6)
- Status LED feedback for all actions
- Powered from the motherboard's internal USB header (standby 5V — works when PC is off)
- Relay-based switching (phase 1) with MOSFET upgrade path (phase 2)

## Hardware

| Component | Part |
|-----------|------|
| MCU | ESP32-C3 Super Mini |
| IR Receiver | TSOP38238 (38kHz) |
| Relay driver | PC817 optocoupler + S8550 PNP transistor (per relay) |
| Relays | JY3FF-S-DC5V-C SPDT × 2 (K1 = power, K2 = reset) |
| Flyback diodes | 1N914 SOD-323F (per relay) |

### Pin Assignments

| Function | GPIO |
|----------|------|
| IR Receiver OUT | GPIO3 |
| Power relay (→ PWR_SW header) | GPIO1 |
| Reset relay (→ RST_SW header) | GPIO2 |
| Power button / Learn Power | GPIO5 |
| Reset button / Learn Reset | GPIO6 |
| Status LED | GPIO7 (external) or GPIO8 (onboard) |

See `docs/assembly.md` for full wiring details.

## Repository Layout

```
firmware/
  esphome/          — ESPHome YAML (Home Assistant integration)
  arduino/          — Standalone Arduino/PlatformIO sketch (no HA required)
hardware/
  kicad/            — KiCad schematic and PCB files
  gerbers/          — Fabrication files
  bom/              — Bill of materials
docs/
  assembly.md       — Wiring guide and breadboard prototype instructions
CONTEXT.md          — Full project background and design decisions
```

## Status

Breadboard prototype validated. PCB schematic complete, layout in progress.

---

## Firmware Option 1 — ESPHome (Home Assistant)

Best if you run Home Assistant. IR codes are configured in YAML. The device appears
in HA as two buttons (PC Power, PC Reset) and two physical input sensors.

### Flashing (ESPHome)

1. Install the [ESPHome add-on](https://esphome.io/guides/getting_started_hassio) in Home Assistant.
2. Copy `firmware/esphome/secrets.yaml.example` to `firmware/esphome/secrets.yaml` and fill in your WiFi credentials and keys. This file is gitignored and will never be committed.
3. Add `firmware/esphome/pc-ir-remote.yaml` to your ESPHome dashboard.
4. Connect the ESP32-C3 via USB and click **Install → Plug into this computer**.

### Learning IR Codes (ESPHome)

IR codes are learned once and written into the YAML config.

1. Flash the device — `dump: all` is enabled by default.
2. Open the ESPHome logs (dashboard → your device → Logs).
3. Point your remote at the TSOP38238 and press each button you want to use.
4. The log shows the protocol and code for each press, e.g.:
   ```
   [D][remote.nec] Received NEC: address=0x04, command=0x08
   ```
5. Edit `pc-ir-remote.yaml` — find the commented `on_nec` blocks and fill them in:
   ```yaml
   on_nec:
     address: 0x04
     command: 0x08
     then:
       - button.press: pc_power_button
   ```
6. Add a second block for the reset button if needed.
7. Comment out `dump: all`, then reflash.

> If your remote uses a different protocol (Samsung, RC5, etc.) the log will show
> the correct protocol name — replace `on_nec` with `on_samsung`, `on_rc5`, etc.

---

## Firmware Option 2 — Standalone Arduino Sketch

Best if you don't use Home Assistant. IR codes are learned at runtime by pressing
the onboard buttons — no computer or reflash needed after initial setup.

### Flashing (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).
2. Open the folder `firmware/arduino/pc-ir-remote/` in VS Code.
3. PlatformIO will auto-install the required libraries (`IRremoteESP8266`, `Preferences`).
4. Connect the ESP32-C3 via USB.
5. Click **Upload** (→ arrow in the PlatformIO toolbar) or run `pio run --target upload`.
6. Open the Serial Monitor at 115200 baud to confirm startup.

### Learning IR Codes (Arduino sketch)

IR codes are learned at runtime using the onboard buttons. Codes are saved to flash
and survive power cycles — you only need to do this once per remote button.

**To learn the power button code:**
1. Press the **Learn Power** button (GPIO5) on the board.
2. The status LED flashes **3 times repeatedly** — the board is waiting.
3. Point your remote at the TSOP38238 and press the button you want to use for power.
4. The LED flashes **3 quick blinks** — code saved.

**To learn the reset button code:**
1. Press the **Learn Reset** button (GPIO6) on the board.
2. The status LED flashes **2 times repeatedly** — the board is waiting.
3. Press the desired button on your remote.
4. The LED flashes **3 quick blinks** — code saved.

**LED feedback reference:**

| Pattern | Meaning |
|---------|---------|
| 3 repeating flashes | Learn power mode active — waiting for IR |
| 2 repeating flashes | Learn reset mode active — waiting for IR |
| 3 quick blinks | Code saved successfully |
| 2 slow blinks | Learn mode timed out (10s, no IR received) |
| 1 blink | IR command received and relay fired |

**Debug mode:** Set `DUMP_ALL_CODES true` in the sketch to print every received IR
code to Serial. Useful for finding codes if the learn buttons are not yet wired.

---

## License

TBD (likely MIT for firmware, CERN-OHL-P for hardware)
