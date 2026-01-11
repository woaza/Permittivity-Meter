/**
  ******************************************************************************
  * @file    hal_adc.c
  * @brief   ADC hardware abstraction layer implementation
  * @author  Majdedin Al Rashid
  * @date    26.11.2025
  ******************************************************************************
  * @attention
  *
  * This file provides functionality for ADC acquisition using TIM6 trigger
  * and DMA circular mode.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "hl/hal_adc.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/
#define ADC_DMA_BUFFER_SIZE     (ADC_BUFFER_SIZE * 2)   // Ping-Pong buffer

/* Private variables ---------------------------------------------------------*/
static ADC_HandleTypeDef *hadc_local = NULL;
static TIM_HandleTypeDef *htim_trigger = NULL;

static uint16_t dma_buffer[ADC_DMA_BUFFER_SIZE];
static volatile uint16_t *volatile ready_buffer_ptr = NULL;
static volatile bool buffer_ready_flag = false;

/* Exported functions --------------------------------------------------------*/

/**
 * @brief Initialize ADC module
 * @param hadc: Pointer to ADC handle structure
 * @param htim: Pointer to TIM handle structure (used for triggering)
 * @retval ADC_StatusTypeDef: Status of operation
 */
ADC_StatusTypeDef HL_ADC_Init(ADC_HandleTypeDef *hadc, TIM_HandleTypeDef *htim)
{
    if (hadc == NULL || htim == NULL)
    {
        return ADC_ERROR_INVALID_PARAM;
    }

    hadc_local = hadc;
    htim_trigger = htim;

    // Configure Timer for 122.5 kHz
    // Clock source is APB1 (80 MHz)
    // Target: 122500 Hz
    // Divisor = 80,000,000 / 122,500 = 653.06...
    // Use Prescaler = 0, Period = 652 (653 ticks)
    
    htim_trigger->Init.Prescaler = 0;
    htim_trigger->Init.Period = 652;
    htim_trigger->Init.CounterMode = TIM_COUNTERMODE_UP;
    htim_trigger->Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_Base_Init(htim_trigger) != HAL_OK)
    {
        return ADC_ERROR;
    }

    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    
    if (HAL_TIMEx_MasterConfigSynchronization(htim_trigger, &sMasterConfig) != HAL_OK)
    {
        return ADC_ERROR;
    }

    // --- CRITICAL: Override CubeMX default ADC configuration ---
    // CubeMX sets ContinuousConvMode = ENABLE and ExternalTrigConv = SOFTWARE by default.
    // We need ContinuousConvMode = DISABLE and ExternalTrigConv = TIM6_TRGO to strictly follow timing.
    
    hadc_local->Init.ContinuousConvMode = DISABLE;
    hadc_local->Init.ExternalTrigConv = ADC_EXTERNALTRIG_T6_TRGO;
    hadc_local->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;

    if (HAL_ADC_Init(hadc_local) != HAL_OK)
    {
        return ADC_ERROR;
    }

    return ADC_OK;
}

/**
 * @brief Start ADC acquisition
 * @retval ADC_StatusTypeDef: Status of operation
 */
ADC_StatusTypeDef HL_ADC_Start(void)
{
    if (hadc_local == NULL || htim_trigger == NULL)
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }

    // Start ADC in DMA Circular mode
    if (HAL_ADC_Start_DMA(hadc_local, (uint32_t*)dma_buffer, ADC_DMA_BUFFER_SIZE) != HAL_OK)
    {
        return ADC_ERROR;
    }

    // Start Timer to trigger conversions
    if (HAL_TIM_Base_Start(htim_trigger) != HAL_OK)
    {
        HAL_ADC_Stop_DMA(hadc_local);
        return ADC_ERROR;
    }

    return ADC_OK;
}

/**
 * @brief Stop ADC acquisition
 * @retval ADC_StatusTypeDef: Status of operation
 */
ADC_StatusTypeDef HL_ADC_Stop(void)
{
    if (hadc_local == NULL || htim_trigger == NULL)
    {
        return ADC_ERROR_NOT_INITIALIZED;
    }

    // Stop Timer
    HAL_TIM_Base_Stop(htim_trigger);

    // Stop ADC
    HAL_ADC_Stop_DMA(hadc_local);

    return ADC_OK;
}

/**
 * @brief Get pointer to the latest complete buffer
 * @retval uint16_t*: Pointer to buffer, or NULL if not ready
 */
uint16_t* HL_ADC_GetBuffer(void)
{
    if (buffer_ready_flag)
    {
        buffer_ready_flag = false; // Consume-on-read
        return (uint16_t*)ready_buffer_ptr;
    }
    return NULL;
}

/**
 * @brief Check if a new buffer is ready for processing
 * @retval bool: true if ready, false otherwise
 */
bool HL_ADC_IsBufferReady(void)
{
    return buffer_ready_flag;
}

/**
 * @brief Check if the buffer contains all zeros (empty)
 * @param buffer: Pointer to the buffer to check
 * @retval bool: true if all values are zero, false if any non-zero value exists
 */
bool HL_ADC_IsBufferEmpty(const uint16_t *buffer)
{
    if (buffer == NULL)
    {
        return true;  // NULL buffer is considered empty
    }

    for (uint32_t i = 0; i < ADC_BUFFER_SIZE; i++)
    {
        if (buffer[i] != 0)
        {
            return false;  // Found a non-zero value
        }
    }
    return true;  // All values are zero
}

/* Callbacks -----------------------------------------------------------------*/

/**
 * @brief  Conversion complete callback in non-blocking mode
 * @param  hadc: ADC handle
 * @retval None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == hadc_local)
    {
        // Second half of buffer is ready (indices 512 to 1023)
        //Sampling-time test
        //HAL_GPIO_TogglePin(MEAS_LED_GPIO_Port, MEAS_LED_Pin);

        ready_buffer_ptr = &dma_buffer[ADC_BUFFER_SIZE];
        buffer_ready_flag = true;
    }
}

/**
 * @brief  Conversion half DMA transfer callback in non-blocking mode
 * @param  hadc: ADC handle
 * @retval None
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc == hadc_local)
    {
        // First half of buffer is ready (indices 0 to 511)
        ready_buffer_ptr = &dma_buffer[0];
        buffer_ready_flag = true;
    }
}
