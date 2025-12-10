/**
 * @file    bsp_ui.c
 * @brief   Board Support Package for User Interface (LEDs, Buttons)
 * @version 2.1
 * @date    2025-12-09
 * @note    Uses MockBoard for simulation (state machine path).
 *          For direct hardware control, use HalBoard via CLI.
 */

#include "bsp_ui.h"
#include "mocks/mock_board.h"
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
    MockBoard_Init();
    Debug_LogDriver("UI", "init");
}

void BSP_LED_Set(uint8_t led_id, uint8_t state)
{
    MockBoard_UI_SetLED(led_id, state ? 1U : 0U);
    log_led_change(led_id, state ? 1U : 0U);
}

uint8_t BSP_LED_Get(uint8_t led_id)
{
    return MockBoard_UI_GetLED(led_id);
}

void BSP_Button_SetState(uint8_t pressed)
{
    MockBoard_UI_SetButton(pressed ? 1U : 0U);
    Debug_LogDriver("BTN", pressed ? "press" : "release");
}

uint8_t BSP_Button_GetState(void)
{
    return MockBoard_UI_GetButton();
}

