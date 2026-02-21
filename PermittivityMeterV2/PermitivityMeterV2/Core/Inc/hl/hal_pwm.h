/**
  ******************************************************************************
  * @file    hal_pwm.h
  * @brief   Hardware Abstraction Layer for 20 MHz Excitation PWM
  * @author  Majdedin Al Rashid
  * @date    18.11.2025
  ******************************************************************************
  * @details
  * This module controls the generation of the 20 MHz square wave signal used
  * to excite the LC tank circuit. It wraps the STM32 TIM2 peripheral.
  *
  * **Configuration:**
  * - **Timer**: TIM2 Channel 1
  * - **Output Pin**: PA0 (SQR_20M_OUT)
  * - **Target Frequency**: 20 MHz
  *
  * @section pwm_usage How to Use
  *
  * 1. **Initialization**:
  *    @code
  *    HAL_PWM_Init(&htim2);
  *    @endcode
  *
  * 2. **Configure Signal**:
  *    @code
  *    HAL_PWM_SetFrequency(20000000); // 20 MHz
  *    HAL_PWM_SetDutyCycle(50);       // 50% Duty Cycle
  *    @endcode
  *
  * 3. **Control Output**:
  *    @code
  *    HAL_PWM_Start(); // Signal active on PA0
  *    // ... measurement ...
  *    HAL_PWM_Stop();  // Signal inactive
  *    @endcode
  ******************************************************************************
  */

#ifndef HAL_PWM_H
#define HAL_PWM_H

#ifdef __cplusplus
extern "C"
{
	#endif

	/* Includes ------------------------------------------------------------------*/
	#include "stm32l4xx_hal.h"
	#include <stdint.h>
	#include <stdbool.h>

	/* Exported constants --------------------------------------------------------*/
	#define PWM_DUTY_CYCLE_DEFAULT  50  // 50% duty cycle
	#define PWM_FREQUENCY_DEFAULT   20000000UL  // 20 MHz target frequency

	/* Exported types ------------------------------------------------------------*/
	typedef enum
	{
		PWM_OK = 0,
		PWM_ERROR = 1,
		PWM_ERROR_INVALID_PARAM = 2,
		PWM_ERROR_NOT_INITIALIZED = 3
	} PWM_StatusTypeDef;

	/* Exported functions prototypes ---------------------------------------------*/

	/**
	 * @brief Initialize PWM module
	 * @note Must be called before using any other PWM functions
	 * @param htim: Pointer to TIM handle structure
	 * @retval PWM_StatusTypeDef: Status of operation
	 */
	PWM_StatusTypeDef HAL_PWM_Init(TIM_HandleTypeDef *htim);

	/**
	 * @brief Start PWM output
	 * @retval PWM_StatusTypeDef: Status of operation
	 */
	PWM_StatusTypeDef HAL_PWM_Start(void);

	/**
	 * @brief Stop PWM output
	 * @retval PWM_StatusTypeDef: Status of operation
	 */
	PWM_StatusTypeDef HAL_PWM_Stop(void);

	/**
	 * @brief Set PWM duty cycle
	 * @param duty_cycle: Duty cycle in percent (0-100)
	 * @retval PWM_StatusTypeDef: Status of operation
	 */
	PWM_StatusTypeDef HAL_PWM_SetDutyCycle(uint8_t duty_cycle);

	/**
	 * @brief Set PWM frequency
	 * @param frequency_hz: Desired frequency in Hz
	 * @retval PWM_StatusTypeDef: Status of operation
	 * @note The actual frequency may differ slightly due to timer resolution
	 */
	PWM_StatusTypeDef HAL_PWM_SetFrequency(uint32_t frequency_hz);

	/**
	 * @brief Get current PWM duty cycle
	 * @retval uint8_t: Current duty cycle in percent (0-100)
	 */
	uint8_t HAL_PWM_GetDutyCycle(void);

	/**
	 * @brief Get current PWM frequency
	 * @retval uint32_t: Current frequency in Hz
	 */
	uint32_t HAL_PWM_GetFrequency(void);

	/**
	 * @brief Check if PWM is currently running
	 * @retval bool: true if running, false if stopped
	 */
	bool HAL_PWM_IsRunning(void);

	/**
	 * @brief Update PWM pulsing effect (call periodically)
	 * @retval None
	 */
	void HAL_PWM_Pulse_Update(void);

	#ifdef __cplusplus
}
#endif

#endif /* HAL_PWM_H */
