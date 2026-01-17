# Permittivity Meter Firmware

**Target Hardware**: STM32L476RG (NUCLEO-L476RG)  
**Version**: 1.2 (Dev)  
**Date**: Dec 2025

This project implements the firmware for a **Permittivity Meter**, a device designed to measure the dielectric properties of materials (specifically snow) using an RF resonance circuit. The system sweeps a frequency range, detects resonance shifts, and calculates permittivity and density.

---

## 1. System Overview

The firmware is architected around a **Finite State Machine (FSM)** that manages the complex lifecycle of calibration, measurement, and calculation. It separates high-level application logic from low-level hardware drivers using a Board Support Package (BSP) layer.

### Application Lifecycle (FSM)

The system moves through the following states to perform a measurement:

```mermaid
stateDiagram-v2
    [*] --> STATE_INIT
    STATE_INIT --> STATE_IDLE : FSM_EVENT_INIT_DONE

    %% IDLE entry points
    STATE_IDLE --> STATE_CALIBRATION : CMD CAL or Button when no CAL
    STATE_IDLE --> STATE_MEASURE_SEARCH : CMD MEAS or Button when CAL valid
    STATE_IDLE --> STATE_IDLE : CMD CONN / STAT RDY

    %% Manual mode is a safety override and can be entered from any state.
    STATE_INIT --> STATE_MANUAL_OPERATION : CMD MANUAL ON
    STATE_IDLE --> STATE_MANUAL_OPERATION : CMD MANUAL ON
    STATE_CALIBRATION --> STATE_MANUAL_OPERATION : CMD MANUAL ON
    STATE_MEASURE_SEARCH --> STATE_MANUAL_OPERATION : CMD MANUAL ON
    STATE_ERROR --> STATE_MANUAL_OPERATION : CMD MANUAL ON

    STATE_MANUAL_OPERATION --> STATE_IDLE : CMD MANUAL OFF
    STATE_MANUAL_OPERATION --> STATE_MANUAL_OPERATION : CMD CONN / STAT MANUAL

    %% Calibration
    STATE_CALIBRATION --> STATE_IDLE : STAT CAL_OK
    STATE_CALIBRATION --> STATE_ERROR : STAT ERR

    %% Measurement
    STATE_MEASURE_SEARCH --> STATE_IDLE : DAT RES ...
    STATE_MEASURE_SEARCH --> STATE_ERROR : STAT ERR MEAS_INVALID

    %% Error recovery
    STATE_ERROR --> STATE_INIT : Button Press reset
    STATE_ERROR --> STATE_INIT : CMD CONN forces re-init
```

1. **STATE_INIT**: Initializes modules (RF mock/BSP, UI, LCD, command protocol).
2. **STATE_IDLE**: Waits for input (button) or `CMD:*` frames. `CMD:CONN` responds with `STAT:RDY` in this state.
3. **STATE_CALIBRATION**: Runs `RF_PerformAirCalibration()` and returns `STAT:CAL_OK` or `STAT:ERR`.
4. **STATE_MEASURE_SEARCH**: Runs `RF_PerformSnowMeasurement()` and returns `DAT:RES:...` or `STAT:ERR:MEAS_INVALID`.
5. **STATE_MANUAL_OPERATION**: Safety/manual override. RF outputs are forced OFF. `CMD:HAL:*` is only accepted here.
6. **STATE_ERROR**: Indicates a failure; button press (or `CMD:CONN`) re-initializes.

Note: `CMD:CONN` is state-dependent: it returns `STAT:RDY` (IDLE/ERROR), `STAT:CAL` (CALIBRATION), `STAT:MEAS` (MEASURE_SEARCH), or `STAT:MANUAL` (MANUAL_OPERATION).

Note: `STATE_CALCULATION` exists in the enum for future expansion, but the current V2 flow sends the result directly from the measurement state.

---

## Bring-up Notes (Dec 2025)

On some custom boards the external HSE (e.g., a bare 20 MHz crystal) may fail to start. Historically this caused the firmware to hang inside `SystemClock_Config()`.

