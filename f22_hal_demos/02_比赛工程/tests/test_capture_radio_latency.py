import importlib.util
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[1] / "scripts/capture_radio_latency.py"
spec = importlib.util.spec_from_file_location("capture", script)
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


def packet(seq=65535, timestamp=0xffffffff, command=4):
    data = b"\xa5\x5a\x02" + seq.to_bytes(2, "little") + bytes((command, 1)) + timestamp.to_bytes(4, "little")
    return data + bytes((capture.crc8(data),))


class CaptureTests(unittest.TestCase):
    def test_v3_key_event_token(self):
        raw = bytearray(packet(seq=42, timestamp=100, command=1))
        raw[2] = 3
        raw[6] = 0xE2
        raw[-1] = capture.crc8(raw[:-1])
        decoder = capture.FrameDecoder()
        event = decoder.feed(raw)[0]
        self.assertEqual((event['version'], event['keys'], event['event_token']), (3, 2, 14))
        self.assertEqual(event['sequence'], 42)
        raw[2] = 2
        raw[-1] = capture.crc8(raw[:-1])
        self.assertIsNone(capture.decode_frame(raw))

    def test_compact_records_crc_and_expansion(self):
        def encode(text):
            content = text.encode().ljust(59, b" ")
            return content + f"*{capture.crc8(content):02X}\r".encode()
        raw = packet()
        line = encode(f"R {raw.hex().upper()} 00000123")
        expanded = capture.parse_receiver_line(line)
        self.assertIn("seq=65535 cmd=4", expanded)
        self.assertIn("rx_ms=291", expanded)
        self.assertIsNone(capture.parse_receiver_line(line[:3] + b"0" + line[4:]))
        self.assertIsNone(capture.parse_receiver_line(line[:10] + line[11:]))
        self.assertIsNone(capture.parse_receiver_line(line[:-4]))
        self.assertIsNone(capture.parse_receiver_line(b"R broken"))
        stats = encode("S 0000000C 00000001 00000000 00000000 00000000 00000000 1 0")
        self.assertEqual(capture.parse_receiver_line(stats), "STAT bytes=12 frames=1 reject=0 err=0 drop=0 tx_err=0 link=1 motors=0")
        self.assertEqual(capture.parse_receiver_line(encode("D 00000000 00000000 00000000 00000002")), "DIAG crc=0 timeout=0 timeout_bytes=0 gap_max=2")
        self.assertEqual(capture.parse_receiver_line(b"STAT bytes=0\r"), "STAT bytes=0")

    def test_split_noise_corrupt_and_concatenated_frames(self):
        decoder = capture.FrameDecoder()
        raw = packet()
        bad = raw[:-1] + bytes((raw[-1] ^ 1,))
        self.assertEqual(decoder.feed(b"noise" + bad + raw[:5]), [])
        events = decoder.feed(raw[5:] + packet(0, 0, 3))
        self.assertEqual([e["sequence"] for e in events], [65535, 0])
        self.assertEqual(events[0]["tx_ms"], 0xffffffff)
        self.assertEqual(decoder.bad_frames, 1)

    def test_matching_receive_first_and_identical_repeats(self):
        matcher = capture.Matcher()
        event = capture.decode_frame(packet())
        rx = dict(event, pc_ns=120_000_000, rx_ms=6000)
        tx = dict(event, pc_ns=100_000_000)
        self.assertIsNone(matcher.add("rx", rx))
        self.assertEqual(matcher.add("tx", tx), (tx, rx))
        self.assertEqual(matcher.counts(), {"tx": 0, "rx": 0})
        self.assertIsNone(matcher.add("tx", tx))
        self.assertIsNone(matcher.add("tx", tx))
        self.assertEqual(matcher.add("rx", rx), (tx, rx))
        self.assertEqual(matcher.counts(), {"tx": 1, "rx": 0})

    def test_bad_command_and_raw_integrity(self):
        self.assertIsNone(capture.decode_hex_frame("ABC"))
        self.assertIsNone(capture.decode_frame(packet(command=5)))
        event = capture.decode_frame(packet())
        self.assertEqual(event["raw"], packet().hex().upper())
        self.assertEqual(event["keys"], 1)

    def test_expiry_prevents_sequence_wrap_or_restart_mismatch(self):
        matcher = capture.Matcher()
        event = dict(capture.decode_frame(packet()), pc_ns=0)
        matcher.add("tx", event)
        matcher.expire(6_000_000_000)
        self.assertIsNone(matcher.add("rx", dict(event, pc_ns=6_000_000_000)))
        self.assertEqual(matcher.expired, {"tx": 1, "rx": 0})
        matcher.clear()
        self.assertEqual(matcher.counts(), {"tx": 0, "rx": 0})


if __name__ == "__main__":
    unittest.main()
