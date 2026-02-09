# Software-Dokumentation – Permittivity Meter V2

## 1. Einleitung

### 1.1 Zweck des Dokuments

### 1.2 Geltungsbereich

### 1.3 Zielgruppe

### 1.4 Referenzen und verwandte Dokumente

### 1.5 Abkürzungen und Begriffe
---

## 2. Systemübersicht

### 2.1 Projektbeschreibung

The Permittivity Meter is an embedded device that measures the dielectric properties (permittivity, density) of snow — using an RF resonance circuit. A 20 MHz excitation signal drives a resonator whose resonance frequency shifts depending on the snow under test. The firmware sweeps a DAC-controlled varicap voltage range, detects the resonance dip, and calculates the permittivity from the frequency shift between an air calibration and the snow measurement.

The system is controlled via an ASCII command protocol (`CMD:*` / `STAT:*` / `DAT:*`) over USB (USART2). A Bluetooth interface (UART4 / NINA module) is planned to inegrate a smarthpone app. The device can be used standalone via hardware buttons. A built-in mock layer allows full-cycle testing without RF hardware.

### 2.2 Architekturübersicht (Layer-Diagramm)
Todo replace with image from Präsentation
```
┌──────────────────────────────────────────────────────┐
│  PC / Host                                           │
│  (CLI, GUI, Test Scripts)                            │
└────────────────────────┬─────────────────────────────┘
                         │ ASCII over USART2 (USB VCP)
┌────────────────────────▼─────────────────────────────┐
│  Transport Layer                                     │
│  usb_cdc_bridge.c  (DMA Receive-to-Idle, line queue) │
└────────────────────────┬─────────────────────────────┘
                         │ complete lines
┌────────────────────────▼─────────────────────────────┐
│  Protocol Layer                                      │
│  bt_manager.c  (CMD parser, STAT/DAT responses)      │
│  debug_log.c   (ring-buffer logging)                 │
└──────┬─────────────────┬─────────────────┬───────────┘
       │ FSM events      │ CMD:HAL:*       │ CMD:MOCK:*
┌──────▼──────┐   ┌──────▼──────┐   ┌─────▼──────┐
│ Application │   │ HAL Board   │   │ Mock Board │
│ fsm_main.c  │   │ hal_board.c │   │ mock_board │
│ rf_measure  │   │ (Manual Mode│   │ (RF sim)   │
│ rf_trace    │   │  direct HW) │   │            │
│ math_model  │   └──────┬──────┘   └─────┬──────┘
└──────┬──────┘          │                │
       │                 │                │
┌──────▼─────────────────┴────────────────┴────────────┐
│  BSP Layer  (Board Support Package)                  │
│  bsp_rf.c   — RF frontend (switch: mock ↔ real HW)  │
│  bsp_ui.c   — Button + LEDs                         │
│  bsp_lcd.c  — LCD buffer (I2C)                      │
└────────────────────────┬─────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────┐
│  HAL Driver Layer  (hl/)                             │
│  hal_gpio.c, hal_dac.c, hal_adc.c, hal_pwm.c        │
└────────────────────────┬─────────────────────────────┘
                         │
┌────────────────────────▼─────────────────────────────┐
│  STM32 HAL / CMSIS  (vendor library, CubeMX generated│)
└────────────────────────┬─────────────────────────────┘
                         │
                  [ STM32L476RG ]
```

### 2.3 Zielhardware (STM32L476RG / NUCLEO-L476RG)

| Parameter | Value |
|-----------|-------|
| MCU | STM32L476RG (ARM Cortex-M4F, 80 MHz) |
| Board | NUCLEO-L476RG |
| System Clock | 80 MHz (HSE 20 MHz; MSI fallback if HSE fails) |
| USB Interface | USART2 via ST-LINK VCP (PA2/PA3) |
| Bluetooth | UART4 / NINA module (PA0/PA1) — planned |
| DAC | 2 channels — PA4 (frequency tuning), PA5 (Q-factor tuning) |
| ADC | Bandpass-sampled at ~122.5 kHz (undersampling of 20 MHz RF) |
| PWM Excitation | TIM1 CH2 on PA9, 20 MHz square wave (attenuated to 1 Vpp via external OpAmp) |
| UI | 1 button (PC13), 4 LEDs (PA6, PA7, PC7, PB6), I2C LCD (PB8/PB9) |
| RF Gain Select | 2-bit GPIO (PC8, PC9) |

### 2.4 Softwareschichten im Überblick

| Layer | Files | Responsibility |
|-------|-------|----------------|
| **Application / FSM** | `fsm_main.c`, `rf_measure.c`, `rf_trace.c`, `math_model.c` | State machine (INIT → IDLE → CAL → MEAS → ERROR), sweep algorithms (coarse/fine), trace capture, permittivity math |
| **Protocol** | `bt_manager.c`, `debug_log.c` | ASCII command parsing (`CMD:*`), response generation (`STAT:*`, `DAT:*`), event dispatch to FSM, debug ring buffer |
| **Transport** | `usb_cdc_bridge.c`, `bt_communication.c` (planned) | USART2 DMA Receive-to-Idle with byte ring buffer and line queue; future UART4/BT transport |
| **BSP** | `bsp_rf.c`, `bsp_ui.c`, `bsp_lcd.c` | Hardware abstraction: RF frontend (currently mock-backed), button/LED management, LCD line buffer over I2C |
| **Mock Board** | `mocks/mock_board.c` | Simulated RF response (parabolic dip model) for testing without hardware |
| **HAL Board** | `hl/hal_board.c` | Direct hardware control wrappers for manual-mode commands (`CMD:HAL:*`) |
| **HAL Drivers** | `hl/hal_gpio.c`, `hl/hal_dac.c`, `hl/hal_adc.c`, `hl/hal_pwm.c` | Thin wrappers around STM32 HAL for GPIO, DAC, ADC, PWM |
| **Vendor HAL** | `Drivers/STM32L4xx_HAL_Driver/`, CMSIS | ST-provided HAL library and CMSIS core (CubeMX generated) |

---

## 3. HAL Board Layer (`hl/hal_board.c`)

### 3.1 Zweck und Verantwortlichkeit

### 3.2 Schnittstelle zum darunterliegenden HAL-Treiber

### 3.3 Bereitgestellte Funktionen

#### 3.3.1 LED-Steuerung (`HalBoard_LED_Set`, `HalBoard_LED_Get`, `HalBoard_LED_Toggle`)

#### 3.3.2 Button-Abfrage (`HalBoard_BTN_Read`)

#### 3.3.3 DAC-Steuerung (`HalBoard_DAC_Set`, `HalBoard_DAC_SetRaw`)

#### 3.3.4 ADC-Abfrage (`HalBoard_ADC_Read`, `HalBoard_ADC_ReadRaw`)

#### 3.3.5 PWM-Steuerung (`HalBoard_PWM_Start`, `HalBoard_PWM_Stop`, `HalBoard_PWM_SetFreq`, `HalBoard_PWM_SetDuty`)

#### 3.3.6 Gain-Steuerung (`HalBoard_GAIN_Set`, `HalBoard_GAIN_Get`)

#### 3.3.7 NINA-Modul-Steuerung (`HalBoard_NINA_Reset`, `HalBoard_NINA_Stop`)

#### 3.3.8 LCD-Steuerung (`HalBoard_LCD_SetLine`)

#### 3.3.9 Initialisierung (`HalBoard_Init`)

### 3.4 Manual-Mode-Konzept (Handbetrieb)

### 3.5 CMD:HAL:\*-Befehlsrouting

### 3.6 Push-Style ACK Frames (`STAT:HW:*`)

---

## 4. Mock Board Layer (`mocks/mock_board.c`)

### 4.1 Zweck und Einsatzbereich

The mock board provides a software-only simulation of the RF frontend, UI elements (LEDs, button), BT command queue, and debug log buffer. It allows the complete firmware — FSM, measurement algorithms, protocol parser — to run and be tested without any physical hardware attached.

Primary use cases:

- **Development**: Iterate on sweep algorithms and FSM logic on a bare Nucleo board (no RF circuit needed).
- **Automated testing**: PC-side test scripts (`tools/`) can run calibration and measurement cycles against deterministic simulated responses.
- **Failure injection**: Force `NAN` returns to verify error-handling paths.

### 4.2 RF-Antwort-Modell (Parabolisches Dip-Modell)

The mock simulates a resonance circuit whose amplitude shows a parabolic dip (minimum) at the resonance voltage. The measurement algorithms search for this minimum during coarse and fine sweeps.

#### 4.2.1 Mathematische Formel

```
amplitude = base_amplitude + curvature × δ² − (q_voltage × 0.02) + noise
```

where:

- `δ = freq_voltage − resonance_voltage` — distance from resonance centre
- `curvature = 0.3 + (gain_idx × 0.05)` — parabola sharpness, influenced by gain setting
- `noise` — random component scaled by the configured noise level

When force-failure is enabled, the function returns `NAN` regardless of input.

#### 4.2.2 Konfigurierbare Parameter (Resonanz, Rauschen, Basis, Krümmung)

| Parameter | Default | Set via | Description |
|-----------|---------|---------|-------------|
| Resonance voltage | 1.2 V | `MockBoard_RF_SetResonanceVoltage()` | DAC voltage at which the amplitude dip occurs |
| Base amplitude | 1.0 V | `MockBoard_RF_SetBaseAmplitude()` | Amplitude value at the resonance minimum |
| Noise level | 0.01 V | `MockBoard_RF_SetNoise()` | Peak random perturbation added to the output |
| Curvature | 0.3 + gain × 0.05 | gain index (0–3) | Controls how steeply amplitude rises away from resonance |
| Force failure | off | `MockBoard_RF_SetForceFailure()` | When enabled, all reads return `NAN` |

### 4.3 Bereitgestellte Funktionen

#### 4.3.1 `MockBoard_Init` / `MockBoard_Reset`

`MockBoard_Init()` and `MockBoard_Reset()` set all internal state to defaults: resonance = 1.2 V, base = 1.0 V, noise = 0.01 V, force-failure off, LED/button states cleared, BT queue and debug log emptied.

#### 4.3.2 `MockBoard_RF_ComputeAmplitude`

```c
float MockBoard_RF_ComputeAmplitude(float freq_voltage,
                                     float q_voltage,
                                     uint8_t gain_idx);
```

Core function called by `BSP_RF_ReadAmplitude()`. Evaluates the parabolic model for the given DAC voltages and gain index. Returns `NAN` if force-failure is active.

#### 4.3.3 `MockBoard_RF_SetResonanceVoltage` / `SetNoise` / `SetBaseAmplitude` / `SetForceFailure`

Configuration setters that modify the simulated RF behaviour at runtime. Typically invoked via `CMD:MOCK:RF:*` commands (see 4.4).

#### 4.3.4 UI, BT Queue, and Debug Helpers

| Group | Functions | Purpose |
|-------|-----------|---------|
| **UI** | `MockBoard_UI_SetLED`, `GetLED`, `SetButton`, `GetButton` | Simulate LED and button state |
| **BT Queue** | `MockBoard_BT_QueueCommand`, `DequeueCommand` | 4-deep FIFO for incoming command simulation |
| **BT History** | `MockBoard_BT_SetLastTx`, `GetLastTx`, `GetHistoryEntry`, `ClearHistory` | 16-deep ring buffer recording transmitted frames |
| **Debug Log** | `MockBoard_DebugPush`, `DebugCopy`, `DebugGetLast`, `DebugPeek`, `DebugClear` | 32-deep circular buffer for internal debug entries |

### 4.4 CMD:MOCK:\*-Befehle

These commands are parsed by `bt_manager.c` → `handle_mock_command()` and forwarded to `bsp_rf.c` wrapper functions.

