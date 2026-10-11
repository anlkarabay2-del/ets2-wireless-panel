import unittest

from ble_udp_rpm_bridge import effective_rpm, parse_rpm_datagram


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


class EffectiveRpmTests(unittest.TestCase):
    def test_no_sample_yet_returns_none(self):
        self.assertIsNone(effective_rpm(None, None, 100.0))

    def test_fresh_sample_is_preserved(self):
        self.assertEqual(effective_rpm(1420, 99.5, 100.0), 1420)

    def test_stale_sample_falls_back_to_zero(self):
        self.assertEqual(effective_rpm(1420, 98.9, 100.0), 0)

    def test_sample_at_timeout_boundary_is_still_fresh(self):
        self.assertEqual(effective_rpm(1420, 99.0, 100.0), 1420)


if __name__ == "__main__":
    unittest.main()
