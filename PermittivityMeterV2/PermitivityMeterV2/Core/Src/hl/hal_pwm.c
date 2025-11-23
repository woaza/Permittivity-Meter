/**
  ******************************************************************************
  * @file    hal_pwm.c
  * @brief   PWM hardware abstraction layer implementation
  * @author  Majdedin Al Rashid
  * @date    18.11.2025
  ******************************************************************************
  * @attention
  *
  * This file provides PWM functionality for TIM1 CH2 (PA9 - SQR_20M_OUT).
  * The timer is configured to generate a square wave at the desired frequency
  * and duty cycle.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hl/hal_pwm.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/
#define PWM_CHANNEL         TIM_CHANNEL_2
#define PWM_TIMER_CLOCK     80000000UL  // TIM1 clock frequency (80 MHz from SYSCLK)

/* Private variables ---------------------------------------------------------*/
static TIM_HandleTypeDef *htim_pwm = NULL;
static uint8_t pwm_is_running = 0;
static uint8_t pwm_duty_cycle = PWM_DUTY_CYCLE_DEFAULT;
static uint32_t pwm_frequency = PWM_FREQUENCY_DEFAULT;

/* Private function prototypes -----------------------------------------------*/
static uint32_t PWM_Calculate_Pulse(uint8_t duty_cycle, uint32_t period);
static PWM_StatusTypeDef PWM_Configure_Timer(uint32_t frequency_hz, uint8_t duty_cycle);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief Calculate pulse width from duty cycle percentage
 * @param duty_cycle: Percentage (0-100)
 * @param period: Timer period value
 * @retval Pulse value for CCR register
 */
static uint32_t PWM_Calculate_Pulse(uint8_t duty_cycle, uint32_t period)
{
    if (duty_cycle > 100)
    {
        duty_cycle = 100;
    }
    return ((period + 1) * duty_cycle) / 100;
}

/**
 * @brief Configure timer for desired frequency and duty cycle
 * @param frequency_hz: Desired frequency in Hz
 * @param duty_cycle: Desired duty cycle in percent (0-100)
 * @retval PWM_StatusTypeDef: Status of operation
 */
static PWM_StatusTypeDef PWM_Configure_Timer(uint32_t frequency_hz, uint8_t duty_cycle)
{
    if (htim_pwm == NULL)
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }

    if (duty_cycle > 100)
    {
        return PWM_ERROR_INVALID_PARAM;
    }

    // Calculate prescaler and period for desired frequency
    // Timer frequency = Timer_Clock / ((Prescaler + 1) * (Period + 1))
    // For maximum resolution, we want the largest period value possible

    uint32_t prescaler = 0;
    uint32_t period = 0;

    // Try to find optimal prescaler and period values
    for (prescaler = 0; prescaler < 65536; prescaler++)
    {
        uint32_t timer_freq = PWM_TIMER_CLOCK / (prescaler + 1);
        period = (timer_freq / frequency_hz) - 1;

        if (period <= 65535 && period > 0)
        {
            break;
        }
    }

    // Check if valid configuration was found
    if (prescaler >= 65536 || period == 0 || period > 65535)
    {
        return PWM_ERROR_INVALID_PARAM;
    }

    // Stop timer if running
    bool was_running = pwm_is_running;
    if (was_running)
    {
        HAL_TIM_PWM_Stop(htim_pwm, PWM_CHANNEL);
    }

    // Update timer configuration (prescaler and period only)
    htim_pwm->Init.Prescaler = prescaler;
    htim_pwm->Init.Period = period;

    // Apply new prescaler and period values to hardware registers
    __HAL_TIM_SET_PRESCALER(htim_pwm, prescaler);
    __HAL_TIM_SET_AUTORELOAD(htim_pwm, period);

    // Update pulse width (duty cycle)
    uint32_t pulse = PWM_Calculate_Pulse(duty_cycle, period);
    __HAL_TIM_SET_COMPARE(htim_pwm, PWM_CHANNEL, pulse);

    // Generate update event to load the new values
    htim_pwm->Instance->EGR = TIM_EGR_UG;

    // Restart timer if it was running
    if (was_running)
    {
        HAL_TIM_PWM_Start(htim_pwm, PWM_CHANNEL);
    }

    // Update stored values
    pwm_duty_cycle = duty_cycle;
    pwm_frequency = PWM_TIMER_CLOCK / ((prescaler + 1) * (period + 1));

    return PWM_OK;
}

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize PWM module
 * @param htim: Pointer to TIM handle structure
 * @retval PWM_StatusTypeDef: Status of operation
 */
