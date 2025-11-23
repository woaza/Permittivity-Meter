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

The system is configured for the **STM32L476RG** MCU with the following peripherals:

*   **ADC1 (Channel 1)**: Measures the RF amplitude signal. Configured for Single-ended mode with DMA.
*   **DAC1 (Channel 1 & 2)**:
    *   **Channel 1**: Controls the Frequency Tuning Varicap voltage.
    *   **Channel 2**: Controls the Q-Factor Varicap voltage.
*   **UART4**: Communicates with the **NINA-B1** Bluetooth/WiFi module (115200 baud, RTS/CTS flow control).
*   **USART2**: USB CDC Bridge for PC communication and debugging (115200 baud).
*   **TIM1**: Generates the PWM excitation signal for the RF circuit.
*   **GPIOs**:
    *   **LEDs**: PA5 (Init), PA6 (Meas), PC7 (Excite), PB6 (Error).
    *   **Control**: PC0 (OpAmp Disable), PC1 (Excite Enable), PC2/PC3 (Gain Select).
    *   **Button**: PC13 (User Button).

## Missing Functionality / To-Do

To achieve full functionality, the following areas need attention:

1.  **RTC Integration**: The `RF_PerformAirCalibration` function currently uses a placeholder timestamp (`0U`). Integration with the STM32 RTC is needed for valid timestamps.
2.  **Advanced Math Model**: The current `Math_CalculateEpsilon` function uses a simplified linear approximation. A more physically accurate model for converting frequency shift/Q-factor changes to permittivity is likely required.
3.  **DAC Waveform Integration**: A blocking test function `Test_HL_DAC_GenerateWaveform` exists in `main.c` but is commented out. Dynamic waveform generation for advanced diagnostics needs to be integrated into the non-blocking FSM.
4.  **Watchdog Management**: While the watchdog is refreshed in the main loop, long-running operations within FSM states (if any) should ensure they do not trigger a reset.

## Layout Overview

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

