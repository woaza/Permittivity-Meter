#!/usr/bin/env python3
"""Snow Permittivity Meter host CLI.

Connects to the firmware's USB CDC (or mock loopback) port and emits CMD:* frames
matching the mobile protocol so developers can drive calibration/measurement
flows and inject synthetic button events while viewing responses.
"""
from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Iterable, Optional

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover
    print("pyserial is required: pip install -r tools/requirements.txt", file=sys.stderr)
    raise

# Default baud rate / timeout mirror the firmware's BT/USB transport
DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 0.2
PROMPT = "snow> "

COMMAND_MAP = {
    "conn": "CMD:CONN",
    "cal": "CMD:CAL",
    "meas": "CMD:MEAS",
    "manual-on": "CMD:MANUAL:ON",
    "manual-off": "CMD:MANUAL:OFF",
    "manual_on": "CMD:MANUAL:ON",
    "manual_off": "CMD:MANUAL:OFF",
    "btn-press": "CMD:BTN:PRESS",
    "btn-release": "CMD:BTN:RELEASE",
    "leds": "CMD:LEDS",
    "lcd": "CMD:LCD",
    "log": "CMD:LOG",
    "trace": "CMD:TRACE",
}


@dataclass
class SerialConfig:
    port: str
    baud: int = DEFAULT_BAUD
    timeout: float = DEFAULT_TIMEOUT