| Command | Response | Effect |
|---------|----------|--------|
| `CMD:MOCK:RF:RES:<float>` | `STAT:MOCK_RF_RES` | Set resonance voltage |
| `CMD:MOCK:RF:NOISE:<float>` | `STAT:MOCK_RF_NOISE` | Set noise level |
| `CMD:MOCK:RF:BASE:<float>` | `STAT:MOCK_RF_BASE` | Set base amplitude |
| `CMD:MOCK:RF:FAIL:ON` | `STAT:MOCK_RF_FAIL_ON` | Force all reads to return `NAN` |
| `CMD:MOCK:RF:FAIL:OFF` | `STAT:MOCK_RF_FAIL_OFF` | Resume normal operation |

Invalid or unparseable mock commands return `STAT:MOCK_ERR`.

### 4.5 Umschaltung Mock ↔ reale Hardware

Currently there is **no runtime switch** between mock and real hardware. `bsp_rf.c` always delegates RF reads to `MockBoard_RF_ComputeAmplitude()`. To transition to real hardware, the BSP functions (`BSP_RF_SetFreqVaricap`, `BSP_RF_ReadAmplitude`, etc.) must be updated to call the HAL drivers (`HL_DAC_SetVoltage`, `HL_ADC_Read`, etc.) instead of — or in addition to — the mock. This transition is tracked as a high-priority TODO (see Chapter 17).

### 4.6 Einschränkungen und bekannte Limitierungen

- **No Q-factor modelling**: The Q-factor voltage only adds a small linear offset (`−0.02 × q_voltage`); a realistic Q-dependent bandwidth change is not simulated.
- **Static curvature**: The parabola shape is fixed per gain index; real hardware exhibits non-linear and asymmetric resonance curves.
- **No frequency-domain simulation**: The mock returns a scalar amplitude — it does not produce a time-domain waveform for undersampling / DFT processing.
- **Deterministic noise**: Uses `rand()` seeded at init; results are reproducible but not representative of real RF noise characteristics.

---

## 5. BSP Layer – Board Support Package

### 5.1 Überblick und Zweck

The BSP layer sits between the application logic (FSM, measurement) and the lower-level drivers (HAL / mock). It provides hardware-agnostic interfaces so upper layers never call HAL or mock functions directly. Currently all three BSP modules delegate to the mock board; the transition to real HAL drivers only requires changes inside the BSP — no application code needs to be modified.

### 5.2 BSP RF (`bsp_rf.c` / `bsp_rf.h`)

#### 5.2.1 Zweck (Abstraktion des RF-Frontends)

Abstracts the entire RF signal chain: DAC varicap control, gain selection, excitation enable, op-amp enable, and amplitude readback. Upper layers (`rf_measure.c`) call BSP RF functions without knowing whether a mock or real hardware is behind them.

#### 5.2.2 Bereitgestellte Funktionen

| Function | Description |
|----------|-------------|
| `BSP_RF_Init(void)` | Initialises internal state and calls `MockBoard_Init()`. |
| `BSP_RF_SetFreqVaricap(float voltage_v)` | Stores frequency-tuning voltage (DAC CH1). |
| `BSP_RF_SetQVaricap(float voltage_v)` | Stores Q-factor-tuning voltage (DAC CH2). |
| `BSP_RF_SetGain(uint8_t gain_idx)` | Sets RF gain (masked to 2 bits → 0–3). |
| `BSP_RF_SetOpAmpEnable(uint8_t enable)` | Enables/disables the external op-amp buffer. |
| `BSP_RF_EnableExcitation(uint8_t enable)` | Enables/disables 20 MHz PWM excitation. |
| `BSP_RF_ReadAmplitude(void)` | Returns amplitude from `MockBoard_RF_ComputeAmplitude()` using stored voltages and gain. |

Mock-configuration wrappers (forwarded to `MockBoard_RF_*`):

| Function | Effect |
|----------|--------|
| `BSP_RF_MockSetResonanceVoltage(float v)` | Set mock resonance centre |
| `BSP_RF_MockSetNoiseLevel(float v)` | Set mock noise |
| `BSP_RF_MockSetBaseAmplitude(float v)` | Set mock base amplitude |
| `BSP_RF_MockSetForceFailure(uint8_t en)` | Enable/disable NAN failure mode |

Internal state: `s_freq_voltage`, `s_q_voltage`, `s_gain_idx`, `s_opamp_enabled`, `s_excitation_enabled`. All voltage/gain changes are logged via `Debug_LogDriver()`.

#### 5.2.3 Switch-Point: Mock vs. reale Hardware

`BSP_RF_ReadAmplitude()` currently calls `MockBoard_RF_ComputeAmplitude(s_freq_voltage, s_q_voltage, s_gain_idx)`. To switch to real hardware, this single call must be replaced with a sequence of `HL_DAC_SetVoltage()` → settle delay → `HL_ADC_Read()`. The setter functions (`SetFreqVaricap`, `SetQVaricap`, `SetGain`) will additionally need to forward values to the HAL drivers.

#### 5.2.4 Geplante Erweiterungen (DMA Buffer Capture)

A future `BSP_RF_CaptureBuffer(float *buf, size_t len)` will use DMA to capture a block of ADC samples for frequency-domain processing (undersampling + DFT/Goertzel in `math_model.c`).

### 5.3 BSP UI (`bsp_ui.c` / `bsp_ui.h`)

#### 5.3.1 Zweck (Button- und LED-Verwaltung)

Provides a unified interface for the user button (PC13) and the four status LEDs. All calls delegate to the mock board.

#### 5.3.2 Bereitgestellte Funktionen

| Function | Description |
|----------|-------------|
| `BSP_UI_Init(void)` | Initialises UI state, calls `MockBoard_Init()`. |
| `BSP_LED_Set(uint8_t led_id, uint8_t state)` | Set LED on (1) or off (0). |
| `BSP_LED_Get(uint8_t led_id)` | Read current LED state. |
| `BSP_Button_SetState(uint8_t pressed)` | Inject button state (used by command simulation). |
| `BSP_Button_GetState(void)` | Read current button state. |

LED identifiers (enum): `LED_STATUS` (0), `LED_MEAS` (1), `LED_EXCITE` (2), `LED_ERROR` (3), `LED_COUNT` (4).

#### 5.3.3 Button-Logik und Entprellung

No debouncing is implemented at BSP level. `BSP_Button_SetState()` stores the raw value via `MockBoard_UI_SetButton()`. Debounce logic, if needed, is the responsibility of the FSM or a future interrupt-driven GPIO handler.

#### 5.3.4 LED-Zustandsverwaltung

LED state is held inside the mock board's internal array (indexed by `led_id`). `BSP_LED_Set()` normalises the state to 0/1 before storing. The FSM sets LEDs according to the current application state (e.g., `LED_ERROR` on in `STATE_ERROR`).

### 5.4 BSP LCD (`bsp_lcd.c` / `bsp_lcd.h`)

#### 5.4.1 Zweck (LCD-Pufferverwaltung)

Manages a 2-line × 16-character in-memory buffer that mirrors the physical I2C LCD content. Upper layers write to this buffer; a future driver will flush it to the display.

#### 5.4.2 Bereitgestellte Funktionen

| Function | Description |
|----------|-------------|
| `BSP_LCD_Init(void)` | Clears the buffer (fills with spaces). |
| `BSP_LCD_Clear(void)` | Resets both lines to spaces. |
| `BSP_LCD_DisplayStringAt(uint8_t line, const char *str)` | Writes `str` into the line buffer. Short strings are space-padded; long strings are truncated to 16 chars. |
| `BSP_LCD_GetLine(uint8_t line, char *buffer, uint8_t max_len)` | Copies the line content into a user-provided buffer with bounds checking. |

Constants: `LCD_LINE_COUNT = 2`, `LCD_CHAR_COUNT = 16`.

#### 5.4.3 I2C-Anbindung und Zeilenpuffer

The current implementation is **buffer-only** — no I2C transactions are performed. The internal buffer `s_lcd_lines[2][17]` holds null-terminated strings. When real hardware is connected, `BSP_LCD_DisplayStringAt()` will additionally push the buffer content to the I2C LCD controller (PB8/PB9). The `CMD:LCD` and `CMD:HAL:LCD:SET` commands read from / write to this buffer via `BSP_LCD_GetLine()` / `BSP_LCD_DisplayStringAt()`.

---

## 6. Applikationsschicht – Finite State Machine (`fsm_main.c`)

### 6.1 Überblick und Verantwortlichkeit

The FSM is the central coordinator of the firmware. It owns the application lifecycle — initialisation, calibration, measurement, result display, manual override, and error recovery. It consumes events from the button and the BT protocol parser, drives BSP outputs (LEDs, LCD, RF enable), and delegates measurement work to `rf_measure.c`.

Public API:

| Function | Description |
|----------|-------------|
| `FSM_Init(void)` | Reset FSM to `STATE_INIT`, clear event queue. |
| `FSM_PostEvent(FSM_Event_t event)` | Enqueue an event for the next cycle. |
| `FSM_RunOnce(void)` | Execute one FSM cycle (poll button, drain BT events, process queue, run state handler). |
| `FSM_GetState(void)` | Return current `AppState_t`. |

### 6.2 Zustandsdiagramm

```
                  ┌──────────────────────────────────────────────┐
                  │           BT_MANUAL_ON (from any state)      │
                  ▼                                              │
           ┌──────────────┐  BT_MANUAL_OFF  ┌──────────┐        │
           │   MANUAL     │────────────────►│  IDLE    │◄───┐   │
           │  OPERATION   │                 │          │    │   │
           └──────────────┘                 └────┬─────┘    │   │
                                                 │          │   │
                              ┌───────────────┬──┴──────┐   │   │
                              │ BTN / BT_CAL  │BTN(cal  │   │   │
                              │ (no valid cal)│valid) / │   │   │
                              │               │BT_MEAS  │   │   │
                              ▼               ▼         │   │   │
       ┌─────────┐    ┌──────────────┐ ┌───────────┐   │   │   │
       │  INIT   │───►│ CALIBRATION  │ │  MEASURE  │   │   │   │
       │         │    │              │ │  SEARCH   │   │   │   │
       └─────────┘    └──────┬───────┘ └─────┬─────┘   │   │   │
         ▲                   │               │         │   │   │
         │          CAL_DONE │      MEAS_DONE│         │   │   │
         │          (valid)  │      (valid)  │         │   │   │
         │               ┌──►│◄──────────┐   │         │   │   │
         │               │   ▼           │   ▼         │   │   │
         │               │  IDLE ◄───── CALCULATION ───┘   │   │
         │               │              (result sent,      │   │
         │               │               cal invalidated)  │   │
         │               │                                 │   │
         │    CAL_DONE   │    MEAS_DONE                    │   │
         │    (invalid)  │    (invalid)                    │   │
         │               ▼                                 │   │
         │         ┌──────────┐                            │   │
         └─────────┤  ERROR   │────────────────────────────┘   │
          BTN /    │          │                                 │
          BT_CONN  └──────────┘                                │
```

### 6.3 Zustände im Detail

#### 6.3.1 `STATE_INIT`

On entry: clear queue, initialise BSP RF / UI / LCD, disable excitation and op-amp, set `LED_STATUS` on, display `"INIT" / "Booting..."`, enqueue `FSM_EVENT_INIT_DONE`. The handler dequeues `INIT_DONE` and transitions to IDLE.

#### 6.3.2 `STATE_IDLE`

On entry: `LED_STATUS` on, others off, disable excitation/op-amp, clear pending flags. LCD shows `"IDLE" / "Ready"` (if calibration valid) or `"IDLE" / "Need CAL"`.

Handler:
- **Button press** → CALIBRATION if no valid calibration; MEASURE_SEARCH if calibration valid.
- **`BT_CAL`** → CALIBRATION.
- **`BT_MEAS`** → MEASURE_SEARCH (rejected with `STAT:ERR` if calibration invalid).
- **`BT_CONN`** → sends `STAT:RDY`, stays in IDLE.

#### 6.3.3 `STATE_CALIBRATION`

