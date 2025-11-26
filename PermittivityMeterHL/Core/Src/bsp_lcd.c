#include "bsp_lcd.h"

#include <string.h>
#include <stdio.h>

#include "debug_log.h"

static char s_lcd_lines[LCD_LINE_COUNT][LCD_CHAR_COUNT + 1];

void BSP_LCD_Init(void)
{
    BSP_LCD_Clear();
    Debug_LogDriver("LCD", "init");
}

void BSP_LCD_Clear(void)
{
    for (int i = 0; i < LCD_LINE_COUNT; ++i) {
        memset(s_lcd_lines[i], ' ', LCD_CHAR_COUNT);
        s_lcd_lines[i][LCD_CHAR_COUNT] = '\0';
    }
    Debug_LogDriver("LCD", "clear");
}

void BSP_LCD_DisplayStringAt(uint8_t line, const char *str)
{
    if (line >= LCD_LINE_COUNT || str == NULL) {
        return;
    }

    // Copy string, pad with spaces if short, truncate if long
    size_t len = strlen(str);
    for (int i = 0; i < LCD_CHAR_COUNT; ++i) {
        if (i < (int)len) {
            s_lcd_lines[line][i] = str[i];
        } else {
            s_lcd_lines[line][i] = ' ';
        }
    }
    s_lcd_lines[line][LCD_CHAR_COUNT] = '\0';

    char log_buf[32];
    snprintf(log_buf, sizeof(log_buf), "L%u:%s", line, s_lcd_lines[line]);
    Debug_LogDriver("LCD", log_buf);
}

void BSP_LCD_GetLine(uint8_t line, char *buffer, uint8_t max_len)
{
    if (line >= LCD_LINE_COUNT || buffer == NULL || max_len == 0) {
        return;
    }
    strncpy(buffer, s_lcd_lines[line], max_len - 1);
    buffer[max_len - 1] = '\0';
}
