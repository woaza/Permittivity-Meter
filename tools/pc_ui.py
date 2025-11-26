#!/usr/bin/env python3
"""Snow Permittivity Meter desktop UI.

Provides a simple PySimpleGUI front-end for the ASCII CMD:* protocol so
engineers can drive calibration/measurement cycles, observe LED/LCD state,
and view RF trace data (ADC/DAC) without hand-typing commands. Also exposes
basic reset and raw send actions to mimic the Bluetooth transport.
"""
from __future__ import annotations

import argparse
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Optional

try:
    import PySimpleGUI as sg  # type: ignore
except ImportError:  # pragma: no cover
    print("PySimpleGUI is required: pip install -r tools/requirements.txt", file=sys.stderr)
    raise

try:
    import serial  # type: ignore
except ImportError as exc:  # pragma: no cover
    print("pyserial is required: pip install -r tools/requirements.txt", file=sys.stderr)
    raise

DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT = 0.2

LED_KEYS = ["STATUS", "MEAS", "EXCITE", "ERROR"]
LED_COLORS = {
    "on": "#00c853",
    "off": "#616161",
    "error": "#d50000",
}

CMD = {
    "connect": "CMD:CONN",
    "cal": "CMD:CAL",
    "meas": "CMD:MEAS",
    "btn_press": "CMD:BTN:PRESS",
    "btn_release": "CMD:BTN:RELEASE",
    "lcd": "CMD:LCD",
    "leds": "CMD:LEDS",
    "log": "CMD:LOG",
    "trace": "CMD:TRACE",
}

AUTO_REFRESH_INTERVALS = {
    "leds": 0.75,
    "lcd": 1.5,
    "log": 2.5,
}


def timestamp() -> str:
    return datetime.now().strftime("%H:%M:%S")


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
        self._queue: "queue.Queue[str]" = queue.Queue()
        self._stop = threading.Event()
        self._reader = threading.Thread(target=self._rx_worker, daemon=True)
        self._reader.start()

    def close(self) -> None:
        self._stop.set()
        if self._reader.is_alive():
            self._reader.join(timeout=0.5)
        self._serial.close()

    def _rx_worker(self) -> None:
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except serial.SerialException as exc:  # pragma: no cover
                self._queue.put(f"[ERR] serial read failed: {exc}")
                break
            if not raw:
                continue
            line = raw.decode("ascii", errors="replace").strip()
            self._queue.put(line)

    def send_line(self, line: str) -> None:
        payload = line if line.endswith("\n") else f"{line}\n"
        self._serial.write(payload.encode("ascii"))
        self._serial.flush()

    def poll(self) -> list[str]:
        lines: list[str] = []
        while True:
            try:
                lines.append(self._queue.get_nowait())
            except queue.Empty:
                break
        return lines


class AppState:
    def __init__(self) -> None:
        self.last_lcd = ["", ""]
        self.last_leds = {key: 0 for key in LED_KEYS}
        self.last_measure = "N/A"
        self.last_trace = "No samples"
        self.client: Optional[SerialClient] = None
        self.trace_min: Optional[tuple[float, float]] = None
        self.next_refresh: dict[str, float] = {name: 0.0 for name in AUTO_REFRESH_INTERVALS}

    def reset_measurements(self) -> None:
        self.last_measure = "N/A"
        self.last_trace = "No samples"
        self.trace_min = None

    def force_refresh(self) -> None:
        now = time.monotonic()
        for key in self.next_refresh:
            self.next_refresh[key] = now

    def defer_refresh(self, name: str) -> None:
        if name not in AUTO_REFRESH_INTERVALS:
            return
        self.next_refresh[name] = time.monotonic() + AUTO_REFRESH_INTERVALS[name]