On entry: set `s_calibration_pending`, enable `LED_MEAS` + `LED_EXCITE`, enable op-amp and excitation, LCD `"CAL" / "Sweeping..."`.

Handler: calls `RF_PerformAirCalibration()`, enqueues `CAL_DONE`. On `CAL_DONE`: if `s_calibration.is_valid` → IDLE + `STAT:CAL_OK`; else → ERROR + `STAT:ERR`. LED blink on `LED_MEAS` every 4 FSM cycles during sweep.

#### 6.3.4 `STATE_MEASURE_SEARCH`

On entry: set `s_measurement_pending`, enable `LED_MEAS` + `LED_EXCITE`, enable op-amp and excitation, LCD `"MEAS" / "Sampling..."`.

Handler: calls `RF_PerformSnowMeasurement(s_calibration)`, enqueues `MEAS_DONE`. On `MEAS_DONE`: if `FSM_IsMeasurementValid()` → CALCULATION; else → ERROR + `STAT:ERR:MEAS_INVALID`.

#### 6.3.5 `STATE_MANUAL_OPERATION`

On entry: immediately disable excitation and op-amp (safety), clear pending flags, `LED_STATUS` on, LCD `"MANUAL" / "HAL cmds OK"`, send `STAT:MANUAL_ON`.

Can be entered from **any state** via `BT_MANUAL_ON`. `BT_MANUAL_OFF` → IDLE. `BT_CAL` / `BT_MEAS` rejected with `STAT:MANUAL_ACTIVE`. `CMD:HAL:*` commands are only accepted while in this state.

#### 6.3.6 `STATE_ERROR`

On entry: disable excitation/op-amp, all LEDs off except `LED_ERROR`, LCD `"ERROR" / "Check host"`.

Recovery: button press or `BT_CONN` → INIT (full reinitialisation). `BT_CAL` / `BT_MEAS` rejected with `STAT:ERR`.

#### 6.3.7 `STATE_CALCULATION` (geplant)

On entry: disable excitation, `LED_EXCITE` off, **invalidate calibration** (`s_calibration.is_valid = 0`), display result on LCD (`"ER X.XX EI Y.YY" / "D Z.ZZkg/m3"`), send `DAT:RES:…` via `BT_SendResult()`.

Button press → IDLE (acknowledge). `BT_CAL` → CALIBRATION (new cycle). `BT_MEAS` rejected (`STAT:ERR:NEED_CAL` — must recalibrate after each measurement).

### 6.4 Events und Event-Queue

#### 6.4.1 FSM-Event-Typen

```c
typedef enum {
    FSM_EVENT_NONE = 0,
    FSM_EVENT_INIT_DONE,
    FSM_EVENT_BUTTON_PRESS,
    FSM_EVENT_BT_CONN,
    FSM_EVENT_BT_CAL,
    FSM_EVENT_BT_MEAS,
    FSM_EVENT_BT_MANUAL_ON,
    FSM_EVENT_BT_MANUAL_OFF,
    FSM_EVENT_CAL_DONE,
    FSM_EVENT_MEAS_DONE,
    FSM_EVENT_ERROR_FLAG
} FSM_Event_t;
```

#### 6.4.2 Event-Quellen (Button, BT-Manager)

| Source | Mechanism |
|--------|-----------|
| **Button** | `process_button()` runs each `FSM_RunOnce()` cycle; detects rising edge on `BSP_Button_GetState()` → enqueues `FSM_EVENT_BUTTON_PRESS`. |
| **BT Manager** | `BT_PopEvent()` drained each cycle; `BT_EVENT_CONN/CAL/MEAS` mapped to `FSM_EVENT_BT_*` and enqueued. `BT_EVENT_MANUAL_ON/OFF` handled directly (immediate transition). |
| **Internal** | `FSM_EVENT_INIT_DONE`, `CAL_DONE`, `MEAS_DONE` generated by state handlers after completing their work. |

#### 6.4.3 Event-Verarbeitung und Priorisierung

Circular buffer with depth **8** (`FSM_EVENT_QUEUE_SIZE`). On overflow the oldest event is dropped and `"QUEUE overflow"` is logged. `FSM_EVENT_NONE` is silently discarded. Events are processed FIFO — no priority levels. `BT_MANUAL_ON/OFF` bypass the queue and take effect immediately.

### 6.5 Zustandsübergänge

| Current State | Event | Next State | Response |
|---------------|-------|------------|----------|
| INIT | `INIT_DONE` | IDLE | — |
| IDLE | `BUTTON_PRESS` (no cal) | CALIBRATION | `STAT:CAL_REQ` |
| IDLE | `BUTTON_PRESS` (cal valid) | MEASURE_SEARCH | `STAT:MEAS_REQ` |
| IDLE | `BT_CAL` | CALIBRATION | `STAT:CAL_REQ` |
| IDLE | `BT_MEAS` (cal valid) | MEASURE_SEARCH | `STAT:MEAS_REQ` |
| IDLE | `BT_MEAS` (no cal) | IDLE | `STAT:ERR` |
| IDLE | `BT_CONN` | IDLE | `STAT:RDY` |
| CALIBRATION | `CAL_DONE` (valid) | IDLE | `STAT:CAL_OK` |
| CALIBRATION | `CAL_DONE` (invalid) | ERROR | `STAT:ERR` |
| CALIBRATION | `BT_CONN` | CALIBRATION | `STAT:CAL` |
| MEASURE_SEARCH | `MEAS_DONE` (valid) | CALCULATION | `DAT:RES:…` |
| MEASURE_SEARCH | `MEAS_DONE` (invalid) | ERROR | `STAT:ERR:MEAS_INVALID` |
| MEASURE_SEARCH | `BT_CONN` | MEASURE_SEARCH | `STAT:MEAS` |
| CALCULATION | `BUTTON_PRESS` | IDLE | — |
| CALCULATION | `BT_CAL` | CALIBRATION | `STAT:CAL_REQ` |
| CALCULATION | `BT_MEAS` | CALCULATION | `STAT:ERR:NEED_CAL` |
| CALCULATION | `BT_CONN` | CALCULATION | `STAT:RDY` |
| ERROR | `BUTTON_PRESS` | INIT | — |
| ERROR | `BT_CONN` | INIT | `STAT:RDY` |
| ERROR | `BT_CAL` / `BT_MEAS` | ERROR | `STAT:ERR` |
| *any* | `BT_MANUAL_ON` | MANUAL_OPERATION | `STAT:MANUAL_ON` |
| MANUAL_OPERATION | `BT_MANUAL_OFF` | IDLE | `STAT:MANUAL_OFF` |
| MANUAL_OPERATION | `BT_CAL` / `BT_MEAS` | MANUAL_OPERATION | `STAT:MANUAL_ACTIVE` |
| MANUAL_OPERATION | `BT_CONN` | MANUAL_OPERATION | `STAT:MANUAL` |

### 6.6 Fehlerbehandlung und Recovery

| Scenario | Detection | Action |
|----------|-----------|--------|
| Calibration failure | `RF_PerformAirCalibration()` returns `is_valid = 0` | → ERROR, `STAT:ERR` |
| Invalid measurement | `FSM_IsMeasurementValid()`: non-finite voltage or exceeds threshold | → ERROR, `STAT:ERR:MEAS_INVALID` |
| Measurement without cal | `BT_MEAS` while `!s_calibration.is_valid` | Rejected, `STAT:ERR` (stays in IDLE) |
| Post-result recalibration | Entry to CALCULATION invalidates `s_calibration` | Forces recalibration before next measurement |
| Event queue overflow | 9th event into full queue | Oldest dropped, `"QUEUE overflow"` logged |
| Manual mode safety | Entry to MANUAL_OPERATION from any state | Excitation + op-amp disabled immediately |
| Error recovery | Button press or `BT_CONN` in ERROR state | Full reinitialisation via → INIT |

---

## 7. Messlogik (`rf_measure.c`)

### 7.1 Überblick und Verantwortlichkeit

Implements the two core measurement routines — air calibration and snow measurement — using a coarse-then-fine sweep strategy with parabolic interpolation. The module calls BSP RF functions exclusively and has no direct hardware dependency.

Public API:

| Function | Returns | Description |
|----------|---------|-------------|
| `RF_PerformAirCalibration(void)` | `CalibrationData_t` | Full-range sweep to find air resonance. |
| `RF_PerformSnowMeasurement(CalibrationData_t calib)` | `MeasurementResult_t` | Narrowed sweep around calibration point to detect permittivity shift. |

Data structures:

```c
typedef struct {
    float   air_dac_freq_voltage;   // DAC voltage at air resonance
    float   air_adc_min;            // ADC amplitude at that minimum
    uint32_t timestamp;             // reserved (currently 0)
    uint8_t is_valid;               // 1 = usable, 0 = failed
} CalibrationData_t;

typedef struct {
    float   epsilon_real;           // ε'
    float   epsilon_imag;           // ε''
    float   snow_density;           // kg/m³ (linear estimate)
    float   temperature;            // reserved for thermal compensation
    float   dac_freq_voltage;       // resonance voltage in snow
    float   dac_q_voltage;          // Q-trim voltage used
    float   adc_voltage_min;        // minimum amplitude detected
    float   frequency_shift;        // dac_freq_voltage − air_dac_freq_voltage
    uint8_t gain_index;             // 0–3
} MeasurementResult_t;
```

### 7.2 Kalibrierung (Air Calibration)

#### 7.2.1 Ablauf (`RF_PerformAirCalibration`)

1. Begin trace (`RF_TRACE_MODE_CALIBRATION`), enable excitation and set default gain.
2. **Coarse sweep** → find approximate resonance voltage.
3. **Fine sweep** → narrow ±0.025 V around coarse result.
4. **Parabolic interpolation** → refine to sub-step precision.
5. Validate amplitude (`isfinite()`). On success: fill `CalibrationData_t` with refined vertex and amplitude, set `is_valid = 1`. On failure: fall back to coarse result, `is_valid = 0`.
6. Disable excitation, end trace, return result.

#### 7.2.2 Coarse Sweep

`coarse_sweep(float *best_voltage, float *best_amplitude)` — static.

Scans the full DAC range **0.0 – 2.5 V** in **0.05 V** steps (≈ 51 samples). At each step `sample_at()` is called. Non-finite amplitudes are skipped. The voltage producing the **lowest** (minimum) amplitude is recorded. Returns `true` if at least one finite sample was found.

#### 7.2.3 Fine Sweep

`fine_sweep(float coarse_best_v, float *best_voltage, float *best_amplitude)` — static.

Searches a ±0.025 V window (`FINE_WINDOW_STEPS × FINE_STEP_V = 5 × 0.005`) around the coarse result in **0.005 V** steps (≈ 11 samples). Start/end clamped to [0.0, 2.5] V. Again finds the minimum amplitude.

#### 7.2.4 Parabolische Interpolation

`refine_vertex(float center)` — static.

Samples three points at `center − 0.005`, `center`, `center + 0.005` and fits a parabola through them:

```
vertex_x = numerator / (2 × denominator)
```

where numerator and denominator are derived from the standard three-point quadratic vertex formula. Falls back to `center` if the denominator is zero, any value is non-finite, or the computed vertex lies outside [0.0, 2.5] V.

#### 7.2.5 Ergebnis und Speicherung

On success the returned `CalibrationData_t` contains the parabola-refined resonance voltage (`air_dac_freq_voltage`), the corresponding minimum amplitude (`air_adc_min`), and `is_valid = 1`. The FSM stores this struct and gates subsequent measurement requests on `is_valid`.

### 7.3 Messung (Snow Measurement)

#### 7.3.1 Ablauf (`RF_PerformSnowMeasurement`)

