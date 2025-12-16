#include "fsm_main.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include "bsp_lcd.h"
#include "bsp_rf.h"
#include "bsp_ui.h"
#include "bt_manager.h"
#include "debug_log.h"
#include "rf_measure.h"
#include "usb_cdc_bridge.h"

#define FSM_EVENT_QUEUE_SIZE 8U
#define CAL_BLINK_PERIOD_TICKS 4U

static AppState_t s_state = STATE_INIT;
static CalibrationData_t s_calibration;
static MeasurementResult_t s_last_result;
static FSM_Event_t s_event_queue[FSM_EVENT_QUEUE_SIZE];
static uint8_t s_queue_head;
static uint8_t s_queue_tail;
static uint8_t s_queue_count;
static uint8_t s_last_button_state;
static uint8_t s_calibration_pending;
static uint8_t s_measurement_pending;
static uint8_t s_result_pending;
static uint8_t s_cal_blink_ticks;
static uint8_t s_cal_led_level;

static void enqueue_event(FSM_Event_t evt);
static FSM_Event_t dequeue_event(void);
static void handle_bt_event(BT_Event_t evt);
static void process_button(void);
static void FSM_TransitionTo(AppState_t new_state, const char *reason);
static void FSM_OnEnter(AppState_t new_state);
static void FSM_HandleInit(void);
static void FSM_HandleIdle(void);
static void FSM_HandleManual(void);
static void FSM_HandleCalibration(void);
static void FSM_HandleMeasureSearch(void);
static void FSM_HandleCalculation(void);
static void FSM_HandleError(void);
static void FSM_StartCalibration(const char *reason_tag);
static void FSM_StartMeasurement(const char *reason_tag);
static uint8_t FSM_IsMeasurementValid(const MeasurementResult_t *result);
static void FSM_UpdateLCD(const char *line0, const char *line1);

static void fmt_fixed_2(char *out, size_t out_len, float value);
static void FSM_ShowResultOnLCD(const MeasurementResult_t *result);

static void enqueue_event(FSM_Event_t evt)
{
    if (evt == FSM_EVENT_NONE) {
        return;
    }

    if (s_queue_count >= FSM_EVENT_QUEUE_SIZE) {
        s_queue_head = (uint8_t)((s_queue_head + 1U) % FSM_EVENT_QUEUE_SIZE);
        --s_queue_count;
        Debug_LogEvent("QUEUE", "overflow");
    }

    s_event_queue[s_queue_tail] = evt;
    s_queue_tail = (uint8_t)((s_queue_tail + 1U) % FSM_EVENT_QUEUE_SIZE);
    ++s_queue_count;
}

static FSM_Event_t dequeue_event(void)
{
    if (s_queue_count == 0U) {
        return FSM_EVENT_NONE;
    }

    FSM_Event_t evt = s_event_queue[s_queue_head];
    s_queue_head = (uint8_t)((s_queue_head + 1U) % FSM_EVENT_QUEUE_SIZE);
    --s_queue_count;
    return evt;
}

static void handle_bt_event(BT_Event_t evt)
{
    switch (evt) {
    case BT_EVENT_CONN:
        enqueue_event(FSM_EVENT_BT_CONN);
        Debug_LogEvent("BT", "CONN");
        break;
    case BT_EVENT_CAL:
        enqueue_event(FSM_EVENT_BT_CAL);
        Debug_LogEvent("BT", "CAL");
        break;
    case BT_EVENT_MEAS:
        enqueue_event(FSM_EVENT_BT_MEAS);
        Debug_LogEvent("BT", "MEAS");
        break;
    case BT_EVENT_MANUAL_ON:
        Debug_LogEvent("BT", "MAN_ON");
        /* Allow dropping into manual mode from any state. */
        FSM_TransitionTo(STATE_MANUAL_OPERATION, "bt_man_on");
        break;
    case BT_EVENT_MANUAL_OFF:
        Debug_LogEvent("BT", "MAN_OFF");
        if (s_state == STATE_MANUAL_OPERATION) {
            BT_SendStatus("MANUAL_OFF");
            FSM_TransitionTo(STATE_IDLE, "bt_man_off");
        } else {
            BT_SendStatus("MANUAL_NOT_ACTIVE");
        }
        break;
    default:
        break;
    }
}