The firmware now:

- Falls back to MSI so the device stays controllable.
- Uses DMA Receive-to-Idle on USART2 when RX DMA is configured (recommended). If clock-fallback is active and no RX DMA is available, it falls back to polling RX.

Expected boot output (order) on USART2 (ST-LINK VCP):

```text
STAT:UART_RX:DMA_IDLE   (preferred)
# or: STAT:UART_RX:IT_IDLE
# or: STAT:UART_RX:POLL (only if clock-fallback is active and no RX DMA)
STAT:RESET_CAUSE:<...>
STAT:BOOT_V2
```

After that, `CMD:CONN` should respond with exactly one `STAT:RDY`.

---

## 2. Hardware Configuration

### Pinout Configuration

| Pin | Name | Function | Description |
| :--- | :--- | :--- | :--- |
| **PC13** | `B1` | **User Button** | Triggers Calibration (1st press) or Measurement (2nd press). |
| **PA6** | `INIT_LED` | **LED (Green)** | Indicates System Initialization / Idle State. |
| **PA7** | `MEAS_LED` | **LED (Blue)** | Indicates Measurement/Calibration in progress. |
| **PC7** | `EXCITE_LED` | **LED (Yellow)** | Indicates RF Excitation is Active. |
| **PB6** | `ERR_LED` | **LED (Red)** | Indicates Error State. |
| **PA4** | `FRQ_TN` | **DAC1 Ch1** | Frequency Tuning Varicap Voltage. |
| **PA5** | `Q_FACT_TN` | **DAC1 Ch2** | Q-Factor Tuning Varicap Voltage. |
| **PA9** | `SQR_20M_OUT` | **TIM1 CH2** | 20 MHz PWM Excitation Signal. |
| **PA1** | `NINA_RX` | **UART4 RX** | Bluetooth Module RX. |
| **PA0** | `NINA_TX` | **UART4 TX** | Bluetooth Module TX. |
| **PA2** | `VCP_TX` | **USART2 TX** | USB Virtual COM Port TX. |
| **PA3** | `VCP_RX` | **USART2 RX** | USB Virtual COM Port RX. |
| **PC8** | `GAIN_SLCT_1` | **GPIO Out** | RF Gain Select Bit 0. |
| **PC9** | `GAIN_SLCT_2` | **GPIO Out** | RF Gain Select Bit 1. |
| **PB8** | `I2C1_SCL` | **I2C SCL** | LCD Display Clock. |
| **PB9** | `I2C1_SDA` | **I2C SDA** | LCD Display Data. |

### Electronics Notes

* **PWM Output**: The GPIO (PA9) outputs 3.3V, but the RF circuit requires a **1V peak**. An **OpAmp buffer** with attenuation is required between the MCU and the circuit.
* **Varicap Control**: The DAC outputs (PA4, PA5) control the Varicaps. Channel 1 tunes the frequency (D1), and Channel 2 tunes the Q-factor.

---

## 3. Software Architecture

The project follows a layered architecture to ensure testability and separation of concerns.

