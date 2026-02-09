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

### 5.2 BSP RF (`bsp_rf.c` / `bsp_rf.h`)

#### 5.2.1 Zweck (Abstraktion des RF-Frontends)

#### 5.2.2 Bereitgestellte Funktionen (`BSP_RF_Init`, `BSP_RF_SetFreqVoltage`, `BSP_RF_SetQVoltage`, `BSP_RF_ReadAmplitude`)

#### 5.2.3 Switch-Point: Mock vs. reale Hardware

#### 5.2.4 Geplante Erweiterungen (DMA Buffer Capture)

### 5.3 BSP UI (`bsp_ui.c` / `bsp_ui.h`)

#### 5.3.1 Zweck (Button- und LED-Verwaltung)

#### 5.3.2 Bereitgestellte Funktionen

#### 5.3.3 Button-Logik und Entprellung

#### 5.3.4 LED-Zustandsverwaltung

### 5.4 BSP LCD (`bsp_lcd.c` / `bsp_lcd.h`)

#### 5.4.1 Zweck (LCD-Pufferverwaltung)

#### 5.4.2 Bereitgestellte Funktionen

#### 5.4.3 I2C-Anbindung und Zeilenpuffer

---

## 6. Applikationsschicht – Finite State Machine (`fsm_main.c`)

### 6.1 Überblick und Verantwortlichkeit

### 6.2 Zustandsdiagramm

### 6.3 Zustände im Detail

#### 6.3.1 `STATE_INIT`

#### 6.3.2 `STATE_IDLE`

#### 6.3.3 `STATE_CALIBRATION`

#### 6.3.4 `STATE_MEASURE_SEARCH`

#### 6.3.5 `STATE_MANUAL_OPERATION`

#### 6.3.6 `STATE_ERROR`

#### 6.3.7 `STATE_CALCULATION` (geplant)

### 6.4 Events und Event-Queue

#### 6.4.1 FSM-Event-Typen

#### 6.4.2 Event-Quellen (Button, BT-Manager)

#### 6.4.3 Event-Verarbeitung und Priorisierung

### 6.5 Zustandsübergänge

### 6.6 Fehlerbehandlung und Recovery

---

## 7. Messlogik (`rf_measure.c`)

### 7.1 Überblick und Verantwortlichkeit

### 7.2 Kalibrierung (Air Calibration)

#### 7.2.1 Ablauf (`RF_PerformAirCalibration`)

#### 7.2.2 Coarse Sweep

#### 7.2.3 Fine Sweep

#### 7.2.4 Parabolische Interpolation

#### 7.2.5 Ergebnis und Speicherung

### 7.3 Messung (Snow Measurement)

#### 7.3.1 Ablauf (`RF_PerformSnowMeasurement`)

#### 7.3.2 Suchbereich relativ zur Kalibrierung

#### 7.3.3 Ergebnisberechnung

### 7.4 Sampling-Funktion (`sample_at`)

### 7.5 Konfigurierbare Parameter (Sweep-Bereich, Schrittweiten)

### 7.6 Fehlerbehandlung (ungültige Messungen)

---

## 8. RF Trace (`rf_trace.c`)

### 8.1 Zweck (Sweep-Daten-Aufzeichnung)

### 8.2 Bereitgestellte Funktionen

### 8.3 Datenformat und Ausgabe (`DAT:TRACE:*`)

### 8.4 Nutzung zur Diagnose

---

## 9. Mathematisches Modell (`math_model.c`)

### 9.1 Zweck und Verantwortlichkeit

### 9.2 Permittivitätsberechnung

### 9.3 Signalverarbeitung (Undersampling / Bandpass Sampling)

#### 9.3.1 Prinzip des Undersamplings

#### 9.3.2 Alias-Frequenz-Berechnung

#### 9.3.3 DFT/Goertzel-Algorithmus (geplant)

### 9.4 Fehlerbetrachtung und Genauigkeit

---

## 10. Debug-Logging (`debug_log.c`)

### 10.1 Zweck (interner Ringpuffer für Diagnose)

### 10.2 Bereitgestellte Funktionen

### 10.3 Log-Domänen und Filterung

### 10.4 Ausgabe über `CMD:LOG`

---

## 11. Kommunikationsschicht – Protokoll und Transport

### 11.1 Überblick (ASCII-Protokoll)

