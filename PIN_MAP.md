# ESP32-S3 Wireless Panel — Pin Map

This is the **firmware pin assignment**, not yet a verified physical-board layout. Confirm the exact ESP32-S3 board model/revision and the labels printed on the board before wiring.

## GPIO assignment

| Function | Signal / index | ESP32-S3 GPIO | Wiring / notes |
|---|---:|---:|---|
| Button 1 | HID button 1 | GPIO 6 | Normally-open switch between GPIO and GND |
| Button 2 | HID button 2 | GPIO 7 | Normally-open switch between GPIO and GND |
| Button 3 | HID button 3 | GPIO 8 | Normally-open switch between GPIO and GND |
| Button 4 | HID button 4 | GPIO 9 | Normally-open switch between GPIO and GND |
| Button 5 | HID button 5 | GPIO 10 | Normally-open switch between GPIO and GND |
| Button 6 | HID button 6 | GPIO 11 | Normally-open switch between GPIO and GND |
| Button 7 | HID button 7 | GPIO 12 | Normally-open switch between GPIO and GND |
| Button 8 | HID button 8 | GPIO 13 | Normally-open switch between GPIO and GND |
| Button 9 | HID button 9 | GPIO 14 | Normally-open switch between GPIO and GND |
| Button 10 | HID button 10 | GPIO 15 | Normally-open switch between GPIO and GND |
| Button 11 | HID button 11 | GPIO 16 | Normally-open switch between GPIO and GND |
| Button 12 | HID button 12 | GPIO 17 | Normally-open switch between GPIO and GND |
| Button 13 | HID button 13 | GPIO 18 | Normally-open switch between GPIO and GND |
| Button 14 | HID button 14 | GPIO 21 | Normally-open switch between GPIO and GND |
| Joystick ADC 1 | HID X axis | GPIO 1 | Analog wiper; physical stick/axis assignment to confirm |
| Joystick ADC 2 | HID Y axis | GPIO 2 | Analog wiper; physical stick/axis assignment to confirm |
| Joystick ADC 3 | HID Z axis | GPIO 3 | **Boot-strapping pin—test boot behavior before final wiring** |
| Joystick ADC 4 | HID Rx axis | GPIO 4 | Analog wiper; physical stick/axis assignment to confirm |
| RPM LED 1 | Lowest segment | GPIO 5 | LED + individual series resistor |
| RPM LED 2 | Segment 2 | GPIO 38 | **May conflict with onboard RGB LED on ESP32-S3-DevKitC-1 v1.1** |
| RPM LED 3 | Segment 3 | GPIO 39 | LED + individual series resistor |
| RPM LED 4 | Segment 4 | GPIO 40 | LED + individual series resistor |
| RPM LED 5 | Segment 5 | GPIO 41 | LED + individual series resistor |
| RPM LED 6 | Segment 6 | GPIO 42 | LED + individual series resistor |
| RPM LED 7 | Segment 7 | GPIO 47 | LED + individual series resistor |
| RPM LED 8 | Segment 8 | GPIO 48 | **May conflict with onboard RGB LED on ESP32-S3-DevKitC-1 v1.0** |
| RPM LED 9 | Highest segment | GPIO 33 | Confirm GPIO 33 is exposed on the exact board/revision |

## Wiring rules

- **Buttons:** firmware configures all 14 button pins as `INPUT_PULLUP`. Wire each normally-open button between its GPIO and **GND**. Pressed = LOW. Do not feed external voltage into a button GPIO.
- **Joystick modules/potentiometers:** power from the board's **3V3** and GND rails; connect each axis wiper/output to its assigned ADC GPIO. Never feed 5 V into an ESP32-S3 ADC pin. The four physical axis assignments are not confirmed yet.
- **RPM LEDs:** each discrete LED needs its own current-limiting resistor. Firmware turns a segment on with GPIO HIGH. Do not connect a 12 V LED strip directly to a GPIO.
- **Ground:** buttons, joystick modules and any LED driver must share ESP32-S3 GND.
- **Board-specific pin collision:** the official ESP32-S3-DevKitC-1 v1.0 commonly uses GPIO48 for its onboard RGB LED; v1.1 uses GPIO38. Because this pin map currently assigns both GPIO38 and GPIO48 to external LEDs, **one of those external LED assignments will conflict with the onboard RGB LED on either of those revisions**. Do not permanently wire the RPM LEDs until the board revision is confirmed and the pin map is adjusted. Other ESP32-S3 boards may differ.

## Firmware behavior

- Buttons are scanned every 2 ms with an 8 ms debounce.
- Joystick ADC values use four-sample averaging. Sampling runs every 5 ms (200 Hz); BLE gamepad reports run every 10 ms (100 Hz).
- ADC reads use 12-bit resolution (nominal range 0–4095). Center calibration and physical joystick-to-axis mapping still need to be tested on the assembled hardware.
- RPM LEDs are currently scaled in firmware to a 0–3500 RPM range; final calibration depends on the chosen ETS2 vehicle.

## To verify when the parts arrive

1. Confirm the exact ESP32-S3 board model/revision and match GPIO labels to this table.
2. Resolve the GPIO38/GPIO48 onboard RGB LED conflict before soldering the external LED wiring.
3. Test boot with the joystick connected to GPIO 3 before permanent installation.
4. Identify each joystick's X/Y wipers and confirm HID axis direction in Windows' game-controller panel.
5. Test all 14 buttons and each LED individually.
6. Measure joystick center noise and full travel before finalizing deadzone/sensitivity.
