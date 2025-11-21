# Permittivity-Meter

Target hardware remains the STM32L476RG (NUCLEO-L476RG), while all application
logic, mocks, serial protocol handlers, host tools, and lifecycle tests now live
inside this `Permittivity-Meter-dev` workspace alongside the STM32Cube project.

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

