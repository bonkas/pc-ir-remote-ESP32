/*
 * PC IR Remote — Standalone Arduino/ESP32-C3 sketch
 * No Home Assistant required. Uses IRremoteESP8266 library.
 *
 * See CONTEXT.md for full project background and wiring notes.
 *
 * WIRING:
 *   TSOP38238 OUT     → GPIO3
 *   TSOP38238 VS      → 3.3V via 100Ω resistor + 100nF cap to GND
 *   TSOP38238 GND     → GND
 *   GPIO1 → relay coil trigger  (relay contacts → PWR_SW header)
 *   GPIO2 → relay coil trigger  (relay contacts → RST_SW header)
 *   Learn Power btn   → GPIO5  (other leg to GND, active-low)
 *   Learn Reset btn   → GPIO6  (other leg to GND, active-low)
 *
 * LEARNING IR CODES:
 *   Press Learn Power button, then press the desired button on your remote.
 *   Press Learn Reset button, then press the desired button on your remote.
 *   Codes are saved to flash and survive power cycles.
 *
 * DEBUG:
 *   Set DUMP_ALL_CODES true to print every received IR code to Serial.
 *   Useful for finding codes if the buttons are not yet wired.
 *
 * LIBRARIES: IRremoteESP8266, Preferences (built-in ESP32 Arduino core)
 */

#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include <IRutils.h>
#include <Preferences.h>

// =============================================================================
// CONFIGURATION — edit these to match your hardware
// =============================================================================

#define DUMP_ALL_CODES          false  // true: print all received IR codes to Serial

#define RELAY_ACTIVE_ON_HIGH    false  // true = relay fires on HIGH, false = LOW
#define STATUS_LED_ACTIVE_ON_HIGH false

#define IR_RECV_PIN             3
#define POWER_PIN               1      // relay coil trigger → PWR_SW header
#define RESET_PIN               2      // relay coil trigger → RST_SW header
#define LEARN_POWER_BTN_PIN     5      // active-low, internal pull-up
#define LEARN_RESET_BTN_PIN     6      // active-low, internal pull-up

// STATUS_LED_PIN options:
//   LED_BUILTIN  → onboard LED (GPIO8 on ESP32-C3 Super Mini)
//   7            → external LED on GPIO7 (wire: GPIO7 → 330Ω → LED → GND)
//   -1           → no LED (disable all LED feedback)
#define STATUS_LED_PIN          LED_BUILTIN

#define PULSE_MS                500    // relay pulse duration (ms)
#define LEARN_TIMEOUT_MS        10000  // cancel learn mode after 10s with no IR
#define DEBOUNCE_MS             20     // button debounce window (ms)
#define BLINK_MS                80     // LED blink on/off duration (ms)
#define LEARN_PAUSE_MS          500    // gap between blink groups in learn mode

// =============================================================================
// DERIVED VALUES — do not edit
// =============================================================================

#define RELAY_ON_LEVEL          (RELAY_ACTIVE_ON_HIGH ? HIGH : LOW)
#define RELAY_OFF_LEVEL         (RELAY_ACTIVE_ON_HIGH ? LOW : HIGH)
#define LED_ON                  (STATUS_LED_ACTIVE_ON_HIGH ? HIGH : LOW)
#define LED_OFF                 (STATUS_LED_ACTIVE_ON_HIGH ? LOW : HIGH)

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

// =============================================================================
// STATE
// =============================================================================

enum LearnState { IDLE, LEARN_POWER, LEARN_RESET };
LearnState learnState = IDLE;
unsigned long learnStartTime = 0;

uint32_t irCodePower = 0;  // loaded from flash at boot
uint32_t irCodeReset = 0;

IRrecv irrecv(IR_RECV_PIN);
decode_results results;
Preferences prefs;
Button learnPowerBtn;
Button learnResetBtn;

// =============================================================================
// LED
// ledSet / ledBlink are blocking helpers used for one-shot feedback (e.g. saved,
// timed out). updateLearnLed is non-blocking and called every loop iteration
// while in learn mode.
// =============================================================================

void ledSet(bool on) {
  if (STATUS_LED_PIN < 0) return;
  digitalWrite(STATUS_LED_PIN, on ? LED_ON : LED_OFF);
}

void ledBlink(int times) {
  for (int i = 0; i < times; i++) {
    ledSet(true);  delay(BLINK_MS);
    ledSet(false); delay(BLINK_MS);
  }
}

// Drives the repeating learn-mode blink pattern. Called every loop iteration.
//   LEARN_POWER → 3 quick flashes then pause, repeating
//   LEARN_RESET → 2 quick flashes then pause, repeating
void updateLearnLed() {
  if (learnState == IDLE) return;
  int blinks = (learnState == LEARN_POWER) ? 3 : 2;
  unsigned long period = (unsigned long)blinks * 2 * BLINK_MS + LEARN_PAUSE_MS;
  unsigned long t = (millis() - learnStartTime) % period;
  int idx = t / (2 * BLINK_MS);
  unsigned long blinkT = t % (2 * BLINK_MS);
  ledSet(idx < blinks && blinkT < BLINK_MS);
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
  delay(PULSE_MS);
  relaySet(pin, false);
  Serial.println("Done.");
}

