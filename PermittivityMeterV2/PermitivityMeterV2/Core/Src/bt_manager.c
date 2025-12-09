#include "bt_manager.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_rf.h"
#include "bsp_lcd.h"
#include "bsp_ui.h"
#include "debug_log.h"
#include "drv/driver_board.h"
#include "rf_trace.h"
#include "usb_cdc_bridge.h"

static BT_Event_t s_pending_event = BT_EVENT_NONE;

static void output_line(const char *line)
{
    if (line == NULL) {
        return;
    }
    Driver_BT_SetLastTx(line);
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

void BT_Manager_Init(void)
{
    Driver_Init();
    PC_HostBridge_Init();
    s_pending_event = BT_EVENT_NONE;
    Debug_LogDriver("BT", "init");
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

void BT_MockEnqueueCommand(const char *cmd)
{
    Driver_BT_QueueCommand(cmd);
}

void BT_MockPump(void)
{
    const char *cmd = Driver_BT_DequeueCommand();
    if (cmd != NULL) {
        BT_ProcessIncoming(cmd);
    }
}

const char *BT_MockGetLastTx(void)
{
    return Driver_BT_GetLastTx();
}

