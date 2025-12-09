# Project Milestones & Progress

**Last Updated:** 01-Dec-2025

---

## Phase 1: Hardware Abstraction Layer (HAL) Verification

**Status:** Complete

- [x] **ADC Driver (`hal_adc`)**
  - Verified configuration for **122.5 kHz** sampling rate.
  - Implemented DMA Circular Buffer with "Consume-on-Read" pattern to prevent race conditions.
  - Added `volatile` qualifiers for thread safety.
- [x] **PWM Driver (`hal_pwm`)**
  - Verified **20 MHz** square wave output on PA9.
  - Confirmed Timer 1 configuration (80MHz / 4).
- [x] **DAC Driver (`hal_dac`)**
  - Verified control for Frequency Tuning (PA4) and Q-Factor (PA5).
  - Removed redundant initialization in `main.c`.
- [x] **System Clock**
  - Verified **80 MHz** SYSCLK (HSE = 20MHz).

## Phase 2: Peak Detection Algorithm

**Status:** In Progress

- [x] **Algorithm Logic**
  - Existing logic in `rf_measure.c` reviewed (Coarse Sweep -> Fine Sweep -> Parabolic Interpolation).
- [ ] **BSP Porting**
  - Port `bsp_rf.c` from "Mock" to Real Hardware using verified HAL drivers.
  - Implement Gain Control (PC8, PC9) and OpAmp Control (PC4).
- [ ] **Integration**
  - Connect `rf_measure.c` to the main FSM.
  - Verify sweep functionality on hardware.

## Phase 3: Application Logic & UI

**Status:** Pending

- [ ] **Bluetooth Integration**
  - Send measurement results to host.
- [ ] **UI/LCD**
  - Display status and results on local screen.
- [ ] **Power Management**
  - Implement sleep modes and battery monitoring.
