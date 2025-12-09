/**
 * @file    bsp_ui.c
 * @brief   Board Support Package for User Interface (LEDs, Buttons)
 * @version 2.0
 * @date    2025-12-09
 * @note    Uses driver layer for hardware abstraction.
 */

#include "bsp_ui.h"
#include "drv/driver_board.h"
#include "debug_log.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*                              Private Helpers                               */
/* -------------------------------------------------------------------------- */

static void log_led_change(uint8_t led_id, uint8_t state)
{
    char buffer[16];
    (void)snprintf(buffer, sizeof(buffer), "LED%u=%u", (unsigned)led_id, (unsigned)state);
    Debug_LogDriver("UI", buffer);
}

/* -------------------------------------------------------------------------- */
/*                              Public Functions                              */
/* -------------------------------------------------------------------------- */

void BSP_UI_Init(void)
{
    Driver_Init();
    Debug_LogDriver("UI", "init");
}

void BSP_LED_Set(uint8_t led_id, uint8_t state)
{
    Driver_UI_SetLED(led_id, state ? 1U : 0U);
    log_led_change(led_id, state ? 1U : 0U);
}

uint8_t BSP_LED_Get(uint8_t led_id)
{
    return Driver_UI_GetLED(led_id);
}

void BSP_Button_SetState(uint8_t pressed)
{
    Driver_UI_SetButton(pressed ? 1U : 0U);
    Debug_LogDriver("BTN", pressed ? "press" : "release");
}

uint8_t BSP_Button_GetState(void)
{
    return Driver_UI_GetButton();
}
