# Permittivity-Meter

Target hardware remains the STM32L476RG (NUCLEO-L476RG), while all application
logic, mocks, serial protocol handlers, host tools, and lifecycle tests now live
inside this `Permittivity-Meter-dev` workspace alongside the STM32Cube project.

## Project Structure

### Key Directories
- **`PermittivityMeterV2/PermitivityMeterV2/`**: The active STM32CubeIDE project containing the firmware source code.
    - **`Core/Src/main.c`**: Application entry point, hardware initialization, and main loop.
    - **`Core/Src/fsm_main.c`**: Finite State Machine (FSM) implementation controlling the application flow.
    - **`Core/Src/rf_measure.c`**: RF measurement logic, including frequency sweeps and resonance finding.
    - **`Core/Src/math_model.c`**: Mathematical models for Varicap voltage-to-capacitance conversion and permittivity calculations.
    - **`Core/Src/bt_manager.c`**: Manages communication with the NINA module and PC serial bridge.
- **`tools/`**: Python-based tools for testing and interaction.
    - **`run_hw_lifecycle.py`**: Automated script for testing the hardware lifecycle (Happy Path & Failure Scenarios).
    - **`pc_cli.py`**: Command-line interface for manual interaction with the device.

## Application Lifecycle

The firmware operates based on a Finite State Machine (FSM) defined in `fsm_main.c`. The lifecycle consists of the following states:

1.  **STATE_INIT**: Initializes all hardware peripherals (RF, UI, LCD, Bluetooth).
2.  **STATE_IDLE**: The device waits for commands (`CMD:CONN`, `CMD:CAL`, `CMD:MEAS`) from the PC/Bluetooth or a user button press.
3.  **STATE_CALIBRATION**:
    *   Triggered by `CMD:CAL`.
    *   Performs a **Coarse Sweep** (0V - 2.5V) to find the approximate resonance frequency.
    *   Performs a **Fine Sweep** around the peak to pinpoint the resonance vertex.
    *   Stores the "Air" calibration values (`air_dac_freq_voltage`, `air_adc_min`).
4.  **STATE_MEASURE_SEARCH**:
    *   Triggered by `CMD:MEAS`.
    *   Performs a **Fine Sweep** around the calibrated "Air" voltage to find the new resonance peak (shifted by the snow).
