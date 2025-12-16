from __future__ import annotations

import os
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

import pytest

# Ensure `tools/` is importable so tests can reuse pc_cli.SerialClient.
TOOLS_DIR = Path(__file__).resolve().parents[1]
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from pc_cli import DEFAULT_BAUD, DEFAULT_TIMEOUT, SerialClient, SerialConfig  # noqa: E402


def pytest_addoption(parser: pytest.Parser) -> None:
    parser.addoption(
        "--port",
        action="store",
        default=os.environ.get("PERMITTIVITY_METER_PORT", ""),
        help="Serial port (e.g. COM6). Also via env PERMITTIVITY_METER_PORT.",
    )
    parser.addoption(
        "--baud",
        action="store",
        type=int,
        default=int(os.environ.get("PERMITTIVITY_METER_BAUD", str(DEFAULT_BAUD))),
        help="UART baud rate (default 115200). Also via env PERMITTIVITY_METER_BAUD.",
    )
    parser.addoption(
        "--serial-timeout",
        action="store",
        type=float,
        default=float(os.environ.get("PERMITTIVITY_METER_TIMEOUT", str(DEFAULT_TIMEOUT))),
        help="pyserial read timeout seconds.",
    )
    parser.addoption(
        "--cmd-delay",
        action="store",
        type=float,
        default=float(os.environ.get("PERMITTIVITY_METER_CMD_DELAY", "0.25")),
        help="Delay between commands (seconds).", 
    )
    parser.addoption(
        "--stage-timeout",
        action="store",
        type=float,
        default=float(os.environ.get("PERMITTIVITY_METER_STAGE_TIMEOUT", "10.0")),
        help="Timeout for an expected milestone response (seconds).",
    )
    parser.addoption(
        "--reset-wait",
        action="store",
        type=float,
        default=float(os.environ.get("PERMITTIVITY_METER_RESET_WAIT", "0.5")),
        help="Seconds to wait after opening port / reset before first command.",
    )


@dataclass(frozen=True)
class HwTestConfig:
    port: str
    baud: int
    timeout: float
    cmd_delay: float
    stage_timeout: float
    reset_wait: float


@pytest.fixture(scope="session")
def hw_cfg(pytestconfig: pytest.Config) -> HwTestConfig:
    return HwTestConfig(
        port=str(pytestconfig.getoption("--port")),
        baud=int(pytestconfig.getoption("--baud")),
        timeout=float(pytestconfig.getoption("--serial-timeout")),
        cmd_delay=float(pytestconfig.getoption("--cmd-delay")),
        stage_timeout=float(pytestconfig.getoption("--stage-timeout")),
        reset_wait=float(pytestconfig.getoption("--reset-wait")),
    )


@pytest.fixture(scope="function")
def serial_client(hw_cfg: HwTestConfig) -> Iterable[SerialClient]:
    if not hw_cfg.port:
        pytest.skip("Hardware tests skipped: provide --port COMx or set PERMITTIVITY_METER_PORT")

    try:
        client = SerialClient(SerialConfig(port=hw_cfg.port, baud=hw_cfg.baud, timeout=hw_cfg.timeout))
    except Exception as exc:
        pytest.fail(
            f"Could not open serial port {hw_cfg.port}. Close pc_cli/GUI/other serial tools and retry. ({exc})"
        )
    try:
        # Always reset the MCU at the start of each test.
        # Preferred: CMD:RESET (firmware-triggered reset). Fallback: DTR/RTS pulse if wired.
        # NOTE: On this hardware (ST-LINK VCP), it's easy to miss early BOOT frames.
        # Treat STAT:RESETTING as the key proof that the reset command was processed.
        saw_resetting = False
        saw_boot_like = False
        for _attempt in range(3):
            # Drain any noise, then attempt reset.
            _ = client.expect(200, timeout=0.05)
            client.send_line("CMD:RESET")
            client.reset_target()

            # First: wait briefly for the immediate ACK.
            ack_deadline = time.monotonic() + 1.5
            while time.monotonic() < ack_deadline and not saw_resetting:
                for line in client.expect(10, timeout=0.25):
                    if line.startswith("STAT:RESETTING"):
                        saw_resetting = True
                        break

            # Second: best-effort wait for boot-era frames.
            boot_deadline = time.monotonic() + max(3.0, hw_cfg.reset_wait + 2.5)
            while time.monotonic() < boot_deadline and not saw_boot_like:
                for line in client.expect(10, timeout=0.25):
                    if (
                        line.startswith("STAT:BOOT_V2")
                        or line.startswith("STAT:UART_RX:")
                        or line.startswith("STAT:ERR:RCC")
                    ):
                        saw_boot_like = True
                        break

            if saw_resetting:
                break

        if not saw_resetting:
            pytest.fail(
                "Could not reset MCU automatically (did not receive STAT:RESETTING after CMD:RESET). "
                "Please press the MCU RESET button and re-run pytest."
            )

        # Give the board some additional time to settle and print diagnostics.
        time.sleep(max(0.0, hw_cfg.reset_wait))
        # Drain any remaining boot noise so tests start from a clean RX window.
        _ = client.expect(200, timeout=0.25)
        yield client
    finally:
        client.close()
