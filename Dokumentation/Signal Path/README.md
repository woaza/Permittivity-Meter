# explaining The Signal Path
The diagram illustrates the signal path and processing flow within a system based on the STM32L476RG microcontroller (MCU).

## PA8: TIM1_CH1 PWM Output
PA8 is a multifunctional GPIO pin on the STM32L476RG (LQFP64 package, pin 41), capable of digital I/O, alternate functions (AF), and analog modes. In our design, it's configured RCC_MCO1 (Master Clock Output 1) to output a divided system clock as a square wave.

The PA8 outputs a stable 20 MHz square wave by tapping the RCC_MCO1, derived from SYSCLK (which can go upto 80 MHz) divided by 4. This provides a fixed 50% duty signal for the LC filter.

### Clock Source Selection
#### The high-speed external (HSE) clock
The Software sets SYSCLK to the choosen Frequenzy (HSE/PLL for stability, ±50 ppm with HSE crystal recommended for mountains). 
The high-speed external (HSE) clock can be supplied with a 4 to 48 MHz crystal/ceramic resonator oscillator.

![alt text](image.png)

Source: p.149, STM32L476xx Datasheet

### PLL
Without PLL, possible MCO frequencies from 48 MHz HSE are limited to 48, 24, 12, 6, 3 MHz (dividers 1/2/4/8/16) – no 20 MHz option. PLL is necessary to multiply HSE (e.g., 8 MHz crystal *10=80 MHz) while maintaining low jitter.

![alt text](image-1.png)

Source: P.158, STM32L476xx Datasheet


