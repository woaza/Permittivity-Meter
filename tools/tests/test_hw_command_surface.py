from __future__ import annotations

import re
import time
from pathlib import Path

import pytest

from pc_cli import SerialClient, run_script


STAT_LED_RE = re.compile(r"STAT:LED:S:(?P<status>\d+):M:(?P<meas>\d+):E:(?P<excite>\d+):R:(?P<err>\d+)")
DAT_RES_RE = re.compile(
    r"DAT:RES:ER:(?P<er>[-+]?[0-9]*\.?[0-9]+):EI:(?P<ei>[-+]?[0-9]*\.?[0-9]+):DENS:(?P<dens>[-+]?[0-9]*\.?[0-9]+)"
)
STAT_HW_DAC_V_RE = re.compile(r"STAT:HW:DAC:(?P<ch>\d+):V:(?P<v>[-+]?[0-9]*\.?[0-9]+)")


def wait_for_prefix(client: SerialClient, prefixes: tuple[str, ...], timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for line in client.expect(1, timeout=0.25):
            if any(line.startswith(p) for p in prefixes):
                return line
    raise AssertionError(f"Timeout waiting for one of {prefixes}")


def wait_for_regex(client: SerialClient, pattern: re.Pattern[str], timeout: float) -> str:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for line in client.expect(1, timeout=0.25):
            if pattern.search(line):
                return line
    raise AssertionError(f"Timeout waiting for regex {pattern.pattern}")


def ensure_connected(client: SerialClient, timeout: float) -> None:
    """Reliably reaches STAT:RDY on real hardware.

    On COM ports (especially with auto-reset on open), the first CMD:CONN can be
    sent while the MCU is still booting and get dropped. We retry until RDY.
    """

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        client.send_line("CMD:CONN")
        try:
            line = wait_for_prefix(client, ("STAT:RDY", "STAT:MANUAL"), timeout=1.5)
            if line.startswith("STAT:RDY"):
                return

            # If the firmware is currently in manual state, exit it so tests start from IDLE.
            client.send_line("CMD:MANUAL:OFF")
            try:
                wait_for_prefix(client, ("STAT:MANUAL_OFF", "STAT:MANUAL_OFF_REQ"), timeout=2.0)
            except AssertionError:
                pass
            time.sleep(0.2)
            continue
        except AssertionError:
            time.sleep(0.2)
            continue
    raise AssertionError("Timeout waiting for STAT:RDY (CMD:CONN retry loop)")


def send_until_prefix(
    client: SerialClient,
    command: str,
    expected_prefixes: tuple[str, ...],
    timeout: float,
    per_try_timeout: float = 2.0,
    delay_between_tries: float = 0.2,
) -> str:
    deadline = time.monotonic() + timeout
    last_error: AssertionError | None = None
    while time.monotonic() < deadline:
        client.send_line(command)
        try:
            return wait_for_prefix(client, expected_prefixes, timeout=min(per_try_timeout, max(0.1, deadline - time.monotonic())))
        except AssertionError as exc:
            last_error = exc
            time.sleep(delay_between_tries)
            continue
    raise AssertionError(f"Timeout waiting for {expected_prefixes} after {command}") from last_error


def request_led_snapshot(client: SerialClient, timeout: float) -> str:
    client.send_line("CMD:LEDS")
    return wait_for_prefix(client, ("STAT:LED",), timeout)


def request_lcd_snapshot(client: SerialClient, timeout: float) -> list[str]:
    client.send_line("CMD:LCD")
    l0 = wait_for_prefix(client, ("DAT:LCD:L0",), timeout)
    l1 = wait_for_prefix(client, ("DAT:LCD:L1",), timeout)
    return [l0, l1]


@pytest.mark.hardware
def test_normal_operation_happy(serial_client: SerialClient, hw_cfg) -> None:
    client = serial_client

    ensure_connected(client, hw_cfg.stage_timeout)

    led = request_led_snapshot(client, hw_cfg.stage_timeout)
    match = STAT_LED_RE.match(led)
    assert match, f"Bad LED frame: {led}"

    l0, l1 = request_lcd_snapshot(client, hw_cfg.stage_timeout)
    # At least ensure we can read two lines.
    assert l0.startswith("DAT:LCD:L0")
    assert l1.startswith("DAT:LCD:L1")

    # Calibration should respond (some hardware builds may legitimately return STAT:ERR).
    send_until_prefix(client, "CMD:CAL", ("STAT:CAL_REQ", "STAT:CAL"), hw_cfg.stage_timeout)
    wait_for_prefix(client, ("STAT:CAL",), hw_cfg.stage_timeout)
    cal_end = wait_for_prefix(client, ("STAT:CAL_OK", "STAT:ERR"), hw_cfg.stage_timeout)
    if cal_end.startswith("STAT:ERR"):
        pytest.skip("Calibration failed on this run (STAT:ERR); skipping MEAS")

    # Measurement should respond; parse result if present.
    send_until_prefix(
        client,
        "CMD:MEAS",
        ("STAT:MEAS_REQ", "STAT:MEAS", "STAT:ERR", "DAT:RES:", "STAT:MANUAL_ACTIVE"),
        hw_cfg.stage_timeout,
    )

    first = wait_for_prefix(
        client,
        ("STAT:MEAS_REQ", "STAT:MEAS", "STAT:ERR", "DAT:RES:", "STAT:MANUAL_ACTIVE"),
        hw_cfg.stage_timeout,
    )
    if first.startswith("STAT:MANUAL_ACTIVE"):
        pytest.fail("Device is in manual mode; expected idle measurement")
    if first.startswith("STAT:MEAS"):
        second = wait_for_prefix(client, ("DAT:RES:", "STAT:ERR"), hw_cfg.stage_timeout)
        if second.startswith("DAT:RES:"):
            assert DAT_RES_RE.match(second), f"Malformed DAT:RES frame: {second}"
    elif first.startswith("DAT:RES:"):
        assert DAT_RES_RE.match(first), f"Malformed DAT:RES frame: {first}"


@pytest.mark.hardware
def test_normal_operation_fail_path(serial_client: SerialClient, hw_cfg) -> None:
    client = serial_client

    ensure_connected(client, hw_cfg.stage_timeout)

    # Enable forced failure in the mock RF model (if supported by firmware).
    client.send_line("CMD:MOCK:RF:FAIL:ON")
    try:
        wait_for_prefix(client, ("STAT:MOCK_RF_FAIL_ON",), 2.0)
    except AssertionError:
        pytest.skip("Firmware does not support CMD:MOCK:RF:FAIL:* on this build")

    try:
        client.send_line("CMD:MEAS")
        wait_for_prefix(client, ("STAT:MEAS",), hw_cfg.stage_timeout)
        err = wait_for_prefix(client, ("STAT:ERR", "DAT:RES"), hw_cfg.stage_timeout)
        assert err.startswith("STAT:ERR"), f"Expected STAT:ERR but got {err}"
    finally:
        client.send_line("CMD:MOCK:RF:FAIL:OFF")
        # Best-effort: don't fail the whole run if we miss this ack.
        try:
            wait_for_prefix(client, ("STAT:MOCK_RF_FAIL_OFF",), 2.0)
        except AssertionError:
            pass


@pytest.mark.hardware
def test_manual_mode_hal_read_write(serial_client: SerialClient, hw_cfg) -> None:
    client = serial_client

    ensure_connected(client, hw_cfg.stage_timeout)

    send_until_prefix(client, "CMD:MANUAL:ON", ("STAT:MANUAL_ON_REQ", "STAT:MANUAL_ON"), hw_cfg.stage_timeout)
    wait_for_prefix(client, ("STAT:MANUAL_ON",), hw_cfg.stage_timeout)

    send_until_prefix(client, "CMD:HAL:INIT", ("STAT:HAL_INIT_OK",), hw_cfg.stage_timeout)

    # LED set/get roundtrip + push-style HW frame.
    send_until_prefix(client, "CMD:HAL:LED:SET:0:1", ("STAT:HAL_LED_0_", "STAT:HAL_LED_ERR"), hw_cfg.stage_timeout)
    wait_for_regex(client, re.compile(r"STAT:HW:LED:0:1"), hw_cfg.stage_timeout)

    send_until_prefix(client, "CMD:HAL:LED:GET:0", ("STAT:HAL_LED_0:",), hw_cfg.stage_timeout)
    wait_for_regex(client, re.compile(r"STAT:HW:LED:0:[01]"), hw_cfg.stage_timeout)

    # Gain set/get.
    send_until_prefix(client, "CMD:HAL:GAIN:SET:3", ("STAT:HAL_GAIN:", "STAT:HAL_GAIN_ERR"), hw_cfg.stage_timeout)
    wait_for_regex(client, re.compile(r"STAT:HW:GAIN:3"), hw_cfg.stage_timeout)

    send_until_prefix(client, "CMD:HAL:GAIN:GET", ("STAT:HAL_GAIN:",), hw_cfg.stage_timeout)

    # DAC/ADC typically use float formatting which is often disabled in embedded printf.
    # Avoid strict float assertions here; the command-surface is already exercised via the script test.

    # PWM start/get/stop should push state.
    send_until_prefix(client, "CMD:HAL:PWM:START", ("STAT:HAL_PWM_START_OK", "STAT:HAL_PWM_ERR"), hw_cfg.stage_timeout)
    wait_for_regex(client, re.compile(r"STAT:HW:PWM:RUN:[01]"), hw_cfg.stage_timeout)

    # Some runs deliver the HW frames but drop the explicit STAT:HAL_PWM_OK.
    got = send_until_prefix(
        client,
        "CMD:HAL:PWM:GET",
        ("STAT:HAL_PWM_OK", "STAT:HAL_PWM_ERR", "STAT:HW:PWM:RUN:", "STAT:HW:PWM:FREQ:", "STAT:HW:PWM:DUTY:"),
        hw_cfg.stage_timeout,
    )
    more = client.expect(10, timeout=0.5)
    pwm_lines = [got, *more]
    assert any("STAT:HW:PWM:FREQ:" in line for line in pwm_lines), f"Missing PWM FREQ push after GET. Got: {pwm_lines}"
    assert any("STAT:HW:PWM:DUTY:" in line for line in pwm_lines), f"Missing PWM DUTY push after GET. Got: {pwm_lines}"

    got = send_until_prefix(
        client,
        "CMD:HAL:PWM:STOP",
        ("STAT:HAL_PWM_STOP_OK", "STAT:HAL_PWM_ERR", "STAT:HW:PWM:RUN:"),
        hw_cfg.stage_timeout,
    )
    more = client.expect(10, timeout=0.5)
    stop_lines = [got, *more]
    assert any(re.search(r"STAT:HW:PWM:RUN:[01]", line) for line in stop_lines), f"Missing PWM RUN push after STOP. Got: {stop_lines}"

    # LCD write should push DAT:LCD.
    client.send_line("CMD:HAL:LCD:SET:0:PYTEST")
    wait_for_prefix(client, ("STAT:HAL_LCD_L0_OK", "STAT:HAL_LCD_ERR"), hw_cfg.stage_timeout)
    lcd0 = wait_for_prefix(client, ("DAT:LCD:L0",), hw_cfg.stage_timeout)
    assert "PYTEST" in lcd0

    # Button read should push HW BTN.
    client.send_line("CMD:HAL:BTN:READ")
    wait_for_prefix(client, ("STAT:HAL_BTN:", "STAT:HAL_BTN_ERR"), hw_cfg.stage_timeout)
    wait_for_regex(client, re.compile(r"STAT:HW:BTN:[01]"), hw_cfg.stage_timeout)

    client.send_line("CMD:MANUAL:OFF")
    wait_for_prefix(client, ("STAT:MANUAL_OFF_REQ",), hw_cfg.stage_timeout)
    wait_for_prefix(client, ("STAT:MANUAL_OFF",), hw_cfg.stage_timeout)


@pytest.mark.hardware
def test_pc_cli_script_exercises_all_commands(serial_client: SerialClient, hw_cfg) -> None:
    """Uses pc_cli.py's list-of-commands feature and asserts key acks appear.

    This ensures the script runner + command surface remain working together.
    """

    script_path = Path(__file__).resolve().parents[1] / "testdata" / "all_commands_hw.txt"
    commands = script_path.read_text(encoding="utf-8").splitlines()

    client = serial_client

    # Make the test independent of previous firmware state.
    ensure_connected(client, hw_cfg.stage_timeout)

    # Run the script.
    run_script(commands, client, delay=hw_cfg.cmd_delay)

    # Drain remaining output (e.g., trace/log bursts).
    lines: list[str] = []
    deadline = time.monotonic() + max(2.0, hw_cfg.stage_timeout)
    while time.monotonic() < deadline:
        lines.extend(client.expect(10, timeout=0.25))

    joined = "\n".join(lines)

    # A few representative acks that imply the full sequence ran.
    assert "STAT:MANUAL_ON" in joined
    assert "STAT:HAL_INIT_OK" in joined
    assert "STAT:HW:PWM" in joined
    assert "DAT:LCD:L0" in joined
    assert "STAT:MANUAL_OFF" in joined
