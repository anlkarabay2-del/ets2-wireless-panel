#include <Arduino.h>
#include <BleGamepad.h>

// ESP32-S3 DevKitC-1: 11 regular buttons + 2 paddle shifters.
// Wire each button between its GPIO pin and GND. INPUT_PULLUP is used.
static constexpr uint8_t BUTTON_COUNT = 13;
static constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 5, 21
};

BleGamepad bleGamepad("ETS2 Wireless Panel", "DIY", 100, false);

static bool stableState[BUTTON_COUNT] = {};
static bool lastReading[BUTTON_COUNT] = {};
static uint32_t lastChangeMs[BUTTON_COUNT] = {};
static bool wasConnected = false;
static constexpr uint32_t DEBOUNCE_MS = 20;

void setup() {
  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    const bool pressed = (digitalRead(BUTTON_PINS[i]) == LOW);
    stableState[i] = pressed;
    lastReading[i] = pressed;
    lastChangeMs[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  config.setAutoReport(true);
  config.setButtonCount(BUTTON_COUNT);
  config.setHatSwitchCount(0);
  config.setWhichAxes(false, false, false, false, false, false, false, false);
  config.setWhichSimulationControls(false, false, false, false, false);
  config.setWhichSpecialButtons(false, false, false, false, false, false, false, false);

  bleGamepad.begin(&config);
}

void loop() {
  const uint32_t now = millis();
  const bool connected = bleGamepad.isConnected();

  if (connected && !wasConnected) {
    // Send the actual state of all physical buttons after connecting.
    bleGamepad.resetButtons();
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
      const bool pressed = (digitalRead(BUTTON_PINS[i]) == LOW);
      stableState[i] = pressed;
      lastReading[i] = pressed;
      lastChangeMs[i] = now;
      if (pressed) {
        bleGamepad.press(i + 1);
      }
    }
  }
  wasConnected = connected;

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool reading = (digitalRead(BUTTON_PINS[i]) == LOW);

    if (reading != lastReading[i]) {
      lastReading[i] = reading;
      lastChangeMs[i] = now;
    }

    if (reading != stableState[i] &&
        (uint32_t)(now - lastChangeMs[i]) >= DEBOUNCE_MS) {
      stableState[i] = reading;

      if (connected) {
        if (reading) {
          bleGamepad.press(i + 1);
        } else {
          bleGamepad.release(i + 1);
        }
      }
    }
  }

  delay(1);
}
