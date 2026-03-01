# Project Context — PC IR Remote (pc-ir-remote)

This file is a briefing document for AI assistants (Claude, etc.) to get up to speed
on this project quickly. Read this before helping with any task in this repo.

---

## Project Goal

Build a small custom PCB that sits inside or outside a PC and allows a TV remote (or
similar IR remote) to turn the PC on, off, or perform other functions. Phase 2 extends
this to USB HID control and Home Assistant integration.

---

## Owner / Environment

- Project location: `/mnt/c/Users/bonkas/Nextcloud/PCB Design/KiCad/pc-ir-remote-ESP32`
- Platform: Windows (WSL2 for tooling), KiCad for PCB design
- HA instance running with ESPHome Builder add-on installed
- Has physical components: Arduino Nano (set aside), ESP32 dev boards, TSOP38238 IR
  receiver, existing relay board (for prototyping)

---

## Key Decisions Made

### Microcontroller: ESP32 (chosen over Arduino Nano)
- Arduino Nano was the original idea but requires an extra WiFi module for HA
- ESP32 has built-in WiFi, same price range, Arduino-framework compatible
- ESPHome runs natively on ESP32 with near-zero custom code for HA integration

### Firmware: ESPHome (primary) + Arduino C++ sketch (secondary/standalone)
- ESPHome is the primary target — YAML-based, HA integration is automatic via native API
- A standalone Arduino C++ sketch will also be published for users who don't use HA
- Both will live in separate subdirectories under `firmware/`
- Language: YAML (ESPHome) and C++ (Arduino framework)

### Relay → MOSFET Phased Approach
- Phase 1: Use existing relay board for prototyping (simpler, already owned)
- Phase 2: Replace relay with a small N-channel MOSFET (e.g. AO3400, BS170, 2N7000)
- PC power button header is a momentary short to ground on a low-voltage logic line
  — a small signal MOSFET handles this perfectly
- PCB will include BOTH footprints (relay + MOSFET) with a solder jumper to select
- Code change between the two is minimal

### Power Input: Multiple options on PCB, populate as needed
- Internal USB 2.0 motherboard header (2x5 2.54mm, 1 pin missing) — standby 5V,
  always powered when PC has mains power. Phase 1 uses 5V+GND only.
- External USB-A or USB-C port — for external/bench use
- Screw terminals (3.5mm) — generic 5V from any source
- All three footprints on PCB, populate whichever is needed

---

## Phase Plan

### Phase 1 (current)
- ESP32 + TSOP38238 IR receiver + relay → PC motherboard power button header
- Power from internal USB header (5V/GND only)
- ESPHome config: receive IR codes, trigger relay to pulse power button
- Get working on breadboard FIRST, then design PCB
- Use existing relay board to validate logic before committing to PCB

### Phase 2 (future)
- Replace relay with MOSFET (minor PCB rev, minimal code change)
- Use USB D+/D- from motherboard header for USB HID
- Allow remote to send keystrokes or media commands to PC
- Possibly add ESP32's Bluetooth for additional control options

### Phase 3 (future)
- Bidirectional HA integration (HA can also send commands to the ESP32)
- Monitor PC state (on/off) and report back to HA
- Potentially read PC wake/sleep signals

---

## Component List (confirmed)

| Component | Part | Notes |
|-----------|------|-------|
| MCU | ESP32-C3 Super Mini | PlatformIO board: `lolin_c3_mini`. GPIO1=relay coil trigger (→PWR_SW), GPIO2=relay coil trigger (→RST_SW), GPIO3=IR, GPIO5=learn power btn, GPIO6=learn reset btn, GPIO7=external status LED (330Ω→LED→GND). GPIO8=onboard LED_BUILTIN (alternative, set STATUS_LED_PIN in firmware). LED feedback: 3 repeating flashes=learn power active, 2 repeating flashes=learn reset active, 3 quick blinks=saved, 2 slow blinks=timed out. |
| IR Receiver | TSOP38238 | 38kHz carrier, 3-pin (OUT, GND, VS) |
| Optocoupler | PC817 | One per relay — DC optocoupler, single LED input, isolates ESP32 GPIO from relay coil circuit |
| Relay driver | S8550 PNP (SOT-23) | One per relay — driven by PC817 output |
| Relay | JY3FF-S-DC5V-C SPDT | Two fitted — K1 (power button), K2 (reset button) |
| Flyback diodes | 1N914 (SOD-323F) | One per relay coil — suppresses back-EMF |
| Status LEDs | LED 0805 | One per relay — visual indicator |
| Switching | Relay (phase 1) / MOSFET (phase 2) | AO3400 or BS170 for MOSFET |
| Power | Internal USB 2.0 header / USB port / screw terminals | Populate as needed |
| Decoupling | 100nF cap near TSOP38238 VS pin | Critical — prevents false triggers |
| Gate resistor | 100Ω on MOSFET gate | Also helps with ESP32 WiFi RF noise on TSOP |

