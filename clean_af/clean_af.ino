#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Encoder.h>
#include <Arduino.h>
#include "DRV8834.h"

// ─────────────────────────────────────────────────────────────────────────────
// LCD & Encoder Setup
// ─────────────────────────────────────────────────────────────────────────────
LiquidCrystal_I2C lcd(0x27, 16, 2);
Encoder myEnc(7, 8);

// Encoder maps to mm/s
const int minMM = 1;
const int maxMM = 150;

long oldPosition = -999;
float smearMMPerSec = 100.0;    // Initial mm/s
float smearRPM = 175.0;         // Initial RPM = 100 * 1.75

unsigned long lastEncoderUpdate = 0;
const unsigned long ENCODER_UPDATE_INTERVAL = 100; // ms debounce

// ─────────────────────────────────────────────────────────────────────────────
// Motor & Pin Configuration
// ─────────────────────────────────────────────────────────────────────────────
constexpr uint8_t OPT_SW_PIN       = 2;
constexpr uint8_t BUTTON_PIN       = 3;
constexpr uint8_t DIR_PIN          = 4;
constexpr uint8_t STEP_PIN         = 6;
constexpr uint8_t SLEEP_PIN        = 12;
constexpr uint8_t MICROSTEP_PIN_0  = 9;
constexpr uint8_t MICROSTEP_PIN_1  = 10;

constexpr uint8_t DEBOUNCE_MS      = 50;
constexpr int MOTOR_STEPS          = 200;

constexpr int RAMP_RPM             = 30;
constexpr int BACK_RPM             = 85;
constexpr int JERK_RPM             = 100;
constexpr int SMEARING_DELAY       = 3000;

DRV8834 stepper(MOTOR_STEPS, DIR_PIN, STEP_PIN, SLEEP_PIN, MICROSTEP_PIN_0, MICROSTEP_PIN_1);

// ─────────────────────────────────────────────────────────────────────────────
// Debounce & Interrupts
// ─────────────────────────────────────────────────────────────────────────────
bool          lastRawState    = HIGH;
bool          stableButton    = HIGH;
unsigned long lastChangeMs    = 0;

volatile bool stopMotor = false;

void handleOpticalSwitchInterrupt() {
  stopMotor = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);

  // LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smear RPM Ctrl");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);
  lcd.clear();

  // Encoder setup
  myEnc.write((int)smearMMPerSec);  // Write initial float as int

  // Pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(OPT_SW_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(OPT_SW_PIN), handleOpticalSwitchInterrupt, CHANGE);

  // Stepper setup
  stepper.begin(smearRPM);
  stepper.disable();
}

// ─────────────────────────────────────────────────────────────────────────────
// Encoder Reading and LCD Display
// ─────────────────────────────────────────────────────────────────────────────
void updateEncoderAndDisplay() {
  long newPosition = myEnc.read();
  unsigned long now = millis();

  if (now - lastEncoderUpdate < ENCODER_UPDATE_INTERVAL) return;

  // Clamp encoder value
  if (newPosition < minMM) {
    newPosition = minMM;
    myEnc.write(minMM);
  }
  if (newPosition > maxMM) {
    newPosition = maxMM;
    myEnc.write(maxMM);
  }

  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    lastEncoderUpdate = now;

    smearMMPerSec = (float)newPosition;
    smearRPM = smearMMPerSec * 1.75f;

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Speed:");
    lcd.print(smearMMPerSec, 1);  // 1 decimal place
    lcd.print(" mm/s");

    // lcd.setCursor(0, 1);
    // lcd.print("RPM:");
    // lcd.print(smearRPM, 1);       // 1 decimal place
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Debounce-aware Button Press
// ─────────────────────────────────────────────────────────────────────────────
bool readButtonPressed() {
  bool raw = digitalRead(BUTTON_PIN);

  if (raw != lastRawState) {
    lastChangeMs = millis();
  }

  if (millis() - lastChangeMs >= DEBOUNCE_MS) {
    if (raw != stableButton) {
      stableButton = raw;
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
// Stepper Motion Sequence
// ─────────────────────────────────────────────────────────────────────────────
void runStepperSequence() {
  stepper.enable();
  stepper.setMicrostep(16);

  if (digitalRead(OPT_SW_PIN) == HIGH) {
    Serial.println("Optical switch triggered on startup.");
    stepper.setRPM(smearRPM);
    stepper.rotate(-570);
  } else {
    Serial.println("Ramp Down");
    delay(100);
    stepper.setRPM(RAMP_RPM);
    stepper.rotate(200);

    Serial.println("Moving forward...");
    stepper.setRPM(BACK_RPM);
    delay(100);

    int steps = 1000;
    for (int i = 0; i < steps; ++i) {
      if (stopMotor) {
        Serial.println("Interrupt detected. Stopping motor.");
        stepper.stop();
        break;
      }
      stepper.rotate(1);
    }

    Serial.println("Quick back-n-forth...");
    delay(20);
    stepper.setRPM(JERK_RPM);
    stepper.rotate(-30);

    stepper.setRPM(smearRPM);
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
  updateEncoderAndDisplay();

  if (readButtonPressed()) {
    Serial.println("Button pressed!");
    runStepperSequence();
  }

  // Other non-blocking tasks can go here
}
