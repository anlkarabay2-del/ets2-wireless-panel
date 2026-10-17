import pathlib
import re
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SKETCH = ROOT / "ETS2_WirelessPanel.ino"
SOURCE = SKETCH.read_text(encoding="utf-8")


class FirmwareContractTests(unittest.TestCase):
    def test_button_count_and_pin_map_match(self):
        count = re.search(r"BUTTON_COUNT\s*=\s*(\d+)", SOURCE)
        pins = re.search(r"BUTTON_PINS\[BUTTON_COUNT\]\s*=\s*\{([^}]+)\}", SOURCE, re.S)
        self.assertIsNotNone(count, "BUTTON_COUNT declaration missing")
        self.assertIsNotNone(pins, "BUTTON_PINS declaration missing")
        pin_values = [int(value) for value in re.findall(r"\d+", pins.group(1))]
        self.assertEqual(int(count.group(1)), 13)
        self.assertEqual(pin_values, [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 5, 21])
        self.assertEqual(len(pin_values), len(set(pin_values)), "GPIO pin is assigned more than once")
        self.assertEqual(len(pin_values), int(count.group(1)))

    def test_buttons_use_active_low_pullups(self):
        self.assertIn("pinMode(BUTTON_PINS[i], INPUT_PULLUP)", SOURCE)
        self.assertIn("digitalRead(BUTTON_PINS[i]) == LOW", SOURCE)

    def test_hid_button_count_matches_physical_inputs(self):
        self.assertRegex(SOURCE, r"config\.setButtonCount\(BUTTON_COUNT\)")
        self.assertRegex(SOURCE, r"bleGamepad\.press\(i\s*\+\s*1\)")
        self.assertRegex(SOURCE, r"bleGamepad\.release\(i\s*\+\s*1\)")
        self.assertIn("config.setAutoReport(true)", SOURCE)

    def test_analog_controls_are_disabled(self):
        self.assertIn("config.setWhichAxes(false, false, false, false, false, false, false, false)", SOURCE)
        self.assertIn("config.setWhichSimulationControls(false, false, false, false, false)", SOURCE)
        self.assertIn("config.setHatSwitchCount(0)", SOURCE)

    def test_debounce_is_positive_and_state_change_is_gated(self):
        debounce = re.search(r"DEBOUNCE_MS\s*=\s*(\d+)", SOURCE)
        self.assertIsNotNone(debounce, "DEBOUNCE_MS declaration missing")
        self.assertGreater(int(debounce.group(1)), 0)
        self.assertRegex(
            SOURCE,
            r"rawPressed\s*!=\s*stablePressed\[i\]\s*&&\s*\(uint32_t\)\(now\s*-\s*rawChangedAt\[i\]\)\s*>=\s*DEBOUNCE_MS",
        )

    def test_connection_restores_current_physical_button_state(self):
        self.assertIn("if (connected != previousConnection)", SOURCE)
        self.assertIn("if (connected) {", SOURCE)
        self.assertIn("bleGamepad.resetButtons();", SOURCE)
        self.assertIn("const bool pressed = digitalRead(BUTTON_PINS[i]) == LOW;", SOURCE)
        self.assertIn("rawChangedAt[i] = now;", SOURCE)

    def test_hid_reports_only_when_connected(self):
        self.assertRegex(SOURCE, r"if \(connected\)\s*\{\s*if \(rawPressed\)")
        self.assertIn("bleGamepad.press(i + 1);", SOURCE)
        self.assertIn("bleGamepad.release(i + 1);", SOURCE)


if __name__ == "__main__":
    unittest.main()
