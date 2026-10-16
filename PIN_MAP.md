# ESP32-S3 Wireless Panel — Pin Map

This table describes the current firmware assignment, not a verified physical-board layout. Confirm the labels printed on the exact ESP32-S3 board before wiring.

## GPIO assignment

| Function | HID index / signal | ESP32-S3 GPIO | Wiring / notes |
|---|---:|---:|---|
| Regular button 1 | Button 1 | GPIO 6 | Normally-open switch between GPIO and GND |
| Regular button 2 | Button 2 | GPIO 7 | Normally-open switch between GPIO and GND |
| Regular button 3 | Button 3 | GPIO 8 | Normally-open switch between GPIO and GND |
| Regular button 4 | Button 4 | GPIO 9 | Normally-open switch between GPIO and GND |
| Regular button 5 | Button 5 | GPIO 10 | Normally-open switch between GPIO and GND |
| Regular button 6 | Button 6 | GPIO 11 | Normally-open switch between GPIO and GND |
| Regular button 7 | Button 7 | GPIO 12 | Normally-open switch between GPIO and GND |
| Regular button 8 | Button 8 | GPIO 13 | Normally-open switch between GPIO and GND |
| Regular button 9 | Button 9 | GPIO 14 | Normally-open switch between GPIO and GND |
| Regular button 10 | Button 10 | GPIO 15 | Normally-open switch between GPIO and GND |
| Regular button 11 | Button 11 | GPIO 16 | Normally-open switch between GPIO and GND |
| Menu-stick click | Button 12 | GPIO 17 | Normally-open switch between GPIO and GND |
| Camera-stick click | Button 13 | GPIO 18 | Normally-open switch between GPIO and GND |
| Left paddle shifter | Button 14 | GPIO 5 | Normally-open switch between GPIO and GND |
| Right paddle shifter | Button 15 | GPIO 21 | Normally-open switch between GPIO and GND |
| Joystick ADC 1 | HID X axis | GPIO 1 | First analog wiper; physical assignment to confirm |
| Joystick ADC 2 | HID Y axis | GPIO 2 | Second analog wiper; physical assignment to confirm |
| Joystick ADC 3 | HID Z axis | GPIO 3 | Third analog wiper; **boot-strapping pin—validate before final wiring** |
| Joystick ADC 4 | HID Rz axis | GPIO 4 | Fourth analog wiper; Windows commonly maps Rz to right-stick Y |

## Wiring rules

- **Buttons:** firmware configures all 15 button pins as `INPUT_PULLUP`. Wire each normally-open button between its GPIO and **GND**. Pressed = LOW. Do not feed external voltage into a button GPIO.
- **Joystick modules/potentiometers:** power from the board's **3V3** and GND rails; connect each axis wiper/output to its assigned ADC GPIO. Never feed 5 V into an ESP32-S3 ADC pin. The physical joystick-to-axis assignments still need to be verified.
- **Ground:** buttons and joystick modules must share ESP32-S3 GND. Keep wiring secure and strain-relieved.
- **Board-specific checks:** ESP32-S3 board revisions can differ in exposed pins and onboard peripherals. GPIO 3 is a strapping pin; confirm joystick wiring does not interfere with startup. Some flash/PSRAM module variants reserve additional GPIOs. Check the exact board schematic before permanent wiring.

## Firmware behavior

- Buttons are scanned every 2 ms with an 8 ms debounce.
- Joystick ADC values use four-sample averaging. Sampling runs every 5 ms (200 Hz); BLE gamepad reports run every 10 ms (100 Hz).
- ADC reads use 12-bit resolution (nominal range 0–4095). Firmware uses configurable per-axis centers (default 2048), optional minimum/maximum endpoint calibration (default 0/4095), and separate hysteresis deadzones: menu axes activate at 100 counts/release at 70; camera axes activate at 170/release at 130. These are starting values, not measured hardware results.
- **Calibrate joystick centers:** temporarily set `DEBUG_JOYSTICK_ADC = true` in `ETS2_WirelessPanel.ino`, upload, then open Serial Monitor at **115200 baud**. The firmware prints `JOY_ADC:value1,value2,value3,value4` every 250 ms. Record the stable center readings and update `JOYSTICK_CENTER_ADC`; then set debug back to `false`.
- **Calibrate endpoints:** move each axis to both physical ends and record the readings. If the stick does not reach ADC values near 0 and 4095, update `JOYSTICK_MIN_ADC` and `JOYSTICK_MAX_ADC` to the measured endpoints.
- **Reverse an axis:** set the matching `JOYSTICK_INVERTED` entry to `true`. Array order: GPIO 1/X, GPIO 2/Y, GPIO 3/Z, GPIO 4/Rz.
- HID mapping is X/Y for the menu stick and Z/Rz for the camera stick. Verify the actual Windows game-controller display and ETS2 bindings; physical axis orientation has not been tested.

## To verify on the assembled hardware

1. Confirm the exact ESP32-S3 board model/revision and match GPIO labels to this table.
2. Test startup with the joystick connected to GPIO 3 before permanent installation.
3. Measure the four resting ADC centers and both endpoints; update calibration arrays if needed.
4. Verify all 15 buttons in the Windows game-controller panel.
5. Verify the four axes appear and move in the expected directions, then bind them in ETS2.
