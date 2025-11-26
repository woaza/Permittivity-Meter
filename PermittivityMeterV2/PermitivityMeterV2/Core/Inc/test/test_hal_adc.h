#ifndef TEST_HAL_ADC_H
#define TEST_HAL_ADC_H

#include "main.h"

/**
 * @brief Test the ADC HAL in mock mode.
 *        Verifies that setting DAC voltages results in expected ADC readings
 *        via the MockBoard simulation.
 */
void Test_HL_ADC_Mock_Read(ADC_HandleTypeDef *hadc, DAC_HandleTypeDef *hdac);

#endif // TEST_HAL_ADC_H