```mermaid
flowchart LR
        PC["PC / Test Runner\n(USART2 / ST-LINK VCP)"]

        subgraph TRANSPORT["Transport + Line Assembly"]
            U2["usb_cdc_bridge.c\nUSART2 RX: DMA Receive-to-Idle\nRX byte ring + RX line queue"]
        end

        subgraph PROTO["ASCII Protocol + Event Queues"]
            BT["bt_manager.c\nBT_ProcessIncoming()\n- parses CMD:*\n- emits STAT:/DAT:\n- queues BT events"]
            FSMQ["fsm_main.c\nFSM event queue\n(button + BT events)"]
        end

        subgraph APP["Application / Measurement"]
            FSM["fsm_main.c\nAppState: INIT/IDLE/CAL/MEAS/MANUAL/ERROR"]
            RF["rf_measure.c\ncalibration + measurement routines"]
            TRACE["rf_trace.c\ntrace capture for CMD:TRACE"]
        end

        subgraph BSP["BSP Layer (Switch Point)"]
            BSP_RF["bsp_rf.c\nRF front-end abstraction\n(Current: MOCK)"]
            BSP_UI["bsp_ui.c\nButton + LEDs"]
            BSP_LCD["bsp_lcd.c\nLCD buffer + I2C"]
        end

        subgraph MOCK["Mock / Test Support"]
            MOCKBOARD["mocks/mock_board.c\nRF + BT loopback"]
            DBG["debug_log.c\nring buffer for CMD:LOG"]
        end

        subgraph HAL["HAL Board (Manual Mode)"]
            HLB["hl/hal_board.c\nDirect hardware control (CMD:HAL:*)"]
        end

        PC -->|"ASCII lines (\n terminated)"| U2
        U2 -->|"Complete lines"| BT
        BT -->|"BT events"| FSMQ
        FSMQ --> FSM
        FSM --> RF
        FSM --> BSP_UI
        FSM --> BSP_LCD
        RF --> BSP_RF
        TRACE --> BT

        BSP_RF --> MOCKBOARD
        BT --> DBG

        BT -->|"STAT:/DAT: frames"| U2
        BT --> HLB
```

### File Purpose & Hardware Interaction

| File Path | Purpose | Hardware vs. Mock |
| :--- | :--- | :--- |
| **`Core/Src/fsm_main.c`** | **The Brain**. Controls the high-level state (Idle, Calibrate, Measure). | **Pure Logic**. Does not touch hardware directly. |
| **`Core/Src/rf_measure.c`** | **The Scientist**. Implements the sweep algorithms (Coarse/Fine). | **Logic**. Calls `bsp_rf.c` to set voltages/read values. |
| **`Core/Src/bsp_rf.c`** | **The Bridge**. The Board Support Package (BSP) for the RF frontend. | **Switch Point**. Currently **MOCK**. Needs to call HAL drivers. |
| **`Core/Src/hl/hal_*.c`** | **The Drivers**. Wrappers around STM32 HAL. | **Real Hardware**. Directly manipulates registers. |
| **`Core/Src/bt_manager.c`** | **The Communicator**. Handles ASCII protocol (`CMD:`, `DAT:`). | **Real Hardware**. Mirrors traffic to UART4 (BT) and USART2 (USB). |

### Top-down block diagram (presentation view)

```mermaid
flowchart TB
    subgraph SRC["Command sources"]
        PC["PC / USB VCP (USART2)"]
        NINA["NINA BT module (UART4)"]
    end

    subgraph INGRESS["Parser + bridge"]
        Parser["bt_manager.c\nASCII command parser\nqueues FSM events"]
    end

    subgraph CTRL["Control"]
        FSM["fsm_main.c\nApplication FSM"]
    end

    subgraph AUTO["Automatic path"]
        RF["rf_measure.c\nCAL / MEAS logic"]
        Mock["mocks/mock_board.c\nMock RF + BT loopback"]
        BSP["bsp_rf.c\nBSP switch toward real RF"]
    end

    subgraph MANUAL["Manual / HAL path"]
        Manual["STATE_MANUAL_OPERATION"]
        HAL["hl/hal_board.c\nDirect hardware control"]
    end

    SRC --> INGRESS --> FSM
    FSM --> RF
    RF --> Mock
    RF --> BSP
    FSM --> Manual
    Manual --> HAL
```

The diagram keeps the presentation view focused: commands enter through the Bluetooth/USB parser, the FSM routes either into the automatic measurement flow (mocked or hardware-backed via the BSP) or into manual mode that talks directly to HAL drivers.

### Signal Processing (Undersampling)

The RF signal is **20 MHz**, which exceeds the Nyquist limit of the STM32 ADC (~5 Msps). We use **Undersampling (Bandpass Sampling)**:

1. **Sampling Rate ($f_s$)**: Configured (via TIM6) such that $|20\text{MHz} - N \cdot f_s| \approx 10\text{-}50\text{kHz}$.
2. **Capture**: DMA fills a buffer (e.g., 256 samples).
3. **Processing**: A **DFT/FFT** extracts the magnitude of the alias frequency. This acts as a narrowband filter.