---

## Critical Hardware Notes

1. **TSOP38238 decoupling**: A 100nF ceramic cap between VS and GND, placed physically
   close to the IC, is required by the datasheet. Without it you get false IR triggers,
   especially with ESP32 WiFi active. Also add a small series resistor (100Ω) on the
   TSOP power line.

2. **Optocoupler (PC817)**: Used to isolate the ESP32 GPIO signals from the relay coil
   circuit. The PC817 is a DC optocoupler with a single LED on the input side — correct
   choice for GPIO-driven applications. The LTV-354T (AC/DC, dual anti-parallel LEDs)
   was considered and rejected in favour of the PC817 for this reason.

3. **PC power button header**: Typically a 2-pin header on the motherboard labelled
   PWR_SW. It is a momentary short to ground — usually pulled up to 3.3V or 5V on the
   motherboard side. A MOSFET or relay can safely simulate this. No high voltages.

4. **Standby power**: The internal USB header provides 5V standby power even when the
   PC is off (as long as the PSU is connected to mains). This is how the device can
   receive IR "power on" commands when the PC is off.

5. **USB header pinout (USB 2.0 internal, 9-pin 2x5)**:
   - Pin 1: +5V (red)
   - Pin 2: D- (white)
   - Pin 3: D+ (green)
   - Pin 4: GND (black)
   - Pin 5: Key (missing)
   Each port pair repeats. Use any port's 5V+GND for power in Phase 1.

---

## ESPHome Config Overview

The ESPHome YAML (`firmware/esphome/pc-ir-remote.yaml`) is structured as follows:

- **Board**: `lolin_c3_mini` / `ESP32C3` / `arduino` framework
- **IR receiver**: `remote_receiver` using list format (ESPHome 2026.x requirement):
  ```yaml
  remote_receiver:
    - pin:
        number: GPIO3
        inverted: true
      dump: all   # comment out after learning codes
  ```
- **IR matching**: via `binary_sensor` with `platform: remote_receiver`. `internal: true`
  hides these from the HA UI — they still trigger automations.
  ```yaml
  binary_sensor:
    - platform: remote_receiver
      name: "IR Power Button"
      internal: true
      nec:
        address: 0x????
        command: 0x????
      on_press:
        - button.press: pc_power_button
  ```
- **Relay switches**: GPIO1 (power) and GPIO2 (reset), active-low, `restore_mode: ALWAYS_OFF`
- **Template buttons**: `pc_power_button` and `pc_reset_button` — 500ms relay pulse + LED flash
- **Physical buttons**: GPIO5 (power) and GPIO6 (reset), `internal: true`
- **Status LED**: GPIO7 external (330Ω series) or GPIO8 onboard — controlled via `output` + `binary light`

---

## Repository Structure

```
pc-ir-remote/
├── hardware/
│   ├── gerbers/        # Exported fab files for PCB manufacturing
│   └── bom/            # Bill of materials (CSV + interactive HTML BOM)
│   (kicad/ is gitignored — source files not published)
├── firmware/
│   ├── esphome/        # ESPHome YAML config + secrets template
│   └── arduino/        # Standalone PlatformIO/Arduino sketch
├── docs/
│   ├── assembly.md     # Full wiring guide
│   └── setup.md        # Flashing and IR code setup for both firmware options
├── CONTEXT.md          # This file — AI assistant briefing
└── README.md           # Public-facing project description
```

---

## Workflow / Build Order

1. Breadboard prototype: ESP32 + TSOP38238 + relay
2. Validate IR decoding and relay firing in ESPHome (use `dump: all` to learn codes)
3. Draw schematic in KiCad (capture verified circuit)
4. PCB layout in KiCad (start simple, single layer if possible)
5. Order boards (JLCPCB / PCBWay / OSHPark)
6. Assemble and test
7. Phase 2: Replace relay with MOSFET, minor PCB rev
8. Phase 2: Add USB HID via motherboard header D+/D-

---

## Publishing Intent

- GitHub public repo for community use
- Includes gerbers, BOM, ESPHome YAML, and Arduino sketch
- KiCad source files are gitignored and not published (gerbers are sufficient for fabrication)
- Goal: someone should be able to clone the repo and have everything they need to build and flash
- License: TBD (likely MIT for firmware, CERN-OHL-P for hardware)

---

## Open Questions / TODO

- [ ] Finalise PCB layout in KiCad and export gerbers
- [ ] Choose PCB manufacturer and check design rules (JLCPCB / PCBWay / OSHPark)
- [ ] Phase 2: Replace relay with MOSFET (minor PCB rev, minimal code change)
- [ ] Phase 2: USB HID via motherboard header D+/D-