### 11.2 BT-Manager / Protokoll-Parser (`bt_manager.c`)

#### 11.2.1 Zweck und Verantwortlichkeit

#### 11.2.2 Protokollstruktur (`CMD:*`, `STAT:*`, `DAT:*`)

#### 11.2.3 Befehlsverarbeitung (`BT_ProcessIncoming`)

#### 11.2.4 Befehlsrouting und Dispatch

#### 11.2.5 Antwortgenerierung (`BT_Send`, `BT_Printf`)

#### 11.2.6 Integration mit FSM (Event-Weiterleitung)

#### 11.2.7 Integration mit HAL Board (CMD:HAL:\*-Routing)

#### 11.2.8 Integration mit Mock Board (CMD:MOCK:\*-Routing)

### 11.3 USB CDC Bridge / USART2 Transport (`usb_cdc_bridge.c`)

#### 11.3.1 Zweck (Zeilenweise Empfangs-/Sendeschnittstelle)

#### 11.3.2 DMA Receive-to-Idle (bevorzugt)

#### 11.3.3 Interrupt-basierter RX (Fallback)

#### 11.3.4 Polling RX (Clock-Fallback)

#### 11.3.5 RX-Byte-Ringpuffer

#### 11.3.6 RX-Line-Queue (zeilenweise Verarbeitung)

#### 11.3.7 TX-Ausgabe

### 11.4 Bluetooth-Kommunikation (`bt_communication.c`) (geplant)

#### 11.4.1 UART4 / NINA-Modul

#### 11.4.2 Geplante Integration in den Protokoll-Parser

---

## 12. Hauptprogramm und Initialisierung (`main.c`)

### 12.1 Boot-Sequenz

### 12.2 Systemtakt-Konfiguration (HSE / MSI Fallback)

### 12.3 Peripherie-Initialisierung

### 12.4 Hauptschleife (Event-Loop)

### 12.5 Reset-Ursachen-Erkennung

---

## 13. Tests

### 13.1 Überblick der Teststrategie

### 13.2 Unit-Tests

#### 13.2.1 Test HAL DAC (`test_hal_dac.c`)

#### 13.2.2 Weitere Unit-Tests

### 13.3 Integrationstests

### 13.4 PC-basierte Lifecycle-Tests (`tools/`)

#### 13.4.1 PC CLI (`pc_cli.py`)

#### 13.4.2 Lifecycle Test Script

#### 13.4.3 PySimpleGUI Desktop-Tool

### 13.5 Mock-basiertes Testen

---

## 14. PC-Tools und externe Schnittstellen (`tools/`)

### 14.1 Überblick

### 14.2 PC CLI (`pc_cli.py`)

#### 14.2.1 Funktionalität

#### 14.2.2 Verwendung

### 14.3 GUI-Tool (PySimpleGUI)

#### 14.3.1 Funktionalität

#### 14.3.2 Verwendung

### 14.4 Test-Skripte

---

## 15. Befehlsreferenz (Kommandoprotokoll)

### 15.1 Allgemeine Konventionen (Zeilenende, Encoding)

### 15.2 Kontrollbefehle (`CMD:CONN`, `CMD:RESET`, `CMD:CAL`, `CMD:MEAS`, `CMD:BTN:*`)

### 15.3 Debug- und Status-Befehle (`CMD:LEDS`, `CMD:LCD`, `CMD:LOG`, `CMD:TRACE`)

### 15.4 Mock-Befehle (`CMD:MOCK:RF:*`)

### 15.5 HAL-Board-Befehle (`CMD:HAL:*`)

### 15.6 Manual-Mode-Befehle (`CMD:MANUAL:*`)

### 15.7 Antwortformate (`STAT:*`, `DAT:*`)

---

## 16. Konfiguration und Build

### 16.1 Projektstruktur (Verzeichnisse und Dateien)

### 16.2 Build-Umgebung und Toolchain

### 16.3 Compiler-Flags und Defines

### 16.4 Linker-Konfiguration

### 16.5 Abhängigkeiten (STM32 HAL, CMSIS)

---

## 17. Bekannte Einschränkungen und offene Punkte

### 17.1 Aktuelle Limitierungen

### 17.2 Geplante Erweiterungen

### 17.3 Offene To-Dos (siehe ToDos.md / Milestones.md)
