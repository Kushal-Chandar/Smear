#include <Arduino.h>
#include "DRV8834.h"

// ─────────────────────────────────────────────────────────────────────────────
// Pin & Motor Configuration
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint8_t OPT_SW_PIN       = 2;
constexpr uint8_t BUTTON_PIN       = 3;    // Input with pullup
constexpr uint8_t DIR_PIN          = 4;
constexpr uint8_t STEP_PIN         = 6;
constexpr uint8_t SLEEP_PIN        = 12;
constexpr uint8_t MICROSTEP_PIN_0  = 9;    // DRV8834 M0
constexpr uint8_t MICROSTEP_PIN_1  = 10;   // DRV8834 M1

constexpr uint8_t DEBOUNCE_MS      = 50;   // Debounce interval

constexpr int MOTOR_STEPS          = 200;  // Full‑step count per revolution

constexpr int RAMP_RPM             = 30;   
constexpr int SMEAR_RPM            = 150 + (.17 * 150); // Error adjusted speed 
constexpr int BACK_RPM             = 85;
constexpr int JERK_RPM             = 100;
constexpr int SMEARING_DELAY       = 3000; // Smearing Delay


// Instantiate your DRV8834 driver
DRV8834 stepper(MOTOR_STEPS, DIR_PIN, STEP_PIN, SLEEP_PIN, MICROSTEP_PIN_0, MICROSTEP_PIN_1);

// ─────────────────────────────────────────────────────────────────────────────
// State Tracking for Debounce
// ─────────────────────────────────────────────────────────────────────────────
bool          lastRawState    = HIGH;
bool          stableButton    = HIGH;
unsigned long lastChangeMs    = 0;

// Declare stopMotor as volatile to allow modification in interrupt handler
volatile bool stopMotor = false;

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────

void handleOpticalSwitchInterrupt() {
  stopMotor = true;  // Stop the motor if the interrupt is triggered (e.g., limit switch or optical switch)
}

void setup() {
  Serial.begin(9600);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(OPT_SW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(OPT_SW_PIN), handleOpticalSwitchInterrupt, CHANGE);

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

  if (digitalRead(OPT_SW_PIN) == HIGH) {  // Assuming LOW means triggered
    Serial.println("Optical switch is triggered on startup.");
    // Move the motor back by 570 rotations
    Serial.println("Moving motor back by 570 rotations...");
    stepper.setRPM(SMEAR_RPM);  // Set desired RPM for moving back
    stepper.rotate(-570);  // Move backwards by 570 rotations (negative direction)
    Serial.println("Motor moved back by 570 rotations.");
  }
  else{
    Serial.println("Ramp Down");
    delay(100);
    stepper.setRPM(RAMP_RPM);
    stepper.rotate(200);

    Serial.println("Moving forward...");
    stepper.setRPM(BACK_RPM);
    delay(100);
    // stepper.rotate(520);

    Serial.println(stopMotor);

    int steps = 1000;  // Define how many steps to move forward
    // Start the forward motion, but check if stopMotor flag is set
    for (int i = 0; i < steps; ++i) {
      if (stopMotor) {
        Serial.println(i);
        Serial.println("Interrupt detected. Stopping motor.");
        stepper.stop();
        break; //Stop moving and exit the loop
      }
      stepper.rotate(1);  // Move 1 step at a time to allow checking stopMotor flag
    }

    Serial.println("Quick back‐n‐forth...");
    delay(20);
    stepper.setRPM(JERK_RPM);
    stepper.rotate(-30);      

    stepper.setRPM(SMEAR_RPM);
    Serial.println("Smearing and Moving Back");
    delay(SMEARING_DELAY);
    stepper.rotate(-520);
  }

  Serial.println("Ramp Up");
  delay(100);
  stepper.setRPM(RAMP_RPM);
  stepper.rotate(-170);
  
  Serial.println("Sequence complete.");
  delay(100);
  stepper.disable();

  stopMotor = false;
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