1. Validate calibration (`is_valid`). If invalid: return ε′ = 1.0, ε″ = 0.0 immediately.
2. Begin trace (`RF_TRACE_MODE_MEASUREMENT`), enable excitation.
3. Sweep ±0.2 V around `calib.air_dac_freq_voltage` in **0.005 V** steps (≈ 81 samples).
4. **Parabolic interpolation** on the minimum found.
5. Compute results: `frequency_shift`, `epsilon_real`, `epsilon_imag` (via `Math_CalculateEpsilon()`), `snow_density` (linear model: `0.3 + shift × 0.1`).
6. Disable excitation, end trace, return `MeasurementResult_t`.

#### 7.3.2 Suchbereich relativ zur Kalibrierung

The search window is **±0.2 V** (fixed) centred on the air calibration voltage. This is significantly narrower than the full 0–2.5 V coarse sweep, reducing measurement time while covering the expected permittivity-induced shift range.

#### 7.3.3 Ergebnisberechnung

- `frequency_shift = vertex − calib.air_dac_freq_voltage`
- `epsilon_real`, `epsilon_imag` filled by `Math_CalculateEpsilon()` (see Chapter 9).
- `snow_density = 0.3 + frequency_shift × 0.1` (placeholder linear model).

### 7.4 Sampling-Funktion (`sample_at`)

```c
static float sample_at(float freq_voltage, float q_voltage,
                        uint8_t gain_idx, float *out_amp);
```

Sets gain → frequency varicap → Q varicap via BSP, reads amplitude via `BSP_RF_ReadAmplitude()`, logs the point via `RF_Trace_Add()`, and returns the amplitude (also written to `*out_amp` if non-NULL).

### 7.5 Konfigurierbare Parameter (Sweep-Bereich, Schrittweiten)

| Constant | Value | Purpose |
|----------|-------|---------|
| `COARSE_START_V` | 0.0 V | Coarse sweep start |
| `COARSE_END_V` | 2.5 V | Coarse sweep end |
| `COARSE_STEP_V` | 0.05 V | Coarse step size (≈ 51 samples) |
| `FINE_STEP_V` | 0.005 V | Fine / measurement step size |
| `FINE_WINDOW_STEPS` | 5 | Fine window = ±5 × 0.005 = ±0.025 V |
| `DEFAULT_Q_VOLTAGE` | 1.0 V | Q-trim varicap voltage |
| `DEFAULT_GAIN_INDEX` | 1 | RF gain setting (0–3) |
| Measurement window | ±0.2 V | Search range around calibration point |

### 7.6 Fehlerbehandlung (ungültige Messungen)

| Condition | Handling |
|-----------|----------|
| Coarse sweep returns no finite samples | Calibration `is_valid = 0`; logged as `"coarse_fail"` |
| Fine sweep returns no finite samples | Calibration `is_valid = 0`; logged as `"fine_fail"` |
| Amplitude not finite after fine sweep | Calibration `is_valid = 0`; logged as `"amp_invalid"` |
| Parabolic vertex out of range or non-finite | Falls back to centre voltage; logged as `"vertex_fallback"` |
| Snow measurement called with invalid calibration | Returns ε′ = 1.0, ε″ = 0.0; logged as `"no_cal"` |
| Snow sweep finds no finite samples | Sets `adc_voltage_min = FLT_MAX`; logged as `"sweep_fail"` |

All events are logged under domains `"RF_CAL"` or `"RF_MEAS"` via `Debug_LogDriver()`.

---

## 8. RF Trace (`rf_trace.c`)

### 8.1 Zweck (Sweep-Daten-Aufzeichnung)

Records voltage/amplitude sample pairs during calibration and measurement sweeps so they can be dumped to the host for visualisation and debugging. The trace buffer is filled automatically by `sample_at()` in `rf_measure.c` and read out via `CMD:TRACE`.

### 8.2 Bereitgestellte Funktionen

| Function | Description |
|----------|-------------|
| `RF_Trace_Begin(RF_TraceMode_t mode)` | Start a new trace session; resets sample count, sets mode (`RF_TRACE_MODE_CALIBRATION` or `RF_TRACE_MODE_MEASUREMENT`). |
| `RF_Trace_Add(float voltage, float amplitude)` | Append one sample. Silently ignored if the buffer is full or tracing is not active. |
| `RF_Trace_End(void)` | Stop accepting samples; clears the active flag. |
| `RF_Trace_Copy(RFTraceSample_t *out, size_t max, RF_TraceMode_t *out_mode)` | Copy collected samples into a caller-provided buffer; returns the number copied. Stores the mode in `*out_mode` if non-NULL. |
| `RF_Trace_GetMode(void)` | Return current trace mode. |
| `RF_Trace_GetCount(void)` | Return number of samples collected. |

Types:

```c
typedef enum { RF_TRACE_MODE_NONE, RF_TRACE_MODE_CALIBRATION,
               RF_TRACE_MODE_MEASUREMENT } RF_TraceMode_t;

typedef struct { float voltage; float amplitude; } RFTraceSample_t;
```

Buffer depth: `RF_TRACE_MAX_SAMPLES = 128`.

### 8.3 Datenformat und Ausgabe (`DAT:TRACE:*`)

When the host sends `CMD:TRACE`, `bt_manager.c` calls `RF_Trace_Copy()` and emits one line per sample:

```
DAT:TRACE:<mode>:<index>:V:<voltage>:A:<amplitude>
```

where `<mode>` is `CAL` or `MEAS`, `<index>` is 0-based, and voltage/amplitude are formatted as floats.

### 8.4 Nutzung zur Diagnose

- Plot the sweep curve on the PC to verify the parabolic shape and confirm the minimum location.
- Compare calibration and measurement traces to visualise the resonance shift.
- Detect anomalies: flat curves (no resonance), excessive noise, or NAN samples indicating hardware faults.

---

## 9. Mathematisches Modell (`math_model.c`)

### 9.1 Zweck und Verantwortlichkeit

Provides the physical conversion from DAC varicap voltages to electrical parameters (capacitance, permittivity). Called by `rf_measure.c` after the sweep to translate the observed voltage shift into material properties.

### 9.2 Permittivitätsberechnung

```c
void Math_CalculateEpsilon(float v_air, float v_snow,
                            float *epsilon_r, float *epsilon_i);
```

1. Convert both voltages to capacitance via `Math_Varicap_VtoC()`.
2. Compute real part: `ε' = 1.0 + 0.5 × (C_air / C_snow)`.
3. Compute imaginary part: `ε'' = 0.01 + 0.02 × (v_air − v_snow)`.

`Math_Varicap_VtoC(float voltage_v)` maps a varicap voltage (0.0 – 2.5 V) to capacitance (pF) using an 11-point look-up table with linear interpolation between entries. Voltages outside the table range are clamped to the boundary values.

LUT (abridged):

| Voltage (V) | Capacitance (pF) |
|-------------|-------------------|
| 0.00 | 190 |
| 0.50 | 120 |
| 1.00 | 85 |
| 1.50 | 62 |
| 2.00 | 48 |
| 2.50 | 39 |

### 9.3 Signalverarbeitung (Undersampling / Bandpass Sampling)

#### 9.3.1 Prinzip des Undersamplings

The RF signal is at 20 MHz, well above the STM32 ADC Nyquist limit (~5 Msps). Bandpass sampling deliberately violates the baseband Nyquist criterion: choosing a sampling rate f_s such that the 20 MHz signal aliases to a low IF frequency f_IF that the ADC can resolve.

#### 9.3.2 Alias-Frequenz-Berechnung

`f_IF = |20 MHz − N × f_s|` where N is the nearest integer multiple. For example, f_s ≈ 800.1 kHz aliases 20 MHz to ~2.5 kHz.

#### 9.3.3 DFT/Goertzel-Algorithmus (geplant)

Not yet implemented. The planned approach: capture a DMA buffer of ~256 ADC samples and compute the magnitude at f_IF using a single-bin Goertzel algorithm. This will replace the current single-point `BSP_RF_ReadAmplitude()` call and improve noise rejection significantly.

### 9.4 Fehlerbetrachtung und Genauigkeit

- **LUT resolution**: 11 points over 2.5 V → 0.25 V spacing; linear interpolation introduces ≤ ~2 % error between nodes given the monotonic C(V) curve.
- **ε' model**: The current formula (`1.0 + 0.5 × C_air/C_snow`) is a simplified placeholder. A full electromagnetic model is required for production accuracy.
- **ε'' model**: Linear in voltage difference — adequate for relative comparisons but not for absolute loss-tangent measurement.
- **Noise**: Without DFT integration the single-point amplitude read is susceptible to wideband noise; implementing Goertzel will improve SNR by ≈ 10× (narrowband filtering).

---

## 10. Debug-Logging (`debug_log.c`)

### 10.1 Zweck (interner Ringpuffer für Diagnose)

Centralised logging facility used by the FSM, event handlers, and driver layers. Entries are stored in the mock board's 32-deep circular buffer and can be retrieved via `CMD:LOG`.

### 10.2 Bereitgestellte Funktionen

| Function | Description |
|----------|-------------|
| `Debug_LogState(const char *tag, AppState_t from, AppState_t to)` | Log a state transition. Formatted as `"TAG:FROM→TO"` (e.g. `"FSM:IDLE→CAL"`). Updates the internal `s_last_state`. |
| `Debug_LogEvent(const char *source, const char *detail)` | Log an event in the current state. Formatted as `"SOURCE:DETAIL"` (e.g. `"BT:CMD_CAL"`). |
| `Debug_LogDriver(const char *component, const char *detail)` | Log a driver-level action. Formatted as `"COMPONENT:DETAIL"` (e.g. `"RF:gain=2"`). |
| `Debug_LogCopy(DebugLogEntry_t *out, size_t max)` | Copy up to `max` entries into a caller buffer; returns count copied. Delegates to `MockBoard_DebugCopy()`. |
| `Debug_LogGetLast(void)` | Return the text of the most recent entry (via `MockBoard_DebugGetLast()`). |
| `Debug_LogClear(void)` | Reset log state to `STATE_INIT` and clear the buffer. |

Entry structure:

```c
typedef struct {
    DebugLogDomain_t domain;      // STATE, EVENT, or DRIVER
    AppState_t       state;       // FSM state at time of logging
    char             text[48];    // formatted message (max 47 chars + NUL)
} DebugLogEntry_t;
```

### 10.3 Log-Domänen und Filterung

| Domain | Enum | Producers |
|--------|------|-----------|
| State transitions | `DEBUG_LOG_DOMAIN_STATE` | `FSM_HandleStateChange()` |
| Events | `DEBUG_LOG_DOMAIN_EVENT` | `bt_manager.c`, button handler |
| Driver actions | `DEBUG_LOG_DOMAIN_DRIVER` | `bsp_rf.c`, `rf_measure.c`, HAL wrappers |

State names in log output: `INIT`, `IDLE`, `MAN`, `CAL`, `MEAS`, `CALC`, `ERR` (mapped by internal `state_to_string()`).

### 10.4 Ausgabe über `CMD:LOG`

When the host sends `CMD:LOG`, the protocol parser calls `Debug_LogCopy()` and emits one line per entry:

```
DAT:LOG:D:<domain>:S:<state>:<text>
```

where `<domain>` is `STATE` / `EVENT` / `DRIVER` and `<state>` is the short state name. The buffer is **not** cleared automatically after a dump — use separate logic or firmware restart to reset.

---

## 11. Kommunikationsschicht – Protokoll und Transport

### 11.1 Überblick (ASCII-Protokoll)

All host ↔ firmware communication uses newline-terminated ASCII frames over USART2 (USB VCP). Three frame prefixes define the direction and purpose:

| Prefix | Direction | Purpose |
|--------|-----------|---------|
| `CMD:*` | Host → Device | Commands and requests |
| `STAT:*` | Device → Host | Status responses, acknowledgements, errors |
| `DAT:*` | Device → Host | Data payloads (results, traces, logs, LCD content) |

An additional sub-prefix `STAT:HW:*` carries structured push-style hardware state frames for GUI updates.

