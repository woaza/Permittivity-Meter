"""Lifecycle regression tests for the Snow Permittivity Meter serial feed.

These tests replay a representative UART transcript captured during a GUI
session. They assert that critical milestones (handshake, calibration,
LED/LCD refreshes, BT telemetry) appear in the expected order so future
changes to either the firmware or host UI do not break the workflow.
"""
from __future__ import annotations

import re
from pathlib import Path
import unittest
from dataclasses import dataclass


LOG_PATH = Path(__file__).resolve().parents[1] / "testdata" / "serial_lifecycle_idle.log"

LED_PATTERN = re.compile(r"STAT:LED:S:(?P<status>\d+):M:(?P<meas>\d+):E:(?P<excite>\d+):R:(?P<err>\d+)")
TRACE_PATTERN = re.compile(r"DAT:TRACE:(?P<mode>[A-Z]+):(?P<idx>\d+):V:(?P<voltage>\d+\.\d+):A:(?P<amplitude>\d+\.\d+)")
LCD_PATTERN = re.compile(r"DAT:LCD:(?P<line>L[01]):(?P<text>.+)")


@dataclass
class SerialEvent:
    timestamp: str
    payload: str


class SerialTranscript:
    def __init__(self, events: list[SerialEvent]):
        self.events = events

    @classmethod
    def load(cls, path: Path) -> "SerialTranscript":
        events: list[SerialEvent] = []
        for raw in path.read_text().splitlines():
            if not raw.strip():
                continue
            if "] " in raw:
                timestamp, payload = raw.split("] ", 1)
                timestamp = f"{timestamp}]"
            else:
                timestamp, payload = "", raw
            events.append(SerialEvent(timestamp=timestamp, payload=payload.strip()))
        return cls(events)

    def find_indices(self, prefix: str) -> list[int]:
        return [idx for idx, evt in enumerate(self.events) if evt.payload.startswith(prefix)]

    def payloads(self, prefix: str) -> list[str]:
        return [evt.payload for evt in self.events if evt.payload.startswith(prefix)]


class TestSerialLifecycle(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.transcript = SerialTranscript.load(LOG_PATH)

    def test_status_handshake_sequence(self) -> None:
        """STAT:RDY should appear once after connection."""
        ready_indices = self.transcript.find_indices("STAT:RDY")
        self.assertTrue(ready_indices, "STAT:RDY missing from transcript")
        first_ready = ready_indices[0]
        connected_idx = self.transcript.find_indices("Connected on")
        self.assertTrue(connected_idx, "Connection banner missing")
        self.assertGreater(first_ready, connected_idx[0], "Ready reported before connection banner")

    def test_led_snapshots_cover_idle_and_calibration(self) -> None:
        """LED snapshots should show idle (all zeros) and calibration (meas/excite set)."""
        led_states = [LED_PATTERN.search(payload) for payload in self.transcript.payloads("STAT:LED")]
        self.assertTrue(led_states, "No LED snapshots captured")
        idle_seen = any(match and match.group("meas") == "0" and match.group("excite") == "0" for match in led_states)
        self.assertTrue(idle_seen, "Idle LED state never observed")
        cal_seen = any(match and match.group("meas") == "1" and match.group("excite") == "1" for match in led_states)
        self.assertTrue(cal_seen, "Calibration LED state never observed")

    def test_lcd_reports_idle_ready(self) -> None:
        lcd_lines = {
            match.group("line"): match.group("text").strip()
            for payload in self.transcript.payloads("DAT:LCD")
            if (match := LCD_PATTERN.match(payload))
        }
        self.assertEqual(lcd_lines.get("L0"), "IDLE", "LCD line 0 should show IDLE")
        self.assertEqual(lcd_lines.get("L1"), "Ready", "LCD line 1 should show Ready")

    def test_calibration_sequence_precedes_trace(self) -> None:
        cal_idx = self.transcript.find_indices("STAT:CAL")
        self.assertGreaterEqual(len(cal_idx), 2, "Expected two calibration cycles")
        cal_ok_idx = self.transcript.find_indices("STAT:CAL_OK")
        self.assertEqual(len(cal_idx), len(cal_ok_idx), "Each STAT:CAL needs matching STAT:CAL_OK")
        first_trace_idx = self.transcript.find_indices("DAT:TRACE")
        self.assertTrue(first_trace_idx, "Trace data never emitted")
        self.assertLess(cal_idx[0], first_trace_idx[0], "Trace data appeared before calibration start")
        self.assertLess(cal_ok_idx[0], first_trace_idx[0], "Trace data appeared before calibration finished")

    def test_bt_telemetry_contains_tx_and_rx(self) -> None:
        bt_logs = self.transcript.payloads("DAT:LOG")
        tx_entries = [line for line in bt_logs if "BT_TX" in line]
        rx_entries = [line for line in bt_logs if "BT_RX" in line]
        self.assertTrue(tx_entries, "No BT_TX entries recorded")
        self.assertTrue(rx_entries, "No BT_RX entries recorded")

    def test_trace_indexes_are_monotonic(self) -> None:
        trace_entries = [TRACE_PATTERN.match(payload) for payload in self.transcript.payloads("DAT:TRACE")]
        trace_entries = [entry for entry in trace_entries if entry]
        self.assertGreater(len(trace_entries), 5, "Not enough trace samples to validate")
        indices = [int(entry.group("idx")) for entry in trace_entries]
        self.assertEqual(indices, sorted(indices), "Trace indices out of order")
        step_sizes = {b - a for a, b in zip(indices, indices[1:])}
        self.assertTrue(step_sizes.issubset({1}), "Trace indices should increment by 1")

    def test_uart_error_recovery(self) -> None:
        """Each UART error should be followed by a decoded CMD to show recovery."""
        error_indices = self.transcript.find_indices("DAT:LOG:D:2:S:1:UART:rx_fail")
        self.assertTrue(error_indices, "No UART errors captured to validate")
        for idx in error_indices:
            # Search the next few events for a UART_RX decoded command.
            follow_up = self.transcript.events[idx + 1: idx + 5]
            recovered = any(evt.payload.startswith("DAT:LOG") and "UART_RX" in evt.payload for evt in follow_up)
            self.assertTrue(recovered, f"UART error at event {idx} was not followed by UART_RX recovery log")


if __name__ == "__main__":
    unittest.main()
