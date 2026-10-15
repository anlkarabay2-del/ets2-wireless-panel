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
- **Board-specific checks:** ESP32-S3 board revisions can differ in exposed pins and onboard peripherals. GPIO 3 is a strapping pin, and GPIO 48 may be used by an onboard RGB LED on some boards. Verify startup behavior and pin availability before soldering permanently.

## Firmware behavior

- Buttons are scanned every 2 ms with an 8 ms debounce.
- Joystick ADC values use four-sample averaging. On the current optimization branch, sampling runs every 5 ms (200 Hz); BLE gamepad reports run every 10 ms (100 Hz).
- ADC reads use 12-bit resolution (nominal range 0–4095). Firmware uses configurable per-axis centers (default 2048) and separate hysteresis deadzones: menu axes activate at 100 counts/release at 70; camera axes activate at 170/release at 130. These are conservative starting values, not measured hardware results. Adjust `JOYSTICK_CENTER_ADC` after measuring each assembled axis at rest.
- HID axis mapping is X/Y for the menu stick and Z/Rz for the camera stick. In the ESP32 BLE Gamepad library's documented Windows mapping, Z is right-stick X and Rz is right-stick Y; Rx maps to a trigger, so it is intentionally not used for the camera stick.
- RPM LEDs are currently scaled in firmware to a 0–3500 RPM range; confirm the desired ETS2 truck/vehicle behavior before final calibration.

## To verify when the parts arrive

1. Confirm the exact ESP32-S3 board model/revision and match GPIO labels to this table.
2. Test boot with the joystick connected to GPIO 3 before permanent installation.
3. Identify each joystick's X/Y wipers and confirm the HID axis direction in the Windows game-controller panel.
4. Test all 14 buttons, then each LED individually at low current.
5. Measure joystick center noise and full travel before setting deadzone/sensitivity.
