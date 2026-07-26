/*
 * PC IR Remote — Relay test / troubleshooting build
 * NOT for normal use. Standalone sketch for exercising the relay outputs
 * without needing an IR remote. Based on pc-ir-remote.ino.
 *
 * BEHAVIOR:
 *   - Every AUTO_CYCLE_INTERVAL_MS, automatically pulses POWER then RESET,
 *     alternating, so you can confirm both relays click on their own.
 *   - Learn Power / Learn Reset buttons (GPIO5 / GPIO6) manually pulse the
 *     matching relay on demand, independent of the auto cycle — useful for
 *     confirming a specific relay/header wiring without waiting.
 *   - The onboard LED is lit for the exact duration of every relay pulse
 *     (auto or manual), so you get a visual heartbeat alongside the click.
 *
 * WIRING (same as pc-ir-remote.ino, IR receiver not used here):
 *   GPIO1 → relay coil trigger  (relay contacts → PWR_SW header)
 *   GPIO2 → relay coil trigger  (relay contacts → RST_SW header)
 *   Test Power btn → GPIO5  (other leg to GND, active-low)
 *   Test Reset btn → GPIO6  (other leg to GND, active-low)
 *
 * LIBRARIES: none beyond core Arduino/ESP32.
 */

// =============================================================================
// CONFIGURATION — edit these to match your hardware
// =============================================================================

#define RELAY_ACTIVE_ON_HIGH    false  // true = relay fires on HIGH, false = LOW
#define STATUS_LED_ACTIVE_ON_HIGH false

#define POWER_PIN                1      // relay coil trigger → PWR_SW header
#define RESET_PIN                2      // relay coil trigger → RST_SW header
#define TEST_POWER_BTN_PIN       5      // active-low, internal pull-up
#define TEST_RESET_BTN_PIN       6      // active-low, internal pull-up

// STATUS_LED_PIN options:
//   8   → onboard LED on ESP32-C3 SuperMini boards. NOTE: the Arduino
//         LED_BUILTIN macro resolves to GPIO7 for PlatformIO's "lolin_c3_mini"
//         board (genuine Wemos/Lolin pinout), which does NOT match the
//         SuperMini clone boards this project targets — use the literal 8.
//   -1  → no LED (disable all LED feedback)
#define STATUS_LED_PIN           8

#define PULSE_MS                 500    // relay pulse duration (ms)
#define DEBOUNCE_MS              20     // button debounce window (ms)
#define AUTO_CYCLE_INTERVAL_MS   1000   // auto pulse every N ms, alternating power/reset

// =============================================================================
// DERIVED VALUES — do not edit
// =============================================================================

#define RELAY_ON_LEVEL           (RELAY_ACTIVE_ON_HIGH ? HIGH : LOW)
#define RELAY_OFF_LEVEL          (RELAY_ACTIVE_ON_HIGH ? LOW : HIGH)
#define LED_ON                   (STATUS_LED_ACTIVE_ON_HIGH ? HIGH : LOW)
#define LED_OFF                  (STATUS_LED_ACTIVE_ON_HIGH ? LOW : HIGH)

// =============================================================================
// BUTTON HELPER
// Debounced edge-detection. Call justPressed() each loop — returns true once
// per physical press (falling edge only).
// =============================================================================

struct Button {
  uint8_t pin;
  bool stable;
  bool lastRaw;
  unsigned long debounceTime;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    stable = false;
    lastRaw = false;
    debounceTime = 0;
  }

  bool justPressed() {
    bool raw = (digitalRead(pin) == LOW);
    if (raw != lastRaw) {
      debounceTime = millis();
      lastRaw = raw;
    }
    if ((millis() - debounceTime) > DEBOUNCE_MS) {
      if (raw && !stable) { stable = true; return true; }
      if (!raw) stable = false;
    }
    return false;
  }
};

Button testPowerBtn;
Button testResetBtn;

unsigned long lastAutoTime = 0;
bool autoNextIsPower = true;

// =============================================================================
// LED
// =============================================================================

void ledSet(bool on) {
  if (STATUS_LED_PIN < 0) return;
  digitalWrite(STATUS_LED_PIN, on ? LED_ON : LED_OFF);
}

// =============================================================================
// RELAY
// pulseRelay blocks for PULSE_MS — intentional, simulates a real button press.
// =============================================================================

void relaySet(uint8_t pin, bool active) {
  digitalWrite(pin, active ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

void pulseRelay(uint8_t pin, const char* label) {
  Serial.print("Pulsing "); Serial.println(label);
  relaySet(pin, true);
  ledSet(true);
  delay(PULSE_MS);
  relaySet(pin, false);
  ledSet(false);
  Serial.println("Done.");
}

void pulsePowerButton() { pulseRelay(POWER_PIN, "power button..."); }
void pulseResetButton() { pulseRelay(RESET_PIN, "reset button..."); }

// =============================================================================
// SETUP
// =============================================================================

void setup() {
  // Relay outputs — ensure relays are off before anything else
  pinMode(POWER_PIN, OUTPUT);
  pinMode(RESET_PIN, OUTPUT);
  relaySet(POWER_PIN, false);
  relaySet(RESET_PIN, false);

  // Status LED
  if (STATUS_LED_PIN >= 0) {
    pinMode(STATUS_LED_PIN, OUTPUT);
    ledSet(false);
  }

  // Test buttons
  testPowerBtn.begin(TEST_POWER_BTN_PIN);
  testResetBtn.begin(TEST_RESET_BTN_PIN);

  Serial.begin(115200);
  Serial.println("PC IR Remote — RELAY TEST build starting...");
  Serial.printf("Auto-cycling power/reset every %dms. Press test buttons to pulse on demand.\n",
                AUTO_CYCLE_INTERVAL_MS);

  lastAutoTime = millis();
  Serial.println("Ready.");
}

// =============================================================================
// MAIN LOOP
//   1. Manual test buttons — pulse the matching relay on demand
//   2. Auto cycle          — alternates power/reset every AUTO_CYCLE_INTERVAL_MS
// =============================================================================

void loop() {
  // 1. Manual button check
  if (testPowerBtn.justPressed()) pulsePowerButton();
  if (testResetBtn.justPressed()) pulseResetButton();

  // 2. Auto cycle
  if (millis() - lastAutoTime >= AUTO_CYCLE_INTERVAL_MS) {
    lastAutoTime = millis();
    if (autoNextIsPower) pulsePowerButton();
    else                 pulseResetButton();
    autoNextIsPower = !autoNextIsPower;
  }
}
