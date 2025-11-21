"""Hardware lifecycle smoke test for the Snow Permittivity Meter.

This revision reuses the SerialClient from pc_cli.py so the automation shares
the same USB transport plumbing as the interactive CLI. It optionally prompts
the operator to press RESET before starting, guaranteeing that calibration and
measurement always run from a clean FSM state. Two scenarios are supported:

* happy – expect STAT:RDY → STAT:CAL/STAT:CAL_OK → DAT:RES
* fail  – arm CMD:MOCK:RF:FAIL:ON and ensure STAT:ERR surfaces after MEAS
"""
from __future__ import annotations

import argparse
import re
import sys
import time
from dataclasses import dataclass
from typing import Iterable

from pc_cli import DEFAULT_BAUD, DEFAULT_TIMEOUT, SerialClient, SerialConfig

STAT_LED_RE = re.compile(r"STAT:LED:S:(?P<status>\d+):M:(?P<meas>\d+):E:(?P<excite>\d+):R:(?P<err>\d+)")
RESULT_RE = re.compile(r"DAT:RES:ER:(?P<er>[-+]?[0-9]*\.?[0-9]+):EI:(?P<ei>[-+]?[0-9]*\.?[0-9]+):DENS:(?P<dens>[-+]?[0-9]*\.?[0-9]+)")
DEFAULT_PORT = "COM7"


@dataclass
class LifecycleResult:
    calibration_timestamp: float
    result_frame: str
    failure_triggered: bool


class StageTimeout(RuntimeError):
    """Raised when an expected STAT/DAT line is missing."""


def wait_for_prefix(client: SerialClient,
                    prefixes: Iterable[str],
                    timeout: float,
                    description: str) -> str:
    deadline = time.monotonic() + timeout
    normalized = tuple(prefixes)
    while time.monotonic() < deadline:
        lines = client.expect(1, timeout=0.25)
        if not lines:
            continue
        for line in lines:
            if any(line.startswith(prefix) for prefix in normalized):
                return line
    raise StageTimeout(f"Timeout waiting for {description}")


def request_led_snapshot(client: SerialClient) -> str:
    client.send_line("CMD:LEDS")
    return wait_for_prefix(client, ("STAT:LED",), 1.0, "STAT:LED payload")


def ensure_idle_leds(client: SerialClient) -> None:
    snapshot = request_led_snapshot(client)
    match = STAT_LED_RE.match(snapshot)
    if not match:
        raise AssertionError(f"Unexpected LED payload: {snapshot}")
    if any(match.group(key) != "0" for key in ("meas", "excite", "err")):
        raise AssertionError(f"LEDs not idle: {snapshot}")


def wait_for_ready_lcd(client: SerialClient, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        client.send_line("CMD:LCD")
        wait_for_prefix(client, ("DAT:LCD:L0",), 1.0, "LCD line 0")
        ready_line = wait_for_prefix(client, ("DAT:LCD:L1",), 1.0, "LCD line 1")
        if "Ready" in ready_line:
            return
        time.sleep(0.2)
    raise StageTimeout("LCD never reported Ready")


def run_calibration(client: SerialClient, timeout: float, probe_debug: bool) -> float:
    client.send_line("CMD:CAL")
    wait_for_prefix(client, ("STAT:CAL",), timeout, "STAT:CAL")
    wait_for_prefix(client, ("STAT:CAL_OK",), timeout, "STAT:CAL_OK")
    if probe_debug:
        client.send_line("CMD:LOG")
        wait_for_prefix(client, ("DAT:LOG", "STAT:LOG_EMPTY"), timeout, "log dump")
        time.sleep(0.2)
        client.send_line("CMD:TRACE")
        wait_for_prefix(client, ("DAT:TRACE", "STAT:TRACE_EMPTY"), timeout, "trace dump")
    ensure_idle_leds(client)
    wait_for_ready_lcd(client, timeout)
    return time.time()


def run_measurement(client: SerialClient, timeout: float) -> str:
    client.send_line("CMD:MEAS")
    wait_for_prefix(client, ("STAT:MEAS",), timeout, "STAT:MEAS")
    return wait_for_prefix(client, ("DAT:RES:", "STAT:ERR"), timeout, "measurement result")


def toggle_failure(client: SerialClient, enable: bool) -> None:
    cmd = "CMD:MOCK:RF:FAIL:ON" if enable else "CMD:MOCK:RF:FAIL:OFF"
    client.send_line(cmd)
    suffix = "ON" if enable else "OFF"
    wait_for_prefix(client, (f"STAT:MOCK_RF_FAIL_{suffix}",), 1.0, f"mock RF fail {suffix}")


def run_lifecycle(port: str,
                  baud: int,
                  timeout: float,
                  scenario: str,
                  stage_timeout: float,
                  prompt_reset: bool,
                  probe_debug: bool,
                  strict_result: bool) -> LifecycleResult:
    if prompt_reset:
        input("Press the board RESET button now, then press Enter to continue...")
    cfg = SerialConfig(port=port, baud=baud, timeout=timeout)
    client = SerialClient(cfg)
    try:
        time.sleep(0.5)
        client.send_line("CMD:CONN")
        wait_for_prefix(client, ("STAT:RDY",), stage_timeout, "STAT:RDY")
        ensure_idle_leds(client)
        cal_timestamp = run_calibration(client, stage_timeout, probe_debug)
        force_failure = scenario == "fail"
        try:
            if force_failure:
                toggle_failure(client, True)
            result = run_measurement(client, stage_timeout)
            if force_failure:
                if not result.startswith("STAT:ERR"):
                    raise AssertionError(f"Expected STAT:ERR but received '{result}'")
            else:
                if strict_result and RESULT_RE.match(result) is None:
                    raise AssertionError(f"DAT:RES frame malformed: {result}")
            return LifecycleResult(calibration_timestamp=cal_timestamp,
                                   result_frame=result,
                                   failure_triggered=force_failure)
        finally:
            if force_failure:
                try:
                    toggle_failure(client, False)
                except StageTimeout:
                    pass
    finally:
        client.close()


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Snow Permittivity Meter lifecycle runner")
    parser.add_argument("--port", default=DEFAULT_PORT, help="Serial port (default: COM7)")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="UART baud rate")
    parser.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT, help="Serial timeout")
    parser.add_argument("--stage-timeout", type=float, default=8.0,
                        help="Seconds to wait for each lifecycle milestone")
    parser.add_argument("--scenario", choices=("happy", "fail"), default="happy",
                        help="Run the nominal flow or force STAT:ERR via mock RF controls")
    parser.add_argument("--no-reset-prompt", action="store_true",
                        help="Skip the \"press RESET\" prompt before opening the port")
    parser.add_argument("--probe-debug", action="store_true",
                        help="Also fetch LOG/TRACE data during calibration (slower)")
    parser.add_argument("--strict-result", action="store_true",
                        help="Require DAT:RES numeric fields to parse")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        result = run_lifecycle(port=args.port,
                               baud=args.baud,
                               timeout=args.timeout,
                               scenario=args.scenario,
                               stage_timeout=args.stage_timeout,
                               prompt_reset=not args.no_reset_p...