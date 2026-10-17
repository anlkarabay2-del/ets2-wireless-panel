#include <Arduino.h>
#include <BleGamepad.h>

// BLE HID gamepad: 15 physical buttons, 2 virtual cruise buttons, and 2 joystick axes.

static const uint8_t PHYSICAL_BUTTON_COUNT = 15;
static const uint8_t GAMEPAD_BUTTON_COUNT = 17;
// Working joystick stays on GPIO 2/1; B10K potentiometer replaces the other stick on GPIO 3.
static const uint8_t BUTTON_PINS[PHYSICAL_BUTTON_COUNT] = {
  38, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 47, 21
};
static const uint8_t JOYSTICK_PINS[2] = {2, 1};
static const uint8_t CRUISE_POT_PIN = 3;
static const uint8_t CRUISE_UP_BUTTON = 16;
static const uint8_t CRUISE_DOWN_BUTTON = 17;

// v2 changes the BLE identity so Windows rebuilds the cached HID axis descriptor.
BleGamepad bleGamepad("ETS2 Custom Wheel v3", "DIY", 100, false);

static bool stableButton[PHYSICAL_BUTTON_COUNT] = {false};
static bool lastRawButton[PHYSICAL_BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[PHYSICAL_BUTTON_COUNT] = {0};
static int joystickValues[2] = {2048, 2048};
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
static int JOYSTICK_CENTER_ADC[2] = {2048, 2048};
// Optional endpoint calibration; defaults use the full 12-bit ADC range.
static const int JOYSTICK_MIN_ADC[2] = {0, 0};
static const int JOYSTICK_MAX_ADC[2] = {4095, 4095};
static const bool JOYSTICK_INVERTED[2] = {false, true};

// Set true to print joystick and cruise-pot ADC readings every 250 ms.
static const bool DEBUG_JOYSTICK_ADC = true;
static const uint32_t JOYSTICK_DEBUG_MS = 250;
static uint32_t lastJoystickDebug = 0;

// The remaining joystick controls menu navigation on X/Y.
// Small deadzone preserves short travel; release threshold adds minimal hysteresis.
static const int JOYSTICK_ACTIVATE_COUNTS[2] = {12, 12};
static const int JOYSTICK_RELEASE_COUNTS[2] = {6, 6};
static bool joystickAxisActive[2] = {false, false};

static uint32_t lastInputRead = 0;
static uint32_t lastAnalogRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t INPUT_SCAN_MS = 2;
static const uint32_t ANALOG_SCAN_MS = 5;
static const uint32_t REPORT_MS = 10;

static int16_t axisFromAdc(int raw, uint8_t axisIndex) {
  if (axisIndex >= 2) return 0;

  const int axisMin = constrain(JOYSTICK_MIN_ADC[axisIndex], 0, 4093);
  const int axisMax = constrain(JOYSTICK_MAX_ADC[axisIndex], axisMin + 2, 4095);
  const int center = constrain(JOYSTICK_CENTER_ADC[axisIndex], axisMin + 1, axisMax - 1);
  raw = constrain(raw, axisMin, axisMax);

  const int delta = raw - center;
  const int distance = abs(delta);
  const int activate = JOYSTICK_ACTIVATE_COUNTS[axisIndex];
  const int release = JOYSTICK_RELEASE_COUNTS[axisIndex];

  if (!joystickAxisActive[axisIndex]) {
    if (distance <= activate) return 0;
    joystickAxisActive[axisIndex] = true;
  } else if (distance <= release) {
    joystickAxisActive[axisIndex] = false;
    return 0;
  }

  const int direction = (delta > 0) ? 1 : -1;
  const int available = (direction > 0 ? axisMax - center : center - axisMin) - activate;
  if (available <= 0) {
    joystickAxisActive[axisIndex] = false;
    return 0;
  }

  // Hysteresis controls when an axis activates/releases, but output remains
  // zero inside the activation deadzone. This avoids a full-scale jump at
  // exactly the activation threshold and a sudden drop immediately beyond it.
  int32_t magnitude = 0;
  if (distance > activate) {
    magnitude = ((int32_t)(distance - activate) * 32767L) / available;
  }
  magnitude = constrain(magnitude, 0L, 32767L);

  int32_t signedValue = direction * magnitude;
  if (JOYSTICK_INVERTED[axisIndex]) signedValue = -signedValue;
  return (int16_t)signedValue;
}

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

static void calibrateJoystickCenters() {
  // Keep the working joystick centered during startup.
  uint32_t sums[2] = {0, 0};
  for (uint8_t sample = 0; sample < 16; sample++) {
    for (uint8_t i = 0; i < 2; i++) sums[i] += readAnalogMedian(JOYSTICK_PINS[i]);
    delay(3);
  }
  for (uint8_t i = 0; i < 2; i++) {
    JOYSTICK_CENTER_ADC[i] = (int)(sums[i] / 16);
    joystickValues[i] = JOYSTICK_CENTER_ADC[i];
  }
  cruisePotFiltered = readAnalogMedian(CRUISE_POT_PIN);
  cruisePotStepReference = cruisePotFiltered;
  Serial.printf("JOY_CENTER:%d,%d POT_START:%d\n",
                JOYSTICK_CENTER_ADC[0], JOYSTICK_CENTER_ADC[1], cruisePotFiltered);
}

static void readJoysticks() {
  // Median rejects spikes; a 1/4 IIR low-pass suppresses remaining jitter.
  for (uint8_t i = 0; i < 2; i++) {
    const int raw = readAnalogMedian(JOYSTICK_PINS[i]);
    joystickValues[i] += (raw - joystickValues[i]) / 4;
  }
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
  calibrateJoystickCenters();

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
  // X/Y = menu joystick; all other axes are disabled.
  // Individual setters avoid relying on setWhichAxes() argument ordering.
  config.setIncludeXAxis(true);
  config.setIncludeYAxis(true);
  config.setIncludeZAxis(false);
  config.setIncludeRxAxis(false);
  config.setIncludeRyAxis(false);
  config.setIncludeRzAxis(false);
  config.setIncludeSlider1(false);
  config.setIncludeSlider2(false);
  Serial.printf("HID_AXES: X=%d Y=%d Z=%d RZ=%d\n",
                config.getIncludeXAxis(), config.getIncludeYAxis(),
                config.getIncludeZAxis(), config.getIncludeRzAxis());
  bleGamepad.begin(&config);

  Serial.println("ETS2_PANEL_READY");
  Serial.println("BLE HID: 15 physical buttons + 2 cruise-pot virtual buttons; X/Y joystick axes");
}

void loop() {
  const uint32_t now = millis();

  if ((uint32_t)(now - lastInputRead) >= INPUT_SCAN_MS) {
    lastInputRead = now;
    readButtons(now);
  }

  if ((uint32_t)(now - lastAnalogRead) >= ANALOG_SCAN_MS) {
    lastAnalogRead = now;
    readJoysticks();
    updateCruisePot(now);
  }

  if (DEBUG_JOYSTICK_ADC && (uint32_t)(now - lastJoystickDebug) >= JOYSTICK_DEBUG_MS) {
    lastJoystickDebug = now;
    Serial.printf("JOY_ADC:%d,%d POT_ADC:%d\n",
                  joystickValues[0], joystickValues[1], cruisePotFiltered);
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

    bleGamepad.setAxes(
      axisFromAdc(joystickValues[0], 0),
      axisFromAdc(joystickValues[1], 1),
      0, 0, 0, 0, 0, 0
    );
    bleGamepad.sendReport();
  }

  delay(1);
}
