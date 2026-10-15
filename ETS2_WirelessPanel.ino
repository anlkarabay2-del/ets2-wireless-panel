#include <Arduino.h>
#include <BleGamepad.h>
#include <NuSerial.hpp>
#include <NimBLEDevice.h>

// BLE HID = buttons/joysticks. BLE NuS = incoming RPM text from the PC bridge.

static const uint8_t BUTTON_COUNT = 14;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21
};
static const uint8_t JOYSTICK_PINS[4] = {1, 2, 3, 4};
static const uint8_t LED_COUNT = 9;
static const long LED_BAR_MAX = 3500L;
static const uint8_t LED_PINS[LED_COUNT] = {5, 38, 39, 40, 41, 42, 47, 48, 33};

BleGamepad bleGamepad("ETS2 Custom Wheel", "DIY", 100, true);

static bool stableButton[BUTTON_COUNT] = {false};
static bool lastRawButton[BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[BUTTON_COUNT] = {0};
static int joystickValues[4] = {2048, 2048, 2048, 2048};

// ADC centers are deliberately explicit rather than auto-calibrated at boot:
// a stick held off-center during startup must not become the new neutral.
// Enable DEBUG_JOYSTICK_ADC to measure resting values in the Serial Monitor.
static const int JOYSTICK_CENTER_ADC[4] = {2048, 2048, 2048, 2048};
// Optional measured endpoint calibration; defaults retain the full 12-bit ADC range.
// Record the resting center and both physical endpoints for each axis if needed.
static const int JOYSTICK_MIN_ADC[4] = {0, 0, 0, 0};
static const int JOYSTICK_MAX_ADC[4] = {4095, 4095, 4095, 4095};
// Set an entry true if that physical axis moves in the wrong direction in Windows.
static const bool JOYSTICK_INVERTED[4] = {false, false, false, false};
// Temporarily set true to print averaged ADC readings every 250 ms.
static const bool DEBUG_JOYSTICK_ADC = false;
static const uint32_t JOYSTICK_DEBUG_MS = 250;
static uint32_t lastJoystickDebug = 0;

// Hysteresis thresholds in raw ADC counts. Axes 0-1 are the menu stick;
// axes 2-3 are the camera stick, given a larger noise-rejection band.
static const int JOYSTICK_ACTIVATE_COUNTS[4] = {100, 100, 170, 170};
static const int JOYSTICK_RELEASE_COUNTS[4] = {70, 70, 130, 130};
static bool joystickAxisActive[4] = {false, false, false, false};
static char nusBuffer[32];
static size_t nusLength = 0;
static bool discardingNusLine = false;
static long currentLedRpm = 0;
static uint32_t lastValidRpmAt = 0;
static uint32_t lastInputRead = 0;
static uint32_t lastAnalogRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t INPUT_SCAN_MS = 2;
static const uint32_t ANALOG_SCAN_MS = 5;
static const uint32_t REPORT_MS = 10;
static const uint32_t RPM_TIMEOUT_MS = 1000;
static const char *NUS_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";

static int16_t axisFromAdc(int raw, uint8_t axisIndex) {
  if (axisIndex >= 4) return 0;

  const int axisMin = constrain(JOYSTICK_MIN_ADC[axisIndex], 0, 4094);
  const int axisMax = constrain(JOYSTICK_MAX_ADC[axisIndex], axisMin + 1, 4095);
  const int center = constrain(JOYSTICK_CENTER_ADC[axisIndex], axisMin + 1, axisMax - 1);
  raw = constrain(raw, axisMin, axisMax);

  const int delta = raw - center;
  const int distance = abs(delta);
  const int activate = JOYSTICK_ACTIVATE_COUNTS[axisIndex];
  const int release = JOYSTICK_RELEASE_COUNTS[axisIndex];

  // Hysteresis gates activation at a wider threshold and releases at a
  // narrower one. In the band between them, fade the output smoothly toward
  // zero instead of snapping to zero or letting threshold noise flicker.
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

  int32_t magnitude = 0;
  if (distance > activate) {
    magnitude = ((int32_t)(distance - activate) * 32767L) / available;
  } else {
    // Fade through the release band instead of snapping abruptly to zero.
    const int32_t oneCount = max(1L, 32767L / available);
    magnitude = ((int32_t)(distance - release) * oneCount)
                / (activate - release);
  }
  magnitude = constrain(magnitude, 0L, 32767L);
  int32_t signedValue = direction * magnitude;
  if (JOYSTICK_INVERTED[axisIndex]) signedValue = -signedValue;
  return (int16_t)signedValue;
}

void updateRpmLeds(long rpm) {
  rpm = constrain(rpm, 0L, LED_BAR_MAX);
  int active = (int)((rpm * LED_COUNT + LED_BAR_MAX - 1) / LED_BAR_MAX);
  active = constrain(active, 0, LED_COUNT);
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    digitalWrite(LED_PINS[i], i < active ? HIGH : LOW);
  }
}

