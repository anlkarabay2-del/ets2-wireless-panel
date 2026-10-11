#include <Arduino.h>
#include <BleGamepad.h>
#include <NuSerial.hpp>
#include <NimBLEDevice.h>

// Based on the user's existing ESP32-BLE-Gamepad sketch.
// BLE HID = buttons/joysticks. BLE NuS = incoming RPM text from the PC bridge.

static const uint8_t BUTTON_COUNT = 14;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21
};
static const uint8_t JOYSTICK_PINS[4] = {1, 2, 3, 4};
static const uint8_t LED_COUNT = 9;
static const uint8_t LED_PINS[LED_COUNT] = {5, 38, 39, 40, 41, 42, 47, 48, 33};

// Fourth argument delays advertising so NuS can be registered on the same server.
BleGamepad bleGamepad("ETS2 Custom Wheel", "DIY", 100, true);

static bool stableButton[BUTTON_COUNT] = {false};
static bool lastRawButton[BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[BUTTON_COUNT] = {0};
static int joystickValues[4] = {2048, 2048, 2048, 2048};
static char nusBuffer[32];
static size_t nusLength = 0;
static long currentRpm = 0;
static uint32_t lastInputRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t REPORT_MS = 10;
static const char *NUS_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";

static int16_t axisFromAdc(int raw) {
  raw = constrain(raw, 0, 4095);
  // Small center deadzone to reduce joystick drift.
  int32_t value = map(raw, 0, 4095, -32767, 32767);
  if (abs(value) < 1800) value = 0;
  return (int16_t)value;
}

void updateRpmLeds(long rpm) {
  rpm = constrain(rpm, 0L, 3500L);
  int active = (int)((rpm * LED_COUNT + 3499) / 3500); // turn on progressively
  active = constrain(active, 0, LED_COUNT);
  for (uint8_t i = 0; i < LED_COUNT; i++) {
    digitalWrite(LED_PINS[i], i < active ? HIGH : LOW);
  }
}

void readInputs() {
  uint32_t now = millis();
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

  // Average a few reads to soften ADC noise without adding long blocking delays.
  for (uint8_t i = 0; i < 4; i++) {
    uint32_t sum = 0;
    for (uint8_t n = 0; n < 4; n++) sum += analogRead(JOYSTICK_PINS[i]);
    joystickValues[i] = (int)(sum / 4);
  }
}

void handleRpmLine(char *line) {
  // Accept either "1234" or "RPM:1234" from the PC bridge.
  char *value = line;
  if (strncmp(value, "RPM:", 4) == 0 || strncmp(value, "rpm:", 4) == 0) value += 4;
  while (*value == ' ' || *value == '\t') value++;
  char *endPtr = nullptr;
  long rpm = strtol(value, &endPtr, 10);
  while (endPtr && (*endPtr == ' ' || *endPtr == '\t')) endPtr++;
  if (endPtr != value && endPtr && *endPtr == '\0' && rpm >= 0 && rpm <= 30000) {
    currentRpm = rpm;
    updateRpmLeds(currentRpm);
    Serial.printf("RPM_OK:%ld\n", currentRpm);
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
      nusBuffer[nusLength] = '\0';
      if (nusLength > 0) handleRpmLine(nusBuffer);
      nusLength = 0;
    } else if (c >= 32 && c <= 126) {
      if (nusLength < sizeof(nusBuffer) - 1) {
        nusBuffer[nusLength++] = c;
      } else {
        nusLength = 0; // discard overlong line
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
  config.setAxes(true, true, true, true, false, false, false, false);
  bleGamepad.begin(&config);

  // Official ESP32-BLE-Gamepad NuSCompatibility.md initialization order.
  while (NimBLEDevice::getServer() == nullptr) delay(10);
  NuSerial.start(false);
  NimBLEAdvertising *advertising = NimBLEDevice::getServer()->getAdvertising();
  advertising->enableScanResponse(true);
  if (!advertising->addServiceUUID(NUS_UUID)) {
    Serial.println("WARN: NuS UUID did not fit scan response; connect then discover services.");
  }
  // Short scan-response alias leaves room for the 128-bit NuS service UUID.
  if (!advertising->setName("ETS2-NuS")) {
    Serial.println("WARN: short NuS scan name did not fit scan response.");
  }
  advertising->start();

  Serial.println("ETS2_PANEL_READY");
  Serial.println("BLE HID: buttons + 4 axes; BLE NuS: send RPM number followed by newline.");
}

void loop() {
  uint32_t now = millis();
  if ((uint32_t)(now - lastInputRead) >= 2) {
    lastInputRead = now;
    readInputs();
  }

  pollNuS();

  if (bleGamepad.isConnected() && (uint32_t)(now - lastReport) >= REPORT_MS) {
    lastReport = now;
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
      if (stableButton[i]) bleGamepad.press(i + 1);
      else bleGamepad.release(i + 1);
    }
    bleGamepad.setAxes(
      axisFromAdc(joystickValues[0]), axisFromAdc(joystickValues[1]),
      axisFromAdc(joystickValues[2]), axisFromAdc(joystickValues[3]),
      0, 0, 0, 0
    );
    bleGamepad.sendReport();
  }
  delay(1);
}
