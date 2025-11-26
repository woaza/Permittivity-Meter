/**
  ******************************************************************************
  * @file    hal_adc.h
  * @brief   Header file for ADC hardware abstraction layer
  * @author  Majdedin Al Rashid
  * @date    26.11.2025
  ******************************************************************************
  * @attention
  *
  * This file provides an abstraction layer for ADC acquisition on PC0 (ADC1_IN1).
  * It handles Timer-Triggered acquisition (TIM6) and DMA Circular buffering.
  *
  ******************************************************************************
  */

#ifndef HAL_ADC_H
#define HAL_ADC_H

#ifdef __cplusplus
extern "C"
{
	#endif

	/* Includes ------------------------------------------------------------------*/
	#include "stm32l4xx_hal.h"
	#include <stdint.h>
	#include <stdbool.h>

	/* Exported constants --------------------------------------------------------*/
	#define ADC_BUFFER_SIZE         512     // Number of samples per buffer
	#define ADC_SAMPLE_RATE_HZ      122500  // Target sampling rate (122.5 kHz)

	/* Exported types ------------------------------------------------------------*/
	typedef enum
	{
		ADC_OK = 0,
		ADC_ERROR = 1,
		ADC_ERROR_INVALID_PARAM = 2,
		ADC_ERROR_NOT_INITIALIZED = 3,
		ADC_ERROR_BUSY = 4
	} ADC_StatusTypeDef;

	/* Exported functions prototypes ---------------------------------------------*/

	/**
	 * @brief Initialize ADC module
	 * @note Must be called before using any other ADC functions
	 * @param hadc: Pointer to ADC handle structure
	 * @param htim: Pointer to TIM handle structure (used for triggering)
	 * @retval ADC_StatusTypeDef: Status of operation
	 */
	ADC_StatusTypeDef HL_ADC_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim);

	/**
	 * @brief Start ADC acquisition
	 * @retval ADC_StatusTypeDef: Status of operation
	 */
	ADC_StatusTypeDef HL_ADC_Start(void);

	/**
	 * @brief Stop ADC acquisition
	 * @retval ADC_StatusTypeDef: Status of operation
	 */
	ADC_StatusTypeDef HL_ADC_Stop(void);

	/**
	 * @brief Get pointer to the latest complete buffer
	 * @retval uint16_t*: Pointer to buffer, or NULL if not ready
	 */
	uint16_t* HL_ADC_GetBuffer(void);

	/**
	 * @brief Check if a new buffer is ready for processing
	 * @retval bool: true if ready, false otherwise
	 */
	bool HL_ADC_IsBufferReady(void);

	/**
	 * @brief Clear the buffer ready flag (call after processing)
	 * @retval None
	 */
	void HL_ADC_ClearBufferReady(void);

	#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
