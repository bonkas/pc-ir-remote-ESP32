# PC IR Remote

A custom PCB that lets you control your PC's power and reset buttons using any IR remote.
Built around an ESP32-C3 Super Mini. Works with Home Assistant via ESPHome, or fully
standalone — no smart home required.

---

## How It Works

The TSOP38238 IR receiver picks up signals from any standard TV remote. The ESP32-C3
decodes the signal and briefly activates a relay, which shorts the two pins of the
motherboard's power button header — exactly as if you pressed the physical button.
A second relay does the same for the reset button.

The board can be powered in two ways. For internal installation, it taps 5V standby
from the motherboard's internal USB 2.0 header — this keeps the board powered whenever
the PC is plugged into mains, so it can receive a power-on command even when the PC is
off. For external or bench use, it can be powered directly via the USB-C port on the
ESP32-C3 Super Mini.

```
IR Remote → TSOP38238 → ESP32-C3 → Relay → PWR_SW / RST_SW header on motherboard
```

---

## Features

- Power on/off and reset your PC with any IR remote
- Powered from the motherboard internal USB header (standby 5V) — works when the PC is off
- Alternatively powered via USB-C on the ESP32-C3 for external or bench use
- Two firmware options: Home Assistant (ESPHome) or fully standalone (Arduino sketch)
- IR codes stored in flash and survive power cycles
- Physical buttons on the board for direct manual triggering (no remote needed)
- Status LED feedback for all actions

---

## Documentation

| File | What it covers | Read when |
|------|---------------|-----------|
| `docs/assembly.md` | Full wiring guide — TSOP38238, relays, buttons, LED, power supply, breadboard prototype | Building or wiring the hardware |
| `docs/setup.md` | Step-by-step flashing and IR code setup for both firmware options | Setting up the firmware |
| `CONTEXT.md` | Full project background, design decisions, phase plan, component choices | Understanding why things are done the way they are |

---

## Project Status

Breadboard prototype validated. PCB schematic complete, layout in progress.
Gerbers and BOM will be added once the PCB layout is finalised.

---

## What You Need to Build One

### Components

| Component | Part | Qty |
|-----------|------|-----|
| MCU | ESP32-C3 Super Mini | 1 |
| IR Receiver | TSOP38238 (38kHz) | 1 |
| Optocoupler | PC817 (DIP-4) | 2 |
| Relay driver transistor | S8550 PNP (SOT-23) | 2 |
| Relay | JY3FF-S-DC5V-C SPDT | 2 |
| Flyback diode | 1N914 (SOD-323F) | 2 |
| Decoupling capacitor | 100nF ceramic (0805) | 1 |
| TSOP series resistor | 100Ω (0805) | 1 |
| Status LED | LED (0805) | 1 |
| LED resistor | 330Ω (0805) | 1 |

See `hardware/bom/` for the full BOM with values and supplier links.

### PCB

Gerbers for fabrication are in `hardware/gerbers/`. Upload the zip to your preferred
PCB manufacturer (JLCPCB, PCBWay, OSHPark, etc.) with default 2-layer settings.

### Tools and Software

- Soldering iron (SMD-capable)
- VS Code + PlatformIO extension (Arduino sketch), or Home Assistant + ESPHome add-on
- USB-C cable for flashing

---

## Connecting to Your PC

1. Locate the **PWR_SW** and **RST_SW** headers on your motherboard (usually labelled
   on the PCB near the front panel connector block — check your motherboard manual).
2. Connect the relay contacts to the appropriate header:
   - K1 relay COM + NO → PWR_SW header (2 pins, polarity does not matter)
   - K2 relay COM + NO → RST_SW header (2 pins, polarity does not matter)
3. For power, tap 5V and GND from the motherboard's internal USB 2.0 header
   (the 9-pin 2×5 header, 2.54mm pitch). Use any port's 5V and GND pins.

See `docs/assembly.md` for the full wiring guide including the TSOP38238 and buttons.

---

## Pin Assignments (ESP32-C3 Super Mini)

| Function | GPIO | Notes |
|----------|------|-------|
| IR Receiver OUT | GPIO3 | Input |
| Power relay | GPIO1 | Output — relay contacts → PWR_SW header |
| Reset relay | GPIO2 | Output — relay contacts → RST_SW header |
| Learn Power / Power trigger | GPIO5 | Input, active-low, internal pull-up |
| Learn Reset / Reset trigger | GPIO6 | Input, active-low, internal pull-up |
| Status LED | GPIO7 | Output — external LED via 330Ω, or change to GPIO8 for onboard LED |