---

## 4. User Interface (UI)

The device operates autonomously using the onboard Button, LEDs, and LCD.

### Button Logic (PC13)

* **1st Press**: Triggers **Calibration** (if no valid calibration exists).
* **2nd Press**: Triggers **Measurement** (if calibration is valid).
* **Press in Error State**: Resets the system to Init.

### LED & LCD Status

| State | LCD Line 1 | LCD Line 2 | LEDs Active | Description |
| :--- | :--- | :--- | :--- | :--- |
| **INIT** | `INIT` | `Booting...` | **Init (Green)** | System startup. |
| **IDLE** | `IDLE` | `Ready` (or `Need CAL`) | **Init (Green)** | Waiting for input. |
| **CALIBRATION** | `CAL` | `Sweeping...` | **Meas (Blue)**, **Excite (Yellow)** | Finding "Air" resonance. |
| **MEASURE** | `MEAS` | `Sampling...` | **Meas (Blue)**, **Excite (Yellow)** | Finding "Snow" resonance. |
| **ERROR** | `ERROR` | `Check host` | **Error (Red)** | Hardware failure. |

Note: there is no separate FSM state called "RESULT" in the current V2 flow. The result is emitted as `DAT:RES:...` and the FSM returns to **IDLE**.

---

## 5. Development Roadmap (To-Dos)

### A. Transition from Mock to Real Hardware (Priority: High)

* **Context**: `bsp_rf.c` is currently a mock that logs to UART but doesn't touch pins.
* **Tasks**:
    1. [ ] **Enable Drivers**: In `bsp_rf.c`, replace `Debug_LogDriver` with:
        * `HL_DAC_SetVoltage(DAC_CH_FREQ, v)`
        * `HL_DAC_SetVoltage(DAC_CH_Q, v)`
    2. [ ] **Clean Main Loop**: Remove the manual waveform generation code in `main.c`'s `while(1)` loop. It conflicts with the FSM.
    3. [ ] **Verify Timing**: Add `HAL_Delay(1)` in `rf_measure.c`'s `sample_at()` to allow Varicap voltage to settle.

### B. Signal Processing Implementation

* **Context**: We need to measure 20MHz using a slower ADC via undersampling.
* **Tasks**:
    1. [ ] **ADC Timer**: Configure `main.c` / `hal_adc.c` (TIM6) to a specific frequency $f_s$ (e.g., ~800.1 kHz) to alias 20MHz to ~2.5kHz.
    2. [ ] **Buffer Capture**: Implement `BSP_RF_CaptureBuffer(float* buffer, size_t len)` in `bsp_rf.c` using DMA.
    3. [ ] **DFT Logic**: Implement a simple DFT/Goertzel in `math_model.c` to calculate magnitude at the alias frequency.
    4. [ ] **Update Measure**: Modify `rf_measure.c` to use the buffer+DFT method instead of single-point sampling.

### C. User Interface Enhancements

* **Context**: The LCD currently only shows status strings, not results.
* **Tasks**:
    1. [ ] **Display Result**: Update `FSM_HandleCalculation` to format the result (e.g., `E: 1.54 D: 0.32`) and display it on the LCD.

### D. PC Control & Mirroring

* **Context**: The PC CLI (`tools/pc_cli.py`) should control the device exactly like the Bluetooth app.
* **Tasks**:
    1. [ ] **Add UART4/NINA transport**: Implement UART4 RX/TX for the NINA module and route its received lines into the same `BT_ProcessIncoming()` command parser.
    2. [x] **USB Input**: USART2 RX is line-wise and feeds directly into the command parser via `usb_cdc_bridge.c` → `BT_ProcessIncoming()`.
    3. [x] **Burst Safety**: Back-to-back commands are protected by a UART RX line queue and an FSM event queue.

### E. Hardware / Electronics

