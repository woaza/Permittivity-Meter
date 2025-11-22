/**
  ******************************************************************************
  * @file    hal_dac.c
  * @brief   DAC hardware abstraction layer implementation
  * @author  Majdedin Al Rashid
  * @date    20.11.2025
  ******************************************************************************
  * @attention
  *
  * This file provides functionality to control the DAC outputs for:
  * - Frequency Tuning (PA4)
  * - Q-Factor Tuning (PA5)
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hl/hal_dac.h"
#include "main.h"

/* Private variables ---------------------------------------------------------*/
static DAC_HandleTypeDef *hdac_local = NULL;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize DAC module
 * @param hdac: Pointer to DAC handle structure
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Init(DAC_HandleTypeDef *hdac)
{
    if (hdac == NULL)
    {
        return DAC_ERROR_INVALID_PARAM;
    }

    hdac_local = hdac;

    // Start both channels by default with 0V output
    if (HL_DAC_Start(DAC_CH_FREQ_TUNE) != DAC_OK || 
        HL_DAC_Start(DAC_CH_Q_FACTOR) != DAC_OK)
    {
        return DAC_ERROR;
    }

    // Set initial values to 0
    HL_DAC_SetRawValue(DAC_CH_FREQ_TUNE, 0);
    HL_DAC_SetRawValue(DAC_CH_Q_FACTOR, 0);

    return DAC_OK;
}

/**
 * @brief Set DAC output voltage for a specific channel
 * @param channel: DAC channel (DAC_CH_FREQ_TUNE or DAC_CH_Q_FACTOR)
 * @param voltage: Desired voltage in Volts (0.0 to 3.3)
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_SetVoltage(DAC_ChannelTypeDef channel, float voltage)
{
    if (hdac_local == NULL)
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }

    if (voltage < 0.0f || voltage > DAC_VOLTAGE_REF)
    {
        return DAC_ERROR_INVALID_PARAM;
    }

    // Calculate raw value: (Voltage / Vref) * MaxValue
    uint32_t raw_value = (uint32_t)((voltage / DAC_VOLTAGE_REF) * DAC_MAX_VALUE);

    return HL_DAC_SetRawValue(channel, raw_value);
}

/**
 * @brief Set DAC output raw value for a specific channel
 * @param channel: DAC channel (DAC_CH_FREQ_TUNE or DAC_CH_Q_FACTOR)
 * @param raw_value: 12-bit raw value (0 to 4095)
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_SetRawValue(DAC_ChannelTypeDef channel, uint32_t raw_value)
{
    if (hdac_local == NULL)
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }

    if (raw_value > DAC_MAX_VALUE)
    {
        raw_value = DAC_MAX_VALUE;
    }

    if (HAL_DAC_SetValue(hdac_local, channel, DAC_ALIGN_12B_R, raw_value) != HAL_OK)
    {
        return DAC_ERROR;
    }

    return DAC_OK;
}

/**
 * @brief Start DAC output for a specific channel
 * @param channel: DAC channel to start
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Start(DAC_ChannelTypeDef channel)
{
    if (hdac_local == NULL)
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }

    // Call the STM32 HAL function
    if (HAL_DAC_Start(hdac_local, channel) != HAL_OK)
    {
        return DAC_ERROR;
    }

    return DAC_OK;
}

/**
 * @brief Stop DAC output for a specific channel
 * @param channel: DAC channel to stop
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Stop(DAC_ChannelTypeDef channel)
{
    if (hdac_local == NULL)
    {
        return DAC_ERROR_NOT_INITIALIZED;
    }

    // Call the STM32 HAL function
    if (HAL_DAC_Stop(hdac_local, channel) != HAL_OK)
    {
        return DAC_ERROR;
    }

    return DAC_OK;
}
