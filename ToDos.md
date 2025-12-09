# Permittivity Meter – Development ToDo List

**ECE.23.D** | **Version 1.3** | **08-Dec-2025**

---

## Step-by-Step Implementation Order

The tasks below are ordered by **dependency** – complete each step before moving to the next.

---

### Phase 1: Hardware Prerequisites

> These hardware tasks MUST be completed before firmware can be tested on real hardware.

- [ ] **1.1 PWM Output Attenuation**
  - Add OpAmp buffer/divider after PA9 to attenuate 3.3V PWM to **1V peak**.
  - The GPIO cannot drive 50Ω directly; an OpAmp buffer is required.

- [ ] **1.2 Varicap Wiring Verification**
  - Confirm PA4 (DAC Ch1) connects to D1 (Frequency Tuning).
  - Confirm PA5 (DAC Ch2) connects to D2 (Q-Factor Tuning).

---

### Phase 2: Mock-to-Hardware Transition  *Priority: High*

> These firmware tasks transition from simulation to real hardware control.

- [ ] **2.1 Enable HAL Drivers in BSP**
  - In `bsp_rf.c`, replace `Debug_LogDriver()` calls with:
    - `HL_DAC_SetVoltage(DAC_CH_FREQ, v)` for frequency tuning.
    - `HL_DAC_SetVoltage(DAC_CH_Q, v)` for Q-factor tuning.
  - Verify PWM is started via `HL_PWM_Start()`.

- [ ] **2.2 Clean Up `main.c`**
  - Remove the manual waveform generation code in the `while(1)` loop.
  - Let the FSM control all hardware via BSP functions.

- [ ] **2.3 Add Settling Delay**
  - In `rf_measure.c`, add `HAL_Delay(1)` in `sample_at()` to allow Varicap voltage to settle before ADC read.

- [ ] **2.4 Integration Test**
  - Run Calibration via button press.
  - Verify DAC voltages change on PA4/PA5 (use multimeter or scope).
  - Verify 20 MHz PWM is active on PA9 during sweep.

---

### Phase 3: Signal Processing (Undersampling)

> Implement the ADC capture and DFT processing to measure 20 MHz.

- [ ] **3.1 Configure ADC Sampling Timer (TIM6)**
  - Set TIM6 to trigger ADC at ~800.1 kHz.
  - This aliases 20 MHz down to ~2.5 kHz (within ADC Nyquist limit).

- [ ] **3.2 Implement DMA Buffer Capture**
  - In `bsp_rf.c`, implement `BSP_RF_CaptureBuffer(float* buffer, size_t len)`.git
  - Use DMA to fill a buffer (e.g., 256 samples) from ADC.

- [ ] **3.3 Implement DFT/Goertzel Algorithm**
  - In `math_model.c`, implement Goertzel algorithm for single-frequency magnitude extraction.
  - Target the alias frequency (~2.5 kHz).

- [ ] **3.4 Update Measurement Logic**
  - Modify `rf_measure.c` to use buffer+DFT method instead of single-point sampling.
  - Return magnitude at each DAC voltage step.

- [ ] **3.5 Signal Processing Test**
  - Inject known signal and verify magnitude extraction.
  - Plot sweep curve (Voltage vs. Magnitude) to verify resonance dip detection.

---

### Phase 4: Calibration & Measurement Algorithms

> Implement the scientific measurement routines.

- [ ] **4.1 Calibration Routine (Air Reference)**
  - Coarse sweep: Sweep D1 (PA4) to find rough minimum (resonance frequency in air).
  - Fine sweep: Adjust D2 (PA5) to optimize the minimum.
  - Store reference voltage `V_Ref` and Q-factor baseline.

- [ ] **4.2 Measurement Routine (Snow/Sample)**
  - Fine sweep: Tune D1 to bring minimum back to 20 MHz in sample.
  - Store measurement voltage `V_M` and Q-factor.

- [ ] **4.3 Permittivity Calculation**
  - Implement Denoth's formula: `ε' = 1 + k_D × log(V_M / V_Ref)`.
  - Constant `k_D = 0.5963` (from BB353 varactor).
  - Implement imaginary part: `ε'' = 1 / (ω × Rp × C0)`.

---

### Phase 5: User Interface & Communication

> Display results and ensure robust communication.

- [ ] **5.1 Display Result on LCD**
  - Update `FSM_HandleCalculation()` to format result (e.g., `E: 1.54 D: 0.32`).
  - Display permittivity and density on LCD Line 2.

- [ ] **5.2 Verify UART Mirroring**
  - Confirm `bt_manager.c` sends all outputs to both UART4 (BT) and USART2 (USB).
  - Test with PC CLI (`tools/pc_cli.py`).

- [ ] **5.3 USB Input Handling**
  - Ensure USB CDC input is forwarded to the command parser.
  - Test `CMD:CAL` and `CMD:MEAS` from PC.

---

### Phase 6: Power & Environmental Design

> Prepare for field deployment.

- [ ] **6.1 Low-Temperature Design**
  - Select components rated for -40°C to +85°C.
  - Test hardware operation at low temperature.

- [ ] **6.2 Battery Support**
  - Design battery/powerbank input circuitry.
  - Select regulator (Buck/Boost) and calculate runtime.

- [ ] **6.3 Shield Pinout Definition**
  - Define "Permittivity Shield v1.0" using outer peripheral pins of Nucleo.
  - Document final pinout and connector placement.

---

## Completed Tasks

- [x] **Driver Verification** (01-Dec-2025)
  - [x] ADC driver (122.5 kHz sampling) – Verified.
  - [x] PWM driver (20 MHz output) – Verified.
  - [x] DAC driver (dual channel) – Verified.

- [x] **External Crystal Selection** (07-Nov-2025)
  - Part: 449-LFXTAL058284BULK (X3 on BOM).
  - [Mouser Link](https://www.mouser.at/ProductDetail/IQD/LFXTAL058284Bulk?qs=sGAEpiMZZMsBj6bBr9Q9af1kE%252BXo19x3mGiGn1Dh61%2FyhX7eZvTWqw%3D%3D)

---

## Meeting Notes Archive

### 27.11.2025 – Algorithm & Hardware Discussion

**Algorithm**:

- Adjust D1 (lower varicap, parallel to C20) first to find frequency minimum.
- Then optimize with D2 (upper varicap) to improve Q-factor.
- Process: Fix upper varicap → Sweep lower varicap → Fine-tune with upper.

**Hardware**:

- GPIO (PA9) cannot drive 50Ω input directly.
- Solution: OpAmp buffer after GPIO for impedance matching and 1V attenuation.
