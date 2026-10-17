#include <Arduino.h>
#include <BleGamepad.h>

// BLE HID gamepad: 15 digital buttons and 4 analog axes.

static const uint8_t BUTTON_COUNT = 15;
// Regular buttons 1-11: GPIO 38, then GPIO 7-16. GPIO 6 is now an analog axis.
// Joystick push-buttons: GPIO 17 and 18.
// Paddle shifters: GPIO 5 and 21.
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  38, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 5, 21
};
static const uint8_t JOYSTICK_PINS[4] = {2, 1, 6, 4};

// v2 changes the BLE identity so Windows rebuilds the cached HID axis descriptor.
BleGamepad bleGamepad("ETS2 Custom Wheel v2", "DIY", 100, false);

static bool stableButton[BUTTON_COUNT] = {false};
static bool lastRawButton[BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[BUTTON_COUNT] = {0};
static int joystickValues[4] = {2048, 2048, 2048, 2048};

// Calibrated at startup: leave both sticks untouched and centered while powering on.
// This avoids a fixed 2048 center masking movement on an offset ADC signal.
static int JOYSTICK_CENTER_ADC[4] = {2048, 2048, 2048, 2048};
// Optional endpoint calibration; defaults use the full 12-bit ADC range.
static const int JOYSTICK_MIN_ADC[4] = {0, 0, 0, 0};
static const int JOYSTICK_MAX_ADC[4] = {4095, 4095, 4095, 4095};
static const bool JOYSTICK_INVERTED[4] = {false, true, false, false};

// Set true temporarily to print the four raw ADC readings every 250 ms.
static const bool DEBUG_JOYSTICK_ADC = true;
static const uint32_t JOYSTICK_DEBUG_MS = 250;
static uint32_t lastJoystickDebug = 0;

// Axes 0-1 are the menu stick; axes 2-3 are the camera stick.
// Small deadzone preserves short travel; release threshold adds minimal hysteresis.
static const int JOYSTICK_ACTIVATE_COUNTS[4] = {12, 12, 12, 12};
static const int JOYSTICK_RELEASE_COUNTS[4] = {6, 6, 6, 6};
static bool joystickAxisActive[4] = {false, false, false, false};

static uint32_t lastInputRead = 0;
static uint32_t lastAnalogRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t INPUT_SCAN_MS = 2;
static const uint32_t ANALOG_SCAN_MS = 5;
static const uint32_t REPORT_MS = 10;

static int16_t axisFromAdc(int raw, uint8_t axisIndex) {
  if (axisIndex >= 4) return 0;

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
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
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

static void calibrateJoystickCenters() {
  // Keep both sticks centered during startup. Average 32 readings per axis.
  uint32_t sums[4] = {0, 0, 0, 0};
  for (uint8_t sample = 0; sample < 32; sample++) {
    for (uint8_t i = 0; i < 4; i++) sums[i] += analogRead(JOYSTICK_PINS[i]);
    delay(5);
  }
  for (uint8_t i = 0; i < 4; i++) {
    JOYSTICK_CENTER_ADC[i] = (int)(sums[i] / 32);
  }
  Serial.printf("JOY_CENTER:%d,%d,%d,%d\n",
                JOYSTICK_CENTER_ADC[0], JOYSTICK_CENTER_ADC[1],
                JOYSTICK_CENTER_ADC[2], JOYSTICK_CENTER_ADC[3]);
}

static void readJoysticks() {
  // Four-sample averaging reduces ADC noise without slowing button scans.
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t sum = 0;
    for (uint8_t n = 0; n < 4; n++) {
      sum += analogRead(JOYSTICK_PINS[i]);
    }
    joystickValues[i] = (int)(sum / 4);
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  calibrateJoystickCenters();

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    const bool initial = (digitalRead(BUTTON_PINS[i]) == LOW);
    stableButton[i] = initial;
    lastRawButton[i] = initial;
    rawChangedAt[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setButtonCount(BUTTON_COUNT);
  // Explicit HID axis selection: X/Y = menu stick; Z/RZ = camera stick.
  // RX, RY and sliders are intentionally disabled.
  // Individual setters avoid relying on setWhichAxes() argument ordering.
  config.setIncludeXAxis(true);
  config.setIncludeYAxis(true);
  config.setIncludeZAxis(true);
  config.setIncludeRxAxis(false);
  config.setIncludeRyAxis(false);
  config.setIncludeRzAxis(true);
  config.setIncludeSlider1(false);
  config.setIncludeSlider2(false);
  Serial.printf("HID_AXES: X=%d Y=%d Z=%d RZ=%d\n",
                config.getIncludeXAxis(), config.getIncludeYAxis(),
                config.getIncludeZAxis(), config.getIncludeRzAxis());
  bleGamepad.begin(&config);

  Serial.println("ETS2_PANEL_READY");
  Serial.println("BLE HID: 15 buttons (11 + 2 stick clicks + 2 paddles) + 4 analog axes");
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
  }

  if (DEBUG_JOYSTICK_ADC && (uint32_t)(now - lastJoystickDebug) >= JOYSTICK_DEBUG_MS) {
    lastJoystickDebug = now;
    Serial.printf("JOY_ADC:%d,%d,%d,%d\n",
                  joystickValues[0], joystickValues[1],
                  joystickValues[2], joystickValues[3]);
  }

  if (bleGamepad.isConnected() && (uint32_t)(now - lastReport) >= REPORT_MS) {
    lastReport = now;

    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (stableButton[i]) bleGamepad.press(i + 1);
      else bleGamepad.release(i + 1);
    }

    bleGamepad.setAxes(
      axisFromAdc(joystickValues[0], 0),
      axisFromAdc(joystickValues[1], 1),
      axisFromAdc(joystickValues[2], 2),
      0, 0,
      axisFromAdc(joystickValues[3], 3),
      0, 0
    );
    bleGamepad.sendReport();
  }

  delay(1);
}
