//#include "hal_pwm.h"
#include "main.h"

// ###### extern variables from main.c

extern TIM_HandleTypeDef htim1;

// ###### defines

#define PWM_CHANNEL TIM_CHANNEL_2
#define PWM_DUTY_CYCLE_DEFAULT 50  // 50%
#define PWM_PERIOD_VALUE 4  // For 20 MHz output with current prescaler

// ###### global variables

static uint8_t pwm_is_running = 0;
static uint8_t pwm_duty_cycle = PWM_DUTY_CYCLE_DEFAULT;

// ###### private functions

/**
 * Calculate pulse width from duty cycle percentage
 * @param duty_cycle: Percentage (0-100)
 * @return Pulse value for CCR register
 */
static uint32_t PWM_Calculate_Pulse(uint8_t duty_cycle)
{
    return (htim1.Init.Period * duty_cycle) / 100;
}