PWM_StatusTypeDef HAL_PWM_Init(TIM_HandleTypeDef *htim)
{
    if (htim == NULL)
    {
        return PWM_ERROR_INVALID_PARAM;
    }

    // Store timer handle - CubeMX already configured the timer
    htim_pwm = htim;
    pwm_is_running = 0;
    // Read current configuration from CubeMX settings
    uint32_t prescaler = htim_pwm->Init.Prescaler;
    uint32_t period = htim_pwm->Init.Period;

    // Calculate current frequency from CubeMX settings
    pwm_frequency = PWM_TIMER_CLOCK / ((prescaler + 1) * (period + 1));

    // Read current pulse value to determine duty cycle
    uint32_t pulse = __HAL_TIM_GET_COMPARE(htim_pwm, PWM_CHANNEL);
    pwm_duty_cycle = (pulse * 100) / (period + 1);

    return PWM_OK;
}

/**
 * @brief Start PWM output
 * @retval PWM_StatusTypeDef: Status of operation
 */
PWM_StatusTypeDef HAL_PWM_Start(void)
{
    if (htim_pwm == NULL)
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }

    if (pwm_is_running)
    {
        return PWM_OK;  // Already running
    }

    if (HAL_TIM_PWM_Start(htim_pwm, PWM_CHANNEL) != HAL_OK)
    {
        return PWM_ERROR;
    }

    pwm_is_running = 1;
    return PWM_OK;
}

/**
 * @brief Stop PWM output
 * @retval PWM_StatusTypeDef: Status of operation
 */
PWM_StatusTypeDef HAL_PWM_Stop(void)
{
    if (htim_pwm == NULL)
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }

    if (!pwm_is_running)
    {
        return PWM_OK;  // Already stopped
    }

    if (HAL_TIM_PWM_Stop(htim_pwm, PWM_CHANNEL) != HAL_OK)
    {
        return PWM_ERROR;
    }

    pwm_is_running = 0;
    return PWM_OK;
}

/**
 * @brief Set PWM duty cycle
 * @param duty_cycle: Duty cycle in percent (0-100)
 * @retval PWM_StatusTypeDef: Status of operation
 */
PWM_StatusTypeDef HAL_PWM_SetDutyCycle(uint8_t duty_cycle)
{
    if (htim_pwm == NULL)
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }

    if (duty_cycle > 100)
    {
        return PWM_ERROR_INVALID_PARAM;
    }

    // Calculate new pulse value
    uint32_t period = htim_pwm->Init.Period;
    uint32_t pulse = PWM_Calculate_Pulse(duty_cycle, period);

    // Update CCR register
    __HAL_TIM_SET_COMPARE(htim_pwm, PWM_CHANNEL, pulse);

    pwm_duty_cycle = duty_cycle;
    return PWM_OK;
}

/**
 * @brief Set PWM frequency
 * @param frequency_hz: Desired frequency in Hz
 * @retval PWM_StatusTypeDef: Status of operation
 */
PWM_StatusTypeDef HAL_PWM_SetFrequency(uint32_t frequency_hz)
{
    if (htim_pwm == NULL)
    {
        return PWM_ERROR_NOT_INITIALIZED;
    }

    if (frequency_hz == 0 || frequency_hz > PWM_TIMER_CLOCK)
    {
        return PWM_ERROR_INVALID_PARAM;
    }

    return PWM_Configure_Timer(frequency_hz, pwm_duty_cycle);
}

/**
 * @brief Get current PWM duty cycle
 * @retval uint8_t: Current duty cycle in percent (0-100)
 */
uint8_t HAL_PWM_GetDutyCycle(void)
{
    return pwm_duty_cycle;
}

/**
 * @brief Get current PWM frequency
 * @retval uint32_t: Current frequency in Hz
 */
uint32_t HAL_PWM_GetFrequency(void)
{
    return pwm_frequency;
}

/**
 * @brief Check if PWM is currently running
 * @retval bool: true if running, false if stopped
 */
bool HAL_PWM_IsRunning(void)
{
    return (pwm_is_running != 0);
}

/**
 * @brief Update PWM pulsing effect (call periodically)
 * @retval None
 */
void HAL_PWM_Pulse_Update(void)
{
    static uint8_t duty = 0;
    static int8_t step = 1;

    HAL_PWM_SetDutyCycle(duty);
    duty += step;
    if (duty >= 100) step = -1;
    if (duty <= 0) step = 1;
}
