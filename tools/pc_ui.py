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
    "manual_on": "CMD:MANUAL:ON",
    "manual_off": "CMD:MANUAL:OFF",
    "btn_press": "CMD:BTN:PRESS",
    "btn_release": "CMD:BTN:RELEASE",
    "lcd": "CMD:LCD",
    "leds": "CMD:LEDS",
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
        self.client: Optional[SerialClient] = None
        self.is_manual = False
        self.pwm_running: Optional[int] = None
        self.pwm_freq_hz: Optional[int] = None
        self.pwm_duty: Optional[int] = None
        self.dac_volts = [0.0, 0.0]
        self.btn_pressed: Optional[int] = None
        self._last_slider_tx: dict[int, float] = {0: 0.0, 1: 0.0}


def build_layout() -> list[list[sg.Element]]:
    led_row = [
        sg.Text(
            name,
            key=f"-LED-{name}-",
            size=(8, 1),
            text_color="white",
            background_color=LED_COLORS["off"],
            justification="center",
            relief=sg.RELIEF_SUNKEN,
        )
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

    device_header = [
        [
            sg.Text("Mode:"),
            sg.Text("AUTO", key="-MODE-", size=(8, 1)),
            sg.Button("Manual ON", key="-CMD-MANUAL-ON-"),
            sg.Button("Manual OFF", key="-CMD-MANUAL-OFF-"),
            sg.Text("Excitation:"),
            sg.Text("?", key="-PWM-RUN-", size=(6, 1)),
            sg.Button("Excite ON", key="-CMD-PWM-ON-"),
            sg.Button("Excite OFF", key="-CMD-PWM-OFF-"),
        ]
    ]

    lcd_frame = [
        [sg.Text(" " * 20, key="-LCD0-", size=(24, 1), relief=sg.RELIEF_SUNKEN)],
        [sg.Text(" " * 20, key="-LCD1-", size=(24, 1), relief=sg.RELIEF_SUNKEN)],
    ]

    button_frame = [
        [sg.Button("B1 Press", key="-CMD-BTN-PRESS-"), sg.Button("B1 Release", key="-CMD-BTN-REL-")],
        [sg.Text("Button:"), sg.Text("?", key="-BTN-STATE-", size=(10, 1))],
    ]

    tuning_frame = [
        [
            sg.Text("FRQ_TN (DAC0)"),
            sg.Slider(
                range=(0.0, 3.3),
                resolution=0.01,
                orientation="h",
                size=(30, 15),
                key="-DAC0-",
                enable_events=True,
            ),
            sg.Text("0.00V", key="-DAC0-TXT-", size=(8, 1)),
        ],
        [
            sg.Text("Q_FACT_TN (DAC1)"),
            sg.Slider(
                range=(0.0, 3.3),
                resolution=0.01,
                orientation="h",
                size=(30, 15),
                key="-DAC1-",
                enable_events=True,
            ),
            sg.Text("0.00V", key="-DAC1-TXT-", size=(8, 1)),
        ],
    ]

    serial_frame = [
        [sg.Multiline(size=(70, 12), key="-LOG-", autoscroll=True, disabled=True, reroute_stdout=False)],
    ]

    layout = [
        [sg.Frame("Connection", connection_frame)],
        [sg.Frame("Device", device_header)],
        [sg.Frame("LEDs", [[*led_row]]), sg.Frame("LCD", lcd_frame)],
        [sg.Frame("Button", button_frame), sg.Frame("Tuning", tuning_frame)],
        [sg.Frame("Serial", serial_frame)],
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


def parse_hw_report(line: str, state: AppState, window: sg.Window) -> None:
    # STAT:HW:<...>
    parts = line.split(":")
    if len(parts) < 3:
        return

    # LED updates: STAT:HW:LED:<id>:<0/1>
    if len(parts) >= 5 and parts[2] == "LED":
        try:
            led_id = int(parts[3])
            level = int(parts[4])
        except ValueError:
            return
        mapping = {0: "STATUS", 1: "MEAS", 2: "EXCITE", 3: "ERROR"}
        key = mapping.get(led_id)
        if key is None:
            return
        state.last_leds[key] = 1 if level else 0
        color = LED_COLORS["error"] if key == "ERROR" and level else (LED_COLORS["on"] if level else LED_COLORS["off"])
        window[f"-LED-{key}-"].update(key, background_color=color)
        return

    # DAC: STAT:HW:DAC:<ch>:V:<volts>
    if len(parts) >= 6 and parts[2] == "DAC":
        try:
            ch = int(parts[3])
        except ValueError:
            return
        if parts[4] == "V":
            try:
                volts = float(parts[5])
            except ValueError:
                return
            if 0 <= ch <= 1:
                state.dac_volts[ch] = volts
                window[f"-DAC{ch}-"].update(volts)
                window[f"-DAC{ch}-TXT-"].update(f"{volts:.2f}V")
        return

    # PWM: STAT:HW:PWM:RUN:<0/1> / FREQ / DUTY
    if len(parts) >= 5 and parts[2] == "PWM":
        field = parts[3]
        try:
            value = int(parts[4])
        except ValueError:
            return
        if field == "RUN":
            state.pwm_running = value
            window["-PWM-RUN-"].update("ON" if value else "OFF")
        elif field == "FREQ":
            state.pwm_freq_hz = value
        elif field == "DUTY":
            state.pwm_duty = value
        return

    # Button: STAT:HW:BTN:<0/1>
    if len(parts) >= 4 and parts[2] == "BTN":
        try:
            pressed = int(parts[3])
        except ValueError:
            return
        state.btn_pressed = pressed
        window["-BTN-STATE-"].update("PRESSED" if pressed else "RELEASED")
        return


def append_log(window: sg.Window, message: str) -> None:
    existing = window["-LOG-"].get()
    text = f"[{timestamp()}] {message}\n"
    window["-LOG-"].update(existing + text)


def handle_line(line: str, state: AppState, window: sg.Window) -> None:
    append_log(window, line)
    if line.startswith("STAT:LED"):
        parse_led_report(line, state, window)
    elif line.startswith("STAT:HW:"):
        parse_hw_report(line, state, window)
    elif line.startswith("DAT:LCD"):
        parse_lcd_line(line, state, window)
    elif line == "STAT:MANUAL_ON":
        state.is_manual = True
        window["-MODE-"].update("MANUAL")
    elif line == "STAT:MANUAL_OFF":
        state.is_manual = False
        window["-MODE-"].update("AUTO")
    elif line == "STAT:RDY":
        # One-time snapshot after connect; no periodic polling.
        send_command(state, CMD["leds"])
        send_command(state, CMD["lcd"])


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
            window["-STATUS-"].update(f"Status: Connected to {port}")
            send_command(state, CMD["connect"])
            append_log(window, f"Connected on {port}")
        elif event == "-DISCONNECT-":
            if state.client:
                state.client.close()
                state.client = None
            window["-STATUS-"].update("Status: Disconnected")
            append_log(window, "Disconnected")
        elif event == "-CMD-MANUAL-ON-":
            send_command(state, CMD["manual_on"])
        elif event == "-CMD-MANUAL-OFF-":
            send_command(state, CMD["manual_off"])
        elif event == "-CMD-BTN-PRESS-":
            send_command(state, CMD["btn_press"])
        elif event == "-CMD-BTN-REL-":
            send_command(state, CMD["btn_release"])

        elif event == "-CMD-PWM-ON-":
            send_command(state, "CMD:HAL:PWM:START")
        elif event == "-CMD-PWM-OFF-":
            send_command(state, "CMD:HAL:PWM:STOP")

        elif event in ("-DAC0-", "-DAC1-"):
            # Debounce slider traffic; still no periodic polling.
            if state.client is None:
                continue
            ch = 0 if event == "-DAC0-" else 1
            try:
                volts = float(values[event])
            except (TypeError, ValueError):
                continue
            window[f"-DAC{ch}-TXT-"].update(f"{volts:.2f}V")
            now = time.monotonic()
            if now - state._last_slider_tx[ch] < 0.15:
                continue
            state._last_slider_tx[ch] = now
            send_command(state, f"CMD:HAL:DAC:SET:{ch}:{volts:.2f}")

        if state.client:
            for line in state.client.poll():
                handle_line(line, state, window)

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