def build_layout() -> list[list[sg.Element]]:
    led_row = [
        sg.Text(key=f"-LED-{name}-", size=(8, 1), text_color="white", background_color=LED_COLORS["off"],
                justification="center", relief=sg.RELIEF_SUNKEN)
        for name in LED_KEYS
    ]

    connection_frame = [
        [
            sg.Text("Port"),
            sg.Input(default_text="COM4", size=(10, 1), key="-PORT-"),
            sg.Text("Baud"),
            sg.Input(default_text=str(DEFAULT_BAUD), size=(8, 1), key="-BAUD-"),
            sg.Button("Connect", key="-CONNECT-", button_color=("white", "green")),
            sg.Button("Disconnect", key="-DISCONNECT-", button_color=("white", "firebrick")),
            sg.Text("Status: Disconnected", key="-STATUS-"),
        ]
    ]

    control_frame = [
        [sg.Button("Handshake", key="-CMD-CONN-"), sg.Button("Calibrate", key="-CMD-CAL-"),
         sg.Button("Measure", key="-CMD-MEAS-"), sg.Button("Trace", key="-CMD-TRACE-")],
        [sg.Button("Refresh LCD", key="-CMD-LCD-"), sg.Button("Refresh LEDs", key="-CMD-LEDS-"),
         sg.Button("Pull Logs", key="-CMD-LOG-"), sg.Button("Reset Device", key="-CMD-RESET-")],
        [sg.Button("Button Press", key="-CMD-BTN-PRESS-"), sg.Button("Button Release", key="-CMD-BTN-REL-")],
    ]

    lcd_frame = [
        [sg.Text("LCD Line 0:"), sg.Text("", size=(40, 1), key="-LCD0-")],
        [sg.Text("LCD Line 1:"), sg.Text("", size=(40, 1), key="-LCD1-")],
    ]

    bt_frame = [
        [sg.Multiline(size=(70, 15), key="-LOG-", autoscroll=True, disabled=True, reroute_stdout=False)],
        [sg.Input(key="-SEND-INPUT-", expand_x=True), sg.Button("Send Raw", key="-SEND-RAW-")],
    ]

    adc_frame = [
        [sg.Text("Last Measurement:"), sg.Text("N/A", key="-MEAS-RESULT-", size=(45, 1))],
        [sg.Text("Trace Summary:"), sg.Text("No samples", key="-TRACE-SUMMARY-", size=(45, 2))],
    ]

    layout = [
        [sg.Frame("Connection", connection_frame)],
        [sg.Frame("LEDs", [[*led_row]])],
        [sg.Frame("Controls", control_frame)],
        [sg.Frame("LCD", lcd_frame), sg.Frame("ADC / DAC", adc_frame)],
        [sg.Frame("Bluetooth Feed", bt_frame)],
    ]
    return layout


def parse_led_report(line: str, state: AppState, window: sg.Window) -> None:
    parts = line.split(":")
    # STAT:LED:S:1:M:0:E:0:R:0
    if len(parts) < 10:
        return
    mapping = {
        "STATUS": int(parts[3]),
        "MEAS": int(parts[5]),
        "EXCITE": int(parts[7]),
        "ERROR": int(parts[9]),
    }
    state.last_leds.update(mapping)
    for key, value in mapping.items():
        color = LED_COLORS["error"] if key == "ERROR" and value else (LED_COLORS["on"] if value else LED_COLORS["off"])
        window[f"-LED-{key}-"].update(key.capitalize(), background_color=color)


def parse_lcd_line(line: str, state: AppState, window: sg.Window) -> None:
    # DAT:LCD:L0:TEXT
    parts = line.split(":", 3)
    if len(parts) != 4:
        return
    idx = 0 if parts[2] == "L0" else 1
    state.last_lcd[idx] = parts[3]
    window[f"-LCD{idx}-"].update(parts[3])


def parse_measurement(line: str, state: AppState, window: sg.Window) -> None:
    # DAT:RES:ER:<val>:EI:<val>:DENS:<val>
    state.last_measure = line.replace("DAT:RES:", "")
    window["-MEAS-RESULT-"].update(state.last_measure)


def parse_trace(line: str, state: AppState, window: sg.Window) -> None:
    # DAT:TRACE:MODE:IDX:V:<val>:A:<val>
    try:
        parts = line.split(":")
        idx = int(parts[3])
        voltage = float(parts[5])
        amplitude = float(parts[7])
    except (ValueError, IndexError):
        return
    # Track extrema as confirmation of DAC/ADC health
    if idx == 0 or state.trace_min is None:
        state.trace_min = (amplitude, voltage)
    else:
        min_amp, min_v = state.trace_min
        if amplitude < min_amp:
            state.trace_min = (amplitude, voltage)
    min_amp, min_v = state.trace_min
    summary = f"Min A={min_amp:.4f} @ V={min_v:.4f}"
    state.last_trace = summary
    window["-TRACE-SUMMARY-"].update(summary)


