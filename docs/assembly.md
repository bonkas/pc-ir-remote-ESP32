# Assembly Guide

> This document will be filled in once the PCB design is complete.
> For now, refer to the breadboard wiring below for the prototype phase.

## Breadboard Prototype Wiring

### ESP32-C3 Super Mini — Confirmed Pin Assignments

| Function | GPIO | Notes |
|----------|------|-------|
| IR Receiver OUT | GPIO3 | Input |
| PC Power Button relay | GPIO1 | Output — relay coil trigger (relay contacts → PWR_SW header) |
| PC Reset Button relay | GPIO2 | Output — relay coil trigger (relay contacts → RST_SW header) |
| Learn Power Button | GPIO5 | Input, pull-up, active-low (button to GND) |
| Learn Reset Button | GPIO6 | Input, pull-up, active-low (button to GND) |
| Status LED | GPIO7 | Output — external LED (GPIO7 → 330Ω → LED → GND). Change `STATUS_LED_PIN` in firmware to `LED_BUILTIN` to use onboard LED (GPIO8) instead, or `-1` to disable. |

### TSOP38238 IR Receiver

| TSOP38238 Pin | Connects To |
|---------------|-------------|
| OUT (pin 1)   | GPIO3 |
| GND (pin 2)   | GND |
| VS (pin 3)    | 3.3V via 100Ω series resistor |

- Place a 100nF ceramic capacitor between VS and GND, as close to the IC as possible.
- This is required — without it, ESP32 WiFi causes false IR triggers.
- Use 3.3V supply only — ESP32-C3 GPIOs are not 5V tolerant.

### Learn Buttons (GPIO5 and GPIO6)

- One leg to GPIO, other leg to GND. No external resistor needed (`INPUT_PULLUP`).
- Not pressed = HIGH, pressed = LOW (active-low), debounced in firmware.
- **Learn Power (GPIO5):** press, then press the desired remote button → saved to flash
- **Learn Reset (GPIO6):** press, then press the desired remote button → saved to flash
- Codes survive power cycles (stored via `Preferences` in ESP32 flash).
- LED feedback: 3 repeating flashes = learn power mode active, 2 repeating flashes = learn reset mode active, 3 quick blinks = code saved, 2 slow blinks = timed out.
- Learn mode cancels automatically after 10 seconds if no IR signal received.

### PC Reset Button (RST_SW)

- GPIO2 → relay coil trigger (relay contacts → PC motherboard RST_SW header).
- Same wiring approach as the power button relay.
- A short pulse (500ms) triggers a warm reset on the PC.
- **Note:** GPIO2 is a strapping pin on the ESP32-C3. If your relay module pulls GPIO2 LOW at boot it could affect boot behaviour. If you see boot issues, add a 10kΩ pull-up resistor between GPIO2 and 3.3V.

### Relay Driver Circuit (per relay)

Each relay is driven by a **PC817 optocoupler** + **S8550 PNP transistor** chain:

```
ESP32 GPIO → PC817 LED (anode+cathode) → PC817 phototransistor → S8550 base → relay coil
```

- PC817 input: anode to GPIO via series resistor, cathode to GND
- PC817 output (phototransistor): collector to S8550 base via resistor, emitter to GND
- S8550 PNP: drives relay coil from 5V
- 1N914 flyback diode across relay coil (cathode to VCC, anode to coil-)
- Status LED in parallel with relay coil for visual indication

**Note:** PC817 chosen over LTV-354T — PC817 is a DC optocoupler (single LED input),
better suited to GPIO-driven applications. LTV-354T is AC/DC (dual anti-parallel LEDs)
and not needed here.

### PC Motherboard PWR_SW Header

- Connect the relay's COM and NO terminals across the two pins of the PWR_SW header.
- Polarity does not matter — it is just a short circuit.
- A 500ms pulse simulates a normal power button press.
- A 5+ second hold simulates a force shutdown (implement with longer delay if needed).

### Power Supply (Breadboard Phase)

- Power the ESP32 from USB-C during development.
- For installed use: tap 5V and GND from the motherboard's internal USB 2.0 header into the ESP32-C3 Super Mini 5V pin.
- The 5V pin feeds the onboard LDO which generates 3.3V for the chip.

---

## PCB Assembly

*Coming once PCB design is complete.*
