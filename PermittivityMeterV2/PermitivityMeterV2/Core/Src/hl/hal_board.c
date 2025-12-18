/**
 * @file    hal_board.c
 * @brief   HAL Board Implementation - Direct Hardware Control for Manual Mode
 * @author  Majdedin Al Rashid
 * @version 1.0
 * @date    2025-12-09
 * 
 * @details Implements direct hardware control functions for manual/debug operation.
 *          Uses HAL drivers (hal_gpio, hal_adc, hal_dac) for real hardware access.
 */

#include "hl/hal_board.h"
#include "hl/hal_gpio.h"
#include "hl/hal_adc.h"
#include "hl/hal_dac.h"
#include "main.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Private Data                                  */
/* -------------------------------------------------------------------------- */

static uint8_t s_initialized = 0;

/* -------------------------------------------------------------------------- */
/*                              Pin Mapping Tables                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Map LED ID (0-3) to HAL GPIO pin enum
 */
static HL_GPIO_Pin_t led_id_to_gpio(uint8_t led_id)
{
    switch (led_id)
    {
        case 0: return HL_GPIO_LED_INIT;
        case 1: return HL_GPIO_LED_MEAS;
        case 2: return HL_GPIO_LED_EXCITE;
        case 3: return HL_GPIO_LED_ERR;
        default: return HL_GPIO_LED_INIT;
    }
}

/**
 * @brief Map channel ID (0-1) to DAC channel enum
 */
static DAC_ChannelTypeDef channel_id_to_dac(uint8_t channel)
{
    return (channel == 0) ? DAC_CH_FREQ_TUNE : DAC_CH_Q_FACTOR;
}

/* -------------------------------------------------------------------------- */
/*                              Initialization                                */
/* -------------------------------------------------------------------------- */

void HalBoard_Init(void)
{
    if (!s_initialized)
    {
        /* HAL drivers are already initialized in main.c */
        HL_GPIO_Init();
        s_initialized = 1;
    }
}