### 11.2 BT-Manager / Protokoll-Parser (`bt_manager.c`)

#### 11.2.1 Zweck und Verantwortlichkeit

Central command parser and event dispatcher. Receives complete lines from the transport layer, matches command prefixes, routes to the appropriate handler, and emits `STAT:*` / `DAT:*` responses. Also maintains a BT event queue consumed by the FSM.

Public API:

| Function | Description |
|----------|-------------|
| `BT_Manager_Init(void)` | Reset event queue and manual-mode flag. |
| `BT_ProcessIncoming(const char *buffer)` | Parse one command line and dispatch. |
| `BT_PopEvent(void)` | Dequeue next `BT_Event_t` (or `BT_EVENT_NONE`). |
| `BT_SendStatus(const char *tag)` | Send `STAT:<tag>\n`. |
| `BT_SendResult(MeasurementResult_t r)` | Send `DAT:RES:ER:<ε'>:EI:<ε''>:DENS:<ρ>` (3 decimal places). |
| `BT_SetManualMode(uint8_t en)` | Enable/disable manual-mode gate. |
| `BT_IsManualMode(void)` | Query manual-mode flag. |

#### 11.2.2 Protokollstruktur (`CMD:*`, `STAT:*`, `DAT:*`)

Response examples:

```
STAT:BOOT_V2                              (status tag)
STAT:HW:LED:0:1                           (push-style hardware frame)
DAT:RES:ER:1.540:EI:0.020:DENS:150.300   (measurement result)
DAT:TRACE:CAL:0:V:0.500:A:1.230          (trace sample)
DAT:LOG:D:STATE:S:IDLE:FSM:IDLE→CAL      (debug log entry)
DAT:LCD:L0:IDLE                           (LCD line content)
```

#### 11.2.3 Befehlsverarbeitung (`BT_ProcessIncoming`)

The function performs prefix matching on the incoming line and routes to the first matching handler. Matching is by exact character count or `strncmp`.

#### 11.2.4 Befehlsrouting und Dispatch

| Prefix | Handler | Action |
|--------|---------|--------|
| `CMD:RESET` | direct | Send `STAT:RESETTING`, delay 20 ms, `NVIC_SystemReset()` |
| `CMD:CONN` | `push_event()` | Enqueue `BT_EVENT_CONN` |
| `CMD:CAL` | `push_event()` + send | Send `STAT:CAL_REQ`, enqueue `BT_EVENT_CAL` |
| `CMD:MEAS` | `push_event()` + send | Send `STAT:MEAS_REQ`, enqueue `BT_EVENT_MEAS` |
| `CMD:BTN:PRESS` | `handle_button_command()` | Send `STAT:BTN_PRESS` |
| `CMD:BTN:RELEASE` | `handle_button_command()` | Send `STAT:BTN_REL` |
| `CMD:LEDS` | `send_led_snapshot()` | Dump all 4 LED states |
| `CMD:LCD` | `send_lcd_snapshot()` | Dump LCD line buffers as `DAT:LCD:L0/L1:…` |
| `CMD:LOG` | `send_log_dump()` | Dump debug ring buffer as `DAT:LOG:…` lines |
| `CMD:TRACE` | `send_trace_dump()` | Dump RF trace as `DAT:TRACE:…` lines |
| `CMD:MANUAL:ON` | `push_event()` + send | Send `STAT:MANUAL_ON_REQ`, enqueue `BT_EVENT_MANUAL_ON` |
| `CMD:MANUAL:OFF` | `push_event()` + send | Send `STAT:MANUAL_OFF_REQ`, enqueue `BT_EVENT_MANUAL_OFF` |
| `CMD:MOCK:*` | `handle_mock_command()` | Route to BSP RF mock setters (see Ch. 4) |
| `CMD:HAL:*` | `handle_hal_command()` | Route to HAL Board functions (see Ch. 3); **gated** — returns `STAT:HAL_LOCKED` if manual mode is not active |

HAL sub-commands cover: `LED:SET/GET/TOGGLE`, `ADC:READ/RAW`, `DAC:SET/RAW`, `GAIN:SET/GET`, `BTN:READ`, `LCD:SET`, `PWM:START/STOP/GET/FREQ/DUTY`, `NINA:RST/STOP`, `INIT`. Invalid sub-commands return `STAT:HAL_CMD_ERR`.

#### 11.2.5 Antwortgenerierung (`BT_Send`, `BT_Printf`)

- `BT_SendStatus(tag)` → formats `"STAT:<tag>"` and calls `PC_HostBridge_Send()`.
- `BT_SendResult(result)` → formats `"DAT:RES:ER:<ε_r>:EI:<ε_i>:DENS:<ρ>"` using `fmt_fixed_3()` (3 decimal places).
- `output_hw_framef(fmt, …)` → formats `"STAT:HW:<payload>"` (max 96 bytes) for push-style hardware state updates.

#### 11.2.6 Integration mit FSM (Event-Weiterleitung)

BT event queue: 8-deep circular buffer (`BT_EVENT_QUEUE_SIZE = 8`). On overflow the oldest event is dropped. The FSM drains this queue each `FSM_RunOnce()` cycle via `BT_PopEvent()`.

```c
typedef enum {
    BT_EVENT_NONE = 0,
    BT_EVENT_CONN,
    BT_EVENT_CAL,
    BT_EVENT_MEAS,
    BT_EVENT_MANUAL_ON,
    BT_EVENT_MANUAL_OFF
} BT_Event_t;
```

#### 11.2.7 Integration mit HAL Board (CMD:HAL:\*-Routing)

All `CMD:HAL:*` commands are rejected with `STAT:HAL_LOCKED` unless the FSM is in `STATE_MANUAL_OPERATION` (checked via `BT_IsManualMode()`). On success each handler emits both a legacy `STAT:HAL_*` acknowledgement **and** a structured `STAT:HW:*` push frame so the PC GUI can update immediately.

#### 11.2.8 Integration mit Mock Board (CMD:MOCK:\*-Routing)

`handle_mock_command()` parses the sub-command after `CMD:MOCK:` and calls the corresponding `BSP_RF_Mock*` wrapper. Invalid payloads return `STAT:MOCK_ERR`. See Chapter 4.4 for the full command table.

### 11.3 USB CDC Bridge / USART2 Transport (`usb_cdc_bridge.c`)

#### 11.3.1 Zweck (Zeilenweise Empfangs-/Sendeschnittstelle)

Manages USART2 RX and TX. Receives raw bytes from the UART peripheral, assembles them into newline-terminated lines, queues them, and hands complete lines to `BT_ProcessIncoming()`. Sends response strings with automatic newline appending.

Public API:

| Function | Description |
|----------|-------------|
| `PC_HostBridge_Init(void)` | Select RX mode (DMA / IT / poll), arm receiver, report mode once. |
| `PC_HostBridge_OnRx(const uint8_t *buf, uint32_t len)` | ISR callback — push raw bytes into the byte ring. |
| `PC_HostBridge_Send(const char *str)` | Transmit a string on USART2 (appends `\n` if missing). |
| `PC_HostBridge_Poll(void)` | Main-loop call: poll RX (if polling mode), assemble lines, drain line queue → `BT_ProcessIncoming()`. |

#### 11.3.2 DMA Receive-to-Idle (bevorzugt)

Preferred mode when `hdmarx != NULL` and the system clock is not in fallback. Uses `HAL_UARTEx_ReceiveToIdle_DMA()` with a 256-byte DMA buffer. The half-transfer interrupt is disabled to reduce overhead. On idle-line detection the ISR copies received bytes into a shadow buffer, **re-arms the DMA immediately** (minimising dead time), then pushes the copied bytes into the byte ring.

Boot status: `STAT:UART_RX:DMA_IDLE`.

#### 11.3.3 Interrupt-basierter RX (Fallback)

If no DMA channel is available but the clock is stable, `HAL_UARTEx_ReceiveToIdle_IT()` is used with the same 256-byte buffer and identical re-arm-then-process strategy.

Boot status: `STAT:UART_RX:IT_IDLE`.

#### 11.3.4 Polling RX (Clock-Fallback)

Activated only when `g_clock_fallback_active` is set **and** no RX DMA is available. `PC_HostBridge_Poll()` reads up to 256 bytes per call from the UART RDR register, pushing each byte into the ring buffer. ORE (overrun) flags are cleared before each read.

Boot status: `STAT:UART_RX:POLL`.

#### 11.3.5 RX-Byte-Ringpuffer

512-byte circular buffer (`RX_BYTE_RING_SIZE = 512`). ISR writes via `rx_ring_push_byte()`; main loop reads via `rx_ring_pop_byte()`. On overflow the oldest byte is dropped and `"rxring_ovf"` is logged.

#### 11.3.6 RX-Line-Queue (zeilenweise Verarbeitung)

32-entry queue of 128-byte strings (`RX_LINE_QUEUE_SIZE = 32`). `PC_HostBridge_Poll()` pops bytes from the ring, appends to a 128-byte working buffer until `\n` or `\r` is seen, then enqueues the complete line. On queue overflow the oldest line is dropped (`"rxq_ovf"` logged). Lines exceeding 127 characters are truncated and logged as `"overflow"`.

After assembly, the poll function drains up to 32 lines per call and passes each to `BT_ProcessIncoming()`.

#### 11.3.7 TX-Ausgabe

`PC_HostBridge_Send()` copies up to 158 bytes into a 160-byte local buffer, appends `\n` if not already present, and calls `HAL_UART_Transmit()` with a 200 ms timeout. TX failures are logged as `"tx_fail"`.

Buffer sizes summary:

| Buffer | Size | Purpose |
|--------|------|---------|
| RX byte ring | 512 B | ISR → main-loop bridge |
| RX-to-idle DMA/IT | 256 B × 2 | DMA target + shadow copy |
| RX working buffer | 128 B | Line assembly |
| RX line queue | 32 × 128 B | Complete-line FIFO |
| TX output buffer | 160 B | UART transmit formatting |
| BT event queue | 8 entries | Command → FSM event bridge |

### 11.4 Bluetooth-Kommunikation (`bt_communication.c`) (geplant)

#### 11.4.1 UART4 / NINA-Modul

The NINA Bluetooth module is connected via UART4 (PA0 TX / PA1 RX). Hardware control pins (`NINA:RST`, `NINA:STOP`) are accessible through `CMD:HAL:NINA:*` in manual mode. The UART4 peripheral is initialised by CubeMX but **no firmware RX/TX logic is implemented yet**.

#### 11.4.2 Geplante Integration in den Protokoll-Parser

The planned approach mirrors the USB path: a second `HostBridge` instance for UART4 with its own byte ring and line queue, feeding received lines into the same `BT_ProcessIncoming()` parser. Responses would be echoed to both transports so the PC and Bluetooth app see identical output. This is tracked as a development TODO (see Chapter 17).

---

## 12. Hauptprogramm und Initialisierung (`main.c`)

### 12.1 Boot-Sequenz

Initialisation runs top-to-bottom before entering the infinite loop:

