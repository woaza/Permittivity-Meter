/**
  ******************************************************************************
  * @file    test_hal_adc.c
  * @brief   Unit tests for ADC HAL
  * @author  Majdedin Al Rashid
  * @date    24.11.2025
  ******************************************************************************
  */

#include "test/test_hal_adc.h"
#include "hl/hal_adc.h"
#include "hl/hal_dac.h"
#include "mocks/mock_board.h"
#include "debug_log.h"
#include <stdio.h>
#include <math.h>

// Simple assertion macro
#define ASSERT_FLOAT_NEAR(val, expected, tol) \
    do { \
        if (fabsf((val) - (expected)) > (tol)) { \
            char buf[64]; \
            snprintf(buf, sizeof(buf), "FAIL: %.3f != %.3f", (val), (expected)); \
            Debug_LogEvent("TEST_ADC", buf); \
            return; \
        } \
    } while(0)

void Test_HL_ADC_Mock_Read(ADC_HandleTypeDef *hadc, DAC_HandleTypeDef *hdac)
{
    Debug_LogEvent("TEST_ADC", "Starting Mock Read Test...");

    // Reset LEDs
    HAL_GPIO_WritePin(MEAS_LED_GPIO_Port, MEAS_LED_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(ERR_LED_GPIO_Port, ERR_LED_Pin, GPIO_PIN_RESET);

    // 1. Initialize Drivers
    if (HL_ADC_Init(hadc) != ADC_OK)
    {
        Debug_LogEvent("TEST_ADC", "ADC Init Failed");
        return;
    }
    
    // Ensure DAC is also initialized as ADC mock depends on it
    if (HL_DAC_Init(hdac) != DAC_OK)
    {
        Debug_LogEvent("TEST_ADC", "DAC Init Failed");
        return;
    }

    // 2. Enable Mock Mode
    HL_ADC_SetMockMode(true);
    
    // 3. Setup the "Physical World"
    // We set the resonance frequency to correspond to 2.0V on the tuning varactor.
    float target_resonance_v = 2.0f;
    MockBoard_RF_SetResonanceVoltage(target_resonance_v);
    MockBoard_RF_SetBaseAmplitude(3.0f); // 3.0V baseline (off-resonance)
    MockBoard_RF_SetNoise(0.0f);         // Disable noise for deterministic testing

    // 4. Test Case A: Off-Resonance (Should read high amplitude)
    // Set DAC to 0.5V (far from 2.0V)
    HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 0.5f);
    HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, 1.0f); // Standard Q
    
    float val_off = HL_ADC_GetVoltage();
    char log_buf[32];
    snprintf(log_buf, sizeof(log_buf), "Off-Res: %.3fV", val_off);
    Debug_LogEvent("TEST_ADC", log_buf);
    
    // Expect close to base amplitude (3.0V)
    ASSERT_FLOAT_NEAR(val_off, 3.0f, 0.1f);

    // 5. Test Case B: On-Resonance (Should read low amplitude)
    // Set DAC to 2.0V (exactly on resonance)
    HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 2.0f);
    
    float val_on = HL_ADC_GetVoltage();
    snprintf(log_buf, sizeof(log_buf), "On-Res: %.3fV", val_on);
    Debug_LogEvent("TEST_ADC", log_buf);
    
    // Expect a significant dip. The mock model usually dips to ~0.1-0.5V depending on Q.
    if (val_on > 2.0f)
    {
         Debug_LogEvent("TEST_ADC", "FAIL: No resonance dip detected");
         // Turn on Error LED (PB1)
         HAL_GPIO_WritePin(ERR_LED_GPIO_Port, ERR_LED_Pin, GPIO_PIN_SET);
         return;
    }

    Debug_LogEvent("TEST_ADC", "Passed");
    // Turn on Measurement LED (PA7) to indicate Success
    HAL_GPIO_WritePin(MEAS_LED_GPIO_Port, MEAS_LED_Pin, GPIO_PIN_SET);
}
