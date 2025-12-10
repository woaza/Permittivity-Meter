/**
 * @file    hal_board.h
 * @brief   HAL Board Interface - Direct Hardware Control for Manual Mode
 * @author  Majdedin Al Rashid
 * @version 1.0
 * @date    2025-12-09
 * 
 * @details This module provides direct hardware access for manual/debug operation.
 *          Unlike MockBoard (which is for simulation/state machine testing),
 *          this module directly controls real hardware via HAL drivers.
 *          
 *          Use Cases:
 *          - Manual LED control via CLI/UI
 *          - Direct ADC value reading
 *          - Direct DAC voltage setting
 *          - RF gain control
 *          - Button state reading
 *          
 *          Architecture:
 *              UI/CLI → HalBoard (this) → HAL (hal_gpio, hal_adc, hal_dac)
 */

#ifndef HL_HAL_BOARD_H_
#define HL_HAL_BOARD_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*                              Status Codes                                  */
/* -------------------------------------------------------------------------- */

typedef enum 
{
    HAL_BOARD_OK = 0,
    HAL_BOARD_ERROR,
    HAL_BOARD_ERROR_INVALID_PARAM,
    HAL_BOARD_ERROR_NOT_READY
} HalBoard_Status_t;

/* -------------------------------------------------------------------------- */
/*                              Initialization                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Initialize HAL Board module.
 * @note  Call once at startup after HAL initialization.
 */
void HalBoard_Init(void);

/* -------------------------------------------------------------------------- */
/*                              LED Control                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set LED state directly via HAL GPIO.
 * @param led_id LED identifier (0=INIT, 1=MEAS, 2=EXCITE, 3=ERR)
 * @param state  0=OFF, 1=ON
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_LED_Set(uint8_t led_id, uint8_t state);

/**
 * @brief Get LED state directly from HAL GPIO.
 * @param led_id LED identifier (0=INIT, 1=MEAS, 2=EXCITE, 3=ERR)
 * @param state  Pointer to store state (0=OFF, 1=ON)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_LED_Get(uint8_t led_id, uint8_t *state);

/**
 * @brief Toggle LED state directly via HAL GPIO.
 * @param led_id LED identifier (0=INIT, 1=MEAS, 2=EXCITE, 3=ERR)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_LED_Toggle(uint8_t led_id);

/* -------------------------------------------------------------------------- */
/*                              Button Control                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read button state directly from HAL GPIO.
 * @param state Pointer to store state (1=pressed, 0=not pressed)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_Button_Read(uint8_t *state);

/* -------------------------------------------------------------------------- */
/*                              ADC Control                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Read single ADC value (blocking conversion).
 * @param value Pointer to store 12-bit ADC value (0-4095)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_ADC_ReadSingle(uint16_t *value);

/**
 * @brief Get ADC voltage representation.
 * @param voltage Pointer to store voltage (0.0V to 3.3V)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_ADC_ReadVoltage(float *voltage);

/**
 * @brief Check if ADC DMA buffer is ready.
 * @retval true if buffer ready, false otherwise
 */
bool HalBoard_ADC_IsBufferReady(void);

/* -------------------------------------------------------------------------- */
/*                              DAC Control                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set DAC channel voltage directly.
 * @param channel 0=FREQ_TUNE (PA4), 1=Q_FACTOR (PA5)
 * @param voltage Voltage in range 0.0V to 3.3V
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_DAC_SetVoltage(uint8_t channel, float voltage);

/**
 * @brief Set DAC channel raw value directly.
 * @param channel 0=FREQ_TUNE (PA4), 1=Q_FACTOR (PA5)
 * @param raw_value 12-bit value (0-4095)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_DAC_SetRaw(uint8_t channel, uint16_t raw_value);

/* -------------------------------------------------------------------------- */
/*                              RF Gain Control                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set RF gain via GPIO pins (2-bit encoding).
 * @param gain_level Gain level 0-3
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_RF_SetGain(uint8_t gain_level);

/**
 * @brief Get current RF gain setting.
 * @param gain_level Pointer to store gain level (0-3)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_RF_GetGain(uint8_t *gain_level);

/* -------------------------------------------------------------------------- */
/*                              NINA Module Control                           */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set NINA module reset pin.
 * @param state 0=active (reset), 1=inactive (run)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_NINA_SetReset(uint8_t state);

/**
 * @brief Set NINA module stop pin.
 * @param state 0=run, 1=stop
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_NINA_SetStop(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* HL_HAL_BOARD_H_ */