* [ ] **PWM Level**: Add OpAmp buffer/divider to PA9 to attenuate 3.3V PWM to 1V peak.
* [ ] **Power**: Design for -40°C operation and battery support.

---

## 6. CLI Command Reference

The following ASCII commands are supported on **USART2 (USB VCP)**. The same protocol is intended for **UART4/NINA (Bluetooth)**, but the UART4 transport is not wired up yet.

All commands must be terminated with `\n` or `\r\n`.

### Control Commands

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:CONN` | Establishes connection / handshake. | State-dependent: `STAT:RDY` (IDLE/ERROR), `STAT:CAL` (CALIBRATION), `STAT:MEAS` (MEASURE_SEARCH), `STAT:MANUAL` (MANUAL_OPERATION) |
| `CMD:RESET` | Reboot MCU (useful for tests). | `STAT:RESETTING` then reset |
| `CMD:CAL` | Trigger Calibration (Air Sweep). | `STAT:CAL_REQ` → `STAT:CAL` → `STAT:CAL_OK` or `STAT:ERR` |
| `CMD:MEAS` | Trigger Measurement (Snow Sweep). | `STAT:MEAS_REQ` → `STAT:MEAS` → `DAT:RES:...` or `STAT:ERR:MEAS_INVALID` (if measurement invalid). If no valid calibration exists, the request is rejected with `STAT:ERR`. |
| `CMD:BTN:PRESS` | Simulates Button Press. | `STAT:BTN_PRESS` |
| `CMD:BTN:RELEASE` | Simulates Button Release. | `STAT:BTN_REL` |

### Debug & Status Commands

| Command | Description | Response Format |
| :--- | :--- | :--- |
| `CMD:LEDS` | Request LED status snapshot. | `STAT:LED:S:<0/1>:M:<0/1>:E:<0/1>:R:<0/1>` |
| `CMD:LCD` | Request LCD content snapshot. | `DAT:LCD:L0:<text>` then `DAT:LCD:L1:<text>` |
| `CMD:LOG` | Dump internal debug log buffer. | `DAT:LOG:D:<domain>:S:<state>:<msg>` |
| `CMD:TRACE` | Dump last RF sweep data. | `DAT:TRACE:<mode>:<idx>:V:<volt>:A:<amp>` |

### Mock Control Commands (Simulation Only)

These commands configure the internal mock engine when hardware is not available.

| Command | Description |
| :--- | :--- |
| `CMD:MOCK:RF:RES:<float>` | Set the resonance voltage (location of the minimum). Default: `1.2`. |
| `CMD:MOCK:RF:NOISE:<float>` | Set the random noise level added to the signal. Default: `0.01`. |
| `CMD:MOCK:RF:BASE:<float>` | Set the base amplitude (vertical offset). Default: `1.0`. |
| `CMD:MOCK:RF:FAIL:<ON/OFF>` | Force the RF mock to return `NAN` (simulating sensor failure). |

### HAL Board Commands (Manual/Debug Hardware Control)

These commands provide direct hardware control for manual testing and debugging.

To reduce the risk of accidentally toggling real hardware during normal measurement flows, `CMD:HAL:*` is **locked by default** and only accepted while the firmware is in **Manual Mode**.

#### Manual Mode (Handbetrieb)

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:MANUAL:ON` | Enter `STATE_MANUAL_OPERATION` (disables excitation/op-amp, enables HAL commands). | `STAT:MANUAL_ON_REQ`, then `STAT:MANUAL_ON` |
| `CMD:MANUAL:OFF` | Leave manual mode and return to idle. | `STAT:MANUAL_OFF_REQ`, then `STAT:MANUAL_OFF` |

While manual mode is active, `CMD:CAL` / `CMD:MEAS` are rejected with `STAT:MANUAL_ACTIVE`.

If you send a `CMD:HAL:*` command while manual mode is not active, the device responds with `STAT:HAL_LOCKED`.

If you send `CMD:MANUAL:OFF` while manual mode is not active, the device responds with `STAT:MANUAL_NOT_ACTIVE`.