void readButtons(uint32_t now) {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    bool raw = (digitalRead(BUTTON_PINS[i]) == LOW);
    if (raw != lastRawButton[i]) {
      lastRawButton[i] = raw;
      rawChangedAt[i] = now;
    }
    if ((uint32_t)(now - rawChangedAt[i]) >= DEBOUNCE_MS) {
      stableButton[i] = raw;
    }
  }
}

void readJoysticks() {
  // Four-sample averaging at 200 Hz reduces ADC noise while keeping the
  // 100 Hz BLE report rate smooth. Sampling separately avoids doing 16 ADC
  // conversions during every 2 ms button scan.
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t sum = 0;
    for (uint8_t n = 0; n < 4; n++) sum += analogRead(JOYSTICK_PINS[i]);
    joystickValues[i] = (int)(sum / 4);
  }
}

void handleRpmLine(char *line) {
  char *value = line;
  if (strncmp(value, "RPM:", 4) == 0 || strncmp(value, "rpm:", 4) == 0) value += 4;
  while (*value == ' ' || *value == '\t') value++;
  char *endPtr = nullptr;
  long rpm = strtol(value, &endPtr, 10);
  while (endPtr && (*endPtr == ' ' || *endPtr == '\t')) endPtr++;
  if (endPtr != value && endPtr && *endPtr == '\0' && rpm >= 0 && rpm <= 30000) {
    // The BLE bridge normally sends a 0-3500 display-scale value. Clamp here
    // too so older absolute-RPM payloads remain safe and predictable.
    currentLedRpm = constrain(rpm, 0L, LED_BAR_MAX);
    lastValidRpmAt = millis();
    updateRpmLeds(currentLedRpm);
    Serial.printf("RPM_BAR_OK:%ld\n", currentLedRpm);
  } else {
    Serial.printf("NUS_ERR_BAD_RPM:%s\n", line);
  }
}

void pollNuS() {
  while (NuSerial.available() > 0) {
    int incoming = NuSerial.read();
    if (incoming < 0) break;
    char c = (char)incoming;
    if (c == '\r') continue;
    if (c == '\n') {
      if (!discardingNusLine) {
        nusBuffer[nusLength] = '\0';
        if (nusLength > 0) handleRpmLine(nusBuffer);
      }
      nusLength = 0;
      discardingNusLine = false;
    } else if (c >= 32 && c <= 126 && !discardingNusLine) {
      if (nusLength < sizeof(nusBuffer) - 1) {
        nusBuffer[nusLength++] = c;
      } else {
        // Discard the entire overlong line; never parse its trailing fragment.
        nusLength = 0;
        discardingNusLine = true;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    bool initial = (digitalRead(BUTTON_PINS[i]) == LOW);
    stableButton[i] = initial;
    lastRawButton[i] = initial;
    rawChangedAt[i] = millis();
  }
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    digitalWrite(LED_PINS[i], LOW);
  }
  updateRpmLeds(0);

  BleGamepadConfiguration config;
  config.setAutoReport(false);
  config.setButtonCount(BUTTON_COUNT);
  // Windows maps Z to right-stick X and RZ to right-stick Y.
  // RX is a trigger axis, so it must not be used for the camera stick.
  config.setWhichAxes(true, true, true, false, false, true, false, false);
  bleGamepad.begin(&config);

  // Register NuS after NimBLE is initialized. Preserve the same local/GATT
  // device name supplied to BleGamepad; adding a different scan name here
  // makes Windows rename the device after connecting and can confuse pairing.
  while (NimBLEDevice::getServer() == nullptr) delay(10);
  NuSerial.start(false);
  NimBLEAdvertising *advertising = NimBLEDevice::getServer()->getAdvertising();
  advertising->enableScanResponse(true);
  if (!advertising->addServiceUUID(NUS_UUID)) {
    Serial.println("WARN: NuS UUID did not fit scan response; connect then discover services.");
  }
  advertising->start();

  Serial.println("ETS2_PANEL_READY");
  Serial.println("BLE HID: buttons + X/Y menu axes + Z/RZ camera axes; BLE NuS: send RPM number followed by newline.");
}

void loop() {
  uint32_t now = millis();
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

  pollNuS();

  // Clear the RPM bar if the PC bridge/SimHub stops sending telemetry.
  uint32_t rpmNow = millis();
  if (currentLedRpm != 0 && (uint32_t)(rpmNow - lastValidRpmAt) >= RPM_TIMEOUT_MS) {
    currentLedRpm = 0;
    updateRpmLeds(0);
    Serial.println("RPM_BAR_STALE:0");
  }

  if (bleGamepad.isConnected() && (uint32_t)(now - lastReport) >= REPORT_MS) {
    lastReport = now;
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (stableButton[i]) bleGamepad.press(i + 1);
      else bleGamepad.release(i + 1);
    }
    // HID axis order: X/Y (menu stick), Z/RZ (camera stick).
    // The physical joystick-to-axis wiring still needs to be confirmed on-device.
    bleGamepad.setAxes(
      axisFromAdc(joystickValues[0], 0), axisFromAdc(joystickValues[1], 1),
      axisFromAdc(joystickValues[2], 2), 0, 0, axisFromAdc(joystickValues[3], 3),
      0, 0
    );
    bleGamepad.sendReport();
  }
  delay(1);
}