5.  **STATE_CALCULATION**:
    *   Computes the frequency shift.
    *   Calculates Real and Imaginary Permittivity ($\epsilon' $, $\epsilon'' $) and Snow Density using `math_model.c`.
    *   Sends the result (`DAT:RES`) to the host.
6.  **STATE_ERROR**: Entered if calibration fails, measurement times out, or hardware errors are detected.

## Hardware Configuration

### Pinout Configuration

| Pin | Name | Function | Description |
| :--- | :--- | :--- | :--- |
| **PC13** | `B1` | **User Button** | Triggers Calibration (if needed) or Measurement. |
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

### User Interface (UI) Behavior

The device operates autonomously using the onboard Button, LEDs, and LCD.

#### Button Logic (PC13)
*   **1st Press**: Triggers **Calibration** (if no valid calibration exists).
*   **2nd Press**: Triggers **Measurement** (if calibration is valid).
*   **Press in Error State**: Resets the system to Init.

#### LED & LCD Status Indicators

| State | LCD Text | LEDs Active | Description |
| :--- | :--- | :--- | :--- |
| **INIT** | `INIT / Booting...` | **Init (Green)** | System startup and hardware check. |
| **IDLE** | `IDLE / Ready` | **Init (Green)** | Waiting for user input. |
| **CALIBRATION** | `CAL / Sweeping...` | **Meas (Blue)**, **Excite (Yellow)** | Performing frequency sweep for "Air" reference. |
| **MEASURE** | `MEAS / Sampling...` | **Meas (Blue)**, **Excite (Yellow)** | Performing frequency sweep for "Snow" sample. |
| **RESULT** | `RESULT / Sending...` | **Meas (Blue)** | Calculation complete. **TODO**: Show value on LCD. |
| **ERROR** | `ERROR / Check host` | **Error (Red)** | Hardware failure or timeout. |

### Signal Processing Strategy (Undersampling & Fourier Analysis)

The RF excitation frequency is **20 MHz**, which is significantly higher than the Nyquist frequency of the STM32L4's ADC (max sampling rate ~5 Msps). To accurately measure the amplitude of this high-frequency signal without expensive high-speed ADCs, we employ **Undersampling (Bandpass Sampling)**.

1.  **Concept**: By sampling at a frequency $f_s$ that is **not** a whole integer fraction of the signal frequency $f_{sig}$ (20 MHz), the high-frequency signal aliases down to a lower baseband frequency $f_{alias}$.
    *   Formula: $f_{alias} = | f_{sig} - N \cdot f_s |$
    *   **Requirement**: We target an alias frequency in the **low kHz range** (e.g., 10-50 kHz). This requires precise configuration of the ADC trigger timer. We must **avoid** sampling at exactly $20 \text{ MHz} / N$, as this would alias to DC (0 Hz) and lose phase/amplitude information due to 1/f noise and drift.

2.  **Implementation**:
    *   **ADC**: Captures a buffer of samples (e.g., 256 or 512 points) via DMA.
    *   **Processing**: We perform a **Discrete Fourier Transform (DFT)** or **FFT** on the captured buffer.
    *   **Extraction**: We look for the magnitude of the spectral peak at the expected $f_{alias}$. This magnitude corresponds to the amplitude of the 20 MHz RF signal.
    *   **Advantage**: This method acts as a narrowband filter, rejecting noise and DC offsets effectively.

### PC Control & Debugging

The system is designed to be controllable from both the **NINA-B1 Bluetooth module** and a **PC via USB**.

*   **Mirroring**: The firmware mirrors all traffic between the internal logic and the external interfaces.
    *   Commands received from **UART4 (Bluetooth)** are processed, and responses are sent back to UART4.
    *   Commands received from **USART2 (USB CDC)** are processed identically.
    *   **Output Mirroring**: Any status message (`DAT:RES`, `LOG:...`) sent by the firmware is broadcast to **both** UART4 and USART2.
*   **PC Tools**: The `tools/pc_cli.py` script acts as a host controller. It connects to the STM32's USB COM port and sends the same ASCII commands (`CMD:MEAS`, `CMD:CAL`) that the Bluetooth mobile app would send. This allows for full hardware lifecycle testing and debugging without needing the mobile app or wireless connection.

## Missing Functionality / To-Do

To achieve full functionality, the following areas need attention:

1.  **RTC Integration**: The `RF_PerformAirCalibration` function currently uses a placeholder timestamp (`0U`). Integration with the STM32 RTC is needed for valid timestamps.
2.  **Advanced Math Model**: The current `Math_CalculateEpsilon` function uses a simplified linear approximation. A more physically accurate model for converting frequency shift/Q-factor changes to permittivity is likely required.
3.  **DAC Waveform Integration**: A blocking test function `Test_HL_DAC_GenerateWaveform` exists in `main.c` but is commented out. Dynamic waveform generation for advanced diagnostics needs to be integrated into the non-blocking FSM.
4.  **Watchdog Management**: While the watchdog is refreshed in the main loop, long-running operations within FSM states (if any) should ensure they do not trigger a reset.

## Layout Overview

### File Purpose & Hardware Interaction

This section clarifies where specific logic lives, distinguishing between high-level application logic (Mock/Real agnostic) and low-level hardware drivers.

| File Path | Purpose | Hardware vs. Mock |
| :--- | :--- | :--- |
| **`Core/Src/fsm_main.c`** | **The Brain**. Controls the high-level state (Idle, Calibrate, Measure). | **Pure Logic**. Does not touch hardware directly. Calls `rf_measure.c` and `bt_manager.c`. |
| **`Core/Src/rf_measure.c`** | **The Scientist**. Implements the sweep algorithms (Coarse/Fine) to find resonance. | **Logic**. Calls `bsp_rf.c` to set voltages and read values. Agnostic to *how* those values are set/read. |
| **`Core/Src/bsp_rf.c`** | **The Bridge**. The Board Support Package (BSP) for the RF frontend. | **The Switch Point**. Currently contains **MOCK** code (logging only). **TODO**: Must be updated to call `HL_DAC` and `HL_ADC` for real hardware. |
| **`Core/Src/hl/hal_*.c`** | **The Drivers**. Wrappers around STM32 HAL for DAC, ADC, PWM. | **Real Hardware**. These files directly manipulate the STM32 registers/HAL. |
| **`Core/Src/bt_manager.c`** | **The Communicator**. Handles ASCII protocol (`CMD:`, `DAT:`) for Bluetooth & PC. | **Real Hardware**. Writes to UART4 (BT) and calls `usb_cdc_bridge.c` (PC). |
| **`Core/Src/main.c`** | **The Setup**. Initializes the MCU, clocks, and peripherals. | **Real Hardware**. Configures the physical chip. Contains the main `while(1)` loop. |

### Acronym Lookup Table

| Acronym | Full Name | Meaning in this Project |
| :--- | :--- | :--- |
| **FSM** | Finite State Machine | The main control loop structure (Idle -> Cal -> Meas). |
| **BSP** | Board Support Package | The layer that abstracts the specific hardware pins/drivers from the logic. |
| **HAL** | Hardware Abstraction Layer | ST's low-level driver library (or our wrappers around it). |
| **DAC** | Digital-to-Analog Converter | Generates voltage to tune the Varicaps (change frequency). |
| **ADC** | Analog-to-Digital Converter | Reads the voltage amplitude of the RF signal. |
| **PWM** | Pulse Width Modulation | Generates the 20MHz square wave to excite the circuit. |
| **CDC** | Communications Device Class | The USB protocol used to create a virtual COM port on the PC. |
| **DFT/FFT** | Discrete Fourier Transform | Math used to extract the signal amplitude from undersampled data. |

- `PermittivityMeterV2/PermitivityMeterV2/Core/Inc`, `PermittivityMeterV2/PermitivityMeterV2/Core/Src`
	- Shared firmware logic (FSM, RF math/model, BSP shims, UART bridge, mocks)
	- V2 CubeMX project provides the hardware config (ADC DMA, TIM1 PWM, UART4 with
		RTS/CTS, watchdog); only the FSM hooks and UART bridge live in user sections.
	- The older `PermittivityMeterHL` folder still holds a snapshot of the same
		sources, but V2 is now the source of truth.
- `tools/`
	- Python CLI + PySimpleGUI UI that speak the ASCII `CMD:*` protocol and now
		automatically refresh LEDs/LCD/log panes.
- `tests/`
	- Host lifecycle tests that drive the FSM through handshake, calibration, and
		measurement flows via the mocked serial interface while asserting status
		frames, LED/LCD states, and BT telemetry ordering.

## Running the Lifecycle Tests

The tests are pure C and build on any desktop toolchain. From this folder:

```powershell
cmake -S tests -B tests/build -G "Ninja"
cmake --build tests/build
ctest --test-dir tests/build
```

> **Note:** Ensure a host C compiler (MSVC, clang, or GCC) and the matching
> build tools (e.g., Ninja or NMake) are available in your shell before
> configuring. The test runner defines `UNIT_TESTS`, so no HAL drivers are
> required at runtime.

## External Links

- Online MCU sim: <https://docs.cirkitdesigner.com/component/46c022d9-5fc0-44c3-87da-f741bf7207b9/nucleo-l476rg>
- STM32L476RG pinout: <https://os.mbed.com/platforms/ST-Nucleo-L476RG/#microcontroller-features>

