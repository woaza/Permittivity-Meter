# Open Questions for Permittivity Meter ReDesign

This document tracks unresolved questions and their current status, aiding project planning.

## Meeting 27.10.2025

### Question 1: Why and If an External Signal is Needed?

#### Status: Under Consideration
- **Context**: The project uses the STM32L476RG on a NUCLEO-L476RG board to control a Twin-T notch filter at ~20 MHz, with varactor diodes (D1 for ε', D2 for ε'') tuned via DAC outputs. The ADC samples the undersampled signal (~10 kHz alias) for FFT analysis, and USART2 communicates with the NINA-W152 Bluetooth module.
- **Current Configuration**: We are using the internal High-Speed Internal (HSI) oscillator at 16 MHz, multiplied to 80 MHz SYSCLK via PLL, with MCO outputting ~20 MHz (PLLCLK /4) to the filter.
- **Need for External Signal (HSE)**:
  - **Why It Might Be Needed**:
    - **Stability**: HSI has ±1% accuracy over temperature (-40°C to +85°C), which can drift with environmental changes (e.g., mountain conditions). Rippel's thesis (p.63) recommends an external 20 MHz crystal (HSE) for precise undersampling and filter resonance, reducing timing errors in permittivity measurement.
    - **Filter Accuracy**: The 7th-order LC filter requires a stable ~20 MHz input for sine wave generation and harmonic suppression. HSI's drift may misalign the notch frequency, affecting ε'/ε'' detection (per Denoth, 1994).
    - **EMC Compliance**: An external clock can be better filtered (e.g., with ferrite beads) to meet RED regulations for NINA-W152 Bluetooth, minimizing interference.
  - **Why It Might Not Be Needed**:
    - **Simplicity**: No external crystal (board mod on X3 pads) simplifies prototyping and reduces cost. HSI is auto-calibrated and sufficient for initial testing or if software compensation handles drift.
    - **Power Savings**: HSE adds ~10 µA, while HSI is lower power in some modes. For our low-power design (STOP2 ~1.4 µA), this is minor but relevant.
    - **Performance**: 80 MHz SYSCLK from HSI meets FFT/radix-2 needs (100 DMIPS), and MCO ~20 MHz is close enough for filter input with AL adjustments.
- **Decision Point**: 
  - **If Needed**: Add a 20 MHz crystal (e.g., Abracon ABS07-20.000MHZ-4-T) and capacitors (10-12 pF) to PH0/PH1 (X3 pads). Reconfigure CubeMX Clock tab: HSE 20 MHz, PLL to 80 MHz, MCO /4.
  - **If Not Needed**: Continue with HSI, monitor drift in field tests, and calibrate in firmware (e.g., adjust MCO divisor).
- **Recommendation**: Use HSE for production accuracy (thesis preference), but proceed with HSI for prototyping. Test HSI stability with MATLAB simulation (e.g., 1% drift on 20 MHz) and decide based on results.
- **Action Items**:
  - Simulate HSI drift impact on filter timing in MATLAB.
  - Document test results in Circuit Explanation (Teichtmeister, 10.08.2025).
  - Update Risk Analysis for clock instability.

### Answer


### Question 2: Pinout

### Answer

## TODOs


