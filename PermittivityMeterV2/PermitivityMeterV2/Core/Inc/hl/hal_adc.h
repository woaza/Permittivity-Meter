/**
  ******************************************************************************
  * @file    hal_adc.h
  * @brief   ADC hardware abstraction layer header
  * @author  Majdedin Al Rashid
  * @date    24.11.2025
  ******************************************************************************
  */

#ifndef HL_HAL_ADC_H
#define HL_HAL_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* Exported types ------------------------------------------------------------*/
typedef enum
{
  ADC_OK       = 0x00U,
  ADC_ERROR    = 0x01U,
  ADC_BUSY     = 0x02U,
  ADC_TIMEOUT  = 0x03U
} ADC_StatusTypeDef;

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief  Initialize the ADC HAL module
 * @param  hadc: Pointer to the ADC handle
 * @retval ADC_StatusTypeDef
 */
ADC_StatusTypeDef HL_ADC_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  Start ADC conversion in DMA mode (continuous or single)
 * @retval ADC_StatusTypeDef
 */
ADC_StatusTypeDef HL_ADC_Start(void);

/**
 * @brief  Stop ADC conversion
 * @retval ADC_StatusTypeDef
 */
ADC_StatusTypeDef HL_ADC_Stop(void);

/**
 * @brief  Get the latest converted value converted to Voltage
 * @retval float: Voltage in Volts (0.0 - 3.3V)
 */
float HL_ADC_GetVoltage(void);

/**
 * @brief  Get the latest raw ADC value
 * @retval uint32_t: Raw 12-bit value
 */
uint32_t HL_ADC_GetRawValue(void);

/**
 * @brief  Enable or disable mock mode for testing without hardware
 * @param  enable: true to enable mock mode, false for real hardware
 */
void HL_ADC_SetMockMode(bool enable);

#ifdef __cplusplus
}
#endif

#endif /* HL_HAL_ADC_H */
