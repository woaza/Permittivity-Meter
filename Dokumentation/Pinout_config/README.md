## Pinout Configuration Documentation for STM32L476RG in Permittivity Meter ReDesign

This document details the pinout configuration for the STM32L476RG microcontroller in the Permittivity Meter ReDesign project. The configuration is based on STM32CubeMX settings for the Hardware Layer (HL), supporting data acquisition from the Twin-T notch filter (ADC), varactor control (DAC), Bluetooth communication with NINA-W152 (USART2), button wake-up (GPIO interrupt), and clock output (MCO).

The goal is to create shield with the external circuit (Twin-T notch filter, 7th-order LC filter, OPA2690 stages) using the male pins of the NUCLEO-L476RG. 

### Pinout Summary Table

## Arduino-Compatible Pin Configuration for Permittivity Meter ReDesign

# Project Signal Pin Mapping

| Project Signal | STM32 Pin | Function/Mode | Morpho Connector | Pin Number (Odd/Even) | Notes |
|----------------|------------|----------------|------------------|------------------------|-------|
| **MCO Output (17.5–20 MHz Square Wave)** | PA8 | RCC_MCO1 (Alternate Function) | CN10 | Odd 23 | Sourced from HSE/PLL; `HAL_RCC_MCOConfig`. Connect to 7th-order Chebyshev LC filter input for sine conversion (THD <1%). |
| **DAC Channel 1 (D1: Freq/ε')** | PA4 | DAC_OUT1 (Analog) | CN7 | Even 32 | 0–2.5 V bias (external amp to 0–5 V); `HAL_DAC_SetValue`. Tunes frequency shift in feedback loop. |
| **DAC Channel 2 (D2: Q/ε'')** | PA5 | DAC_OUT2 (Analog) | CN10 | Odd 11 | Similar to PA4; adjusts quality factor for imaginary permittivity detection. |
| **GPIO Gain Switch 1 (1×/10×/100×)** | PC1 | GPIO Output (Push-Pull) | CN7 | Even 36 | Controls BJT in OPA2690 stage; `HAL_GPIO_WritePin`. Default SB56 ON (PC1). |
| **GPIO Gain Switch 2 (1×/10×/100×)** | PC2 | GPIO Output (Push-Pull) | CN7 | Odd 35 | Companion to PC1 for gain selection based on notch attenuation (~mVpp min). |
| **ADC Input (Notch Amplitude)** | PC0 | ADC1_IN1 (Analog) | CN7 | Even 38 | Undersampling (122.5 kHz aliased to 30.6 kHz); `HAL_ADC_Start_DMA(512 samples)`. Default SB51 ON (PC0). Clock via PLLSAI1 for precision. |
| **USART2 TX (to NINA-W152)** | PA0 | UART2_TX (AF7) | CN10 | Odd 35 | 115200 baud for AT commands (e.g. `AT+UBTLE=1`, `AT+UDSC`); `HAL_UART_Transmit`. SB62 OFF. |
| **USART2 RX (from NINA-W152)** | PA1 | UART2_RX (AF7) | CN10 | Odd 37 | Receives BLE data strings (ε'/ε''); `HAL_UART_Receive`. SB63 OFF. |
| **HSE OSC_IN (External 20 MHz Crystal)** | PH0 | HSE Input | CN7 | Odd 29 | Connect to X3 with load cap; enables stable clock for resonant shift compensation. SB54/SB55 OFF by default (configure ON for HSE). |
| **HSE OSC_OUT (External 20 MHz Crystal)** | PH1 | HSE Output | CN7 | Odd 31 | Companion to PH0; `HAL_RCC_OscConfig(RCC_HSE_ON)`. Supports PLL tuning for 17.5 MHz adjustments. |




### Pinout View
![alt text](image-6.png)

![alt text](image-1.png)

![alt text](image-2.png)

![alt text](image-3.png)

![alt text](image-4.png)

![alt text](image-5.png)
