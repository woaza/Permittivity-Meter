#pragma once

#include <stdint.h>

enum {
    LED_STATUS = 0,
    LED_MEAS,
    LED_EXCITE,
    LED_ERROR,
    LED_COUNT
};

void BSP_UI_Init(void);
void BSP_LED_Set(uint8_t led_id, uint8_t state);
uint8_t BSP_LED_Get(uint8_t led_id);

void BSP_Button_SetState(uint8_t pressed);
uint8_t BSP_Button_GetState(void);
