#include <Arduino.h>
#include <BleGamepad.h>

// BLE HID gamepad: 15 physical buttons and 2 virtual cruise-control buttons; no joystick axes.

static const uint8_t PHYSICAL_BUTTON_COUNT = 15;
static const uint8_t GAMEPAD_BUTTON_COUNT = 17;
// Both joysticks removed. B10K cruise-control potentiometer uses GPIO 3.
static const uint8_t BUTTON_PINS[PHYSICAL_BUTTON_COUNT] = {
  38, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 47, 21
};
static const uint8_t CRUISE_POT_PIN = 3;
static const uint8_t CRUISE_UP_BUTTON = 16;
static const uint8_t CRUISE_DOWN_BUTTON = 17;

// v2 changes the BLE identity so Windows rebuilds the cached HID axis descriptor.
BleGamepad bleGamepad("ETS2 Custom Wheel v3", "DIY", 100, false);

static bool stableButton[PHYSICAL_BUTTON_COUNT] = {false};
static bool lastRawButton[PHYSICAL_BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[PHYSICAL_BUTTON_COUNT] = {0};
static bool cruiseUpPulse = false;
static bool cruiseDownPulse = false;
static uint32_t cruisePulseStartedAt = 0;
static uint32_t cruiseLastStepAt = 0;
static int cruisePotFiltered = 0;
static int cruisePotStepReference = 0;
static const int CRUISE_POT_STEP_COUNTS = 90;
static const uint32_t CRUISE_POT_REPEAT_MS = 120;
static const uint32_t CRUISE_BUTTON_PULSE_MS = 70;

// Calibrated at startup: leave both sticks untouched and centered while powering on.
// This avoids a fixed 2048 center masking movement on an offset ADC signal.
// Optional endpoint calibration; defaults use the full 12-bit ADC range.

// Set true to print the cruise-pot ADC reading every 250 ms.
static const bool DEBUG_CRUISE_POT_ADC = true;
static const uint32_t CRUISE_POT_DEBUG_MS = 250;
static uint32_t lastCruisePotDebug = 0;

static uint32_t lastInputRead = 0;
static uint32_t lastAnalogRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t INPUT_SCAN_MS = 2;
static const uint32_t ANALOG_SCAN_MS = 5;
static const uint32_t REPORT_MS = 10;

static void readButtons(uint32_t now) {
  for (uint8_t i = 0; i < PHYSICAL_BUTTON_COUNT; i++) {
    const bool raw = (digitalRead(BUTTON_PINS[i]) == LOW);
    if (raw != lastRawButton[i]) {
      lastRawButton[i] = raw;
      rawChangedAt[i] = now;
    }
    if ((uint32_t)(now - rawChangedAt[i]) >= DEBOUNCE_MS) {
      stableButton[i] = raw;
    }
  }
}

// ESP32-S3 ADC multiplexing can produce a stale first conversion after
// switching pins. Discard it, then use the median to reject isolated spikes.
static int readAnalogMedian(uint8_t pin) {
  (void)analogRead(pin);
  int samples[9];
  for (uint8_t i = 0; i < 9; i++) samples[i] = analogRead(pin);

  for (uint8_t i = 1; i < 9; i++) {
    const int value = samples[i];
    int j = i - 1;
    while (j >= 0 && samples[j] > value) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = value;
  }
  return samples[4];
}

static void initializeCruisePot() {
  cruisePotFiltered = readAnalogMedian(CRUISE_POT_PIN);
  cruisePotStepReference = cruisePotFiltered;
  Serial.printf("POT_START:%d\n", cruisePotFiltered);
}

static void updateCruisePot(uint32_t now) {
  const int raw = readAnalogMedian(CRUISE_POT_PIN);
  cruisePotFiltered += (raw - cruisePotFiltered) / 4;
  const int delta = cruisePotFiltered - cruisePotStepReference;

  if (abs(delta) >= CRUISE_POT_STEP_COUNTS &&
      (uint32_t)(now - cruiseLastStepAt) >= CRUISE_POT_REPEAT_MS) {
    cruiseUpPulse = (delta > 0);
    cruiseDownPulse = (delta < 0);
    cruisePulseStartedAt = now;
    cruiseLastStepAt = now;
    cruisePotStepReference = cruisePotFiltered;
  }

  if ((uint32_t)(now - cruisePulseStartedAt) >= CRUISE_BUTTON_PULSE_MS) {
    cruiseUpPulse = false;
    cruiseDownPulse = false;
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  pinMode(CRUISE_POT_PIN, INPUT);
  initializeCruisePot();

  for (uint8_t i = 0; i < PHYSICAL_BUTTON_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    const bool initial = (digitalRead(BUTTON_PINS[i]) == LOW);
    stableButton[i] = initial;
    lastRawButton[i] = initial;
    rawChangedAt[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setButtonCount(GAMEPAD_BUTTON_COUNT);
  // All joystick axes are disabled in the BLE HID descriptor.
  config.setIncludeXAxis(false);
  config.setIncludeYAxis(false);
  config.setIncludeZAxis(false);
  config.setIncludeRxAxis(false);
  config.setIncludeRyAxis(false);
  config.setIncludeRzAxis(false);
  config.setIncludeSlider1(false);
  config.setIncludeSlider2(false);
  bleGamepad.begin(&config);

  Serial.println("ETS2_PANEL_READY");
  Serial.println("BLE HID: 15 physical buttons + 2 cruise-pot virtual buttons; joystick axes disabled");
}

void loop() {
  const uint32_t now = millis();

  if ((uint32_t)(now - lastInputRead) >= INPUT_SCAN_MS) {
    lastInputRead = now;
    readButtons(now);
  }

  if ((uint32_t)(now - lastAnalogRead) >= ANALOG_SCAN_MS) {
    lastAnalogRead = now;
    updateCruisePot(now);
  }

  if (DEBUG_CRUISE_POT_ADC && (uint32_t)(now - lastCruisePotDebug) >= CRUISE_POT_DEBUG_MS) {
    lastCruisePotDebug = now;
    Serial.printf("POT_ADC:%d\n", cruisePotFiltered);
  }

  if (bleGamepad.isConnected() && (uint32_t)(now - lastReport) >= REPORT_MS) {
    lastReport = now;

    for (uint8_t i = 0; i < PHYSICAL_BUTTON_COUNT; i++) {
      if (stableButton[i]) bleGamepad.press(i + 1);
      else bleGamepad.release(i + 1);
    }
    if (cruiseUpPulse) bleGamepad.press(CRUISE_UP_BUTTON);
    else bleGamepad.release(CRUISE_UP_BUTTON);
    if (cruiseDownPulse) bleGamepad.press(CRUISE_DOWN_BUTTON);
    else bleGamepad.release(CRUISE_DOWN_BUTTON);

    bleGamepad.sendReport();
  }

  delay(1);
}
