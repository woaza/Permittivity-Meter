#pragma once

#include <stdint.h>

typedef struct {
    float epsilon_real;      // Real Permittivity (ε')
    float epsilon_imag;      // Imaginary Permittivity (ε'')
    float snow_density;      // Calculated density (optional)
    float temperature;       // Reserved for future thermal compensation

    float dac_freq_voltage;  // Varactor voltage at resonance (FRQ_TN)
    float dac_q_voltage;     // Varactor voltage for Q trim (Q_FACT_TN)
    float adc_voltage_min;   // Minimum amplitude detected via ADC
    float frequency_shift;   // Offset from calibration min (Hz or normalized)
    uint8_t gain_index;      // 0–3 encoded gain (PC9:PC8 as MSB:LSB)
} MeasurementResult_t;

typedef struct {
    float air_dac_freq_voltage; // FRQ_TN voltage for resonance in air
    float air_adc_min;          // ADC minimum captured during calibration
    uint32_t timestamp;         // Systick/RTC snapshot
    uint8_t is_valid;           // 1 = Valid, 0 = Invalid
} CalibrationData_t;

typedef enum {
    STATE_INIT = 0,
    STATE_IDLE,
    STATE_CALIBRATION,
    STATE_MEASURE_SEARCH,
    STATE_CALCULATION,
    STATE_ERROR
} AppState_t;
