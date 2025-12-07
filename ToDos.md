# Permittivity Meter ReDesign – TODO List

**ECE.23.D** | **Version 1.2** | **01-Dec-2025**

---

## Priority Tasks (Firmware & Hardware)

### Hardware / Electronics

- [ ] **PWM Output Voltage**: The GPIO pin outputs 3.3V, but the circuit requires a **1V peak**.
  - **Action**: Add a voltage divider or OpAmp buffer with attenuation after the GPIO pin (PA9).
  - **Note**: The GPIO cannot drive the 50 Ohm input directly. An OpAmp buffer is required anyway (as noted in meeting notes).
- [ ] **Shield Pinout**: Define "Permittivity Shield v1.0" using outside peripheral pins.
- [ ] **Power Architecture**:
  - [ ] Design for -40 to +85 °C.
  - [ ] Battery/Powerbank support.
  - [ ] Runtime calculation.
  - [ ] Regulator selection (Buck/Boost) and placement.

### Firmware / Algorithms

- [ ] **Peak Detection Algorithm**:
  - [ ] **Step 1 (Coarse)**: Set the lower varicap (D1) to a fixed voltage. Sweep the upper varicap to find the rough frequency range.
  - [ ] **Step 2 (Fine)**: Adjust the lower varicap to fine-tune the minimum.
  - [ ] **Input**: Read the signal via ADC.
- [ ] **Driver Verification**: (Done)
  - [x] ADC (122.5kHz)
  - [x] PWM (20MHz)
  - [x] DAC

---

## Meeting Notes (27.11.2025)

### Algorithm Details

- The lower varicap (D1, parallel to C20) should be adjusted first to find the minimum.
- The varicap needs a DC voltage.
- The output signal is read by the ADC.
- **Process**: Leave the upper varicap fixed -> Adjust lower varicap to find frequency -> Optimize with the other varicap.

### Hardware Notes

- **GPIO Drive Strength**: The microcontroller GPIO cannot drive the 50 Ohm input directly.
- **Solution**: Place an OpAmp (OPV) after the GPIO pin. This allows for impedance matching and gain adjustment (attenuation to 1V).

### Raw Notes

Algorithmus: Die untere varicap soll mit eingestellt werden bis ein minimum gefunden wird, die variacap braucht einfach eine gleichspannung angelegt und dann soll am ausgang mit unserem adc das signal gelesen werden. Die oberen auf irgendwas lassen und zuerst die untere (D1 (p zu c20)) einstellen. Die Untere muss so eingestellt werden dass die richtige frequenz im spektrum für das minimum erreicht wurde. Danach kann man dann mit der anderen Varicap das minimum verbessern.

## Firmware Fine-Tuning (Dec 2025)

### 1. Signal Processing (Undersampling & FFT)
*   **Context**: The current `rf_measure.c` uses `BSP_RF_ReadAmplitude()` which likely returns a single DC sample. This is insufficient for measuring a 20MHz signal with the STM32 ADC.
*   **Task**: Implement the Undersampling + FFT approach.
    *   [ ] **`main.c` / `hal_adc.c`**: Configure the ADC trigger timer (e.g., TIM6) to a specific frequency $f_s$ such that $|20\text{MHz} - N \cdot f_s| \approx 10\text{-}50\text{kHz}$.
        *   *Example*: If $f_s = 800.1 \text{ kHz}$, then $25 \cdot 800.1 = 20.0025 \text{ MHz}$. Alias = $2.5 \text{ kHz}$.
    *   [ ] **`bsp_rf.c`**: Implement `BSP_RF_CaptureBuffer(float* buffer, size_t len)` to fill a buffer via DMA.
    *   [ ] **`math_model.c`**: Implement a simple DFT (Goertzel algorithm or single-bin FFT) to calculate the magnitude at the expected alias frequency.
    *   [ ] **`rf_measure.c`**: Update `sample_at()` to use the buffer capture + DFT magnitude instead of the raw ADC value.

### 2. Hardware Initialization
*   [ ] **`main.c`**: Verify that `HAL_PWM_SetFrequency(20000000UL)` actually results in a clean 20MHz signal. Check prescaler/period calculations.
*   [ ] **`main.c`**: Ensure `HL_DAC_Init` sets the output buffers correctly (buffered vs unbuffered) to drive the Varicaps without loading effects.

### 3. PC Control & Mirroring
*   [ ] **`bt_manager.c`**: Verify that `output_line` is consistently used for *all* outputs. Currently, it seems correct, but check for any direct `HAL_UART_Transmit` calls that might bypass the USB bridge.
*   [ ] **`usci_a_uart.c`**: Ensure the USB CDC receive callback forwards data to the command parser, effectively treating USB input exactly like Bluetooth input.

### 4. Transition from Mock to Real Hardware
*   **Context**: Currently, `bsp_rf.c` is a mock implementation that does not interact with the physical DAC/ADC.
*   **Plan**:
    1.  **Enable Drivers**: In `bsp_rf.c`, replace the `Debug_LogDriver` calls with actual HAL calls.
        *   `BSP_RF_SetFreqVaricap(v)` -> `HL_DAC_SetVoltage(DAC_CH_FREQ, v)`
        *   `BSP_RF_SetQVaricap(v)` -> `HL_DAC_SetVoltage(DAC_CH_Q, v)`
    2.  **Enable ADC**: In `bsp_rf.c`, implement `BSP_RF_ReadAmplitude()` to trigger a DMA transfer and wait for completion (or use the buffer approach from Task 1).
    3.  **Clean Main Loop**: Remove the manual waveform generation code in `main.c`'s `while(1)` loop. It conflicts with the FSM.
    4.  **Verify Timing**: Add `HAL_Delay(1)` in `rf_measure.c`'s `sample_at()` to allow Varicap voltage to settle before reading the ADC.

### 5. User Interface (UI) Enhancements
*   **Context**: The current FSM updates the LCD with status messages ("Sweeping...", "Sending..."), but it does not display the final calculated permittivity value.
*   **Task**: Update `FSM_HandleCalculation` in `fsm_main.c`.
    *   Instead of just showing "Sending...", format the result string (e.g., `E: 1.54 D: 0.32`) and display it on the LCD using `BSP_LCD_DisplayStringAt`.
    *   Ensure the result remains on the screen until the next state transition.

Sonstiges: Der GPIO Pin am µC wird wahrscheinlich nicht den 50Ohm eingang von der Schaltung mit dem rechtecksignal treiben können. Deswegen werden wir einen OPV nach dem GPIO Pin schalten müssen, dann können wir auch die verstärkung einstellen falls wir das brauchen.

---

## Deadlines & Archive

### Deadline 07.11.2025

- [x] Choosing an External Crystal
  - **Part**: 449-LFXTAL058284BULK
  - **BOM line**: X3
  - **Link**: [Mouser](https://www.mouser.at/ProductDetail/IQD/LFXTAL058284Bulk?qs=sGAEpiMZZMsBj6bBr9Q9af1kE%252BXo19x3mGiGn1Dh61%2FyhX7eZvTWqw%3D%3D)
