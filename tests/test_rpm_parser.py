import unittest

from ble_udp_rpm_bridge import (
    effective_rpm,
    is_panel_advertisement,
    parse_rpm_datagram,
    write_response_mode,
)


class ParseRpmDatagramTests(unittest.TestCase):
    def test_accepts_plain_integer(self):
        self.assertEqual(parse_rpm_datagram(b"1420"), 1420)

    def test_accepts_rpm_prefix_case_insensitively(self):
        self.assertEqual(parse_rpm_datagram(b"RPM:1420"), 1420)
        self.assertEqual(parse_rpm_datagram(b"rpm: 1420\r\n"), 1420)

    def test_trims_whitespace(self):
        self.assertEqual(parse_rpm_datagram(b"  750 \n"), 750)

    def test_preserves_legacy_numeric_decimal_behavior(self):
        self.assertEqual(parse_rpm_datagram(b"1420.9"), 1420)

    def test_normalizes_to_full_led_bar_at_vehicle_redline(self):
        self.assertEqual(parse_rpm_datagram(b"RPM:2200:2200"), 3500)

    def test_normalizes_half_rpm_to_half_led_scale(self):
        self.assertEqual(parse_rpm_datagram(b"RPM:1100:2200"), 1750)

    def test_normalizes_idle_rpm_and_clamps_over_redline(self):
        self.assertEqual(parse_rpm_datagram(b"RPM:0:2200"), 0)
        self.assertEqual(parse_rpm_datagram(b"RPM:2500:2200"), 3500)

    def test_rejects_invalid_maximum_rpm(self):
        self.assertIsNone(parse_rpm_datagram(b"RPM:1000:0"))
        self.assertIsNone(parse_rpm_datagram(b"RPM:1000:bad"))
        self.assertIsNone(parse_rpm_datagram(b"RPM:1000:2200:extra"))

    def test_accepts_range_boundaries(self):
        self.assertEqual(parse_rpm_datagram(b"0"), 0)
        self.assertEqual(parse_rpm_datagram(b"30000"), 30000)

    def test_rejects_invalid_values(self):
        invalid_payloads = (
            b"",
            b"RPM:",
            b"RPM:1234:bad",
            b"not-rpm",
            b"nan",
            b"inf",
            b"-inf",
            b"-1",
            b"30001",
            b"\xff",
        )
        for payload in invalid_payloads:
            with self.subTest(payload=payload):
                self.assertIsNone(parse_rpm_datagram(payload))

    def test_rejects_non_ascii_bytes_inside_numeric_payload(self):
        self.assertIsNone(parse_rpm_datagram(b"1\xff420"))


class EffectiveRpmTests(unittest.TestCase):
    def test_no_sample_yet_returns_none(self):
        self.assertIsNone(effective_rpm(None, None, 100.0))

    def test_fresh_sample_is_preserved(self):
        self.assertEqual(effective_rpm(1420, 99.5, 100.0), 1420)

    def test_stale_sample_falls_back_to_zero(self):
        self.assertEqual(effective_rpm(1420, 98.9, 100.0), 0)

    def test_sample_at_timeout_boundary_is_still_fresh(self):
        self.assertEqual(effective_rpm(1420, 99.0, 100.0), 1420)


class PanelAdvertisementTests(unittest.TestCase):
    def test_matches_nus_service_uuid_without_name(self):
        self.assertTrue(
            is_panel_advertisement(
                None,
                None,
                ["6E400001-B5A3-F393-E0A9-E50E24DCCA9E"],
            )
        )

    def test_matches_advertised_local_name_when_windows_name_is_cached(self):
        self.assertTrue(
            is_panel_advertisement(
                "Unknown cached name",
                "ETS2-NuS",
                [],
            )
        )

    def test_matches_windows_device_name_when_local_name_is_missing(self):
        self.assertTrue(
            is_panel_advertisement(
                "ETS2 Custom Wheel",
                None,
                None,
            )
        )

    def test_does_not_match_unrelated_device(self):
        self.assertFalse(
            is_panel_advertisement(
                "Bluetooth Speaker",
                "BT-Speaker",
                ["0000180F-0000-1000-8000-00805F9B34FB"],
            )
        )


class WriteResponseModeTests(unittest.TestCase):
    def test_prefers_write_with_response(self):
        self.assertIs(write_response_mode(["write"]), True)

    def test_supports_write_without_response_only(self):
        self.assertIs(write_response_mode(["write-without-response"]), False)

    def test_prefers_response_when_both_modes_are_supported(self):
        self.assertIs(
            write_response_mode(["write", "write-without-response"]),
            True,
        )

    def test_returns_none_when_no_write_property_is_supported(self):
        self.assertIsNone(write_response_mode(["read", "notify"]))
        self.assertIsNone(write_response_mode(None))


if __name__ == "__main__":
    unittest.main()
