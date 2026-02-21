/**
  ******************************************************************************
  * @file    hal_dac.h
  * @brief   Hardware Abstraction Layer for DAC Control (Tuning Voltages)
  * @author  Majdedin Al Rashid
  * @date    20.11.2025
  ******************************************************************************
  * @details
  * This module abstracts the Digital-to-Analog Converter (DAC) to control the
  * varactor diodes in the analog front-end. It provides thread-safe(ish) access
  * and easy voltage-to-raw conversion.
  *
  * **Channels:**
  * - **Channel 1 (PA4)**: Frequency Tuning (FRQ_TN). Adjusts the center frequency
  *   of the tank circuit.
  * - **Channel 2 (PA5)**: Q-Factor Tuning (Q_FACT_TN). Adjusts the bandwidth/gain.
  *
  * **Ranges:**
  * - Voltage: 0 mV to 3300 mV (VDDA)
  * - Raw: 0 to 4095 (12-bit)
  *
  * @section dac_usage How to Use
  *
  * 1. **Initialization**:
  *    @code
  *    HL_DAC_Init(&hdac1);
  *    @endcode
  *
  * 2. **Setting Voltage**:
  *    @code
  *    // Set Frequency Tuning to 1.65V
  *    HL_DAC_SetVoltage(DAC_CH_FREQ_TUNE, 1650);
  *    
  *    // Set Q-Factor to 3.0V
  *    HL_DAC_SetVoltage(DAC_CH_Q_FACTOR, 3000);
  *    @endcode
  *
  * 3. **Start/Stop Output**:
  *    @code
  *    HL_DAC_Start(DAC_CH_FREQ_TUNE); // Enable output
  *    HL_DAC_Stop(DAC_CH_FREQ_TUNE);  // High-Z (or default buffer state)
  *    @endcode
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
// Reference voltage is 3300 mV
#define DAC_VOLTAGE_REF_MV  3300

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
 * @param voltage_mv: Desired voltage in Millivolts (0 to 3300)
 * @retval DAC_StatusTypeDef: Status of operation
 */
DAC_StatusTypeDef HL_DAC_SetVoltage(DAC_ChannelTypeDef channel, uint16_t voltage_mv);

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
