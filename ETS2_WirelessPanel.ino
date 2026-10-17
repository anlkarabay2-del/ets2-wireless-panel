#include <Arduino.h>
#include <BleGamepad.h>

// Diagnostic build: 11 regular buttons + 2 paddles.
// GPIO 6-16 = buttons 1-11, GPIO 5 = button 12, GPIO 21 = button 13.
static const uint8_t BUTTON_COUNT = 13;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 5, 21
};

BleGamepad bleGamepad("ETS2 Custom Wheel v3", "DIY", 100, false);

static bool rawState[BUTTON_COUNT] = {false};
static bool buttonState[BUTTON_COUNT] = {false};
static uint32_t changedAt[BUTTON_COUNT] = {0};
static const uint32_t DEBOUNCE_MS = 15;

// On each new BLE connection, pulse virtual buttons 1-13 one at a time.
// This checks the HID report path in joy.cpl independently of physical wiring.
static bool diagnosticDone = false;
static uint8_t diagnosticButton = 0;
static uint32_t diagnosticAt = 0;
static bool diagnosticPressed = false;
static bool lastConnected = false;
static const uint32_t DIAGNOSTIC_STEP_MS = 350;

void setup() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    rawState[i] = (digitalRead(BUTTON_PINS[i]) == LOW);
    buttonState[i] = rawState[i];
    changedAt[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setAutoReport(false);
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
  const bool connected = bleGamepad.isConnected();

  if (!connected) {
    lastConnected = false;
    delay(1);
    return;
  }

  if (!lastConnected) {
    lastConnected = true;
    diagnosticDone = false;
    diagnosticButton = 0;
    diagnosticPressed = false;
    diagnosticAt = now;
    bleGamepad.resetButtons();
    bleGamepad.sendReport();
  }

  if (!diagnosticDone) {
    if ((uint32_t)(now - diagnosticAt) >= DIAGNOSTIC_STEP_MS) {
      diagnosticAt = now;

      if (diagnosticPressed) {
        bleGamepad.release(diagnosticButton + 1);
        bleGamepad.sendReport();
        diagnosticPressed = false;
        ++diagnosticButton;

        if (diagnosticButton >= BUTTON_COUNT) {
          diagnosticDone = true;
        }
      } else {
        bleGamepad.press(diagnosticButton + 1);
        bleGamepad.sendReport();
        diagnosticPressed = true;
      }
    }
    delay(1);
    return;
  }

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool reading = (digitalRead(BUTTON_PINS[i]) == LOW);

    if (reading != rawState[i]) {
      rawState[i] = reading;
      changedAt[i] = now;
    }

    if ((uint32_t)(now - changedAt[i]) >= DEBOUNCE_MS &&
        buttonState[i] != rawState[i]) {
      buttonState[i] = rawState[i];

      if (buttonState[i]) {
        bleGamepad.press(i + 1);
      } else {
        bleGamepad.release(i + 1);
      }
      bleGamepad.sendReport();
    }
  }

  delay(1);
}
