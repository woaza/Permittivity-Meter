/**
  ******************************************************************************
  * @file    hal_dac.h
  * @brief   Header file for DAC hardware abstraction layer
  * @author  Majdedin Al Rashid
  * @date    20.11.2025
  ******************************************************************************
  * @attention
  *
  * This file provides an abstraction layer for DAC control.
  * It manages two channels:
  * - Channel 1 (PA4): Frequency Tuning (FRQ_TN)
  * - Channel 2 (PA5): Q-Factor Tuning (Q_FACT_TN)
  *
  ******************************************************************************
  */

#ifndef HL_DAC_H
#define HL_DAC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Exported constants --------------------------------------------------------*/
#define DAC_VOLTAGE_REF     3.3f        // Reference voltage in Volts
#define DAC_RESOLUTION_BITS 12          // DAC resolution
#define DAC_MAX_VALUE       4095        // 2^12 - 1

/* Exported types ------------------------------------------------------------*/
typedef enum
{
    DAC_OK = 0,
    DAC_ERROR = 1,
    DAC_ERROR_INVALID_PARAM = 2,
    DAC_ERROR_NOT_INITIALIZED = 3
} DAC_StatusTypeDef;

typedef enum
{
    DAC_CH_FREQ_TUNE = DAC_CHANNEL_1,   // PA4: Frequency Tuning
    DAC_CH_Q_FACTOR  = DAC_CHANNEL_2    // PA5: Q-Factor Tuning
} DAC_ChannelTypeDef;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Initialize DAC module
 * @param hdac: Pointer to DAC handle structure
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Init(DAC_HandleTypeDef *hdac);

/**
 * @brief Set DAC output voltage for a specific channel
 * @param channel: DAC channel (DAC_CH_FREQ_TUNE or DAC_CH_Q_FACTOR)
 * @param voltage: Desired voltage in Volts (0.0 to 3.3)
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_SetVoltage(DAC_ChannelTypeDef channel, float voltage);

/**
 * @brief Set DAC output raw value for a specific channel
 * @param channel: DAC channel (DAC_CH_FREQ_TUNE or DAC_CH_Q_FACTOR)
 * @param raw_value: 12-bit raw value (0 to 4095)
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_SetRawValue(DAC_ChannelTypeDef channel, uint32_t raw_value);

/**
 * @brief Start DAC output for a specific channel
 * @param channel: DAC channel to start
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Start(DAC_ChannelTypeDef channel);

/**
 * @brief Stop DAC output for a specific channel
 * @param channel: DAC channel to stop
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_Stop(DAC_ChannelTypeDef channel);

#ifdef __cplusplus
}
#endif

#endif /* HL_DAC_H */