---

## Choosing a Firmware

| | ESPHome | Arduino Sketch |
|---|---|---|
| **Best for** | Home Assistant users | Anyone — no smart home needed |
| **IR code setup** | Edit YAML, reflash | Press button on board, point remote |
| **Changing codes later** | Edit YAML, reflash | Press button on board again |
| **HA integration** | Full — appears as devices in HA | None |
| **Internet/WiFi required** | Yes | No |
| **Standalone after setup** | Yes | Yes |

---

## Firmware Option 1 — ESPHome (Home Assistant)

### Setup

1. Install the [ESPHome add-on](https://esphome.io/guides/getting_started_hassio) in Home Assistant.
2. Copy `firmware/esphome/secrets.yaml.example` to `firmware/esphome/secrets.yaml`
   and fill in your WiFi credentials and API key.
   This file is gitignored and will never be committed.
3. Add `firmware/esphome/pc-ir-remote.yaml` to your ESPHome dashboard.
4. Connect the ESP32-C3 via USB-C and click **Install → Plug into this computer**.

### Learning IR Codes

IR codes are identified from logs, then written into the YAML — a one-time process.

1. Flash the device — `dump: all` is enabled by default.
2. Open the ESPHome logs (dashboard → your device → **Logs**).
3. Point your remote at the TSOP38238 and press each button you want to use.
4. The log shows the protocol and code, e.g.:
   ```
   [D][remote.nec] Received NEC: address=0x04, command=0x08
   ```
5. Edit `pc-ir-remote.yaml` — uncomment and fill in the `on_nec` blocks:
   ```yaml
   on_nec:
     address: 0x04
     command: 0x08
     then:
       - button.press: pc_power_button
   ```
6. Comment out `dump: all`, then reflash.

> If your remote uses a different protocol (Samsung, RC5, Sony, etc.) the log will
> show the correct protocol name — replace `on_nec` with `on_samsung`, `on_rc5`, etc.

For the full ESPHome setup walkthrough see `docs/setup.md`.

---

## Firmware Option 2 — Standalone Arduino Sketch

### Setup

1. Install [VS Code](https://code.visualstudio.com/) and the
   [PlatformIO extension](https://platformio.org/install/ide?install=vscode).
2. Open the folder `firmware/arduino/pc-ir-remote/` in VS Code.
3. PlatformIO auto-installs the required libraries on first open.
4. Connect the ESP32-C3 via USB-C.
5. Click the **→ Upload** arrow in the PlatformIO toolbar (bottom of VS Code).
6. Open the **Serial Monitor** (plug icon, 115200 baud) to confirm startup.

If upload fails with "Failed to connect": hold **BOOT**, tap **RESET**, release **BOOT**,
then try again.

### Learning IR Codes

Codes are learned at runtime — no reflash or computer needed after initial setup.

**Power button:**
1. Press the **Learn Power** button (GPIO5).
2. LED flashes **3 times repeatedly** — board is waiting for IR.
3. Point your remote and press the button you want to use for power.
4. LED flashes **3 quick blinks** — saved to flash.

**Reset button:**
1. Press the **Learn Reset** button (GPIO6).
2. LED flashes **2 times repeatedly** — board is waiting for IR.
3. Press the desired button on your remote.
4. LED flashes **3 quick blinks** — saved to flash.

Repeat at any time to change to a different remote or button.

### LED Feedback

| Pattern | Meaning |
|---------|---------|
| 3 repeating flashes | Learn power mode — waiting for IR |
| 2 repeating flashes | Learn reset mode — waiting for IR |
| 3 quick blinks | Code saved successfully |
| 2 slow blinks | Timed out — no IR received within 10s |
| 1 blink | IR command received, relay fired |

For the full Arduino setup walkthrough see `docs/setup.md`.

---

## Repository Layout

```
firmware/
  esphome/            — ESPHome YAML config + secrets template
  arduino/            — Standalone PlatformIO sketch
hardware/
  gerbers/            — PCB fabrication files
  bom/                — Bill of materials
docs/
  assembly.md         — Full wiring guide
  setup.md            — Flashing and IR code setup for both firmware options
CONTEXT.md            — Project background, design decisions, phase plan
```

---

## License

TBD — likely MIT for firmware, CERN-OHL-P for hardware.