/* -------------------------------------------------------------------------- */
/*                              LED Control                                   */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_LED_Set(uint8_t led_id, uint8_t state)
{
    if (led_id > 3)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_Pin_t pin = led_id_to_gpio(led_id);
    HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
    
    if (HL_GPIO_Write(pin, gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_LED_Get(uint8_t led_id, uint8_t *state)
{
    if (led_id > 3 || state == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_Pin_t pin = led_id_to_gpio(led_id);
    HL_GPIO_State_t gpio_state = HL_GPIO_LOW;
    
    if (HL_GPIO_Read(pin, &gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    *state = (gpio_state == HL_GPIO_HIGH) ? 1U : 0U;
    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_LED_Toggle(uint8_t led_id)
{
    if (led_id > 3)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_Pin_t pin = led_id_to_gpio(led_id);
    
    if (HL_GPIO_Toggle(pin) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

/* -------------------------------------------------------------------------- */
/*                              Button Control                                */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_Button_Read(uint8_t *state)
{
    if (state == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_State_t gpio_state = HL_GPIO_LOW;
    
    if (HL_GPIO_Read(HL_GPIO_BTN_USER, &gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    /* Button is active-low on Nucleo boards (pressed = LOW) */
    *state = (gpio_state == HL_GPIO_LOW) ? 1U : 0U;
    return HAL_BOARD_OK;
}

/* -------------------------------------------------------------------------- */
/*                              ADC Control                                   */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_ADC_ReadSingle(uint16_t *value)
{
    if (value == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    /*
     * The application runs ADC1 continuously using TIM6 trigger + DMA circular
     * mode (see HL_ADC_*). Starting a blocking single conversion here would
     * conflict with the running DMA engine and typically fails with HAL_BUSY.
     *
     * So: serve "HAL ADC" reads from the most recent full DMA buffer.
     */
    if (!HL_ADC_IsBufferReady())
    {
        return HAL_BOARD_ERROR_NOT_READY;
    }

    uint16_t *buffer = HL_ADC_GetBuffer();
    if (buffer == NULL)
    {
        return HAL_BOARD_ERROR_NOT_READY;
    }

    uint32_t sum = 0U;
    for (uint32_t i = 0U; i < ADC_BUFFER_SIZE; ++i)
    {
        sum += buffer[i];
    }

    *value = (uint16_t)(sum / ADC_BUFFER_SIZE);
    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_ADC_ReadVoltage(float *voltage)
{
    if (voltage == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    uint16_t raw_value = 0;
    HalBoard_Status_t status = HalBoard_ADC_ReadSingle(&raw_value);
    
    if (status != HAL_BOARD_OK)
    {
        return status;
    }

    /* Convert to voltage: (raw / 4095) * 3.3V */
    *voltage = ((float)raw_value / 4095.0f) * 3.3f;
    return HAL_BOARD_OK;
}

bool HalBoard_ADC_IsBufferReady(void)
{
    return HL_ADC_IsBufferReady();
}

/* -------------------------------------------------------------------------- */
/*                              DAC Control                                   */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_DAC_SetVoltage(uint8_t channel, float voltage)
{
    if (channel > 1)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    if (voltage < 0.0f || voltage > 3.3f)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    DAC_ChannelTypeDef dac_ch = channel_id_to_dac(channel);
    
    if (HL_DAC_SetVoltage(dac_ch, voltage) != DAC_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_DAC_SetRaw(uint8_t channel, uint16_t raw_value)
{
    if (channel > 1)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    if (raw_value > 4095)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    DAC_ChannelTypeDef dac_ch = channel_id_to_dac(channel);
    
    if (HL_DAC_SetRawValue(dac_ch, raw_value) != DAC_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

/* -------------------------------------------------------------------------- */
/*                              RF Gain Control                               */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_RF_SetGain(uint8_t gain_level)
{
    if (gain_level > 3)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    /* Gain is encoded in 2 bits: GAIN_0 (LSB) and GAIN_1 (MSB) */
    HL_GPIO_State_t bit0 = (gain_level & 0x01) ? HL_GPIO_HIGH : HL_GPIO_LOW;
    HL_GPIO_State_t bit1 = (gain_level & 0x02) ? HL_GPIO_HIGH : HL_GPIO_LOW;

    if (HL_GPIO_Write(HL_GPIO_RF_GAIN_0, bit0) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    if (HL_GPIO_Write(HL_GPIO_RF_GAIN_1, bit1) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_RF_GetGain(uint8_t *gain_level)
{
    if (gain_level == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_State_t bit0 = HL_GPIO_LOW;
    HL_GPIO_State_t bit1 = HL_GPIO_LOW;

    if (HL_GPIO_Read(HL_GPIO_RF_GAIN_0, &bit0) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    if (HL_GPIO_Read(HL_GPIO_RF_GAIN_1, &bit1) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    *gain_level = ((bit1 == HL_GPIO_HIGH) ? 2U : 0U) | 
                  ((bit0 == HL_GPIO_HIGH) ? 1U : 0U);
    return HAL_BOARD_OK;
}

/* -------------------------------------------------------------------------- */
/*                              NINA Module Control                           */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_NINA_SetReset(uint8_t state)
{
    HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
    
    if (HL_GPIO_Write(HL_GPIO_NINA_RST, gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_NINA_SetStop(uint8_t state)
{
    HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
    
    if (HL_GPIO_Write(HL_GPIO_NINA_STOP, gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_NINA_SetDTR(uint8_t state)
{
    HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
    
    if (HL_GPIO_Write(HL_GPIO_NINA_DTR, gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_NINA_GetDSR(uint8_t *state)
{
    if (state == NULL)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_State_t gpio_state = HL_GPIO_LOW;
    
    if (HL_GPIO_Read(HL_GPIO_NINA_DSR, &gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    *state = (gpio_state == HL_GPIO_HIGH) ? 1U : 0U;
    return HAL_BOARD_OK;
}

HalBoard_Status_t HalBoard_NINA_GetLED(uint8_t led_id, uint8_t *state)
{
    if (state == NULL || led_id > 2)
    {
        return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_Pin_t pin;
    switch (led_id)
    {
        case 0: pin = HL_GPIO_NINA_LED_RED; break;
        case 1: pin = HL_GPIO_NINA_LED_BLUE; break;
        case 2: pin = HL_GPIO_NINA_LED_GREEN; break;
        default: return HAL_BOARD_ERROR_INVALID_PARAM;
    }

    HL_GPIO_State_t gpio_state = HL_GPIO_LOW;
    
    if (HL_GPIO_Read(pin, &gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    *state = (gpio_state == HL_GPIO_HIGH) ? 1U : 0U;
    return HAL_BOARD_OK;
}

/* -------------------------------------------------------------------------- */
/*                              Op-Amp Control                                */
/* -------------------------------------------------------------------------- */

HalBoard_Status_t HalBoard_OpAmp_SetDisable(uint8_t state)
{
    HL_GPIO_State_t gpio_state = state ? HL_GPIO_HIGH : HL_GPIO_LOW;
    
    if (HL_GPIO_Write(HL_GPIO_OP_DIS, gpio_state) != GPIO_OK)
    {
        return HAL_BOARD_ERROR;
    }

    return HAL_BOARD_OK;
}
