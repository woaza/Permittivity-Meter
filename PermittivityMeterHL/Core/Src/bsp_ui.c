#include "bsp_ui.h"

#include <stdio.h>

#include "debug_log.h"
#include "mocks/mock_board.h"

static void log_led_change(uint8_t led_id, uint8_t state)
{
    char buffer[16];
    (void)snprintf(buffer, sizeof(buffer), "LED%u=%u", (unsigned)led_id, (unsigned)state);
    Debug_LogDriver("UI", buffer);
}

void BSP_UI_Init(void)
{
    MockBoard_Init();
    Debug_LogDriver("UI", "init");
}

void BSP_LED_Set(uint8_t led_id, uint8_t state)
{
    const uint8_t level = state ? 1U : 0U;
    MockBoard_UI_SetLED(led_id, level);
    log_led_change(led_id, level);
}

uint8_t BSP_LED_Get(uint8_t led_id)
{
    return MockBoard_UI_GetLED(led_id);
}

void BSP_Button_SetState(uint8_t pressed)
{
    const uint8_t level = pressed ? 1U : 0U;
    MockBoard_UI_SetButton(level);
    Debug_LogDriver("BTN", level ? "press" : "release");
}

uint8_t BSP_Button_GetState(void)
{
    return MockBoard_UI_GetButton();
}
