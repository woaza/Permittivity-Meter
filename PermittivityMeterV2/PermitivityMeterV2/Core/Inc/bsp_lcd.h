#pragma once

#include <stdint.h>

#define LCD_LINE_COUNT 2
#define LCD_CHAR_COUNT 16

void BSP_LCD_Init(void);
void BSP_LCD_Clear(void);
void BSP_LCD_DisplayStringAt(uint8_t line, const char *str);
void BSP_LCD_GetLine(uint8_t line, char *buffer, uint8_t max_len);