void FSM_Init(void)
{
    memset(&s_calibration, 0, sizeof(s_calibration));
    memset(&s_last_result, 0, sizeof(s_last_result));
    s_state = STATE_INIT;
    s_queue_head = 0U;
    s_queue_tail = 0U;
    s_queue_count = 0U;
    s_last_button_state = 0U;
    s_calibration_pending = 0U;
    s_measurement_pending = 0U;
    s_result_pending = 0U;
    s_cal_blink_ticks = 0U;
    s_cal_led_level = 0U;

    Debug_LogClear();
    Debug_LogState("boot", STATE_INIT, STATE_INIT);

    FSM_OnEnter(STATE_INIT);
}

void FSM_PostEvent(FSM_Event_t event)
{
    enqueue_event(event);
}

AppState_t FSM_GetState(void)
{
    return s_state;
}

static void process_button(void)
{
    const uint8_t state = BSP_Button_GetState();
    if (state && !s_last_button_state) {
        enqueue_event(FSM_EVENT_BUTTON_PRESS);
        Debug_LogEvent("BTN", "PRESS");
    }
    s_last_button_state = state;
}

static void FSM_TransitionTo(AppState_t new_state, const char *reason)
{
    if (new_state == s_state) {
        return;
    }

    const AppState_t old_state = s_state;
    s_state = new_state;
    Debug_LogState((reason != NULL) ? reason : "fsm", old_state, new_state);
    FSM_OnEnter(new_state);
}