| # | Call | Purpose |
|---|------|---------|
| 1 | `HAL_Init()` | Flash interface, SysTick (1 ms tick). |
| 2 | `SystemClock_Config()` | HSE + PLL → 64 MHz; falls back to MSI 4 MHz if HSE fails. |
| 3 | `MX_GPIO_Init()` | Configure all GPIO pins (LEDs, button, gain select, NINA control). |
| 4 | `MX_DMA_Init()` | Enable DMA1 channels for ADC, DAC (×2), USART2 RX/TX. |
| 5 | `MX_ADC1_Init()` | ADC1 CH1 (PC0), 12-bit, continuous + DMA. |
| 6 | `MX_UART4_Init()` | UART4 115200 8N1, HW flow control (NINA BT module). |
| 7 | `MX_DAC1_Init()` | DAC1 Ch1 (PA4 freq tuning) + Ch2 (PA5 Q-factor tuning). |
| 8 | `MX_IWDG_Init()` | Independent watchdog, ~33 s timeout (prescaler 256, reload 4095). |
| 9 | `MX_TIM1_Init()` | TIM1 CH2 (PA9) — PWM carrier for 20 MHz excitation. |
| 10 | `MX_USART2_UART_Init()` | USART2 115200 8N1, no flow control (ST-LINK VCP). |
| 11 | `MX_TIM6_Init()` | TIM6 — ADC trigger timer for DMA sampling. |
| 12 | `HL_DAC_Init / Start` | Start both DAC channels via HAL driver wrapper. |
| 13 | `HL_ADC_Init / Start` | Start ADC + DMA circular buffer. |
| 14 | `HAL_PWM_Init` | Configure PWM to 20 MHz / 50 % duty (output **not** started — FSM controls). |
| 15 | LED indicator | `LED_INIT` on; `LED_ERR` on if clock fallback is active. |
| 16 | `FSM_Init()` | Initialise state machine → enters `STATE_INIT`. |

After step 16 the firmware enters the main loop.

### 12.2 Systemtakt-Konfiguration (HSE / MSI Fallback)

| Parameter | Normal | Fallback |
|-----------|--------|----------|
| Source | HSE 8 MHz + PLL | MSI 4 MHz |
| SYSCLK | 64 MHz | 4 MHz |
| Flash latency | 4 WS | 0 WS |
| PLL M / N / R | 2 / 16 / 2 | — (PLL off) |
| `g_clock_fallback_active` | 0 | 1 |

**Fallback logic**: if `HAL_RCC_OscConfig()` fails with HSE, the function retries with MSI at range 6 (4 MHz), sets `g_clock_fallback_active = 1`, and disables the PLL. This flag is checked later by `PC_HostBridge_Init()` to select polling RX instead of DMA.

**Clock Security System (CSS)**: enabled on the normal path via `HAL_RCC_EnableCSS()`. If HSE is lost at runtime, an NMI fires and the handler sets `g_clock_fallback_active = 1` and turns on `LED_ERR` — the device keeps running on the internal oscillator.

### 12.3 Peripherie-Initialisierung

Key peripheral parameters:

| Peripheral | Config | Notes |
|------------|--------|-------|
| **ADC1** | 12-bit, continuous, DMA circular, CH1 (PC0), 47.5-cycle sample time | Reads `NOTCH_AMP_IN` |
| **DAC1** | 2 channels, software trigger, output buffer enabled | CH1 = PA4 (freq), CH2 = PA5 (Q) |
| **USART2** | 115200 8N1, DMA RX/TX | ST-LINK VCP (PA2 TX / PA15 RX) |
| **UART4** | 115200 8N1, HW RTS/CTS | NINA BT (PA1 TX / PC10 RX) |
| **TIM1** | PWM mode 1, CH2 (PA9), fast mode | 20 MHz excitation output |
| **TIM6** | Up counter, internal trigger | ADC DMA trigger |
| **IWDG** | Prescaler 256, reload 4095 | ~33 s watchdog timeout |
| **DMA1** | 5 channels: ADC, DAC×2, USART2 RX/TX | All priority 0 |
| **GPIO** | 4 LED outputs, 1 button input (EXTI falling), 2 gain-select, NINA control, MCO on PA8 | — |

### 12.4 Hauptschleife (Event-Loop)

```c
while (1) {
    HAL_IWDG_Refresh(&hiwdg);   // feed watchdog
    FSM_RunOnce();               // one FSM iteration
}
```

The loop is purely polling-based with no sleep modes. Each `FSM_RunOnce()` call (see Ch. 6) polls the button, drains BT events, processes the event queue, and runs the current state handler. UART RX processing happens inside `FSM_RunOnce()` via `PC_HostBridge_Poll()`.

The watchdog timeout (~33 s) is generous enough to tolerate a full coarse + fine sweep without resetting.

### 12.5 Reset-Ursachen-Erkennung

On boot the firmware reads `RCC->CSR` reset flags, formats a human-readable cause string, and sends it as `STAT:RESET_CAUSE:<flags>`. Recognised flags:

| Flag | Meaning |
|------|---------|
| `IWDG` | Independent watchdog reset |
| `WWDG` | Window watchdog reset |
| `SW` | Software reset (`NVIC_SystemReset()`) |
| `PIN` | External reset pin (NRST) |
| `BOR` | Brown-out reset |
| `LPWR` | Low-power reset |

After reading, the flags are cleared via `__HAL_RCC_CLEAR_RESET_FLAGS()`. The reset cause is emitted before `STAT:BOOT_V2` during the boot output sequence.

Fault handlers (`HardFault`, `MemManage`, `BusFault`, `UsageFault`) turn on `LED_ERR` and call `NVIC_SystemReset()` so the device recovers automatically. A panic print function (`uart2_panic_print`) outputs `STAT:ERR:<msg>` directly on USART2 before the reset.

---

## 13. Tests

### 13.1 Überblick der Teststrategie

Testing is split into three levels:

| Level | Where | Hardware needed | Runner |
|-------|-------|-----------------|--------|
| **C unit tests** | `tests/test_main.c` | No (PC, mock board) | CMake + CTest |
| **On-target HW tests** | `Core/Src/test/test_hal_dac.c` | Yes (STM32 + scope) | Firmware entry point |
| **Python integration tests** | `tools/tests/` | Optional (`--port`) | pytest |

All three levels use the mock board (Ch. 4) as the simulation backend. No CI pipeline is configured — tests are run manually.

### 13.2 Unit-Tests

#### 13.2.1 Test HAL DAC (`test_hal_dac.c`)

On-target hardware test compiled into the firmware. Validates DAC startup, voltage sweeps (0 → 1.65 → 3.3 V), raw value writes (0 / 2048 / 4095), and a triangle waveform for oscilloscope verification.

```c
DAC_StatusTypeDef Test_HL_DAC_RunAll(DAC_HandleTypeDef *hdac);
void Test_HL_DAC_GenerateWaveform(DAC_HandleTypeDef *hdac, IWDG_HandleTypeDef *hiwdg);
```

#### 13.2.2 Weitere Unit-Tests

`tests/test_main.c` — desktop C tests built with CMake (`-DUNIT_TESTS=1`). Links the core firmware modules (FSM, BT manager, BSP, RF measure, math model) against the mock board and runs on the PC without hardware.

Seven test cases:

| Test | Verifies |
|------|----------|
| `test_init_reaches_idle` | FSM boots to `STATE_IDLE` |
| `test_meas_rejected_without_cal` | `CMD:MEAS` without calibration → `STAT:ERR` |
| `test_button_triggers_calibration` | Physical button → CALIBRATION state |
| `test_measurement_flow` | Full CAL → MEAS → `DAT:RES` sequence |
| `test_conn_status_leds_lcd` | `CMD:CONN` → `STAT:RDY`, correct LEDs and LCD |
| `test_calibration_status_ready_screen` | After CAL: LCD shows `"Ready"` |
| `test_measurement_status_result` | MEAS emits `STAT:MEAS` + `DAT:RES` with parsed values |

Build and run:

```bash
cd tests && cmake -B build && cmake --build build && ctest --output-on-failure
```

### 13.3 Integrationstests

Python-based tests in `tools/tests/`, executed via pytest.

**Offline regression** (`test_serial_lifecycle.py`): Loads a captured UART transcript (`tools/testdata/serial_lifecycle_idle.log`) and validates the protocol sequence — handshake order, LED snapshots, LCD content, trace index monotonicity, and UART error recovery — without any hardware.

**Live hardware** (`test_hw_command_surface.py`, marker `@pytest.mark.hardware`): Connects to the device via `--port COMx` and exercises the full command surface:

| Test | Scope |
|------|-------|
| `test_normal_operation_happy` | CONN → CAL → MEAS → `DAT:RES` with LED/LCD checks |
| `test_normal_operation_fail_path` | `CMD:MOCK:RF:FAIL:ON` → MEAS → `STAT:ERR` |
| `test_manual_mode_hal_read_write` | MANUAL ON → HAL LED/DAC/ADC/PWM/Gain/Button round-trips |
| `test_pc_cli_script_exercises_all_commands` | Replays `testdata/all_commands_hw.txt` |

Configuration via `conftest.py` fixtures: `hw_cfg` (port, baud, timeouts) and `serial_client` (auto-resets MCU before each test). Configurable through CLI args or environment variables (`PERMITTIVITY_METER_PORT`, etc.).

```bash
pytest tools/tests/ --port COM6 -m hardware -v      # live
pytest tools/tests/test_serial_lifecycle.py -v        # offline
```

### 13.4 PC-basierte Lifecycle-Tests (`tools/`)

#### 13.4.1 PC CLI (`pc_cli.py`)

Interactive and scripted serial terminal. Wraps a `SerialClient` class (threaded RX, line-based TX) with named commands (`conn`, `cal`, `meas`, `leds`, `lcd`, `hal-*`, `send <raw>`, `reset`). Supports `--script <file>` for automated command sequences with `wait <sec>` between steps.

```bash
python tools/pc_cli.py --port COM7                    # interactive
python tools/pc_cli.py --port COM7 --script cmds.txt  # scripted
```

#### 13.4.2 Lifecycle Test Script

`tools/run_hw_lifecycle.py` — standalone smoke test. Runs a boot → CONN → CAL → MEAS cycle and validates the `DAT:RES` response. Supports `--scenario fail` (enables mock RF failure, expects `STAT:ERR`) and `--probe-debug` (dumps LOG + TRACE during calibration).

```bash
python tools/run_hw_lifecycle.py --port COM7
python tools/run_hw_lifecycle.py --port COM7 --scenario fail
```

#### 13.4.3 PySimpleGUI Desktop-Tool

`tools/pc_ui.py` — graphical debug panel with LED indicators, LCD mirror, DAC/ADC sliders, PWM controls, button simulation, gain selection, NINA module control, manual-mode toggle, and RF mock parameter injection. Updates from `STAT:HW:*` push frames.

```bash
python tools/pc_ui.py --port COM4
```

### 13.5 Mock-basiertes Testen

The mock board (Ch. 4) is the enabler for all non-hardware tests. It provides:

| Capability | Used by |
|------------|---------|
| `MockBoard_RF_ComputeAmplitude` — deterministic RF response | C unit tests, lifecycle scripts |
| `MockBoard_RF_SetForceFailure` — inject `NAN` returns | Error-path tests (pytest `fail` scenario) |
| `MockBoard_BT_QueueCommand` / `GetLastTx` / `GetHistoryEntry` — command injection and TX capture | C unit tests (assertion on protocol output) |
| `MockBoard_UI_SetLED` / `GetLED` / `SetButton` — LED and button state | C unit tests (FSM LED/button assertions) |
| `CMD:MOCK:RF:*` over UART — runtime mock tuning | Live hardware tests, lifecycle scripts |

Test coverage summary:

| Component | C unit | On-target | pytest HW | pytest offline |
|-----------|--------|-----------|-----------|----------------|
| FSM transitions | ✓ | — | ✓ | ✓ |
| Protocol (CMD/STAT/DAT) | ✓ | — | ✓ | ✓ |
| LED / LCD UI | ✓ | — | ✓ | ✓ |
| RF measurement | ✓ | — | ✓ | — |
| DAC hardware | — | ✓ | ✓ | — |
| Error handling | ✓ | — | ✓ | ✓ |
| Manual / HAL mode | — | — | ✓ | — |

---

## 14. PC-Tools und externe Schnittstellen (`tools/`)

### 14.1 Überblick

Three Python tools communicate with the firmware via the ASCII `CMD:*` / `STAT:*` / `DAT:*` protocol over USB CDC (ST-LINK VCP) at 115200 8N1:

| Tool | Purpose | Hardware required |
|------|---------|-------------------|
| `pc_cli.py` | Interactive terminal + batch scripting | Optional (`loop://` for loopback) |
| `pc_ui.py` | PySimpleGUI desktop panel with real-time HAL control | Recommended |
| `run_hw_lifecycle.py` | Automated smoke test (boot → cal → meas → result) | Yes |

