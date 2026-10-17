#include <Arduino.h>
#include <BleGamepad.h>

// ESP32-S3 DevKitC-1 — 11 normal buttons + 2 paddle shifters.
// Her butonun bir ucu GPIO'ya, diğer ucu GND'ye bağlanır.
static constexpr uint8_t BUTTON_COUNT = 13;
static constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {
  6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 5, 21
};

BleGamepad bleGamepad("ETS2 Wireless Panel", "DIY", 100, false);

static bool stablePressed[BUTTON_COUNT];
static bool lastRawPressed[BUTTON_COUNT];
static uint32_t rawChangedAt[BUTTON_COUNT];
static bool previousConnection = false;
static constexpr uint32_t DEBOUNCE_MS = 25;

void setup() {
  Serial.begin(115200);
  delay(300);

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    const bool pressed = digitalRead(BUTTON_PINS[i]) == LOW;
    stablePressed[i] = pressed;
    lastRawPressed[i] = pressed;
    rawChangedAt[i] = millis();
  }

  BleGamepadConfiguration config;
  config.setControllerType(CONTROLLER_TYPE_GAMEPAD);
  config.setButtonCount(BUTTON_COUNT);
  config.setAutoReport(true);
  config.setHatSwitchCount(0);
  config.setWhichAxes(false, false, false, false, false, false, false, false);
  config.setWhichSimulationControls(false, false, false, false, false);
  config.setWhichSpecialButtons(false, false, false, false, false, false, false, false);

  bleGamepad.begin(&config);

  Serial.println();
  Serial.println("ETS2 Wireless Panel basladi.");
  Serial.println("Buton pinleri: 6-16, 5, 21 (INPUT_PULLUP; buton diger ucu GND).");
  Serial.println("115200 baud Serial Monitor'u ac.");
}

void loop() {
  const uint32_t now = millis();
  const bool connected = bleGamepad.isConnected();

  if (connected != previousConnection) {
    Serial.println(connected ? "BLE BAGLANDI" : "BLE BAGLANTISI KESILDI");
    previousConnection = connected;

    if (connected) {
      // Baglanirken basili olan butonlari da HID durumuna aktar.
      bleGamepad.resetButtons();
      for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
        const bool pressed = digitalRead(BUTTON_PINS[i]) == LOW;
        stablePressed[i] = pressed;
        lastRawPressed[i] = pressed;
        rawChangedAt[i] = now;
        if (pressed) {
          bleGamepad.press(i + 1);
          Serial.printf("GPIO %u -> BUTON %u BASILI (baglanirken)\n",
                        BUTTON_PINS[i], i + 1);
        }
      }
    }
  }

  for (uint8_t i = 0; i < BUTTON_COUNT; ++i) {
    const bool rawPressed = digitalRead(BUTTON_PINS[i]) == LOW;

    if (rawPressed != lastRawPressed[i]) {
      lastRawPressed[i] = rawPressed;
      rawChangedAt[i] = now;
    }

    if (rawPressed != stablePressed[i] &&
        (uint32_t)(now - rawChangedAt[i]) >= DEBOUNCE_MS) {
      stablePressed[i] = rawPressed;

      Serial.printf("GPIO %u -> BUTON %u %s\n",
                    BUTTON_PINS[i], i + 1,
                    rawPressed ? "BASILDI" : "BIRAKILDI");

      if (connected) {
        if (rawPressed) {
          bleGamepad.press(i + 1);
        } else {
          bleGamepad.release(i + 1);
        }
      } else {
        Serial.println("Not: BLE bagli olmadigi icin HID tusu gonderilmedi.");
      }
    }
  }

  delay(1);
}
