/**
 * @file    hal_board.h
 * @brief   HAL Board Interface - Direct Hardware Control for Manual Mode/CLI
 * @author  Majdedin Al Rashid
 * @version 1.0
 * @date    2025-12-09
 * 
 * @details 
 * This module serves as a "Board Support Package" (BSP) for the low-level hardware
 * controls that don't fit into a specific peripheral driver or aggregate multiple
 * drivers. It is primarily used by the **Bluetooth CLI** and **Manual Mode** logic
 * to directly manipulate the hardware for testing and debugging.
 *
 * **Architecture:**
 * The `HalBoard` module sits above `hal_gpio`, `hal_adc`, and `hal_dac`, providing
 * a unified interface for "board-level" operations like "Set LED" or "Read Voltage".
 *
 * @section board_usage Usage Examples
 *
 * **LED Control:**
 * @code
 * HalBoard_LED_Set(0, 1); // Turn ON LED 0 (Init Green)
 * HalBoard_LED_Toggle(1); // Toggle LED 1 (Meas Blue)
 * @endcode
 *
 * **Direct DAC/ADC Access:**
 * @code
 * // Set Frequency Tune DAC to 1.5V
 * HalBoard_DAC_SetVoltage(0, 1500);
 * 
 * // Read current ADC voltage in millivolts
 * uint16_t voltage_mv;
 * HalBoard_ADC_ReadVoltage(&voltage_mv);
 * @endcode
 *
 * **NINA Module Reset:**
 * @code
 * HalBoard_NINA_SetReset(0); // Assert Reset (Active Low)
 * HAL_Delay(10);
 * HalBoard_NINA_SetReset(1); // Release Reset
 * @endcode
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
 * @brief Get ADC voltage in millivolts (integer, no floating-point).
 * @param voltage_mv Pointer to store voltage in mV (0 to 3300)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_ADC_ReadVoltage(uint16_t *voltage_mv);

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
 * @param voltage_mv Voltage in Millivolts (0 to 3300)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_DAC_SetVoltage(uint8_t channel, uint16_t voltage_mv);

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
 * @brief Set RF gain via GPIO pins (3-bit encoding).
 * @param gain_level Gain level 0-7
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_RF_SetGain(uint8_t gain_level);

/**
 * @brief Get current RF gain setting.
 * @param gain_level Pointer to store gain level (0-7)
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

// HalBoard_NINA_SetStop removed
// HalBoard_NINA_SetDTR removed
// HalBoard_NINA_GetDSR removed

/**
 * @brief Get NINA module LED state.
 * @param led_id 0=RED, 1=BLUE, 2=GREEN
 * @param state Pointer to store state (0=low, 1=high)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_NINA_GetLED(uint8_t led_id, uint8_t *state);

/* -------------------------------------------------------------------------- */
/*                              Op-Amp Control                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Set Op-Amp Disable pin.
 * @param state 0=enable (low), 1=disable (high)
 * @retval HalBoard_Status_t status
 */
HalBoard_Status_t HalBoard_OpAmp_SetDisable(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* HL_HAL_BOARD_H_ */