def append_log(window: sg.Window, message: str) -> None:
    existing = window["-LOG-"].get()
    text = f"[{timestamp()}] {message}\n"
    window["-LOG-"].update(existing + text)


def handle_line(line: str, state: AppState, window: sg.Window) -> None:
    append_log(window, line)
    if line.startswith("STAT:LED"):
        parse_led_report(line, state, window)
    elif line.startswith("DAT:LCD"):
        parse_lcd_line(line, state, window)
    elif line.startswith("DAT:RES"):
        parse_measurement(line, state, window)
    elif line.startswith("DAT:TRACE"):
        parse_trace(line, state, window)


def send_command(state: AppState, cmd: str) -> None:
    if state.client is None:
        return
    state.client.send_line(cmd)


def run_ui(args: argparse.Namespace) -> int:
    sg.theme("DarkBlue3")
    window = sg.Window("Snow Permittivity Meter", build_layout(), finalize=True)
    state = AppState()
    window["-PORT-"].update(args.port)
    window["-BAUD-"].update(str(args.baud))

    while True:
        event, values = window.read(timeout=150)
        if event in (sg.WIN_CLOSED, "Exit"):
            break
        if event == "-CONNECT-":
            port = values["-PORT-"]
            baud = int(values["-BAUD-"])
            try:
                client = SerialClient(SerialConfig(port=port, baud=baud))
            except Exception as exc:  # pragma: no cover
                append_log(window, f"[ERR] Failed to open port: {exc}")
                continue
            state.client = client
            state.force_refresh()
            window["-STATUS-"].update(f"Status: Connected to {port}")
            send_command(state, CMD["connect"])
            append_log(window, f"Connected on {port}")
        elif event == "-DISCONNECT-":
            if state.client:
                state.client.close()
                state.client = None
            window["-STATUS-"].update("Status: Disconnected")
            append_log(window, "Disconnected")
        elif event == "-CMD-CONN-":
            send_command(state, CMD["connect"])
        elif event == "-CMD-CAL-":
            send_command(state, CMD["cal"])
        elif event == "-CMD-MEAS-":
            send_command(state, CMD["meas"])
        elif event == "-CMD-TRACE-":
            state.reset_measurements()
            window["-MEAS-RESULT-"].update(state.last_measure)
            window["-TRACE-SUMMARY-"].update(state.last_trace)
            send_command(state, CMD["trace"])
        elif event == "-CMD-LCD-":
            send_command(state, CMD["lcd"])
            state.defer_refresh("lcd")
        elif event == "-CMD-LEDS-":
            send_command(state, CMD["leds"])
            state.defer_refresh("leds")
        elif event == "-CMD-LOG-":
            send_command(state, CMD["log"])
            state.defer_refresh("log")
        elif event == "-CMD-RESET-":
            append_log(window, "Issuing logical reset via CMD:CONN")
            send_command(state, CMD["connect"])
        elif event == "-CMD-BTN-PRESS-":
            send_command(state, CMD["btn_press"])
        elif event == "-CMD-BTN-REL-":
            send_command(state, CMD["btn_release"])
        elif event == "-SEND-RAW-":
            raw = values["-SEND-INPUT-"]
            if raw:
                send_command(state, raw)
                append_log(window, f"[TX] {raw}")
                window["-SEND-INPUT-"].update("")

        if state.client:
            for line in state.client.poll():
                handle_line(line, state, window)

            now = time.monotonic()
            for name, interval in AUTO_REFRESH_INTERVALS.items():
                if now >= state.next_refresh[name]:
                    send_command(state, CMD[name])
                    state.next_refresh[name] = now + interval

    if state.client:
        state.client.close()
    window.close()
    return 0


def parse_args(argv: Optional[list[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Snow Permittivity Meter UI")
    parser.add_argument("--port", default="COM4", help="Serial port or pyserial URL")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Baud rate")
    return parser.parse_args(argv)


def main(argv: Optional[list[str]] = None) -> int:
    args = parse_args(argv)
    return run_ui(args)


if __name__ == "__main__":
    sys.exit(main())
