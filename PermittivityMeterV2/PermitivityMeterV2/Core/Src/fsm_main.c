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
static void FSM_HandleCalibration(void);
static void FSM_HandleMeasureSearch(void);
static void FSM_HandleCalculation(void);
static void FSM_HandleError(void);
static void FSM_StartCalibration(const char *reason_tag);
static void FSM_StartMeasurement(const char *reason_tag);
static uint8_t FSM_IsMeasurementValid(const MeasurementResult_t *result);
static void FSM_UpdateLCD(const char *line0, const char *line1);

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
        s_result_pending = 1U;
        BSP_RF_EnableExcitation(0U);
        BSP_LED_Set(LED_EXCITE, 0U);
        FSM_UpdateLCD("RESULT", "Sending...");
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
        BT_SendStatus("ERR");
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
            FSM_TransitionTo(STATE_CALCULATION, "meas_done");
        } else {
            BT_SendStatus("ERR");
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
    if (!s_result_pending) {
        return;
    }

    BT_SendResult(s_last_result);
    BSP_LED_Set(LED_MEAS, 0U);
    s_result_pending = 0U;

    if (s_calibration.is_valid) {
        FSM_TransitionTo(STATE_IDLE, "calc_done");
    } else {
        FSM_TransitionTo(STATE_ERROR, "calc_no_cal");
    }

    return;
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
