## Pinout Configuration Documentation for STM32L476RG in Permittivity Meter ReDesign

This document details the pinout configuration for the STM32L476RG microcontroller in the Permittivity Meter ReDesign project. The configuration is based on STM32CubeMX settings for the Hardware Layer (HL), supporting data acquisition from the Twin-T notch filter (ADC), varactor control (DAC), Bluetooth communication with NINA-W152 (USART2), button wake-up (GPIO interrupt), and clock output (MCO).

The goal is to create shield with the external circuit (Twin-T notch filter, 7th-order LC filter, OPA2690 stages) using the male pins of the NUCLEO-L476RG. 


## Pinout Mapping Summary Table

### GPIO Configuration Table

| Pin | User Label | GPIO Mode | Initial Output Level | Pull-up/Pull-down | Max Output Speed | Fast Mode | Notes / Function in Project |
|-----|------------|-----------|---------------------|-------------------|------------------|-----------|------------------------------|
| PA6 | STATUS_LED | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | General system status LED |
| PA7 | MEAS_LED | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | Measurement active LED |
| PA11 | NINA_RST | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | NINA-W152 hardware reset (active low) |
| PA12 | NINA_STOP | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | NINA-W152 low-power STOP mode control |
| PB1 | ERR_LED | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | Error state LED (red) |
| PB6 | ERR_LED | Output Push-Pull | Low | No pull-up and no pull-down | Low | Disabled | Error state LED (duplicate/alternative) |
| PC1 | NINA_LED_RED | Input mode | n/a | No pull-up and no pull-down | n/a | n/a | NINA module status LED input (read-back) |
| PC2 | NINA_LED_BLUE | Input mode | n/a | No pull-up and no pull-down | n/a | n/a | NINA module status LED input (read-back) |
| PC3 | NINA_LED_GREEN | Input mode | n/a | No pull-up and no pull-down | n/a | n/a | NINA module status LED input (read-back) |
| PC4 | OP_DIS | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | OPA2690 disable / power-down control |
| PC7 | EXCITE_LED | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | Excitation active LED (20 MHz sine present) |
| PC8 | GAIN_SLCT_1 | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | OPA2690 gain select bit 1 |
| PC9 | GAIN_SLCT_2 | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | OPA2690 gain select bit 2 |
| PC11 | NINA_RTS | Input mode | n/a | No pull-up and no pull-down | n/a | n/a | NINA-W152 RTS (flow control) |
| PC12 | NINA_DTR | Output Push-Pull | Low | No pull-up and no pull-down | Low | n/a | NINA-W152 DTR (data terminal ready) |
| PC13 | B1 [Blue PushButton] | External Interrupt | n/a | No pull-up and no pull-down | n/a | n/a | User button (wake-up / manual trigger) |



## ADC Configuration
| Pin Name | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pul... | Maximum output... | Fast Mode | User Label |
|----------|---------------|-------------------|-----------|---------------------|-------------------|-----------|------------|
| PC0 | ADC1_IN1 | n/a | Analog mode for ADC | No pull-up and n... | n/a | n/a | NOTCH_AMP_IN

## DAC Configuration
| Pin Name | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pull... | Maximum output... | Fast Mode | User Label |
|----------|---------------|-------------------|-----------|----------------------|-------------------|-----------|------------|
| PA4 | DAC1_OUT1 | n/a | Analog mode | No pull-up and n... | n/a | n/a | FRQ_TN |
| PA5 | DAC1_OUT2 | n/a | Analog mode | No pull-up and n... | n/a | n/a | Q_FACT_TN |

## 20MHz Square Wave Signal
| Pin Name | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pull... | Maximum output... | Fast Mode | User Label |
|----------|---------------|-------------------|-----------|----------------------|-------------------|-----------|------------|
| PA9 | TIM1_CH2 | n/a | Alternate Functio... | No pull-up and n... | Low | n/a | NOCH_20M_IN |


## NINA UART Configuration
| Pin Name | Signal on Pin | GPIO output level | GPIO mode | GPIO Pull-up/Pull... | Maximum output... | Fast Mode | User Label |
|----------|---------------|-------------------|-----------|----------------------|-------------------|-----------|------------|
| PA1 | UART4_RX | n/a | Alternate Functio... | No pull-up and n... | Very High | n/a | NINA_RX |
| PA15 (JTDI) | UART4_RTS | n/a | Alternate Functio... | No pull-up and n... | Very High | n/a | |
| PB7 | UART4_CTS | n/a | Alternate Functio... | No pull-up and n... | Very High | Disable | |
| PC10 | UART4_TX | n/a | Alternate Functio... | No pull-up and n... | Very High | n/a | NINA_TX |



### Pinout View
![Current Pinout View](image.png)

![alt text](image-6.png)

![alt text](image-1.png)

![alt text](image-2.png)

![alt text](image-3.png)

![alt text](image-4.png)

![alt text](image-5.png)