#### Push-Style ACK Frames (for UI updates)

In addition to the legacy `STAT:HAL_*` responses, successful `CMD:HAL:*` operations also emit structured frames that include the applied values:

- `STAT:HW:LED:<id>:<0/1>`
- `STAT:HW:DAC:<ch>:V:<volts>` and `STAT:HW:DAC:<ch>:RAW:<value>`
- `STAT:HW:ADC:V:<volts>` and `STAT:HW:ADC:RAW:<value>`
- `STAT:HW:GAIN:<0..3>`
- `STAT:HW:BTN:<0/1>`
- `STAT:HW:NINA:RST:<0/1>` and `STAT:HW:NINA:STOP:<0/1>`
- PWM: `STAT:HW:PWM:RUN:<0/1>`, `STAT:HW:PWM:FREQ:<hz>`, `STAT:HW:PWM:DUTY:<0..100>`

This is intended so the PC GUI can update immediately from responses (without periodic polling).

#### LED Control

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:LED:SET:<id>:<state>` | Set LED state (0=OFF, 1=ON) | `STAT:HAL_LED_<id>_ON/OFF` |
| `CMD:HAL:LED:GET:<id>` | Read LED state | `STAT:HAL_LED_<id>:<0/1>` |
| `CMD:HAL:LED:TOGGLE:<id>` | Toggle LED state | `STAT:HAL_LED_<id>_TOG` |

*LED IDs: 0=INIT (Green), 1=MEAS (Blue), 2=EXCITE (Yellow), 3=ERR (Red)*

#### Button

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:BTN:READ` | Read physical button state | `STAT:HAL_BTN:PRESSED` or `STAT:HAL_BTN:RELEASED` |

#### ADC

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:ADC:READ` | Read ADC as voltage | `STAT:HAL_ADC:1.650V` |
| `CMD:HAL:ADC:RAW` | Read raw 12-bit ADC value | `STAT:HAL_ADC:2048` |

#### DAC

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:DAC:SET:<ch>:<voltage>` | Set DAC channel voltage (0.0-3.3V) | `STAT:HAL_DAC_<ch>:<voltage>V` |
| `CMD:HAL:DAC:RAW:<ch>:<value>` | Set DAC raw 12-bit value (0-4095) | `STAT:HAL_DAC_<ch>:<value>` |

*DAC Channels: 0=FREQ_TUNE (PA4), 1=Q_FACTOR (PA5)*

#### LCD (Simulated)

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:LCD:SET:<line>:<text>` | Overwrite LCD line buffer (0/1) with text (padded/truncated). | `STAT:HAL_LCD_L<line>_OK` |

In addition, the firmware pushes the updated buffer as `DAT:LCD:L<line>:<text>`.

#### PWM / Excitation (TIM1 CH2 / PA9)

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:PWM:START` | Start PWM output. | `STAT:HAL_PWM_START_OK` + `STAT:HW:PWM:*` |
| `CMD:HAL:PWM:STOP` | Stop PWM output. | `STAT:HAL_PWM_STOP_OK` + `STAT:HW:PWM:*` |
| `CMD:HAL:PWM:GET` | Read PWM run/freq/duty. | `STAT:HAL_PWM_OK` + `STAT:HW:PWM:*` |
| `CMD:HAL:PWM:FREQ:<hz>` | Set PWM frequency. | `STAT:HAL_PWM_FREQ_OK` + `STAT:HW:PWM:FREQ:<hz>` |
| `CMD:HAL:PWM:DUTY:<0..100>` | Set PWM duty cycle. | `STAT:HAL_PWM_DUTY_OK` + `STAT:HW:PWM:DUTY:<pct>` |

#### RF Gain

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:GAIN:SET:<level>` | Set RF gain level (0-3) | `STAT:HAL_GAIN:<level>` |
| `CMD:HAL:GAIN:GET` | Read current RF gain level | `STAT:HAL_GAIN:<level>` |

#### NINA Bluetooth Module

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:NINA:RST:<state>` | Set reset pin (0=reset, 1=run) | `STAT:HAL_NINA:RESET` or `STAT:HAL_NINA:RUN` |
| `CMD:HAL:NINA:STOP:<state>` | Set stop pin (0=run, 1=stop) | `STAT:HAL_NINA:RUNNING` or `STAT:HAL_NINA:STOPPED` |

