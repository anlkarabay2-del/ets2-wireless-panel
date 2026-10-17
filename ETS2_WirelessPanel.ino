#include <Arduino.h>
#include <BleGamepad.h>

// ETS2 wireless panel: 11 buttons + 2 paddle shifters.
static const uint8_t BUTTON_COUNT = 13;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 5, 21
};

BleGamepad bleGamepad("ETS2 Custom Wheel v3", "DIY", 100, false);

static bool rawState[BUTTON_COUNT] = {false};
static bool buttonState[BUTTON_COUNT] = {false};
static uint32_t changedAt[BUTTON_COUNT] = {0};
static const uint32_t DEBOUNCE_MS = 15;

void setup() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    rawState[i] = (digitalRead(BUTTON_PINS[i]) == LOW);
    buttonState[i] = rawState[i];
    changedAt[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setAutoReport(true);
  config.setButtonCount(BUTTON_COUNT);
  config.setIncludeXAxis(false);
  config.setIncludeYAxis(false);
  config.setIncludeZAxis(false);
  config.setIncludeRxAxis(false);
  config.setIncludeRyAxis(false);
  config.setIncludeRzAxis(false);
  config.setIncludeSlider1(false);
  config.setIncludeSlider2(false);

  bleGamepad.begin(&config);
}

void loop() {
  const uint32_t now = millis();

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool reading = (digitalRead(BUTTON_PINS[i]) == LOW);

    if (reading != rawState[i]) {
      rawState[i] = reading;
      changedAt[i] = now;
    }

    if ((uint32_t)(now - changedAt[i]) >= DEBOUNCE_MS &&
        buttonState[i] != rawState[i]) {
      buttonState[i] = rawState[i];

      // BleGamepad button numbers are 1-based. Auto-report sends each change.
      if (buttonState[i]) {
        bleGamepad.press(i + 1);
      } else {
        bleGamepad.release(i + 1);
      }
    }
  }

  delay(1);
}
