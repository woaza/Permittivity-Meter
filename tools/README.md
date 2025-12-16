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

If the firmware had to fall back to MSI because the external clock failed, it will also emit
`STAT:UART_RX:POLL` to indicate the RX path is being serviced via polling (not interrupts).

Supported interactive commands:

- `conn` – send `CMD:CONN`, expect `STAT:RDY`
- `cal` – run calibration (`CMD:CAL`)
- `meas` – trigger snow measurement (`CMD:MEAS`)
- `manual-on` / `manual-off` – enter/exit manual mode (`CMD:MANUAL:*`) (aliases: `manual_on`, `manual_off`)
- `btn-press` / `btn-release` – synthesize button edges via `CMD:BTN:*`
- `leds` – request current LED state snapshot (`CMD:LEDS`)
- `lcd` – dump both LCD lines (`CMD:LCD`)
- `log` – dump the buffered debug log entries (`CMD:LOG`)
- `trace` – stream the latest RF ADC sweep samples (`CMD:TRACE`)
- `hal-init` – init the HAL board module (`CMD:HAL:INIT`) (requires manual mode)
- `hal-*` shortcuts – convenience wrappers for `CMD:HAL:*` (requires manual mode):
	- `hal-led-set <id> <0/1>`, `hal-led-get <id>`, `hal-led-toggle <id>`
	- `hal-adc-read`, `hal-adc-raw`
	- `hal-dac-set <ch> <voltage>`, `hal-dac-raw <ch> <value>`
	- `hal-gain-set <level>`, `hal-gain-get`
	- `hal-btn-read`
	- `hal-nina-rst <0/1>`, `hal-nina-stop <0/1>`
	- `hal-lcd-set <0/1> <text>`
	- `hal-pwm-start`, `hal-pwm-stop`, `hal-pwm-get`, `hal-pwm-freq <hz>`, `hal-pwm-duty <0..100>`
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

The UI issues `CMD:CONN` on connect and renders a device-like panel:

- LCD (2 lines)
- 4 LEDs (STATUS/MEAS/EXCITE/ERROR)
- Button B1 press/release
- Two tuning sliders (FRQ_TN / Q_FACT_TN → `CMD:HAL:DAC:SET:0/1:<volts>`) (requires manual mode)
- Excitation toggle (PWM start/stop → `CMD:HAL:PWM:START/STOP`) (requires manual mode)

It updates from incoming `DAT:LCD:*` and `STAT:HW:*` frames (no periodic polling). On the first `STAT:RDY` after connect it requests a one-time snapshot via `CMD:LEDS` and `CMD:LCD`.
