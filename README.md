# Permittivity-Meter

Verwendet wird:
µC: STM32L476RG
Board: NUCLEO-L476RG 

## Flowchart
graph TD
    A[Power On / Firmware Init] --> B["Hardware Layer Init: Clock HSE 20 MHz, ADC, DAC, DMA, UART2 for NINA-W152, GPIO Button Interrupt"]
    B --> C["Enter Low-Power Mode: STOP2 with RTC Wake or Button IRQ"]
    C -->|Button Press Detected| D["Wake MCU: Exit STOP2"]
    D --> E["Application Layer: Perform Auto-Calibration in Air - Tune D1/D2 to Min Amplitude, Store Ref Values"]
    E --> F["Measurement Loop: Set DAC for Varactor D1 Real Part Tuning"]
    F --> G["Acquire ADC Samples via DMA, Apply FFT, Find Amplitude Min via Iterative Sweep 400mV Rough then Fine"]
    G -->|Converged?| H["Set DAC for Varactor D2 Imaginary Part Tuning, Repeat FFT/Amplitude Analysis"]
    G -->|Not Converged| F
    H --> I["Compute ε' and ε'': Voltage-to-Capacitance Conversion per Denoth/Dierer Formulas"]
    I --> J["Driver Layer: Init Bluetooth - Send AT Commands via UART2 +UBTLE=1 Enable BLE, +UBTLECFG Advertise"]
    J --> K["Wait for App Connection: BLE Peripheral Mode"]
    K -->|Connected| L["Send Data: AT+UDCP JSON {eps_r: ε, eps_i: ε}"]
    L --> M["Disconnect BLE, Log Success via USART1 Debug"]
    M --> C
    style A fill:#f9f,stroke:#333,stroke-width:2px
    style C fill:#ff9,stroke:#333,stroke-width:2px
    style M fill:#ff9,stroke:#333,stroke-width:2px
