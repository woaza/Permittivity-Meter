#!/usr/bin/env python3
"""Snow Permittivity Meter desktop UI.

PySimpleGUI front-end for the ASCII `CMD:*` protocol.

Design goals for bring-up on the real device:
- Clear sections for different hardware blocks (LEDs, DAC/ADC, PWM, NINA, LCD).
- Toggles for Manual Mode ("Handbetrieb") and RF mock controls.
- Full "read and set" surface for available HAL commands.
- Works with either push-style `STAT:HW:*` frames or optional polling.
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
LED_ID_TO_KEY = {0: "STATUS", 1: "MEAS", 2: "EXCITE", 3: "ERROR"}
LED_KEY_TO_ID = {v: k for k, v in LED_ID_TO_KEY.items()}
LED_COLORS = {
    "on": "#00c853",
    "off": "#616161",
    "error": "#d50000",
}

CMD = {
    "connect": "CMD:CONN",
    "reset": "CMD:RESET",
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
        self.rx_mode: str = "?"
        self.last_reset_cause: str = "?"
        self.boot_seen: bool = False

        self.adc_volts: Optional[float] = None
        self.adc_raw: Optional[int] = None
        self.gain_level: Optional[int] = None
        self.nina_rst: Optional[int] = None
        self.nina_stop: Optional[int] = None

        self.pwm_running: Optional[int] = None
        self.pwm_freq_hz: Optional[int] = None
        self.pwm_duty: Optional[int] = None
        self.dac_volts = [0.0, 0.0]
        self.btn_pressed: Optional[int] = None
        self._last_slider_tx: dict[int, float] = {0: 0.0, 1: 0.0}
        self._last_poll_s: float = 0.0


def _led_color(key: str, on: int) -> str:
    if key == "ERROR" and on:
        return LED_COLORS["error"]
    return LED_COLORS["on"] if on else LED_COLORS["off"]


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
            sg.Button("Reset", key="-CMD-RESET-"),
            sg.Text("Status: Disconnected", key="-STATUS-"),
        ]
    ]

    mode_frame = [
        [
            sg.Text("RX:"),
            sg.Text("?", key="-RX-MODE-", size=(10, 1), relief=sg.RELIEF_SUNKEN),
            sg.Text("Reset cause:"),
            sg.Text("?", key="-RESET-CAUSE-", size=(18, 1), relief=sg.RELIEF_SUNKEN),
            sg.Text("Boot:"),
            sg.Text("?", key="-BOOT-", size=(8, 1), relief=sg.RELIEF_SUNKEN),
        ],
        [
            sg.Text("Mode:"),
            sg.Text("AUTO", key="-MODE-", size=(8, 1), relief=sg.RELIEF_SUNKEN),
            sg.Checkbox("Manual mode (Handbetrieb)", key="-MANUAL-TOGGLE-", enable_events=True),
            sg.Button("HAL init", key="-CMD-HAL-INIT-"),
            sg.Button("Refresh", key="-CMD-REFRESH-"),
            sg.Checkbox("Auto refresh", key="-AUTO-REFRESH-", default=False, enable_events=True),
            sg.Text("every"),
            sg.Input(default_text="1.0", size=(5, 1), key="-AUTO-REFRESH-S-"),
            sg.Text("s"),
        ],
    ]

    rf_mock_frame = [
        [
            sg.Text("RF mock"),
            sg.Checkbox("Fail", key="-MOCK-FAIL-", enable_events=True),
            sg.Text("Res (V)"),
            sg.Input(default_text="1.20", size=(6, 1), key="-MOCK-RES-"),
            sg.Text("Noise"),
            sg.Input(default_text="0.01", size=(6, 1), key="-MOCK-NOISE-"),
            sg.Text("Base"),
            sg.Input(default_text="1.00", size=(6, 1), key="-MOCK-BASE-"),
            sg.Button("Apply", key="-MOCK-APPLY-"),
        ]
    ]

    leds_control = []
    for name in LED_KEYS:
        led_id = LED_KEY_TO_ID[name]
        leds_control.append(
            [
                sg.Text(f"{name} (id {led_id})", size=(14, 1)),
                sg.Button("ON", key=f"-HAL-LED-ON-{led_id}-", size=(5, 1)),
                sg.Button("OFF", key=f"-HAL-LED-OFF-{led_id}-", size=(5, 1)),
                sg.Button("TOG", key=f"-HAL-LED-TOG-{led_id}-", size=(5, 1)),
                sg.Button("GET", key=f"-HAL-LED-GET-{led_id}-", size=(5, 1)),
            ]
        )

    lcd_frame = [
        [sg.Text(" " * 20, key="-LCD0-", size=(28, 1), relief=sg.RELIEF_SUNKEN)],
        [sg.Text(" " * 20, key="-LCD1-", size=(28, 1), relief=sg.RELIEF_SUNKEN)],
        [
            sg.Text("Set L0"),
            sg.Input(size=(24, 1), key="-LCD0-SET-"),
            sg.Button("Apply", key="-HAL-LCD-SET-0-"),
        ],
        [
            sg.Text("Set L1"),
            sg.Input(size=(24, 1), key="-LCD1-SET-"),
            sg.Button("Apply", key="-HAL-LCD-SET-1-"),
        ],
        [sg.Button("LCD snapshot", key="-CMD-LCD-SNAPSHOT-"), sg.Button("LED snapshot", key="-CMD-LEDS-SNAPSHOT-")],
    ]

    adc_frame = [
        [sg.Text("ADC volts:"), sg.Text("?", key="-ADC-V-", size=(12, 1), relief=sg.RELIEF_SUNKEN)],
        [sg.Text("ADC raw:"), sg.Text("?", key="-ADC-RAW-", size=(12, 1), relief=sg.RELIEF_SUNKEN)],
        [sg.Button("Read V", key="-HAL-ADC-READ-"), sg.Button("Read RAW", key="-HAL-ADC-RAW-")],
    ]

    dac_frame = [
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
            sg.Text("RAW"),
            sg.Input(size=(6, 1), key="-DAC0-RAW-"),
            sg.Button("Set", key="-HAL-DAC-RAW-0-")
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
            sg.Text("RAW"),
            sg.Input(size=(6, 1), key="-DAC1-RAW-"),
            sg.Button("Set", key="-HAL-DAC-RAW-1-")
        ],
    ]

    gain_frame = [
        [sg.Text("Gain:"), sg.Text("?", key="-GAIN-", size=(6, 1), relief=sg.RELIEF_SUNKEN)],
        [sg.Text("Set"), sg.Input(size=(4, 1), key="-GAIN-SET-"), sg.Button("Apply", key="-HAL-GAIN-SET-"), sg.Button("GET", key="-HAL-GAIN-GET-")],
    ]

    pwm_frame = [
        [
            sg.Text("PWM run:"),
            sg.Text("?", key="-PWM-RUN-", size=(6, 1), relief=sg.RELIEF_SUNKEN),
            sg.Text("freq:"),
            sg.Text("?", key="-PWM-FREQ-", size=(8, 1), relief=sg.RELIEF_SUNKEN),
            sg.Text("duty:"),
            sg.Text("?", key="-PWM-DUTY-", size=(6, 1), relief=sg.RELIEF_SUNKEN),
        ],
        [
            sg.Button("START", key="-HAL-PWM-START-"),
            sg.Button("STOP", key="-HAL-PWM-STOP-"),
            sg.Button("GET", key="-HAL-PWM-GET-"),
            sg.Text("FREQ"),
            sg.Input(size=(8, 1), key="-PWM-FREQ-SET-"),
            sg.Button("Apply", key="-HAL-PWM-FREQ-"),
            sg.Text("DUTY"),
            sg.Input(size=(5, 1), key="-PWM-DUTY-SET-"),
            sg.Button("Apply", key="-HAL-PWM-DUTY-"),
        ],
    ]

    nina_frame = [
        [
            sg.Text("NINA RST:"),
            sg.Text("?", key="-NINA-RST-", size=(6, 1), relief=sg.RELIEF_SUNKEN),
            sg.Text("STOP:"),
            sg.Text("?", key="-NINA-STOP-", size=(6, 1), relief=sg.RELIEF_SUNKEN),
        ],
        [
            sg.Checkbox("RST=1 (run)", key="-NINA-RST-SET-", enable_events=True),
            sg.Checkbox("STOP=1", key="-NINA-STOP-SET-", enable_events=True),
        ],
    ]

    button_frame = [
        [
            sg.Button("B1 Press (sim)", key="-CMD-BTN-PRESS-"),
            sg.Button("B1 Release (sim)", key="-CMD-BTN-REL-"),
            sg.Button("Read HW button", key="-HAL-BTN-READ-"),
        ],
        [sg.Text("Button:"), sg.Text("?", key="-BTN-STATE-", size=(10, 1), relief=sg.RELIEF_SUNKEN)],
    ]

    serial_frame = [
        [sg.Multiline(size=(95, 14), key="-LOG-", autoscroll=True, disabled=True, reroute_stdout=False)],
        [sg.Text("Raw"), sg.Input(size=(70, 1), key="-RAW-"), sg.Button("Send", key="-RAW-SEND-")],
        [sg.Text("Last status:"), sg.Text("", key="-LAST-STATUS-", size=(70, 1), relief=sg.RELIEF_SUNKEN)],
    ]

    layout = [
        [sg.Frame("Connection", connection_frame, expand_x=True)],
        [sg.Frame("Mode / Polling", mode_frame, expand_x=True)],
        [sg.Frame("RF / FSM Mock", rf_mock_frame, expand_x=True)],
        [
            sg.Frame("LEDs (indicator)", [[*led_row]]),
            sg.Frame("LEDs (HAL control, requires manual mode)", leds_control),
        ],
        [
            sg.Frame("LCD", lcd_frame),
            sg.Frame("Buttons", button_frame),
        ],
        [
            sg.Frame("DAC", dac_frame),
            sg.Frame("ADC", adc_frame),
        ],
        [
            sg.Frame("PWM / Excitation", pwm_frame),
            sg.Frame("RF Gain", gain_frame),
            sg.Frame("NINA", nina_frame),
        ],
        [sg.Frame("Serial", serial_frame, expand_x=True)],
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
        window[f"-LED-{key}-"].update(key, background_color=_led_color(key, value))


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
        key = LED_ID_TO_KEY.get(led_id)
        if key is None:
            return
        state.last_leds[key] = 1 if level else 0
        window[f"-LED-{key}-"].update(key, background_color=_led_color(key, 1 if level else 0))
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
            window["-PWM-FREQ-"].update(str(value))
        elif field == "DUTY":
            state.pwm_duty = value
            window["-PWM-DUTY-"].update(str(value))
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

    # ADC: STAT:HW:ADC:V:<volts>  / STAT:HW:ADC:RAW:<value>
    if len(parts) >= 5 and parts[2] == "ADC":
        field = parts[3]
        if field == "V":
            try:
                v = float(parts[4])
            except ValueError:
                return
            state.adc_volts = v
            window["-ADC-V-"].update(f"{v:.3f}V")
            return
        if field == "RAW":
            try:
                raw = int(parts[4])
            except ValueError:
                return
            state.adc_raw = raw
            window["-ADC-RAW-"].update(str(raw))
            return

    # Gain: STAT:HW:GAIN:<level>
    if len(parts) >= 4 and parts[2] == "GAIN":
        try:
            level = int(parts[3])
        except ValueError:
            return
        state.gain_level = level
        window["-GAIN-"].update(str(level))
        return

    # NINA: STAT:HW:NINA:RST:<0/1>  / STOP
    if len(parts) >= 5 and parts[2] == "NINA":
        field = parts[3]
        try:
            value = int(parts[4])
        except ValueError:
            return
        if field == "RST":
            state.nina_rst = value
            window["-NINA-RST-"].update(str(value))
            window["-NINA-RST-SET-"].update(value=bool(value))
        elif field == "STOP":
            state.nina_stop = value
            window["-NINA-STOP-"].update(str(value))
            window["-NINA-STOP-SET-"].update(value=bool(value))
        return


def append_log(window: sg.Window, message: str) -> None:
    existing = window["-LOG-"].get()
    text = f"[{timestamp()}] {message}\n"
    window["-LOG-"].update(existing + text)


def set_last_status(window: sg.Window, message: str) -> None:
    window["-LAST-STATUS-"].update(message)


def handle_line(line: str, state: AppState, window: sg.Window) -> None:
    append_log(window, line)
    if line.startswith("STAT:LED"):
        parse_led_report(line, state, window)
    elif line.startswith("STAT:HW:"):
        parse_hw_report(line, state, window)
    elif line.startswith("DAT:LCD"):
        parse_lcd_line(line, state, window)
    elif line.startswith("STAT:UART_RX:"):
        state.rx_mode = line.split(":", 2)[2]
        window["-RX-MODE-"] .update(state.rx_mode)
    elif line.startswith("STAT:RESET_CAUSE:"):
        state.last_reset_cause = line.split(":", 2)[2]
        window["-RESET-CAUSE-"].update(state.last_reset_cause)
    elif line == "STAT:BOOT_V2":
        state.boot_seen = True
        window["-BOOT-"] .update("BOOT_V2")
    elif line in ("STAT:MANUAL_ON", "STAT:MANUAL", "STAT:MANUAL_ACTIVE"):
        state.is_manual = True
        window["-MODE-"].update("MANUAL")
        window["-MANUAL-TOGGLE-"].update(value=True)
    elif line == "STAT:MANUAL_OFF":
        state.is_manual = False
        window["-MODE-"].update("AUTO")
        window["-MANUAL-TOGGLE-"].update(value=False)
    elif line in ("STAT:HAL_LOCKED", "STAT:HAL_CMD_ERR", "STAT:HAL_PWM_ERR", "STAT:HAL_LED_ERR", "STAT:HAL_ADC_ERR", "STAT:HAL_DAC_ERR", "STAT:HAL_GAIN_ERR", "STAT:HAL_NINA_ERR", "STAT:HAL_LCD_ERR", "STAT:MANUAL_NOT_ACTIVE"):
        set_last_status(window, line)
    elif line.startswith("STAT:"):
        # Display any other STAT frames in the status bar.
        set_last_status(window, line)

    if line in ("STAT:RDY", "STAT:MANUAL"):
        # Snapshot after connect/handshake.
        send_command(state, CMD["leds"])
        send_command(state, CMD["lcd"])


def send_command(state: AppState, cmd: str) -> None:
    if state.client is None:
        return
    state.client.send_line(cmd)


def send_refresh(state: AppState) -> None:
    """Poll the current status. HAL reads require manual mode."""
    send_command(state, CMD["leds"])
    send_command(state, CMD["lcd"])
    # Only query HAL-side getters when manual mode is active.
    if state.is_manual:
        for led_id in range(4):
            send_command(state, f"CMD:HAL:LED:GET:{led_id}")
        send_command(state, "CMD:HAL:ADC:READ")
        send_command(state, "CMD:HAL:ADC:RAW")
        send_command(state, "CMD:HAL:GAIN:GET")
        send_command(state, "CMD:HAL:BTN:READ")
        send_command(state, "CMD:HAL:PWM:GET")


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
        elif event == "-CMD-RESET-":
            send_command(state, CMD["reset"])
        elif event == "-MANUAL-TOGGLE-":
            if bool(values.get("-MANUAL-TOGGLE-")):
                send_command(state, CMD["manual_on"])
            else:
                send_command(state, CMD["manual_off"])
        elif event == "-CMD-HAL-INIT-":
            send_command(state, "CMD:HAL:INIT")
        elif event == "-CMD-REFRESH-":
            send_refresh(state)
        elif event == "-CMD-BTN-PRESS-":
            send_command(state, CMD["btn_press"])
        elif event == "-CMD-BTN-REL-":
            send_command(state, CMD["btn_release"])

        elif event == "-HAL-ADC-READ-":
            send_command(state, "CMD:HAL:ADC:READ")
        elif event == "-HAL-ADC-RAW-":
            send_command(state, "CMD:HAL:ADC:RAW")
        elif event == "-HAL-GAIN-GET-":
            send_command(state, "CMD:HAL:GAIN:GET")
        elif event == "-HAL-GAIN-SET-":
            text = str(values.get("-GAIN-SET-", "")).strip()
            if text:
                send_command(state, f"CMD:HAL:GAIN:SET:{text}")
        elif event == "-HAL-BTN-READ-":
            send_command(state, "CMD:HAL:BTN:READ")

        elif event == "-HAL-PWM-START-":
            send_command(state, "CMD:HAL:PWM:START")
        elif event == "-HAL-PWM-STOP-":
            send_command(state, "CMD:HAL:PWM:STOP")
        elif event == "-HAL-PWM-GET-":
            send_command(state, "CMD:HAL:PWM:GET")
        elif event == "-HAL-PWM-FREQ-":
            text = str(values.get("-PWM-FREQ-SET-", "")).strip()
            if text:
                send_command(state, f"CMD:HAL:PWM:FREQ:{text}")
        elif event == "-HAL-PWM-DUTY-":
            text = str(values.get("-PWM-DUTY-SET-", "")).strip()
            if text:
                send_command(state, f"CMD:HAL:PWM:DUTY:{text}")

        elif event == "-CMD-LCD-SNAPSHOT-":
            send_command(state, CMD["lcd"])
        elif event == "-CMD-LEDS-SNAPSHOT-":
            send_command(state, CMD["leds"])
        elif event == "-HAL-LCD-SET-0-":
            text = str(values.get("-LCD0-SET-", ""))
            send_command(state, f"CMD:HAL:LCD:SET:0:{text}")
        elif event == "-HAL-LCD-SET-1-":
            text = str(values.get("-LCD1-SET-", ""))
            send_command(state, f"CMD:HAL:LCD:SET:1:{text}")

        elif event == "-NINA-RST-SET-":
            send_command(state, f"CMD:HAL:NINA:RST:{1 if bool(values.get('-NINA-RST-SET-')) else 0}")
        elif event == "-NINA-STOP-SET-":
            send_command(state, f"CMD:HAL:NINA:STOP:{1 if bool(values.get('-NINA-STOP-SET-')) else 0}")

        elif event == "-RAW-SEND-":
            raw = str(values.get("-RAW-", "")).strip()
            if raw:
                send_command(state, raw)

        elif event == "-MOCK-FAIL-":
            send_command(state, f"CMD:MOCK:RF:FAIL:{'ON' if bool(values.get('-MOCK-FAIL-')) else 'OFF'}")
        elif event == "-MOCK-APPLY-":
            res = str(values.get("-MOCK-RES-", "")).strip()
            noise = str(values.get("-MOCK-NOISE-", "")).strip()
            base = str(values.get("-MOCK-BASE-", "")).strip()
            if res:
                send_command(state, f"CMD:MOCK:RF:RES:{res}")
            if noise:
                send_command(state, f"CMD:MOCK:RF:NOISE:{noise}")
            if base:
                send_command(state, f"CMD:MOCK:RF:BASE:{base}")

        # LED HAL controls
        if isinstance(event, str) and event.startswith("-HAL-LED-"):
            # -HAL-LED-ON-<id>- / OFF / TOG / GET
            try:
                parts = event.strip("-").split("-")
                # ["HAL", "LED", "ON", "0", ""]
                action = parts[2]
                led_id = int(parts[3])
            except Exception:
                action = ""
                led_id = -1
            if 0 <= led_id <= 3:
                if action == "ON":
                    send_command(state, f"CMD:HAL:LED:SET:{led_id}:1")
                elif action == "OFF":
                    send_command(state, f"CMD:HAL:LED:SET:{led_id}:0")
                elif action == "TOG":
                    send_command(state, f"CMD:HAL:LED:TOGGLE:{led_id}")
                elif action == "GET":
                    send_command(state, f"CMD:HAL:LED:GET:{led_id}")

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

        elif event in ("-HAL-DAC-RAW-0-", "-HAL-DAC-RAW-1-"):
            ch = 0 if event.endswith("0-") else 1
            text = str(values.get(f"-DAC{ch}-RAW-", "")).strip()
            if text:
                send_command(state, f"CMD:HAL:DAC:RAW:{ch}:{text}")

        # Optional periodic polling
        if state.client and bool(values.get("-AUTO-REFRESH-")):
            try:
                interval = float(values.get("-AUTO-REFRESH-S-", 1.0))
            except (TypeError, ValueError):
                interval = 1.0
            now = time.monotonic()
            if now - state._last_poll_s >= max(0.2, interval):
                state._last_poll_s = now
                send_refresh(state)

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
