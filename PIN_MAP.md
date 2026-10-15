# ESP32-S3 Wireless Panel — Pin Map

This is the **firmware pin assignment**, not yet a verified physical-board layout. Confirm the labels printed on the exact ESP32-S3 board before wiring.

## GPIO assignment

| Function | Signal / index | ESP32-S3 GPIO | Wiring / notes |
|---|---:|---:|---|
| Button 1 | HID button 1 | GPIO 6 | Momentary switch between GPIO and GND |
| Button 2 | HID button 2 | GPIO 7 | Momentary switch between GPIO and GND |
| Button 3 | HID button 3 | GPIO 8 | Momentary switch between GPIO and GND |
| Button 4 | HID button 4 | GPIO 9 | Momentary switch between GPIO and GND |
| Button 5 | HID button 5 | GPIO 10 | Momentary switch between GPIO and GND |
| Button 6 | HID button 6 | GPIO 11 | Momentary switch between GPIO and GND |
| Button 7 | HID button 7 | GPIO 12 | Momentary switch between GPIO and GND |
| Button 8 | HID button 8 | GPIO 13 | Momentary switch between GPIO and GND |
| Button 9 | HID button 9 | GPIO 14 | Momentary switch between GPIO and GND |
| Button 10 | HID button 10 | GPIO 15 | Momentary switch between GPIO and GND |
| Button 11 | HID button 11 | GPIO 16 | Momentary switch between GPIO and GND |
| Button 12 | HID button 12 | GPIO 17 | Momentary switch between GPIO and GND |
| Button 13 | HID button 13 | GPIO 18 | Momentary switch between GPIO and GND |
| Button 14 | HID button 14 | GPIO 21 | Momentary switch between GPIO and GND |
| Joystick ADC 1 | HID X axis | GPIO 1 | First analog wiper; physical stick/axis assignment to confirm |
| Joystick ADC 2 | HID Y axis | GPIO 2 | Second analog wiper; physical stick/axis assignment to confirm |
| Joystick ADC 3 | HID Z axis | GPIO 3 | Third analog wiper; **boot-strapping pin—validate before final wiring** |
| Joystick ADC 4 | HID Rz axis | GPIO 4 | Fourth analog wiper; Windows maps Rz to right-stick Y |
| RPM LED 1 | Lowest segment | GPIO 5 | LED + individual series resistor |
| RPM LED 2 | Segment 2 | GPIO 38 | LED + individual series resistor |
| RPM LED 3 | Segment 3 | GPIO 39 | LED + individual series resistor |
| RPM LED 4 | Segment 4 | GPIO 40 | LED + individual series resistor |
| RPM LED 5 | Segment 5 | GPIO 41 | LED + individual series resistor |
| RPM LED 6 | Segment 6 | GPIO 42 | LED + individual series resistor |
| RPM LED 7 | Segment 7 | GPIO 47 | LED + individual series resistor |
| RPM LED 8 | Segment 8 | GPIO 48 | Check whether this GPIO is shared with an onboard RGB LED on the exact board revision |
| RPM LED 9 | Highest segment | GPIO 33 | Confirm GPIO 33 is exposed on the exact board/revision |

## Wiring rules

- **Buttons:** firmware configures all 14 button pins as `INPUT_PULLUP`. Wire each normally-open button between its GPIO and **GND**. Pressed = LOW. No external voltage should be fed into a button GPIO.
- **Joystick modules/potentiometers:** power from the board's **3V3** and GND rails; connect each axis wiper/output to its assigned ADC GPIO. Never feed 5 V into an ESP32-S3 ADC pin. The four physical axis assignments are not confirmed yet.
- **RPM LEDs:** each discrete LED needs its own current-limiting resistor. Firmware turns a segment on with GPIO HIGH. Do not connect a 12 V LED strip directly to GPIO; use an appropriate driver if the chosen indicator needs more current/voltage.
- **Ground:** button, joystick and LED-driver grounds must share ESP32-S3 GND. Keep wiring tidy and secure.
- **Board-specific checks:** ESP32-S3-DevKitC-1 revisions can differ in exposed pins and onboard peripherals. GPIO 3 is a strapping pin. The onboard RGB LED uses GPIO 48 on some revisions and GPIO 38 on others, so one entry in this LED list may conflict. Verify the board revision and resolve this conflict before permanent wiring. Some flash/PSRAM module variants also reserve additional GPIOs.

## Firmware behavior

- Buttons are scanned every 2 ms with an 8 ms debounce.
- Joystick ADC values use four-sample averaging. Sampling runs every 5 ms (200 Hz); BLE gamepad reports run every 10 ms (100 Hz).
- ADC reads use 12-bit resolution (nominal range 0–4095). Firmware uses configurable per-axis centers (default 2048) and separate hysteresis deadzones: menu axes activate at 100 counts/release at 70; camera axes activate at 170/release at 130. These are starting values, not measured hardware results. While an axis is active, its output fades toward zero across the release band rather than snapping at the activation boundary.
- **Calibrate joystick centers:** in `ETS2_WirelessPanel.ino`, temporarily set `DEBUG_JOYSTICK_ADC = true`, upload, then open the Serial Monitor at **115200 baud**. The firmware prints `JOY_ADC:value1,value2,value3,value4` every 250 ms. With each stick released at its normal center, record the four stable readings and use them in `JOYSTICK_CENTER_ADC`; set `DEBUG_JOYSTICK_ADC = false` afterward.
- **Reverse an axis:** if moving one direction makes Windows show the opposite direction, set the matching entry in `JOYSTICK_INVERTED` to `true`. The order is GPIO 1/X, GPIO 2/Y, GPIO 3/Z, GPIO 4/Rz.
- HID axis mapping is X/Y for the menu stick and Z/Rz for the camera stick. In the ESP32 BLE Gamepad library's documented Windows mapping, Z is right-stick X and Rz is right-stick Y; Rx maps to a trigger, so it is intentionally not used for the camera stick.
- RPM LEDs use a 0–3500 internal bar scale. The SimHub plugin sends current RPM plus vehicle MaxRpm; the Python bridge normalizes that ratio to the scale so the ninth LED reaches the top of the bar near each vehicle's redline. Legacy single-number packets keep absolute-RPM behavior.
- The Python bridge checks both BLE names and the advertised NuS service UUID. It also selects the GATT write mode from the RX characteristic's actual properties; NuS-NimBLE-Serial 4.2.1 exposes RX as a write-with-response characteristic.

## To verify when the parts arrive

1. Confirm the exact ESP32-S3 board model/revision and match GPIO labels to this table. Resolve any onboard RGB LED pin conflict before wiring all nine RPM LEDs.
2. Test boot with the joystick connected to GPIO 3 before permanent installation.
3. Measure the four resting ADC centers with the optional serial diagnostic; set centers and axis directions as needed.
4. Test all 14 buttons, then each LED individually at low current.
5. Verify Windows sees the four axes as X/Y/Z/Rz, then test menu and camera movement in ETS2.
6. Test RPM end-to-end: SimHub plugin packet count, Python BLE bridge connection, and LED response.