All tools share a common `SerialClient` class (defined in `pc_cli.py`):

```python
@dataclass
class SerialConfig:
    port: str        # e.g. "COM7", "/dev/ttyACM0", "loop://"
    baud: int        # default 115200
    timeout: float   # default 0.2 s
```

| Method | Description |
|--------|-------------|
| `SerialClient(cfg)` | Open port, spawn background RX reader thread. |
| `send_line(line)` | Encode + newline + write. |
| `expect(count, timeout)` | Block until `count` RX lines received. |
| `reset_target(pulse_s=0.08)` | DTR/RTS toggle for hardware reset. |
| `close()` | Stop reader, close port. |

Dependencies: `pyserial ≥ 3.5`, `PySimpleGUI ≥ 4.60` (GUI only), `pytest ≥ 8.0` (tests only). Install via `pip install -r tools/requirements.txt`.

### 14.2 PC CLI (`pc_cli.py`)

#### 14.2.1 Funktionalität

Interactive REPL with prompt `snow>`. Maps short-hand commands to firmware frames:

| Command | Firmware frame |
|---------|----------------|
| `conn` | `CMD:CONN` |
| `cal` | `CMD:CAL` |
| `meas` | `CMD:MEAS` |
| `manual-on` / `manual-off` | `CMD:MANUAL:ON` / `OFF` |
| `btn-press` / `btn-release` | `CMD:BTN:PRESS` / `RELEASE` |
| `leds` / `lcd` / `log` / `trace` | `CMD:LEDS` / `LCD` / `LOG` / `TRACE` |
| `reset` | `CMD:RESET` + DTR/RTS pulse |
| `send <RAW>` | Transmit arbitrary line verbatim |
| `exit` / `quit` | Close session |

HAL commands (require manual mode):

| Command | Firmware frame |
|---------|----------------|
| `hal-led-set <id> <0/1>` | `CMD:HAL:LED:SET:<id>:<0/1>` |
| `hal-led-get <id>` | `CMD:HAL:LED:GET:<id>` |
| `hal-led-toggle <id>` | `CMD:HAL:LED:TOGGLE:<id>` |
| `hal-adc-read` / `hal-adc-raw` | `CMD:HAL:ADC:READ` / `RAW` |
| `hal-dac-set <ch> <V>` | `CMD:HAL:DAC:SET:<ch>:<V>` |
| `hal-dac-raw <ch> <val>` | `CMD:HAL:DAC:RAW:<ch>:<val>` |
| `hal-gain-set <lvl>` / `hal-gain-get` | `CMD:HAL:GAIN:SET:<lvl>` / `GET` |
| `hal-btn-read` | `CMD:HAL:BTN:READ` |
| `hal-lcd-set <line> <text>` | `CMD:HAL:LCD:SET:<line>:<text>` |
| `hal-pwm-start` / `stop` / `get` | `CMD:HAL:PWM:START` / `STOP` / `GET` |
| `hal-pwm-freq <hz>` / `hal-pwm-duty <0..100>` | `CMD:HAL:PWM:FREQ:<hz>` / `DUTY:<pct>` |
| `hal-nina-rst <0/1>` / `hal-nina-stop <0/1>` | `CMD:HAL:NINA:RST:<0/1>` / `STOP:<0/1>` |

Supports batch scripts: one command per line, `#` comments, `wait [<s>]` for delays.

#### 14.2.2 Verwendung

```bash
python tools/pc_cli.py --port COM7                             # interactive
python tools/pc_cli.py --port COM7 --script workflow.txt       # batch + interactive
python tools/pc_cli.py --port COM7 --script workflow.txt --script-only  # batch only
python tools/pc_cli.py --port loop://                          # loopback (no HW)
```

Arguments: `--port`, `--baud` (115200), `--timeout` (0.2), `--script`, `--delay` (0.25), `--script-only`.

### 14.3 GUI-Tool (PySimpleGUI)

#### 14.3.1 Funktionalität

Full-featured desktop panel with the following control groups:

| Panel | Controls |
|-------|----------|
| **Connection** | Port/baud input, connect/disconnect/reset buttons, status display |
| **Mode** | RX mode, reset cause, boot status, manual-mode toggle, CAL/MEAS buttons |
| **RF Mock** | Fail toggle, resonance/noise/base inputs with apply |
| **LED Indicators** | 4 coloured boxes (green = on, grey = off, red = error) |
| **LED HAL** | Per-LED ON / OFF / TOGGLE / GET buttons (IDs 0–3) |
| **LCD** | 2-line mirror (read-only) + set-line inputs |
| **Button** | Press/release simulation + HW read |
| **ADC** | Voltage + raw readback |
| **DAC** | Per-channel slider (0.0–3.3 V, 0.01 step, 150 ms debounce) + raw input |
| **PWM** | Start/stop/get, frequency + duty inputs |
| **Gain** | Set/get RF gain (0–3) |
| **NINA** | Reset/stop pin checkboxes |
| **Serial Log** | Auto-scrolling RX log + raw command input |

The GUI parses incoming `STAT:HW:*` push frames to update all indicators in real time (LED colours, DAC voltage feedback, PWM state, etc.). An optional auto-refresh timer polls `CMD:LEDS` and `CMD:LCD` at a configurable interval.

#### 14.3.2 Verwendung

```bash
python tools/pc_ui.py --port COM4
python tools/pc_ui.py --port COM4 --baud 115200
```

### 14.4 Test-Skripte

`run_hw_lifecycle.py` — automated smoke test. See Chapter 13.4.2 for details.

Additional test data in `tools/testdata/`:

| File | Purpose |
|------|---------|
| `serial_lifecycle_idle.log` | Captured UART transcript for offline pytest regression |
| `all_commands_hw.txt` | Full HAL command surface script for live-hardware validation |

`tools/cli_workflow.txt` — example CLI batch script exercising connect → LCD/LED snapshots → calibration → measurement → trace dump.

---

## 15. Befehlsreferenz (Kommandoprotokoll)

### 15.1 Allgemeine Konventionen (Zeilenende, Encoding)

- **Encoding**: ASCII, 7-bit clean.
- **Line termination**: `\n` or `\r\n`. Firmware strips both.
- **Max line length**: 127 characters (longer lines are truncated).
- **Transport**: USART2 at 115200 8N1 (USB VCP). Same protocol planned for UART4 (Bluetooth).
- **Direction**: `CMD:*` host → device, `STAT:*` / `DAT:*` device → host.

### 15.2 Kontrollbefehle (`CMD:CONN`, `CMD:RESET`, `CMD:CAL`, `CMD:MEAS`, `CMD:BTN:*`)

| Command | Response | Description |
|---------|----------|-------------|
| `CMD:CONN` | `STAT:RDY` / `STAT:CAL` / `STAT:MEAS` / `STAT:MANUAL` | Handshake; response reflects current FSM state. |
| `CMD:RESET` | `STAT:RESETTING` → MCU reset | Reboot via `NVIC_SystemReset()` after 20 ms. |
| `CMD:CAL` | `STAT:CAL_REQ` → `STAT:CAL` → `STAT:CAL_OK` or `STAT:ERR` | Start air calibration (coarse + fine sweep). |
| `CMD:MEAS` | `STAT:MEAS_REQ` → `STAT:MEAS` → `DAT:RES:…` or `STAT:ERR:MEAS_INVALID` | Start snow measurement. Rejected with `STAT:ERR` if no valid calibration. |
| `CMD:BTN:PRESS` | `STAT:BTN_PRESS` | Simulate button press. |
| `CMD:BTN:RELEASE` | `STAT:BTN_REL` | Simulate button release. |

### 15.3 Debug- und Status-Befehle (`CMD:LEDS`, `CMD:LCD`, `CMD:LOG`, `CMD:TRACE`)

| Command | Response format | Description |
|---------|-----------------|-------------|
| `CMD:LEDS` | `STAT:LED:S:<0/1>:M:<0/1>:E:<0/1>:R:<0/1>` | LED snapshot (Status, Meas, Excite, Error). |
| `CMD:LCD` | `DAT:LCD:L0:<text>` + `DAT:LCD:L1:<text>` | LCD line buffer contents. |
| `CMD:LOG` | `DAT:LOG:D:<domain>:S:<state>:<msg>` per entry | Debug ring buffer dump. `STAT:LOG_EMPTY` if empty. |
| `CMD:TRACE` | `DAT:TRACE:<mode>:<idx>:V:<volt>:A:<amp>` per sample | Last RF sweep trace. `STAT:TRACE_EMPTY` if empty. |

### 15.4 Mock-Befehle (`CMD:MOCK:RF:*`)

| Command | Response | Default | Description |
|---------|----------|---------|-------------|
| `CMD:MOCK:RF:RES:<float>` | `STAT:MOCK_RF_RES` | 1.2 V | Set resonance voltage. |
| `CMD:MOCK:RF:NOISE:<float>` | `STAT:MOCK_RF_NOISE` | 0.01 V | Set noise level. |
| `CMD:MOCK:RF:BASE:<float>` | `STAT:MOCK_RF_BASE` | 1.0 V | Set base amplitude. |
| `CMD:MOCK:RF:FAIL:ON` | `STAT:MOCK_RF_FAIL_ON` | off | Force all reads to return `NAN`. |
| `CMD:MOCK:RF:FAIL:OFF` | `STAT:MOCK_RF_FAIL_OFF` | — | Resume normal mock operation. |

Invalid mock commands return `STAT:MOCK_ERR`.

### 15.5 HAL-Board-Befehle (`CMD:HAL:*`)

All `CMD:HAL:*` commands are **locked** unless manual mode is active (see 15.6). Returns `STAT:HAL_LOCKED` when locked. Invalid sub-commands return `STAT:HAL_CMD_ERR`.

Successful commands emit both a legacy `STAT:HAL_*` acknowledgement and a structured `STAT:HW:*` push frame (see 15.7).

| Command | Response | Notes |
|---------|----------|-------|
| `CMD:HAL:INIT` | `STAT:HAL_INIT_OK` | Re-initialise HAL board module. |
| **LED** |||
| `CMD:HAL:LED:SET:<id>:<0/1>` | `STAT:HAL_LED_<id>_ON/OFF` | IDs: 0=Status, 1=Meas, 2=Excite, 3=Error. |
| `CMD:HAL:LED:GET:<id>` | `STAT:HAL_LED_<id>:<0/1>` | |
| `CMD:HAL:LED:TOGGLE:<id>` | `STAT:HAL_LED_<id>_TOG` | |
| **Button** |||
| `CMD:HAL:BTN:READ` | `STAT:HAL_BTN:PRESSED/RELEASED` | Physical button state (PC13). |
| **ADC** |||
| `CMD:HAL:ADC:READ` | `STAT:HAL_ADC:<volts>V` | 12-bit → voltage. |
| `CMD:HAL:ADC:RAW` | `STAT:HAL_ADC:<raw>` | 12-bit raw value. |
| **DAC** |||
| `CMD:HAL:DAC:SET:<ch>:<volts>` | `STAT:HAL_DAC_<ch>:<volts>V` | Ch 0 = freq (PA4), Ch 1 = Q (PA5). |
| `CMD:HAL:DAC:RAW:<ch>:<val>` | `STAT:HAL_DAC_<ch>:<val>` | 12-bit raw (0–4095). |
| **LCD** |||
| `CMD:HAL:LCD:SET:<line>:<text>` | `STAT:HAL_LCD_L<line>_OK` | Line 0/1, pad/truncate to 16 chars. |
| **PWM** |||
| `CMD:HAL:PWM:START` | `STAT:HAL_PWM_START_OK` | Enable TIM1 CH2 output on PA9. |
| `CMD:HAL:PWM:STOP` | `STAT:HAL_PWM_STOP_OK` | Disable PWM output. |
| `CMD:HAL:PWM:GET` | `STAT:HAL_PWM_OK` | Returns current run/freq/duty via `STAT:HW:PWM:*`. |
| `CMD:HAL:PWM:FREQ:<hz>` | `STAT:HAL_PWM_FREQ_OK` | Set frequency (e.g. 20000000). |
| `CMD:HAL:PWM:DUTY:<0..100>` | `STAT:HAL_PWM_DUTY_OK` | Set duty cycle percent. |
| **Gain** |||
| `CMD:HAL:GAIN:SET:<0..3>` | `STAT:HAL_GAIN:<level>` | RF gain select (PC8/PC9). |
| `CMD:HAL:GAIN:GET` | `STAT:HAL_GAIN:<level>` | |
| **NINA** |||
| `CMD:HAL:NINA:RST:<0/1>` | `STAT:HAL_NINA:RESET/RUN` | 0 = hold reset, 1 = run. |
| `CMD:HAL:NINA:STOP:<0/1>` | `STAT:HAL_NINA:RUNNING/STOPPED` | 0 = run, 1 = stop. |

