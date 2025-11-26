/**
  ******************************************************************************
  * @file    hal_adc.c
  * @brief   ADC hardware abstraction layer implementation
  * @author  Majdedin Al Rashid
  * @date    24.11.2025
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hl/hal_adc.h"
#include "mocks/mock_board.h"
#include "hl/hal_dac.h" // Needed to get current DAC voltages for mock calculation

/* Private variables ---------------------------------------------------------*/
static ADC_HandleTypeDef *phadc1 = NULL;
static volatile uint32_t adc_dma_buffer[1] = {0};
static bool use_mock_mode = false;

/* Private constants ---------------------------------------------------------*/
#define ADC_VREF    3.3f
#define ADC_MAX_VAL 4095.0f

/* Exported functions --------------------------------------------------------*/

ADC_StatusTypeDef HL_ADC_Init(ADC_HandleTypeDef *hadc)
{
    if (hadc == NULL)
    {
        return ADC_ERROR;
    }

    phadc1 = hadc;
    
    // Default to real hardware mode
    use_mock_mode = false;

    return ADC_OK;
}

ADC_StatusTypeDef HL_ADC_Start(void)
{
    if (use_mock_mode)
    {
        // In mock mode, we don't need to start the actual hardware
        return ADC_OK;
    }

    if (phadc1 == NULL)
    {
        return ADC_ERROR;
    }

    // Start ADC with DMA in circular mode (if configured that way in CubeMX)
    // or normal mode. We point it to our internal buffer.
    if (HAL_ADC_Start_DMA(phadc1, (uint32_t*)adc_dma_buffer, 1) != HAL_OK)
    {
        return ADC_ERROR;
    }

    return ADC_OK;
}

ADC_StatusTypeDef HL_ADC_Stop(void)
{
    if (use_mock_mode)
    {
        return ADC_OK;
    }

    if (phadc1 == NULL)
    {
        return ADC_ERROR;
    }

    if (HAL_ADC_Stop_DMA(phadc1) != HAL_OK)
    {
        return ADC_ERROR;
    }

    return ADC_OK;
}

float HL_ADC_GetVoltage(void)
{
    if (use_mock_mode)
    {
        // In mock mode, we calculate what the ADC *would* see based on the current
        // state of the DACs (Frequency and Q-Factor tuning).
        // We assume the DAC driver has stored the last set values.
        
        // Note: This requires HL_DAC_GetVoltage to be available and accurate.
        // If HL_DAC is not initialized, we might get 0.0f.
        float freq_volts = HL_DAC_GetVoltage(DAC_CH_FREQ_TUNE);
        float q_volts = HL_DAC_GetVoltage(DAC_CH_Q_FACTOR);
        
        // We also need to know the gain index, but for now we can assume default or 
        // add a dependency on the RF BSP state if needed. 
        // For simplicity in this HAL layer, we'll pass 0 as gain index or 
        // ideally, the MockBoard should track the gain itself if we set it via BSP.
        // However, since this is a low-level HAL, we'll just query the MockBoard directly
        // using the voltages we know.
        
        return MockBoard_RF_ComputeAmplitude(freq_volts, q_volts, 1); // Default gain 1
    }

    // Real Hardware Mode
    uint32_t raw = HL_ADC_GetRawValue();
    return ((float)raw * ADC_VREF) / ADC_MAX_VAL;
}

uint32_t HL_ADC_GetRawValue(void)
{
    if (use_mock_mode)
    {
        // Convert the simulated voltage back to raw ADC counts
        float voltage = HL_ADC_GetVoltage();
        uint32_t raw = (uint32_t)((voltage * ADC_MAX_VAL) / ADC_VREF);
        if (raw > 4095) raw = 4095;
        return raw;
    }

    // Return the latest value from the DMA buffer
    return adc_dma_buffer[0];
}

void HL_ADC_SetMockMode(bool enable)
{
    use_mock_mode = enable;
    
    // If enabling mock mode, ensure the mock board is initialized
    if (enable)
    {
        MockBoard_Init();
    }
}