class SerialClient:
    def __init__(self, cfg: SerialConfig) -> None:
        self._cfg = cfg
        self._serial = serial.serial_for_url(
            cfg.port,
            baudrate=cfg.baud,
            timeout=cfg.timeout,
            write_timeout=cfg.timeout,
        )
        self._rx_queue: "queue.Queue[str]" = queue.Queue()
        self._stop = threading.Event()
        self._reader = threading.Thread(target=self._rx_worker, daemon=True)
        self._reader.start()

    def close(self) -> None:
        self._stop.set()
        self._reader.join(timeout=0.5)
        self._serial.close()

    def _rx_worker(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except serial.SerialException as exc:  # pragma: no cover
                print(f"[ERR] serial read failed: {exc}")
                break
            if not raw:
                continue
            line = raw.decode("ascii", errors="replace").strip()
            timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            print(f"[RX {timestamp}] {line}")
            self._rx_queue.put(line)

    def send_line(self, line: str) -> None:
        payload = line if line.endswith("\n") else f"{line}\n"
        print(f"[TX] {payload.strip()}")
        self._serial.write(payload.encode("ascii"))
        self._serial.flush()

    def expect(self, count: int, timeout: float = 1.0) -> list[str]:
        lines: list[str] = []
        deadline = time.monotonic() + timeout
        while len(lines) < count and time.monotonic() < deadline:
            try:
                lines.append(self._rx_queue.get(timeout=0.05))
            except queue.Empty:
                continue
        return lines


def run_script(commands: Iterable[str], client: SerialClient, delay: float) -> None:
    for raw in commands:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.lower().startswith("wait"):
            parts = line.split()
            duration = delay
            if len(parts) > 1:
                try:
                    duration = float(parts[1])
                except ValueError:
                    print(f"[WARN] Invalid wait duration '{parts[1]}', using default {delay}s")
            time.sleep(max(0.0, duration))
            continue
        dispatch_command(line, client)
        time.sleep(delay)


def dispatch_command(line: str, client: SerialClient) -> None:
    if line in ("quit", "exit"):
        raise KeyboardInterrupt

    if line.startswith("send "):
        _, payload = line.split(" ", 1)
        client.send_line(payload)
        return

    mapped = COMMAND_MAP.get(line)
    if mapped:
        client.send_line(mapped)
        return

    parts = line.split()
    if not parts:
        return

    cmd = parts[0]
    args = parts[1:]

    if cmd == "hal-init":
        client.send_line("CMD:HAL:INIT")
        return

    if cmd == "hal-led-set" and len(args) == 2:
        led_id, state = args
        client.send_line(f"CMD:HAL:LED:SET:{led_id}:{state}")
        return

    if cmd == "hal-led-get" and len(args) == 1:
        client.send_line(f"CMD:HAL:LED:GET:{args[0]}")
        return

    if cmd == "hal-led-toggle" and len(args) == 1:
        client.send_line(f"CMD:HAL:LED:TOGGLE:{args[0]}")
        return

    if cmd == "hal-adc-read" and len(args) == 0:
        client.send_line("CMD:HAL:ADC:READ")
        return

    if cmd == "hal-adc-raw" and len(args) == 0:
        client.send_line("CMD:HAL:ADC:RAW")
        return

    if cmd == "hal-dac-set" and len(args) == 2:
        ch, voltage = args
        client.send_line(f"CMD:HAL:DAC:SET:{ch}:{voltage}")
        return

    if cmd == "hal-dac-raw" and len(args) == 2:
        ch, raw = args
        client.send_line(f"CMD:HAL:DAC:RAW:{ch}:{raw}")
        return

    if cmd == "hal-gain-set" and len(args) == 1:
        client.send_line(f"CMD:HAL:GAIN:SET:{args[0]}")
        return

    if cmd == "hal-gain-get" and len(args) == 0:
        client.send_line("CMD:HAL:GAIN:GET")
        return

    if cmd == "hal-btn-read" and len(args) == 0:
        client.send_line("CMD:HAL:BTN:READ")
        return

    if cmd == "hal-nina-rst" and len(args) == 1:
        client.send_line(f"CMD:HAL:NINA:RST:{args[0]}")
        return

    if cmd == "hal-nina-stop" and len(args) == 1:
        client.send_line(f"CMD:HAL:NINA:STOP:{args[0]}")
        return

    if cmd == "hal-lcd-set" and len(args) >= 2:
        line_idx = args[0]
        text = " ".join(args[1:])
        client.send_line(f"CMD:HAL:LCD:SET:{line_idx}:{text}")
        return

    if cmd == "hal-pwm-start" and len(args) == 0:
        client.send_line("CMD:HAL:PWM:START")
        return

    if cmd == "hal-pwm-stop" and len(args) == 0:
        client.send_line("CMD:HAL:PWM:STOP")
        return

    if cmd == "hal-pwm-get" and len(args) == 0:
        client.send_line("CMD:HAL:PWM:GET")
        return

    if cmd == "hal-pwm-freq" and len(args) == 1:
        client.send_line(f"CMD:HAL:PWM:FREQ:{args[0]}")
        return

    if cmd == "hal-pwm-duty" and len(args) == 1:
        client.send_line(f"CMD:HAL:PWM:DUTY:{args[0]}")
        return

    print(
        "Unknown command. Supported: "
        + ", ".join(sorted(COMMAND_MAP.keys()))
        + ", hal-init, hal-led-set <id> <0/1>, hal-led-get <id>, hal-led-toggle <id>,"
        + " hal-adc-read, hal-adc-raw, hal-dac-set <ch> <voltage>, hal-dac-raw <ch> <value>,"
        + " hal-gain-set <level>, hal-gain-get, hal-btn-read, hal-nina-rst <0/1>, hal-nina-stop <0/1>,"
        + " hal-lcd-set <0/1> <text>, hal-pwm-start, hal-pwm-stop, hal-pwm-get, hal-pwm-freq <hz>, hal-pwm-duty <0..100>,"
        + " send <RAW>, exit",
    )


def interactive_loop(client: SerialClient) -> None:
    while True:
        try:
            line = input(PROMPT).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not line:
            continue
        try:
            dispatch_command(line, client)
        except KeyboardInterrupt:
            break


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Snow Permittivity Meter USB CLI")
    parser.add_argument(
        "--port",
        default="loop://",
        help="Serial port or pyserial URL (default: loop:// for local testing)",
    )
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate (115200)")
    parser.add_argument(
        "--timeout", type=float, default=DEFAULT_TIMEOUT, help="Serial timeout in seconds"
    )
    parser.add_argument(
        "--script",
        type=argparse.FileType("r"),
        help="Optional command script to run before interactive mode",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=0.25,
        help="Delay between scripted commands (seconds)",
    )
    parser.add_argument(
        "--script-only",
        action="store_true",
        help="Run the provided script and exit without entering interactive mode",
    )
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    cfg = SerialConfig(port=args.port, baud=args.baud, timeout=args.timeout)
    client = SerialClient(cfg)
    try:
        if args.script is not None:
            try:
                run_script(args.script.readlines(), client, args.delay)
            except KeyboardInterrupt:
                return 0
            if args.script_only:
                return 0
        interactive_loop(client)
    finally:
        client.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
