/**
  ******************************************************************************
  * @file    hal_adc.h
  * @brief   Hardware Abstraction Layer for High-Speed ADC Acquisition
  * @author  Majdedin Al Rashid
  * @date    26.11.2025
  ******************************************************************************
  * @details
  * This module manages the Analog-to-Digital Converter (ADC1) for sampling the
  * input signal. It is designed for high-speed, continuous data acquisition
  * using hardware timers and DMA to minimize CPU intervention.
  *
  * **Configuration:**
  * - **Input Pin**: PC0 (ADC1 Channel 1) - Connected to Notch Filter Output.
  * - **Trigger Source**: TIM6 Update Event details (Hardware Trigger).
  * - **Data Transfer**: DMA1 Channel 1 in Circular Mode.
  * - **Buffering**: Double-buffering scheme (Half-Cplt and Full-Cplt callbacks).
  *
  * @section adc_usage How to Use
  *
  * 1. **Initialization** (Normally done in main.c):
  *    @code
  *    // Initialize HAL drivers
  *    HL_ADC_Init(&hadc1, &htim6);
  *    @endcode
  *
  * 2. **Start Acquisition**:
  *    @code
  *    HL_ADC_Start();
  *    @endcode
  *
  * 3. **Data Processing Loop**:
  *    The application should check for new data availability in the main loop.
  *    @code
  *    if (HL_ADC_IsBufferReady())
  *    {
  *        uint16_t *pData = HL_ADC_GetBuffer();
  *        // Process pData[0] ... pData[ADC_BUFFER_SIZE-1]
  *    }
  *    @endcode
  *
  * @note The sampling rate is determined by TIM6 configuration.
  *       Target is 122.5 kHz.
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
	 * @brief Check if the buffer contains all zeros (empty)
	 * @param buffer: Pointer to the buffer to check
	 * @retval bool: true if all values are zero, false if any non-zero value exists
	 */
	bool HL_ADC_IsBufferEmpty(const uint16_t *buffer);

	#ifdef __cplusplus
}
#endif

#endif /* HAL_ADC_H */
