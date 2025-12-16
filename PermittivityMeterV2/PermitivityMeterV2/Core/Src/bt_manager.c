#include "bt_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "bsp_rf.h"
#include "bsp_lcd.h"
#include "bsp_ui.h"
#include "debug_log.h"
#include "mocks/mock_board.h"
#include "hl/hal_board.h"
#include "hl/hal_pwm.h"
#include "rf_trace.h"
#include "usb_cdc_bridge.h"

static BT_Event_t s_pending_event = BT_EVENT_NONE;
static uint8_t s_manual_mode_enabled = 0U;

static void output_hw_framef(const char *fmt, ...)
{
    if (fmt == NULL) {
        return;
    }

    char payload[96];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(payload, sizeof(payload), fmt, ap);
    va_end(ap);

    char line[128];
    (void)snprintf(line, sizeof(line), "STAT:HW:%s", payload);
    output_line(line);
}

static void output_line(const char *line)
{
    if (line == NULL) {
        return;
    }
    MockBoard_BT_SetLastTx(line);
    PC_HostBridge_Send(line);
    Debug_LogDriver("BT_TX", line);
}

static void push_event(BT_Event_t evt)
{
    s_pending_event = evt;
}

static void handle_button_command(const char *buffer)
{
    if (strncmp(buffer, "CMD:BTN:PRESS", 13) == 0) {
        BSP_Button_SetState(1U);
        Debug_LogDriver("BTN", "cmd_press");
        BT_SendStatus("BTN_PRESS");
    } else if (strncmp(buffer, "CMD:BTN:RELEASE", 15) == 0) {
        BSP_Button_SetState(0U);
        Debug_LogDriver("BTN", "cmd_release");
        BT_SendStatus("BTN_REL");
    } else {
        Debug_LogDriver("BTN", "cmd_unk");
        BT_SendStatus("BTN_ERR");
    }
}

static void send_led_snapshot(void)
{
    char buffer[64];
    snprintf(buffer,
             sizeof(buffer),
             "STAT:LED:S:%u:M:%u:E:%u:R:%u",
             BSP_LED_Get(LED_STATUS),
             BSP_LED_Get(LED_MEAS),
             BSP_LED_Get(LED_EXCITE),
             BSP_LED_Get(LED_ERROR));
    output_line(buffer);
}

static void send_lcd_snapshot(void)
{
    char line_buffer[LCD_CHAR_COUNT + 1U];
    for (uint8_t line = 0U; line < LCD_LINE_COUNT; ++line) {
        BSP_LCD_GetLine(line, line_buffer, sizeof(line_buffer));
        char frame[128];
        snprintf(frame, sizeof(frame), "DAT:LCD:L%u:%s", line, line_buffer);
        output_line(frame);
    }
}

static const char *trace_mode_to_str(RF_TraceMode_t mode)
{
    switch (mode) {
    case RF_TRACE_MODE_CALIBRATION:
        return "CAL";
    case RF_TRACE_MODE_MEASUREMENT:
        return "MEAS";
    default:
        return "NONE";
    }
}

static void send_trace_dump(void)
{
    RFTraceSample_t samples[RF_TRACE_MAX_SAMPLES];
    RF_TraceMode_t mode = RF_TRACE_MODE_NONE;
    const size_t count = RF_Trace_Copy(samples, RF_TRACE_MAX_SAMPLES, &mode);
    if (count == 0U) {
        BT_SendStatus("TRACE_EMPTY");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        char buffer[160];
        snprintf(buffer,
                 sizeof(buffer),
                 "DAT:TRACE:%s:%03u:V:%.4f:A:%.4f",
                 trace_mode_to_str(mode),
                 (unsigned)i,
                 samples[i].voltage,
                 samples[i].amplitude);
        output_line(buffer);
    }
}

static void send_log_dump(void)
{
    DebugLogEntry_t entries[8];
    const size_t count = Debug_LogCopy(entries, sizeof(entries) / sizeof(entries[0]));
    if (count == 0U) {
        BT_SendStatus("LOG_EMPTY");
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        char buffer[128];
        snprintf(buffer,
                 sizeof(buffer),
                 "DAT:LOG:D:%u:S:%u:%s",
                 (unsigned)entries[i].domain,
                 (unsigned)entries[i].state,
                 entries[i].text);
        output_line(buffer);
    }
}