void pulsePowerButton() { pulseRelay(POWER_PIN, "power button..."); }
void pulseResetButton() { pulseRelay(RESET_PIN, "reset button..."); }

// =============================================================================
// FLASH STORAGE
// IR codes are stored in NVS (non-volatile storage) using the Preferences
// library. Codes survive power cycles.
// =============================================================================

void loadCodes() {
  prefs.begin("ir-remote", true);
  irCodePower = prefs.getUInt("power_code", 0);
  irCodeReset = prefs.getUInt("reset_code", 0);
  prefs.end();
  Serial.printf("Power code: 0x%08X\n", irCodePower);
  Serial.printf("Reset code: 0x%08X\n", irCodeReset);
  if (irCodePower == 0) Serial.println("WARNING: Power code not set — press Learn Power button.");
  if (irCodeReset == 0) Serial.println("WARNING: Reset code not set — press Learn Reset button.");
}

void saveCode(const char* key, uint32_t code) {
  prefs.begin("ir-remote", false);
  prefs.putUInt(key, code);
  prefs.end();
}

// =============================================================================
// LEARN MODE
// enterLearnMode — called when a learn button is pressed (idle state only).
// exitLearnMode  — called on successful save or timeout.
// =============================================================================

void enterLearnMode(LearnState mode) {
  learnState = mode;
  learnStartTime = millis();
  irrecv.resume();  // clear any stale IR data before listening
  if (mode == LEARN_POWER) {
    Serial.println("LEARN POWER: press the remote button to use for power...");
  } else {
    Serial.println("LEARN RESET: press the remote button to use for reset...");
  }
}

void exitLearnMode(bool success) {
  learnState = IDLE;
  ledSet(false);
  if (success) {
    ledBlink(3);  // 3 quick blinks = saved
  } else {
    Serial.println("Learn mode timed out — no code received.");
    // 2 slow blinks = cancelled
    ledSet(true); delay(300); ledSet(false); delay(300);
    ledSet(true); delay(300); ledSet(false);
  }
}

// =============================================================================
// IR RECEIVE HANDLER
// Called every loop iteration. In learn mode, saves the received code and exits
// learn mode. In normal operation, fires the relay matching the stored code.
// =============================================================================

void handleIR() {
  if (!irrecv.decode(&results)) return;

  uint32_t code = (uint32_t)results.value;

  if (code != 0xFFFFFFFF && code != 0) {  // ignore repeat / empty codes

    if (learnState == LEARN_POWER) {
      irCodePower = code;
      saveCode("power_code", code);
      Serial.printf("Power code saved: 0x%08X\n", code);
      exitLearnMode(true);

    } else if (learnState == LEARN_RESET) {
      irCodeReset = code;
      saveCode("reset_code", code);
      Serial.printf("Reset code saved: 0x%08X\n", code);
      exitLearnMode(true);

    } else {
      if (DUMP_ALL_CODES) {
        Serial.printf("Protocol: %s  Code: 0x%08X\n",
                      typeToString(results.decode_type).c_str(), code);
      }
      if (irCodePower != 0 && code == irCodePower) {
        ledBlink(1);
        pulsePowerButton();
      } else if (irCodeReset != 0 && code == irCodeReset) {
        ledBlink(1);
        pulseResetButton();
      }
    }
  }

  irrecv.resume();  // ready to receive next code
}

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

  // Learn buttons
  learnPowerBtn.begin(LEARN_POWER_BTN_PIN);
  learnResetBtn.begin(LEARN_RESET_BTN_PIN);

  // Serial + startup
  Serial.begin(115200);
  Serial.println("PC IR Remote starting...");

  // Load saved IR codes from flash, then start the IR receiver
  loadCodes();
  irrecv.enableIRIn();

  Serial.println("Ready.");
}

// =============================================================================
// MAIN LOOP
// Each iteration runs four tasks in order:
//   1. Check learn buttons  — enters learn mode if a button is pressed
//   2. Update learn LED     — drives the repeating blink pattern (non-blocking)
//   3. Check learn timeout  — cancels learn mode after LEARN_TIMEOUT_MS
//   4. Handle IR input      — saves code (learn mode) or fires relay (normal)
// =============================================================================

void loop() {
  // 1. Learn button check (only while idle)
  if (learnState == IDLE) {
    if (learnPowerBtn.justPressed()) enterLearnMode(LEARN_POWER);
    if (learnResetBtn.justPressed()) enterLearnMode(LEARN_RESET);
  }

  // 2. Learn mode LED pattern
  updateLearnLed();

  // 3. Learn mode timeout
  if (learnState != IDLE && (millis() - learnStartTime) > LEARN_TIMEOUT_MS) {
    exitLearnMode(false);
  }

  // 4. IR receive
  handleIR();
}