#### Initialization

| Command | Description | Response |
| :--- | :--- | :--- |
| `CMD:HAL:INIT` | Initialize HAL Board module | `STAT:HAL_INIT_OK` |

### HAL Board Architecture

The HAL Board module (`hal_board.c`) provides a **direct hardware control interface** that bypasses the FSM. This is useful for manual testing and debugging.

#### Command Flow

```text
Terminal/PC (UART)                                    STM32
      │                                                 │
      │  "CMD:HAL:LED:SET:0:1\n"                        │
      └─────────────────────────────────────────────────►
                          │
                          ▼
                ┌─────────────────────┐
                │  BT_ProcessIncoming │  (bt_manager.c)
                │  checks "CMD:HAL"   │
                └─────────┬───────────┘
                          ▼
                ┌─────────────────────┐
                │  handle_hal_command │  routes to sub-handler
                └─────────┬───────────┘
                          ▼
                ┌─────────────────────────┐
                │  handle_hal_led_command │  parses id=0, state=1
                └─────────┬───────────────┘
                          ▼
                ┌─────────────────────┐
                │  HalBoard_LED_Set() │  (hal_board.c)
                └─────────┬───────────┘
                          ▼
                ┌─────────────────────┐
                │  HL_GPIO_WritePin() │  (hal_gpio.c)
                └─────────┬───────────┘
                          ▼
                      [LED ON!]
```

#### Layer Purpose

| Layer | File | Purpose |
| :--- | :--- | :--- |
| **CLI Parser** | `bt_manager.c` | Parses ASCII commands, routes to handlers |
| **HAL Board** | `hal_board.c` | Wrapper functions for hardware access |
| **HAL Drivers** | `hal_gpio.c`, `hal_dac.c`, etc. | Direct STM32 register manipulation |

#### Usage Example

```bash
# Set LED 0 (Green) ON
CMD:HAL:LED:SET:0:1
→ STAT:HAL_LED_0_ON

# Read ADC voltage
CMD:HAL:ADC:READ
→ STAT:HAL_ADC:1.650V

# Set DAC channel 0 to 1.5V
CMD:HAL:DAC:SET:0:1.5
→ STAT:HAL_DAC_0:1.50V
```

## 7. Mock Simulation Details

When running without real hardware (or when `bsp_rf.c` is in mock mode), the system simulates the RF response mathematically.

### RF Response Model

The mock generates a **parabolic dip (minimum)** to simulate the resonance circuit absorption.

* **Formula**: $Amplitude = Base + Curvature \cdot (V_{dac} - V_{res})^2 + Noise$
* **Behavior**: The firmware's peak detection algorithm looks for a **minimum** amplitude.
* **Default**: A minimum at **1.2V** with a base amplitude of **1.0V**.

### Verification

* **Correctness**: The mock correctly produces a "U" shape, and the `rf_measure.c` logic correctly searches for a minimum (`if (amp < local_best_amp)`).
* **Limitations**: The mock does not currently simulate the Q-factor change significantly (it only adds a small linear offset based on Q-voltage).

### Missing CLI Features (TODO)

* **RF State Readback**: There is currently no command (e.g., `CMD:RF:STAT`) to read the instantaneous state of the RF hardware (Excitation On/Off, Current DAC Voltage). This must be inferred from `CMD:TRACE` or `CMD:LEDS` (Excite LED).
* ~~**Direct Hardware Control**: There are no commands to manually set DAC voltages or toggle pins for low-level testing.~~ **DONE**: `CMD:HAL:*` commands now provide direct hardware control (see Section 6: HAL Board Commands) (STILL NEEDS TO BE TESTED).

For the current actionable task list, see [ToDos.md](ToDos.md).