static bool parse_float_arg(const char *text, float *value)
{
    if (text == NULL || value == NULL) {
        return false;
    }

    char *end = NULL;
    const float parsed = strtof(text, &end);
    if (text == end) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool handle_mock_command(const char *buffer)
{
    if (buffer == NULL || strncmp(buffer, "CMD:MOCK:", 9) != 0) {
        return false;
    }

    const char *payload = buffer + 9;
    float value = 0.0f;
    if (strncmp(payload, "RF:RES:", 7) == 0) {
        if (!parse_float_arg(payload + 7, &value)) {
            BT_SendStatus("MOCK_ERR");
            return true;
        }
        BSP_RF_MockSetResonanceVoltage(value);
        BT_SendStatus("MOCK_RF_RES");
        return true;
    }

    if (strncmp(payload, "RF:NOISE:", 9) == 0) {
        if (!parse_float_arg(payload + 9, &value)) {
            BT_SendStatus("MOCK_ERR");
            return true;
        }
        BSP_RF_MockSetNoiseLevel(value);
        BT_SendStatus("MOCK_RF_NOISE");
        return true;
    }

    if (strncmp(payload, "RF:BASE:", 8) == 0) {
        if (!parse_float_arg(payload + 8, &value)) {
            BT_SendStatus("MOCK_ERR");
            return true;
        }
        BSP_RF_MockSetBaseAmplitude(value);
        BT_SendStatus("MOCK_RF_BASE");
        return true;
    }

    if (strncmp(payload, "RF:FAIL:", 8) == 0) {
        const char *mode = payload + 8;
        if (strncmp(mode, "ON", 2) == 0) {
            BSP_RF_MockSetForceFailure(1U);
            BT_SendStatus("MOCK_RF_FAIL_ON");
            return true;
        }
        if (strncmp(mode, "OFF", 3) == 0) {
            BSP_RF_MockSetForceFailure(0U);
            BT_SendStatus("MOCK_RF_FAIL_OFF");
            return true;
        }
        BT_SendStatus("MOCK_ERR");
        return true;
    }

    BT_SendStatus("MOCK_ERR");
    return true;
}

/* -------------------------------------------------------------------------- */
/*                     HAL Board Command Handlers (Handbetrieb)               */
/* -------------------------------------------------------------------------- */

static bool parse_uint_arg(const char *text, uint32_t *value)
{
    if (text == NULL || value == NULL) {
        return false;
    }
    char *end = NULL;
    const unsigned long parsed = strtoul(text, &end, 10);
    if (text == end) {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static bool handle_hal_led_command(const char *payload)
{
    /* CMD:HAL:LED:SET:<id>:<state>  e.g. CMD:HAL:LED:SET:0:1 */
    if (strncmp(payload, "SET:", 4) == 0) {
        uint32_t led_id = 0, state = 0;
        const char *args = payload + 4;
        
        /* Parse led_id */
        if (!parse_uint_arg(args, &led_id)) {
            BT_SendStatus("HAL_LED_ERR");
            return true;
        }
        
        /* Find colon separator */
        const char *colon = strchr(args, ':');
        if (colon == NULL) {
            BT_SendStatus("HAL_LED_ERR");
            return true;
        }
        
        /* Parse state */
        if (!parse_uint_arg(colon + 1, &state)) {
            BT_SendStatus("HAL_LED_ERR");
            return true;
        }
        
        if (HalBoard_LED_Set((uint8_t)led_id, (uint8_t)state) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_LED_%u_%s", (unsigned)led_id, state ? "ON" : "OFF");
            BT_SendStatus(resp);
            output_hw_framef("LED:%u:%u", (unsigned)led_id, state ? 1U : 0U);
        } else {
            BT_SendStatus("HAL_LED_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:LED:GET:<id> */
    if (strncmp(payload, "GET:", 4) == 0) {
        uint32_t led_id = 0;
        if (!parse_uint_arg(payload + 4, &led_id)) {
            BT_SendStatus("HAL_LED_ERR");
            return true;
        }
        
        uint8_t state = 0;
        if (HalBoard_LED_Get((uint8_t)led_id, &state) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_LED_%u:%u", (unsigned)led_id, state);
            BT_SendStatus(resp);
            output_hw_framef("LED:%u:%u", (unsigned)led_id, (unsigned)state);
        } else {
            BT_SendStatus("HAL_LED_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:LED:TOGGLE:<id> */
    if (strncmp(payload, "TOGGLE:", 7) == 0) {
        uint32_t led_id = 0;
        if (!parse_uint_arg(payload + 7, &led_id)) {
            BT_SendStatus("HAL_LED_ERR");
            return true;
        }
        
        if (HalBoard_LED_Toggle((uint8_t)led_id) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_LED_%u_TOG", (unsigned)led_id);
            BT_SendStatus(resp);
            uint8_t state = 0U;
            if (HalBoard_LED_Get((uint8_t)led_id, &state) == HAL_BOARD_OK) {
                output_hw_framef("LED:%u:%u", (unsigned)led_id, (unsigned)state);
            }
        } else {
            BT_SendStatus("HAL_LED_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_adc_command(const char *payload)
{
    /* CMD:HAL:ADC:READ */
    if (strncmp(payload, "READ", 4) == 0) {
        float voltage = 0.0f;
        if (HalBoard_ADC_ReadVoltage(&voltage) == HAL_BOARD_OK) {
            char resp[48];
            snprintf(resp, sizeof(resp), "HAL_ADC:%.3fV", voltage);
            BT_SendStatus(resp);
            output_hw_framef("ADC:V:%.3f", voltage);
        } else {
            BT_SendStatus("HAL_ADC_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:ADC:RAW */
    if (strncmp(payload, "RAW", 3) == 0) {
        uint16_t value = 0;
        if (HalBoard_ADC_ReadSingle(&value) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_ADC:%u", value);
            BT_SendStatus(resp);
            output_hw_framef("ADC:RAW:%u", (unsigned)value);
        } else {
            BT_SendStatus("HAL_ADC_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_dac_command(const char *payload)
{
    /* CMD:HAL:DAC:SET:<ch>:<voltage>  e.g. CMD:HAL:DAC:SET:0:1.65 */
    if (strncmp(payload, "SET:", 4) == 0) {
        uint32_t channel = 0;
        float voltage = 0.0f;
        const char *args = payload + 4;
        
        if (!parse_uint_arg(args, &channel)) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        const char *colon = strchr(args, ':');
        if (colon == NULL) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        if (!parse_float_arg(colon + 1, &voltage)) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        if (HalBoard_DAC_SetVoltage((uint8_t)channel, voltage) == HAL_BOARD_OK) {
            char resp[48];
            snprintf(resp, sizeof(resp), "HAL_DAC_%u:%.2fV", (unsigned)channel, voltage);
            BT_SendStatus(resp);
            output_hw_framef("DAC:%u:V:%.3f", (unsigned)channel, voltage);
        } else {
            BT_SendStatus("HAL_DAC_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:DAC:RAW:<ch>:<value> */
    if (strncmp(payload, "RAW:", 4) == 0) {
        uint32_t channel = 0, raw_val = 0;
        const char *args = payload + 4;
        
        if (!parse_uint_arg(args, &channel)) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        const char *colon = strchr(args, ':');
        if (colon == NULL) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        if (!parse_uint_arg(colon + 1, &raw_val)) {
            BT_SendStatus("HAL_DAC_ERR");
            return true;
        }
        
        if (HalBoard_DAC_SetRaw((uint8_t)channel, (uint16_t)raw_val) == HAL_BOARD_OK) {
            char resp[48];
            snprintf(resp, sizeof(resp), "HAL_DAC_%u:%u", (unsigned)channel, (unsigned)raw_val);
            BT_SendStatus(resp);
            output_hw_framef("DAC:%u:RAW:%u", (unsigned)channel, (unsigned)raw_val);
        } else {
            BT_SendStatus("HAL_DAC_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_gain_command(const char *payload)
{
    /* CMD:HAL:GAIN:SET:<level> */
    if (strncmp(payload, "SET:", 4) == 0) {
        uint32_t level = 0;
        if (!parse_uint_arg(payload + 4, &level)) {
            BT_SendStatus("HAL_GAIN_ERR");
            return true;
        }
        
        if (HalBoard_RF_SetGain((uint8_t)level) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_GAIN:%u", (unsigned)level);
            BT_SendStatus(resp);
            output_hw_framef("GAIN:%u", (unsigned)level);
        } else {
            BT_SendStatus("HAL_GAIN_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:GAIN:GET */
    if (strncmp(payload, "GET", 3) == 0) {
        uint8_t level = 0;
        if (HalBoard_RF_GetGain(&level) == HAL_BOARD_OK) {
            char resp[32];
            snprintf(resp, sizeof(resp), "HAL_GAIN:%u", level);
            BT_SendStatus(resp);
            output_hw_framef("GAIN:%u", (unsigned)level);
        } else {
            BT_SendStatus("HAL_GAIN_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_btn_command(const char *payload)
{
    /* CMD:HAL:BTN:READ */
    if (strncmp(payload, "READ", 4) == 0) {
        uint8_t state = 0;
        if (HalBoard_Button_Read(&state) == HAL_BOARD_OK) {
            BT_SendStatus(state ? "HAL_BTN:PRESSED" : "HAL_BTN:RELEASED");
            output_hw_framef("BTN:%u", (unsigned)state);
        } else {
            BT_SendStatus("HAL_BTN_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_lcd_command(const char *payload)
{
    /* CMD:HAL:LCD:SET:<line>:<text> */
    if (strncmp(payload, "SET:", 4) == 0) {
        uint32_t line = 0U;
        const char *args = payload + 4;

        if (!parse_uint_arg(args, &line)) {
            BT_SendStatus("HAL_LCD_ERR");
            return true;
        }

        const char *colon = strchr(args, ':');
        if (colon == NULL) {
            BT_SendStatus("HAL_LCD_ERR");
            return true;
        }

        BSP_LCD_DisplayStringAt((uint8_t)line, colon + 1);

        char resp[32];
        snprintf(resp, sizeof(resp), "HAL_LCD_L%u_OK", (unsigned)line);
        BT_SendStatus(resp);

        /* Push-style update with the final buffered line content. */
        char line_buffer[LCD_CHAR_COUNT + 1U];
        BSP_LCD_GetLine((uint8_t)line, line_buffer, sizeof(line_buffer));
        char frame[128];
        snprintf(frame, sizeof(frame), "DAT:LCD:L%u:%s", (unsigned)line, line_buffer);
        output_line(frame);
        return true;
    }

    BT_SendStatus("HAL_LCD_ERR");
    return true;
}

static bool handle_hal_nina_command(const char *payload)
{
    /* CMD:HAL:NINA:RST:<state>  (0=reset active, 1=run) */
    if (strncmp(payload, "RST:", 4) == 0) {
        uint32_t state = 0;
        if (!parse_uint_arg(payload + 4, &state)) {
            BT_SendStatus("HAL_NINA_ERR");
            return true;
        }
        
        if (HalBoard_NINA_SetReset((uint8_t)state) == HAL_BOARD_OK) {
            BT_SendStatus(state ? "HAL_NINA:RUN" : "HAL_NINA:RESET");
            output_hw_framef("NINA:RST:%u", (unsigned)state);
        } else {
            BT_SendStatus("HAL_NINA_ERR");
        }
        return true;
    }
    
    /* CMD:HAL:NINA:STOP:<state>  (0=run, 1=stop) */
    if (strncmp(payload, "STOP:", 5) == 0) {
        uint32_t state = 0;
        if (!parse_uint_arg(payload + 5, &state)) {
            BT_SendStatus("HAL_NINA_ERR");
            return true;
        }
        
        if (HalBoard_NINA_SetStop((uint8_t)state) == HAL_BOARD_OK) {
            BT_SendStatus(state ? "HAL_NINA:STOPPED" : "HAL_NINA:RUNNING");
            output_hw_framef("NINA:STOP:%u", (unsigned)state);
        } else {
            BT_SendStatus("HAL_NINA_ERR");
        }
        return true;
    }
    
    return false;
}

static bool handle_hal_pwm_command(const char *payload)
{
    /* CMD:HAL:PWM:START | STOP | GET | FREQ:<hz> | DUTY:<0..100> */

    if (strncmp(payload, "START", 5) == 0) {
        if (HAL_PWM_Start() == PWM_OK) {
            BT_SendStatus("HAL_PWM_START_OK");
        } else {
            BT_SendStatus("HAL_PWM_ERR");
        }
        output_hw_framef("PWM:RUN:%u", HAL_PWM_IsRunning() ? 1U : 0U);
        output_hw_framef("PWM:FREQ:%lu", (unsigned long)HAL_PWM_GetFrequency());
        output_hw_framef("PWM:DUTY:%u", (unsigned)HAL_PWM_GetDutyCycle());
        return true;
    }

    if (strncmp(payload, "STOP", 4) == 0) {
        if (HAL_PWM_Stop() == PWM_OK) {
            BT_SendStatus("HAL_PWM_STOP_OK");
        } else {
            BT_SendStatus("HAL_PWM_ERR");
        }
        output_hw_framef("PWM:RUN:%u", HAL_PWM_IsRunning() ? 1U : 0U);
        output_hw_framef("PWM:FREQ:%lu", (unsigned long)HAL_PWM_GetFrequency());
        output_hw_framef("PWM:DUTY:%u", (unsigned)HAL_PWM_GetDutyCycle());
        return true;
    }

    if (strncmp(payload, "GET", 3) == 0) {
        BT_SendStatus("HAL_PWM_OK");
        output_hw_framef("PWM:RUN:%u", HAL_PWM_IsRunning() ? 1U : 0U);
        output_hw_framef("PWM:FREQ:%lu", (unsigned long)HAL_PWM_GetFrequency());
        output_hw_framef("PWM:DUTY:%u", (unsigned)HAL_PWM_GetDutyCycle());
        return true;
    }

    if (strncmp(payload, "FREQ:", 5) == 0) {
        uint32_t hz = 0U;
        if (!parse_uint_arg(payload + 5, &hz)) {
            BT_SendStatus("HAL_PWM_ERR");
            return true;
        }
        if (HAL_PWM_SetFrequency(hz) == PWM_OK) {
            BT_SendStatus("HAL_PWM_FREQ_OK");
        } else {
            BT_SendStatus("HAL_PWM_ERR");
        }
        output_hw_framef("PWM:FREQ:%lu", (unsigned long)HAL_PWM_GetFrequency());
        return true;
    }

    if (strncmp(payload, "DUTY:", 5) == 0) {
        uint32_t duty = 0U;
        if (!parse_uint_arg(payload + 5, &duty)) {
            BT_SendStatus("HAL_PWM_ERR");
            return true;
        }
        if (HAL_PWM_SetDutyCycle((uint8_t)duty) == PWM_OK) {
            BT_SendStatus("HAL_PWM_DUTY_OK");
        } else {
            BT_SendStatus("HAL_PWM_ERR");
        }
        output_hw_framef("PWM:DUTY:%u", (unsigned)HAL_PWM_GetDutyCycle());
        return true;
    }

    BT_SendStatus("HAL_PWM_ERR");
    return true;
}

static bool handle_hal_command(const char *buffer)
{
    if (buffer == NULL || strncmp(buffer, "CMD:HAL:", 8) != 0) {
        return false;
    }

    if (!s_manual_mode_enabled) {
        BT_SendStatus("HAL_LOCKED");
        return true;
    }

    const char *payload = buffer + 8;
    
    /* Route to sub-handlers */
    if (strncmp(payload, "LED:", 4) == 0) {
        return handle_hal_led_command(payload + 4);
    }
    
    if (strncmp(payload, "ADC:", 4) == 0) {
        return handle_hal_adc_command(payload + 4);
    }
    
    if (strncmp(payload, "DAC:", 4) == 0) {
        return handle_hal_dac_command(payload + 4);
    }
    
    if (strncmp(payload, "GAIN:", 5) == 0) {
        return handle_hal_gain_command(payload + 5);
    }
    
    if (strncmp(payload, "BTN:", 4) == 0) {
        return handle_hal_btn_command(payload + 4);
    }
    
    if (strncmp(payload, "NINA:", 5) == 0) {
        return handle_hal_nina_command(payload + 5);
    }

    if (strncmp(payload, "LCD:", 4) == 0) {
        return handle_hal_lcd_command(payload + 4);
    }

    if (strncmp(payload, "PWM:", 4) == 0) {
        return handle_hal_pwm_command(payload + 4);
    }
    
    /* CMD:HAL:INIT - Initialize HAL Board */
    if (strncmp(payload, "INIT", 4) == 0) {
        HalBoard_Init();
        BT_SendStatus("HAL_INIT_OK");
        output_hw_framef("HAL:INIT:1");
        return true;
    }
    
    BT_SendStatus("HAL_CMD_ERR");
    return true;
}

void BT_Manager_Init(void)
{
    MockBoard_Init();
    PC_HostBridge_Init();
    s_pending_event = BT_EVENT_NONE;
    s_manual_mode_enabled = 0U;
    Debug_LogDriver("BT", "init");

    /* Proof-of-flash / proof-of-TX for the PC host bridge. */
    BT_SendStatus("BOOT_V2");
}

void BT_ProcessIncoming(const char *buffer)
{
    if (buffer == NULL) {
        return;
    }

    if (strncmp(buffer, "CMD:CONN", 8) == 0) {
        push_event(BT_EVENT_CONN);
        Debug_LogDriver("BT_RX", "CMD:CONN");
    } else if (strncmp(buffer, "CMD:CAL", 7) == 0) {
        push_event(BT_EVENT_CAL);
        Debug_LogDriver("BT_RX", "CMD:CAL");
    } else if (strncmp(buffer, "CMD:MEAS", 8) == 0) {
        push_event(BT_EVENT_MEAS);
        Debug_LogDriver("BT_RX", "CMD:MEAS");
    } else if (strncmp(buffer, "CMD:BTN", 8) == 0) {
        handle_button_command(buffer);
    } else if (strncmp(buffer, "CMD:LEDS", 9) == 0) {
        send_led_snapshot();
    } else if (strncmp(buffer, "CMD:LCD", 8) == 0) {
        send_lcd_snapshot();
    } else if (strncmp(buffer, "CMD:LOG", 8) == 0) {
        send_log_dump();
    } else if (strncmp(buffer, "CMD:TRACE", 10) == 0) {
        send_trace_dump();
    } else if (strncmp(buffer, "CMD:MOCK", 8) == 0) {
        handle_mock_command(buffer);
    } else if (strncmp(buffer, "CMD:HAL", 7) == 0) {
        handle_hal_command(buffer);
    } else if (strncmp(buffer, "CMD:MANUAL:ON", 13) == 0) {
        push_event(BT_EVENT_MANUAL_ON);
        Debug_LogDriver("BT_RX", "CMD:MANUAL:ON");
        BT_SendStatus("MANUAL_ON_REQ");
    } else if (strncmp(buffer, "CMD:MANUAL:OFF", 14) == 0) {
        push_event(BT_EVENT_MANUAL_OFF);
        Debug_LogDriver("BT_RX", "CMD:MANUAL:OFF");
        BT_SendStatus("MANUAL_OFF_REQ");
    } else {
        Debug_LogDriver("BT_RX", "IGN");
        return;
    }
}

BT_Event_t BT_PopEvent(void)
{
    BT_Event_t evt = s_pending_event;
    s_pending_event = BT_EVENT_NONE;
    return evt;
}

void BT_SendStatus(const char *status_tag)
{
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "STAT:%s", status_tag ? status_tag : "UNK");
    output_line(buffer);
}

void BT_SendResult(MeasurementResult_t result)
{
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "DAT:RES:ER:%.3f:EI:%.3f:DENS:%.3f",
             result.epsilon_real,
             result.epsilon_imag,
             result.snow_density);
    output_line(buffer);
}

void BT_SetManualMode(uint8_t enabled)
{
    s_manual_mode_enabled = enabled ? 1U : 0U;
}

uint8_t BT_IsManualMode(void)
{
    return s_manual_mode_enabled;
}

void BT_MockEnqueueCommand(const char *cmd)
{
    MockBoard_BT_QueueCommand(cmd);
}

void BT_MockPump(void)
{
    const char *cmd = MockBoard_BT_DequeueCommand();
    if (cmd != NULL) {
        BT_ProcessIncoming(cmd);
    }
}

const char *BT_MockGetLastTx(void)
{
    return MockBoard_BT_GetLastTx();
}

