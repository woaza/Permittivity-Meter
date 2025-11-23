/**
  ******************************************************************************
  * @file    test_hal_dac.h
  * @brief   Header file for DAC hardware abstraction layer tests
  * @author  Majdedin Al Rashid
  * @date    22.11.2025
  ******************************************************************************
  */

#ifndef TEST_HAL_DAC_H
#define TEST_HAL_DAC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "hl/hal_dac.h"

/* Exported functions prototypes ---------------------------------------------*/

/**
 * @brief Run all DAC tests
 * @param hdac: Pointer to DAC handle structure
 * @retval DAC_StatusTypeDef: Status of operation (DAC_OK if all tests passed)
 */
DAC_StatusTypeDef Test_HL_DAC_RunAll(DAC_HandleTypeDef *hdac);

/**
 * @brief Generate a continuous triangle wave on DAC channels for oscilloscope testing.
 *        This function DOES NOT RETURN (infinite loop).
 * @param hdac: Pointer to DAC handle structure
 * @param hiwdg: Pointer to IWDG handle structure (to refresh watchdog)
 */
void Test_HL_DAC_GenerateWaveform(DAC_HandleTypeDef *hdac, IWDG_HandleTypeDef *hiwdg);

#ifdef __cplusplus
}
#endif

#endif /* TEST_HAL_DAC_H */
