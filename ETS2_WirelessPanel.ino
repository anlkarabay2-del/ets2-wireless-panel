#include <Arduino.h>
#include <BleGamepad.h>

// BLE HID gamepad: 15 physical buttons; no joystick axes or potentiometer.

static const uint8_t PHYSICAL_BUTTON_COUNT = 15;
static const uint8_t GAMEPAD_BUTTON_COUNT = 15;
static const uint8_t BUTTON_PINS[PHYSICAL_BUTTON_COUNT] = {
  38, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 47, 21
};

BleGamepad bleGamepad("ETS2 Custom Wheel v3", "DIY", 100, false);

static bool stableButton[PHYSICAL_BUTTON_COUNT] = {false};
static bool lastRawButton[PHYSICAL_BUTTON_COUNT] = {false};
static uint32_t rawChangedAt[PHYSICAL_BUTTON_COUNT] = {0};

static uint32_t lastInputRead = 0;
static uint32_t lastReport = 0;
static const uint32_t DEBOUNCE_MS = 8;
static const uint32_t INPUT_SCAN_MS = 2;
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

void setup() {
  Serial.begin(115200);

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
  Serial.println("BLE HID: 15 physical buttons; joystick axes and potentiometer disabled");
}

void loop() {
  const uint32_t now = millis();

  if ((uint32_t)(now - lastInputRead) >= INPUT_SCAN_MS) {
    lastInputRead = now;
    readButtons(now);
  }

  if (bleGamepad.isConnected() && (uint32_t)(now - lastReport) >= REPORT_MS) {
    lastReport = now;

    for (uint8_t i = 0; i < PHYSICAL_BUTTON_COUNT; i++) {
      if (stableButton[i]) bleGamepad.press(i + 1);
      else bleGamepad.release(i + 1);
    }

    bleGamepad.sendReport();
  }

  delay(1);
}