### 15.6 Manual-Mode-Befehle (`CMD:MANUAL:*`)

| Command | Response | Description |
|---------|----------|-------------|
| `CMD:MANUAL:ON` | `STAT:MANUAL_ON_REQ` → `STAT:MANUAL_ON` | Enter manual mode from any state. Disables excitation/op-amp. Unlocks `CMD:HAL:*`. |
| `CMD:MANUAL:OFF` | `STAT:MANUAL_OFF_REQ` → `STAT:MANUAL_OFF` | Return to IDLE. Locks `CMD:HAL:*` again. |

While manual mode is active: `CMD:CAL` / `CMD:MEAS` → `STAT:MANUAL_ACTIVE` (rejected).

### 15.7 Antwortformate (`STAT:*`, `DAT:*`)

**Status frames** (`STAT:*`):

| Pattern | Example | Origin |
|---------|---------|--------|
| `STAT:<tag>` | `STAT:RDY`, `STAT:CAL_OK`, `STAT:ERR` | General status / ack. |
| `STAT:LED:S:…:M:…:E:…:R:…` | `STAT:LED:S:1:M:0:E:0:R:0` | LED snapshot response. |
| `STAT:BOOT_V2` | — | Emitted on power-up. |
| `STAT:UART_RX:<mode>` | `STAT:UART_RX:DMA_IDLE` | RX mode report (once at boot). |
| `STAT:RESET_CAUSE:<flags>` | `STAT:RESET_CAUSE:PIN` | RCC reset flags on boot. |
| `STAT:HW:<payload>` | `STAT:HW:LED:0:1` | Push-style hardware state (for GUI). |

**Data frames** (`DAT:*`):

| Pattern | Example |
|---------|---------|
| `DAT:RES:ER:<ε'>:EI:<ε''>:DENS:<ρ>` | `DAT:RES:ER:1.540:EI:0.020:DENS:150.300` |
| `DAT:LCD:L<0/1>:<text>` | `DAT:LCD:L0:IDLE` |
| `DAT:TRACE:<mode>:<idx>:V:<v>:A:<a>` | `DAT:TRACE:CAL:0:V:0.500:A:1.230` |
| `DAT:LOG:D:<domain>:S:<state>:<text>` | `DAT:LOG:D:STATE:S:IDLE:FSM:IDLE→CAL` |

**Push-style hardware frames** (`STAT:HW:*`) — emitted after every successful `CMD:HAL:*`:

| Frame | Values |
|-------|--------|
| `STAT:HW:LED:<id>:<0/1>` | LED state |
| `STAT:HW:DAC:<ch>:V:<volts>` | DAC voltage |
| `STAT:HW:DAC:<ch>:RAW:<val>` | DAC raw |
| `STAT:HW:ADC:V:<volts>` / `RAW:<val>` | ADC readback |
| `STAT:HW:GAIN:<0..3>` | RF gain |
| `STAT:HW:BTN:<0/1>` | Button state |
| `STAT:HW:NINA:RST:<0/1>` / `STOP:<0/1>` | NINA control |
| `STAT:HW:PWM:RUN:<0/1>` / `FREQ:<hz>` / `DUTY:<pct>` | PWM state |

---

## 16. Konfiguration und Build

### 16.1 Projektstruktur (Verzeichnisse und Dateien)

```
Permittivity-Meter/
├── README.md                       Main firmware documentation
├── ToDos.md / Milestones.md        Development roadmap
├── Dokumentation/                  Hardware & signal-path docs
├── tools/                          PC utilities (CLI, GUI, tests)
├── tests/                          Host-side C unit tests (CMake)
├── PermittivityMeterV2/
│   └── PermitivityMeterV2/
│       ├── PermitivityMeterV2.ioc          CubeMX device config
│       ├── STM32L476RGTX_FLASH.ld          Linker script (Flash)
│       ├── STM32L476RGTX_RAM.ld            Linker script (RAM)
│       ├── Core/
│       │   ├── Src/                        Application source
│       │   │   ├── main.c, fsm_main.c, rf_measure.c, …
│       │   │   ├── hl/                     HAL driver wrappers
│       │   │   ├── mocks/                  Mock board
│       │   │   └── test/                   On-target test code
│       │   ├── Inc/                        Application headers
│       │   └── Startup/                    startup_stm32l476rgtx.s
│       └── Drivers/
│           ├── STM32L4xx_HAL_Driver/       ST HAL library
│           └── CMSIS/                      Cortex CMSIS core
└── NINA software von okorn/        Legacy BT module code (not part of V2)
```

### 16.2 Build-Umgebung und Toolchain

| Component | Details |
|-----------|---------|
| IDE | STM32CubeIDE (Eclipse-based CDT) |
| Compiler | ARM GCC (`arm-none-eabi-gcc`) |
| FPU | FPv4-SP-D16, hard float ABI |
| Debug level | `-g3` (full symbols) |
| Code generator | STM32CubeMX (`.ioc` file) |
| Debugger | ST-LINK (integrated on Nucleo) |
| Unit tests | CMake + GCC on host PC |

### 16.3 Compiler-Flags und Defines

Firmware defines:

| Define | Purpose |
|--------|---------|
| `DEBUG` | Enable debug features |
| `USE_HAL_DRIVER` | Use ST HAL library |
| `STM32L476xx` | Target MCU family |
| `UART_SMOKE_TEST` (0/1) | If 1: minimal USART2 echo firmware for bring-up |

Unit test defines:

| Define | Purpose |
|--------|---------|
| `UNIT_TESTS=1` | Replace HAL calls with mock stubs; enables host-side compilation |

### 16.4 Linker-Konfiguration

Memory regions (`STM32L476RGTX_FLASH.ld`):

| Region | Start | Size | Purpose |
|--------|-------|------|---------|
| FLASH | `0x08000000` | 1024 KB | Program code + constants |
| RAM | `0x20000000` | 96 KB | Data, BSS, stack, heap |
| RAM2 | `0x10000000` | 32 KB | CCM (optional) |

Stack: 1 KB (`_Min_Stack_Size = 0x400`). Heap: 512 B (`_Min_Heap_Size = 0x200`).

Key sections: `.isr_vector` → FLASH (interrupt vectors), `.text` → FLASH, `.rodata` → FLASH, `.data` → RAM (initialised from FLASH), `.bss` → RAM.

### 16.5 Abhängigkeiten (STM32 HAL, CMSIS)

| Library | Version | Source |
|---------|---------|--------|
| STM32L4xx HAL Driver | CubeMX-generated | `Drivers/STM32L4xx_HAL_Driver/` |
| CMSIS Core | ARM CMSIS 5 | `Drivers/CMSIS/` |
| CMSIS Device | STM32L4xx device headers | `Drivers/CMSIS/Device/ST/STM32L4xx/` |
| pyserial | ≥ 3.5 | PC tools (serial communication) |
| PySimpleGUI | ≥ 4.60 | PC GUI tool only |
| pytest | ≥ 8.0 | PC-side test runner |

---

## 17. Bekannte Einschränkungen und offene Punkte

### 17.1 Aktuelle Limitierungen

| Limitation | Impact | Workaround |
|------------|--------|------------|
| `bsp_rf.c` delegates exclusively to mock board | No real RF measurement possible | Use `CMD:MOCK:RF:*` for simulated sweeps |
| No undersampling / DFT implemented | Single-point amplitude read, no narrowband filtering | Planned Goertzel algorithm (Ch. 9.3.3) |
| UART4 / NINA BT transport not wired | Bluetooth control unavailable | Use USART2 (USB VCP) only |
| LCD is buffer-only (no I2C flush) | Physical display not updated | Read buffer via `CMD:LCD` |
| Permittivity formula is placeholder | `ε' = 1 + 0.5×C_air/C_snow` — not production-grade | Replace with Denoth's model |
| Snow density is linear estimate | `ρ = 0.3 + shift × 0.1` | Replace with validated empirical model |
| PWM output is 3.3 Vpp | RF circuit needs ≤ 1 Vpp excitation | External OpAmp attenuator required (HW) |
| No `CMD:RF:STAT` readback | Cannot query instantaneous RF state | Infer from `CMD:TRACE` or `CMD:LEDS` |
| `CMD:HAL:*` implemented but not fully tested | May have edge-case issues | Test coverage in `test_hw_command_surface.py` |

### 17.2 Geplante Erweiterungen

**Phase 1 — Hardware prerequisites** (blocking):
- [ ] Add OpAmp buffer/divider after PA9 (3.3 V → 1 Vpp)
- [ ] Verify varicap wiring PA4/PA5 → D1/D2

**Phase 2 — Mock-to-hardware transition** (high priority):
- [ ] Replace `MockBoard_RF_*` calls in `bsp_rf.c` with `HL_DAC_SetVoltage()` + `HL_ADC_Read()`
- [ ] Add settling delay in `sample_at()` for varicap response time
- [ ] Validate DAC voltages on multimeter and PWM on oscilloscope

**Phase 3 — Signal processing (undersampling)**:
- [ ] Configure TIM6 to ~800.1 kHz for bandpass sampling of 20 MHz
- [ ] Implement `BSP_RF_CaptureBuffer()` with DMA
- [ ] Implement Goertzel single-bin magnitude in `math_model.c`
- [ ] Replace single-point reads with buffer + DFT in `rf_measure.c`

**Phase 4 — Calibration & measurement algorithms**:
- [ ] Implement Denoth's formula: `ε' = 1 + k_D × log(V_M / V_Ref)`
- [ ] Two-varicap sweep: coarse on D1 (PA4), fine on D2 (PA5)

**Phase 5 — UI & communication**:
- [ ] Display formatted result on LCD (e.g. `"E:1.54 D:0.32"`)
- [ ] Wire UART4 RX/TX transport for Bluetooth
- [ ] Echo responses to both USB and BT transports
- [x] ~~USB input via USART2~~ — done (DMA Receive-to-Idle)

**Phase 6 — Power & environmental**:
- [ ] Component selection for −40…+85 °C operation
- [ ] Battery input + regulator design
- [ ] Define final "Permittivity Shield v1.0" pinout

### 17.3 Offene To-Dos (siehe ToDos.md / Milestones.md)

No `TODO` or `FIXME` comments remain in the active V2 source code (`Core/`). All tracked development items are maintained in `ToDos.md` and `Milestones.md` at the repository root.

Critical blockers:

| Blocker | Status | Impact |
|---------|--------|--------|
| OpAmp attenuation (Phase 1.1) | Not started | Cannot drive RF circuit safely |
| BSP → real HAL (Phase 2.1) | Mock only | No real measurements |
| Undersampling DSP (Phase 3) | Not started | Single-point sampling, high noise |
| Permittivity formula (Phase 4) | Placeholder | Results not physically meaningful |
| UART4 / NINA transport (Phase 5.2) | Not started | No Bluetooth |
