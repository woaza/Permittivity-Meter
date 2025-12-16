# Snow Permittivity Meter PC CLI

Python helper for exercising the firmware over USB CDC (or any pyserial URL).
It reuses the ASCII `CMD:*` protocol that the mobile application sends over
Bluetooth, letting you trigger calibration/measurement sweeps, inject synthetic
button presses, and inspect LED/log telemetry without touching hardware.

## Installation

```powershell
python -m venv .venv
.\.venv\Scripts\activate
pip install -r tools/requirements.txt
```

## Usage

```powershell
python tools/pc_cli.py --port COM7
```

On reset/power-up the firmware emits `STAT:BOOT_V2` once (a quick sanity check
that the USART2→ST-LINK VCP TX path is working).

Supported interactive commands:

- `conn` – send `CMD:CONN`, expect `STAT:RDY`
- `cal` – run calibration (`CMD:CAL`)
- `meas` – trigger snow measurement (`CMD:MEAS`)
- `btn-press` / `btn-release` – synthesize button edges via `CMD:BTN:*`
- `leds` – request current LED state snapshot (`CMD:LEDS`)
- `lcd` – dump both LCD lines (`CMD:LCD`)
- `log` – dump the buffered debug log entries (`CMD:LOG`)
- `trace` – stream the latest RF ADC sweep samples (`CMD:TRACE`)
- `send <RAW>` – transmit an arbitrary line verbatim
- `exit` / `quit` – leave the CLI

Pass `--script commands.txt` to execute a list of commands (one per line) before
entering interactive mode. For local testing without hardware, the default
`loop://` port echoes data so you can observe formatting.

### Automated lifecycle test

Use the lifecycle script (which reuses `pc_cli.py`'s SerialClient plumbing)
to replay the entire boot → calibration → measurement flow against the MCU
via the ST-LINK USB bridge:

```powershell
python tools/run_hw_lifecycle.py --port COM7                  # happy path
python tools/run_hw_lifecycle.py --port COM7 --scenario fail  # force STAT:ERR
```

On start it prompts you to press RESET so each run begins from a known
state (pass `--no-reset-prompt` if you are already driving NRST via a
tester). The script connects over UART, issues `CMD:CONN`, performs a full
calibration (`CMD:CAL`), triggers a measurement (`CMD:MEAS`), and asserts
that `DAT:RES` (or `STAT:ERR` in fail mode) arrives before the
`--stage-timeout` deadline. Along the way it probes LED/LCD/log telemetry
to ensure the FSM walks through INIT → IDLE → CALIBRATION → MEASURE →
CALCULATION. Use `--scenario fail` to automatically send
`CMD:MOCK:RF:FAIL:ON` and confirm that the firmware surfaces `STAT:ERR`.

Pass `--probe-debug` if you also want to dump the MCU's `CMD:LOG` /
`CMD:TRACE` telemetry during calibration. That path takes longer and can
timeout when the log buffer is very full, so it is disabled by default for
reliable smoke tests.

By default the script only checks that a `DAT:RES:` frame appears. Add
`--strict-result` to additionally parse the epsilon/density fields and fail
if they are missing or malformed.

## Desktop UI

For a point-and-click workflow with LED indicators, LCD mirroring, RF trace
summaries, and reset controls, launch the PySimpleGUI tool:

```powershell
python tools/pc_ui.py --port COM4
```

The UI automatically issues `CMD:CONN` on connect, exposes buttons for
`CAL`, `MEAS`, trace capture, button press/release, and provides a raw
Bluetooth-style send box. The ADC/DAC confirmation panel updates with the
latest `DAT:RES` frame plus the minimum amplitude found in the current RF
trace so you can quickly sanity-check values. LED, LCD, and log panes now
refresh automatically every couple of seconds so state changes pushed by the
MCU appear without manual refresh clicks.
