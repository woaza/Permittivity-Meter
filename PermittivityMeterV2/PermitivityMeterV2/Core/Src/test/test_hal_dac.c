/**
  ******************************************************************************
  * @file    test_hal_dac.c
  * @brief   Implementation of DAC hardware abstraction layer tests
  * @author  Majdedin Al Rashid
  * @date    22.11.2025
  ******************************************************************************
  */

#include "test/test_hal_dac.h"

/**
 * @brief Run all DAC tests
 * @param hdac: Pointer to DAC handle structure
 * @retval DAC_StatusTypeDef: Status of operation (DAC_OK if all tests passed)
 */
DAC_StatusTypeDef Test_HL_DAC_RunAll(DAC_HandleTypeDef *hdac)
{
    DAC_StatusTypeDef status;

    /* 1. Init Test Check */
    if (hdac == NULL)
    {
        return DAC_ERROR_INVALID_PARAM;
    }

    /* 2. Start Channels */
    status = HL_DAC_Start(DAC_CH_FREQ_TUNE);
    if (status != DAC_OK) return status;

    status = HL_DAC_Start(DAC_CH_Q_FACTOR);
    if (status != DAC_OK) return status;

    /* 3. Voltage Sweep Test */
    
    // 0.0V
    status = HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 0.0f);
    if (status != DAC_OK) return status;
    status = HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, 0.0f);
    if (status != DAC_OK) return status;
    HAL_Delay(2000); // Wait 2s for measurement

    // 1.65V (Mid-range)
    status = HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 1.65f);
    if (status != DAC_OK) return status;
    status = HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, 1.65f);
    if (status != DAC_OK) return status;
    HAL_Delay(2000); // Wait 2s for measurement

    // 3.3V (Max)
    status = HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 3.3f);
    if (status != DAC_OK) return status;
    status = HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, 3.3f);
    if (status != DAC_OK) return status;
    HAL_Delay(2000); // Wait 2s for measurement

    /* 4. Raw Value Test */
    
    // Raw 0
    status = HL_DAC_SetRawValue(DAC_CH_FREQ_TUNE, 0);
    if (status != DAC_OK) return status;
    HAL_Delay(1000);

    // Raw 2048
    status = HL_DAC_SetRawValue(DAC_CH_FREQ_TUNE, 2048);
    if (status != DAC_OK) return status;
    HAL_Delay(1000);

    // Raw 4095
    status = HL_DAC_SetRawValue(DAC_CH_FREQ_TUNE, 4095);
    if (status != DAC_OK) return status;
    HAL_Delay(1000);

    /* 5. Stop Channels */
    status = HL_DAC_Stop(DAC_CH_FREQ_TUNE);
    if (status != DAC_OK) return status;

    status = HL_DAC_Stop(DAC_CH_Q_FACTOR);
    if (status != DAC_OK) return status;

    return DAC_OK;
}

/**
 * @brief Generate a continuous triangle wave on DAC channels for oscilloscope testing.
 *        This function DOES NOT RETURN (infinite loop).
 * @param hdac: Pointer to DAC handle structure
 * @param hiwdg: Pointer to IWDG handle structure (to refresh watchdog)
 */
void Test_HL_DAC_GenerateWaveform(DAC_HandleTypeDef *hdac, IWDG_HandleTypeDef *hiwdg)
{
    float voltage = 0.0f;
    float step = 0.05f;
    int direction = 1; // 1 for up, -1 for down

    /* Start Channels */
    HL_DAC_Start(DAC_CH_FREQ_TUNE);
    HL_DAC_Start(DAC_CH_Q_FACTOR);

    int cycle_count = 0;
    while (cycle_count < 5)
    {
        /* Refresh Watchdog */
        if (hiwdg != NULL)
        {
            HAL_IWDG_Refresh(hiwdg);
        }

        /* Set Voltage */
        HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, voltage);
        HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, voltage);

        /* Update Voltage */
        voltage += (step * direction);

        /* Check Bounds */
        if (voltage >= 3.3f)
        {
            voltage = 3.3f;
            direction = -1;
        }
        else if (voltage <= 0.0f)
        {
            voltage = 0.0f;
            direction = 1;
            cycle_count++; // Completed one full cycle (0 -> 3.3 -> 0)
        }

        /* Delay for visibility (approx 10ms per step) */
        HAL_Delay(10);
    }
}
