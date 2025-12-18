/**
 * @file    hal_gpio.c
 * @brief   Hardware Abstraction Layer for GPIO (LEDs, Buttons, Control Signals)
 * @version 1.0
 * @author  Majdedin Al Rashid
 * @date    2025-12-09
 */

#include "hl/hal_gpio.h"
#include "main.h"  /* For GPIO port/pin definitions from STM32CubeMX */

/* -------------------------------------------------------------------------- */
/*                              Private Types                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Internal structure to map application pins to hardware.
 */
typedef struct 
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} GPIO_PinMap_t;

/* -------------------------------------------------------------------------- */
/*                              Private Data                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Pin mapping table.
 *        Index corresponds to HL_GPIO_Pin_t enum values.
 */
static const GPIO_PinMap_t gpio_pin_map[HL_GPIO_PIN_COUNT] = 
{
    /* HL_GPIO_LED_INIT   */ { INIT_LED_GPIO_Port,   INIT_LED_Pin   },
    /* HL_GPIO_LED_MEAS   */ { MEAS_LED_GPIO_Port,   MEAS_LED_Pin   },
    /* HL_GPIO_LED_EXCITE */ { EXCITE_LED_GPIO_Port, EXCITE_LED_Pin },
    /* HL_GPIO_LED_ERR    */ { ERR_LED_GPIO_Port,    ERR_LED_Pin    },
    /* HL_GPIO_BTN_USER   */ { B1_GPIO_Port,         B1_Pin         },
    /* HL_GPIO_RF_GAIN_0  */ { GAIN_SLCT_1_GPIO_Port, GAIN_SLCT_1_Pin },
    /* HL_GPIO_RF_GAIN_1  */ { GAIN_SLCT_2_GPIO_Port, GAIN_SLCT_2_Pin },
    /* HL_GPIO_NINA_RST   */ { NINA_RST_GPIO_Port,   NINA_RST_Pin   },
    /* HL_GPIO_NINA_STOP  */ { NINA_STOP_GPIO_Port,  NINA_STOP_Pin  },
    /* HL_GPIO_NINA_DTR   */ { NINA_DTR_GPIO_Port,   NINA_DTR_Pin   },
    /* HL_GPIO_NINA_DSR   */ { NINA_DSR_GPIO_Port,   NINA_DSR_Pin   },
    /* HL_GPIO_NINA_LED_RED   */ { NINA_LED_RED_GPIO_Port,   NINA_LED_RED_Pin   },
    /* HL_GPIO_NINA_LED_BLUE  */ { NINA_LED_BLUE_GPIO_Port,  NINA_LED_BLUE_Pin  },
    /* HL_GPIO_NINA_LED_GREEN */ { NINA_LED_GREEN_GPIO_Port, NINA_LED_GREEN_Pin },
    /* HL_GPIO_OP_DIS     */ { OP_DIS_GPIO_Port,     OP_DIS_Pin     },
};

/* -------------------------------------------------------------------------- */
/*                              Public Functions                              */
/* -------------------------------------------------------------------------- */

HL_GPIO_Status_t HL_GPIO_Init(void)
{
    /* 
     * GPIO pins are already initialized by MX_GPIO_Init() in main.c.
     * This function serves as a hook for any additional initialization
     * or for resetting pins to a known state if needed.
     */
    return GPIO_OK;
}

HL_GPIO_Status_t HL_GPIO_Write(HL_GPIO_Pin_t pin, HL_GPIO_State_t state)
{
    if (pin >= HL_GPIO_PIN_COUNT)
    {
        return GPIO_ERROR;
    }

    const GPIO_PinMap_t *mapping = &gpio_pin_map[pin];
    GPIO_PinState hal_state = (state == HL_GPIO_HIGH) ? GPIO_PIN_SET : GPIO_PIN_RESET;

    HAL_GPIO_WritePin(mapping->port, mapping->pin, hal_state);

    return GPIO_OK;
}

HL_GPIO_Status_t HL_GPIO_Toggle(HL_GPIO_Pin_t pin)
{
    if (pin >= HL_GPIO_PIN_COUNT)
    {
        return GPIO_ERROR;
    }

    const GPIO_PinMap_t *mapping = &gpio_pin_map[pin];
    HAL_GPIO_TogglePin(mapping->port, mapping->pin);

    return GPIO_OK;
}

HL_GPIO_Status_t HL_GPIO_Read(HL_GPIO_Pin_t pin, HL_GPIO_State_t *state)
{
    if (pin >= HL_GPIO_PIN_COUNT || state == NULL)
    {
        return GPIO_ERROR;
    }

    const GPIO_PinMap_t *mapping = &gpio_pin_map[pin];
    GPIO_PinState hal_state = HAL_GPIO_ReadPin(mapping->port, mapping->pin);

    *state = (hal_state == GPIO_PIN_SET) ? HL_GPIO_HIGH : HL_GPIO_LOW;

    return GPIO_OK;
}