static void FSM_OnEnter(AppState_t new_state)
{
    BT_SetManualMode((new_state == STATE_MANUAL_OPERATION) ? 1U : 0U);

    switch (new_state) {
    case STATE_INIT:
        s_queue_head = 0U;
        s_queue_tail = 0U;
        s_queue_count = 0U;
        s_calibration_pending = 0U;
        s_measurement_pending = 0U;
        s_result_pending = 0U;
        s_cal_blink_ticks = 0U;
        s_cal_led_level = 0U;
        s_last_button_state = 0U;

        BSP_RF_Init();
        BSP_UI_Init();
        BSP_LCD_Init();
        BT_Manager_Init();

        BSP_RF_SetOpAmpEnable(0U);
        BSP_RF_EnableExcitation(0U);

        BSP_LED_Set(LED_STATUS, 1U);
        BSP_LED_Set(LED_MEAS, 0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        BSP_LED_Set(LED_ERROR, 0U);
        FSM_UpdateLCD("INIT", "Booting...");

        enqueue_event(FSM_EVENT_INIT_DONE);
        break;

    case STATE_IDLE:
        BSP_LED_Set(LED_STATUS, 1U);
        BSP_LED_Set(LED_MEAS, 0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        BSP_LED_Set(LED_ERROR, 0U);
        BSP_RF_EnableExcitation(0U);
        BSP_RF_SetOpAmpEnable(0U);
        s_calibration_pending = 0U;
        s_measurement_pending = 0U;
        s_result_pending = 0U;
        s_cal_blink_ticks = 0U;
        FSM_UpdateLCD("IDLE", s_calibration.is_valid ? "Ready" : "Need CAL");
        break;

    case STATE_MANUAL_OPERATION:
        /* Safety: stop any measurement-related outputs immediately. */
        BSP_RF_EnableExcitation(0U);
        BSP_RF_SetOpAmpEnable(0U);
        s_calibration_pending = 0U;
        s_measurement_pending = 0U;
        s_result_pending = 0U;
        s_cal_blink_ticks = 0U;
        s_cal_led_level = 0U;

        BSP_LED_Set(LED_STATUS, 1U);
        BSP_LED_Set(LED_MEAS, 0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        BSP_LED_Set(LED_ERROR, 0U);

        BT_SendStatus("MANUAL_ON");
        FSM_UpdateLCD("MANUAL", "HAL cmds OK");
        break;

    case STATE_CALIBRATION:
        s_calibration_pending = 1U;
        s_cal_blink_ticks = 0U;
        s_cal_led_level = 1U;

        BSP_LED_Set(LED_MEAS, 1U);
        BSP_LED_Set(LED_EXCITE, 1U);
        BSP_LED_Set(LED_ERROR, 0U);
        BSP_RF_SetOpAmpEnable(1U);
        BSP_RF_EnableExcitation(1U);
        FSM_UpdateLCD("CAL", "Sweeping...");
        break;

    case STATE_MEASURE_SEARCH:
        s_measurement_pending = 1U;
        BSP_LED_Set(LED_MEAS, 1U);
        BSP_LED_Set(LED_EXCITE, 1U);
        BSP_RF_SetOpAmpEnable(1U);
        BSP_RF_EnableExcitation(1U);
        FSM_UpdateLCD("MEAS", "Sampling...");
        break;

    case STATE_CALCULATION:
        /* RESULT state: show last measurement and wait for button press.
         * After every successful measurement the system requires a new CAL.
         */
        s_result_pending = 1U;
        BSP_RF_EnableExcitation(0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        s_calibration.is_valid = 0U;
        FSM_ShowResultOnLCD(&s_last_result);
        break;

    case STATE_ERROR:
        BSP_RF_EnableExcitation(0U);
        BSP_RF_SetOpAmpEnable(0U);
        BSP_LED_Set(LED_STATUS, 0U);
        BSP_LED_Set(LED_MEAS, 0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        BSP_LED_Set(LED_ERROR, 1U);
        FSM_UpdateLCD("ERROR", "Check host");
        break;

    default:
        break;
    }
}

static void FSM_HandleInit(void)
{
    const FSM_Event_t evt = dequeue_event();
    if (evt == FSM_EVENT_INIT_DONE) {
        FSM_TransitionTo(STATE_IDLE, "init_done");
    }
}

static void FSM_StartCalibration(const char *reason_tag)
{
    BT_SendStatus("CAL");
    FSM_TransitionTo(STATE_CALIBRATION, reason_tag);
}

static void FSM_StartMeasurement(const char *reason_tag)
{
    if (!s_calibration.is_valid) {
        BT_SendStatus("ERR:NEED_CAL");
        Debug_LogEvent("FSM", "meas_reject");
        return;
    }

    BT_SendStatus("MEAS");
    FSM_TransitionTo(STATE_MEASURE_SEARCH, reason_tag);
}

static void FSM_HandleIdle(void)
{
    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_BT_CONN:
        BT_SendStatus("RDY");
        break;

    case FSM_EVENT_BT_CAL:
        FSM_StartCalibration("bt_cal");
        break;

    case FSM_EVENT_BT_MEAS:
        FSM_StartMeasurement("bt_meas");
        break;

    case FSM_EVENT_BUTTON_PRESS:
        if (s_calibration.is_valid) {
            FSM_StartMeasurement("btn_meas");
        } else {
            FSM_StartCalibration("btn_cal");
        }
        break;

    default:
        break;
    }
}

static void FSM_HandleManual(void)
{
    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_BT_CONN:
        BT_SendStatus("MANUAL");
        break;

    case FSM_EVENT_BT_MANUAL_OFF:
        BT_SendStatus("MANUAL_OFF");
        FSM_TransitionTo(STATE_IDLE, "bt_man_off");
        break;

    case FSM_EVENT_BT_MANUAL_ON:
        BT_SendStatus("MANUAL");
        break;

    case FSM_EVENT_BT_CAL:
    case FSM_EVENT_BT_MEAS:
        BT_SendStatus("MANUAL_ACTIVE");
        break;

    default:
        break;
    }
}

static void FSM_HandleCalibration(void)
{
    if (++s_cal_blink_ticks >= CAL_BLINK_PERIOD_TICKS) {
        s_cal_blink_ticks = 0U;
        s_cal_led_level ^= 1U;
        BSP_LED_Set(LED_MEAS, s_cal_led_level);
    }

    if (s_calibration_pending) {
        s_calibration = RF_PerformAirCalibration();
        s_calibration_pending = 0U;
        enqueue_event(FSM_EVENT_CAL_DONE);
    }

    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_CAL_DONE:
        BSP_LED_Set(LED_MEAS, 0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        BSP_RF_EnableExcitation(0U);
        BSP_RF_SetOpAmpEnable(0U);

        if (s_calibration.is_valid) {
            BT_SendStatus("CAL_OK");
            FSM_TransitionTo(STATE_IDLE, "cal_ok");
        } else {
            BT_SendStatus("ERR");
            FSM_TransitionTo(STATE_ERROR, "cal_fail");
        }
        break;

    case FSM_EVENT_BT_CONN:
        BT_SendStatus("CAL");
        break;

    default:
        break;
    }
}

static void FSM_HandleMeasureSearch(void)
{
    if (s_measurement_pending) {
        s_last_result = RF_PerformSnowMeasurement(s_calibration);
        s_measurement_pending = 0U;
        enqueue_event(FSM_EVENT_MEAS_DONE);
    }

    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_MEAS_DONE:
        BSP_RF_EnableExcitation(0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        if (FSM_IsMeasurementValid(&s_last_result)) {
            BSP_LED_Set(LED_MEAS, 0U);
            FSM_TransitionTo(STATE_CALCULATION, "meas_ok");
        } else {
            BT_SendStatus("ERR:MEAS_INVALID");
            Debug_LogEvent("RF_MEAS", "invalid");
            FSM_TransitionTo(STATE_ERROR, "meas_fail");
        }
        break;

    case FSM_EVENT_BT_CONN:
        BT_SendStatus("MEAS");
        break;

    default:
        break;
    }
}

static void FSM_HandleCalculation(void)
{
    if (s_result_pending) {
        /* Send result once over the host link as well. */
        BT_SendResult(s_last_result);
        s_result_pending = 0U;
    }

    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_BUTTON_PRESS:
        FSM_TransitionTo(STATE_IDLE, "btn_ack");
        break;

    case FSM_EVENT_BT_CAL:
        FSM_StartCalibration("bt_cal_after_result");
        break;

    case FSM_EVENT_BT_MEAS:
        /* Measurement requires a fresh CAL after each result. */
        BT_SendStatus("ERR:NEED_CAL");
        break;

    case FSM_EVENT_BT_CONN:
        BT_SendStatus("RDY");
        break;

    default:
        break;
    }
}

static void fmt_fixed_2(char *out, size_t out_len, float value)
{
    if (out == NULL || out_len == 0U) {
        return;
    }

    if (!isfinite(value)) {
        (void)snprintf(out, out_len, "nan");
        return;
    }

    const float scaled_f = value * 100.0f;
    const int32_t scaled = (int32_t)lroundf(scaled_f);
    const int32_t abs_scaled = (scaled < 0) ? -scaled : scaled;
    const int32_t ip = abs_scaled / 100;
    const int32_t fp = abs_scaled % 100;
    if (scaled < 0) {
        (void)snprintf(out, out_len, "-%ld.%02ld", (long)ip, (long)fp);
    } else {
        (void)snprintf(out, out_len, "%ld.%02ld", (long)ip, (long)fp);
    }
}

static void FSM_ShowResultOnLCD(const MeasurementResult_t *result)
{
    char line0[LCD_CHAR_COUNT + 1U];
    char line1[LCD_CHAR_COUNT + 1U];
    char er[10];
    char ei[10];
    char dens[10];

    if (result == NULL) {
        FSM_UpdateLCD("RESULT", "(no data)");
        return;
    }

    fmt_fixed_2(er, sizeof(er), result->epsilon_real);
    fmt_fixed_2(ei, sizeof(ei), result->epsilon_imag);
    fmt_fixed_2(dens, sizeof(dens), result->snow_density);

    (void)snprintf(line0, sizeof(line0), "ER %s EI %s", er, ei);
    (void)snprintf(line1, sizeof(line1), "D %skg/m3", dens);
    FSM_UpdateLCD(line0, line1);
}

static void FSM_HandleError(void)
{
    const FSM_Event_t evt = dequeue_event();
    switch (evt) {
    case FSM_EVENT_BUTTON_PRESS:
        FSM_TransitionTo(STATE_INIT, "btn_reset");
        break;

    case FSM_EVENT_BT_CONN:
        BT_SendStatus("RDY");
        FSM_TransitionTo(STATE_INIT, "bt_reset");
        break;

    case FSM_EVENT_BT_CAL:
    case FSM_EVENT_BT_MEAS:
        BT_SendStatus("ERR");
        break;

    default:
        break;
    }
}

static void FSM_UpdateLCD(const char *line0, const char *line1)
{
    BSP_LCD_DisplayStringAt(0U, (line0 != NULL) ? line0 : "");
    BSP_LCD_DisplayStringAt(1U, (line1 != NULL) ? line1 : "");
}

static uint8_t FSM_IsMeasurementValid(const MeasurementResult_t *result)
{
    if (result == NULL) {
        return 0U;
    }

    if (!isfinite(result->adc_voltage_min)) {
        return 0U;
    }

    const float threshold = FLT_MAX * 0.5f;
    return (result->adc_voltage_min < threshold) ? 1U : 0U;
}

void FSM_RunOnce(void)
{
    PC_HostBridge_Poll();
    BT_MockPump();
    handle_bt_event(BT_PopEvent());
    process_button();

    switch (s_state) {
    case STATE_INIT:
        FSM_HandleInit();
        break;

    case STATE_IDLE:
        FSM_HandleIdle();
        break;

    case STATE_MANUAL_OPERATION:
        FSM_HandleManual();
        break;

    case STATE_CALIBRATION:
        FSM_HandleCalibration();
        break;

    case STATE_MEASURE_SEARCH:
        FSM_HandleMeasureSearch();
        break;

    case STATE_CALCULATION:
        FSM_HandleCalculation();
        break;

    case STATE_ERROR:
    default:
        FSM_HandleError();
        break;
    }
}
