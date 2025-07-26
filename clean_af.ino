#include <Arduino.h>
#include "DRV8834.h"

// ─────────────────────────────────────────────────────────────────────────────
// Pin & Motor Configuration
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint8_t BUTTON_PIN       = 3;    // Input with pullup
constexpr uint8_t DIR_PIN          = 4;
constexpr uint8_t STEP_PIN         = 6;
constexpr uint8_t SLEEP_PIN        = 2;
constexpr uint8_t MICROSTEP_PIN_0  = 9;    // DRV8834 M0
constexpr uint8_t MICROSTEP_PIN_1  = 10;   // DRV8834 M1

constexpr uint8_t DEBOUNCE_MS      = 50;   // Debounce interval

constexpr int MOTOR_STEPS          = 200;  // Full‑step count per revolution

constexpr int RAMP_RPM             = 30;   
constexpr int SMEAR_RPM            = 150;  
constexpr int SMEARING_DELAY       = 3000; // Smearing Delay

// Instantiate your DRV8834 driver
DRV8834 stepper(MOTOR_STEPS, DIR_PIN, STEP_PIN, SLEEP_PIN, MICROSTEP_PIN_0, MICROSTEP_PIN_1);

// ─────────────────────────────────────────────────────────────────────────────
// State Tracking for Debounce
// ─────────────────────────────────────────────────────────────────────────────
bool          lastRawState    = HIGH;
bool          stableButton    = HIGH;
unsigned long lastChangeMs    = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize stepper (RPM) and disable until button press
  stepper.begin(SMEAR_RPM);
  stepper.disable();
}

// ─────────────────────────────────────────────────────────────────────────────
// Debounce‐aware button reader: returns true only on a new press
// ─────────────────────────────────────────────────────────────────────────────
bool readButtonPressed() {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != lastRawState) {
    // Input changed—reset timer
    lastChangeMs = millis();
  }

  // If the input has been stable for longer than DEBOUNCE_MS…
  if (millis() - lastChangeMs >= DEBOUNCE_MS) {
    // …and the stable state has changed
    if (raw != stableButton) {
      stableButton = raw;

      // Only fire on the falling edge (button press)
      if (stableButton == LOW) {
        lastRawState = raw;
        return true;
      }
    }
  }

  lastRawState = raw;
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Your stepper motion sequence
// ─────────────────────────────────────────────────────────────────────────────
void runStepperSequence() {
  stepper.enable();
  stepper.setMicrostep(16);

  Serial.print("Ramp Down");
  delay(100);
  stepper.setRPM(RAMP_RPM);
  stepper.rotate(200);
  
  Serial.println("Moving forward...");
  stepper.setRPM(SMEAR_RPM);
  delay(100);
  stepper.rotate(520);     

  Serial.println("Quick back‐n‐forth...");
  delay(20);
  stepper.rotate(-50);      

  Serial.println("Smearing and Moving Back");
  delay(SMEARING_DELAY);
  stepper.rotate(-470);

  Serial.println("Ramp Up");
  delay(100);
  stepper.setRPM(RAMP_RPM);
  stepper.rotate(-200);
  
  Serial.println("Sequence complete.");
  delay(100);
  stepper.disable();
}

// ─────────────────────────────────────────────────────────────────────────────
// Main Loop
// ─────────────────────────────────────────────────────────────────────────────
void loop() {
  if (readButtonPressed()) {
    Serial.println("Button pressed!");
    runStepperSequence();
  }

  // ...do any other non‑blocking tasks here...
}
