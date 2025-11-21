#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bt_manager.h"
#include "bsp_lcd.h"
#include "bsp_ui.h"
#include "fsm_main.h"
#include "mocks/mock_board.h"

static void run_ticks(size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        FSM_RunOnce();
    }
}

static void boot_to_idle(void)
{
    MockBoard_Reset();
    FSM_Init();
    run_ticks(2);
    MockBoard_BT_ClearHistory();
}

static const char *last_frame(void)
{
    const char *frame = BT_MockGetLastTx();
    return (frame != NULL) ? frame : "";
}

static const char *history_get(size_t chronological_index)
{
    const uint8_t count = MockBoard_BT_GetHistoryCount();
    if (chronological_index >= count) {
        return NULL;
    }
    const uint8_t offset_from_newest = (uint8_t)(count - 1U - chronological_index);
    return MockBoard_BT_GetHistoryEntry(offset_from_newest);
}

static void lcd_line_trimmed(uint8_t line, char *out, size_t out_len)
{
    char raw[LCD_CHAR_COUNT + 1] = {0};
    BSP_LCD_GetLine(line, raw, sizeof(raw));
    size_t end = strlen(raw);
    while (end > 0U && raw[end - 1U] == ' ') {
        raw[--end] = '\0';
    }
    strncpy(out, raw, out_len - 1U);
    out[out_len - 1U] = '\0';
}

#define ASSERT_TRUE(cond, msg)                                                                    \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            fprintf(stderr, "[ASSERT] %s failed: %s\n", __func__, msg);                         \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define ASSERT_STATE(expected)                                                                    \
    do {                                                                                          \
        if (FSM_GetState() != (expected)) {                                                       \
            fprintf(stderr,                                                                       \
                    "[ASSERT] %s expected state %d got %d\n",                                   \
                    __func__,                                                                     \
                    (int)(expected),                                                              \
                    (int)FSM_GetState());                                                         \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define ASSERT_STR_EQ(actual, expected, msg)                                                      \
    do {                                                                                          \
        const char *val = (actual);                                                               \
        if (val == NULL || strcmp(val, (expected)) != 0) {                                        \
            fprintf(stderr, "[ASSERT] %s %s (got '%s')\n", __func__, msg, val ? val : "<null>"); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

#define ASSERT_STR_PREFIX(actual, prefix, msg)                                                    \
    do {                                                                                          \
        const char *val = (actual);                                                               \
        if (val == NULL || strncmp(val, (prefix), strlen(prefix)) != 0) {                          \
            fprintf(stderr, "[ASSERT] %s %s (got '%s')\n", __func__, msg, val ? val : "<null>"); \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

static int test_init_reaches_idle(void)
{
    boot_to_idle();
    ASSERT_STATE(STATE_IDLE);
    return 0;
}

static int test_meas_rejected_without_cal(void)
{
    boot_to_idle();
    BT_MockEnqueueCommand("CMD:MEAS");
    run_ticks(2);
    ASSERT_STR_EQ(last_frame(), "STAT:ERR", "should reject measurement without calibration");
    ASSERT_STATE(STATE_IDLE);
    return 0;
}

static int test_button_triggers_calibration(void)
{
    boot_to_idle();
    BSP_Button_SetState(1U);
    run_ticks(1);
    BSP_Button_SetState(0U);
    ASSERT_STATE(STATE_CALIBRATION);
    run_ticks(4);
    ASSERT_STATE(STATE_IDLE);
    ASSERT_STR_EQ(last_frame(), "STAT:CAL_OK", "calibration should succeed");
    return 0;
}

static void perform_calibration_via_bt(void)
{
    boot_to_idle();
    BT_MockEnqueueCommand("CMD:CAL");
    run_ticks(5);
}

static int test_measurement_flow(void)
{
    perform_calibration_via_bt();
    ASSERT_STATE(STATE_IDLE);
    ASSERT_STR_EQ(last_frame(), "STAT:CAL_OK", "calibration ack");

    BT_MockEnqueueCommand("CMD:MEAS");
    run_ticks(6);
    ASSERT_STATE(STATE_IDLE);
    ASSERT_STR_PREFIX(last_frame(), "DAT:RES:", "measurement result frame");
    return 0;
}

static int test_conn_status_leds_lcd(void)
{
    boot_to_idle();
    BT_MockEnqueueCommand("CMD:CONN");
    run_ticks(2);

    ASSERT_TRUE(MockBoard_BT_GetHistoryCount() >= 1U, "handshake should emit status");
    ASSERT_STR_EQ(history_get(0), "STAT:RDY", "first frame should ack ready");
    ASSERT_TRUE(BSP_LED_Get(LED_STATUS) == 1U, "status LED on after handshake");
    ASSERT_TRUE(BSP_LED_Get(LED_MEAS) == 0U && BSP_LED_Get(LED_EXCITE) == 0U &&
                    BSP_LED_Get(LED_ERROR) == 0U,
                "non-status LEDs off at idle");

    char line[32];
    lcd_line_trimmed(0U, line, sizeof(line));
    ASSERT_STR_EQ(line, "IDLE", "LCD line 0 shows IDLE");
    lcd_line_trimmed(1U, line, sizeof(line));
    ASSERT_STR_EQ(line, "Need CAL", "LCD prompts for calibration");
    return 0;
}

static int test_calibration_status_and_ready_screen(void)
{
    boot_to_idle();
    BT_MockEnqueueCommand("CMD:CAL");
    run_ticks(6);

    ASSERT_STATE(STATE_IDLE);
    ASSERT_TRUE(MockBoard_BT_GetHistoryCount() >= 2U, "calibration should emit CAL/CAL_OK");
    ASSERT_STR_EQ(history_get(0), "STAT:CAL", "calibration start frame");
    ASSERT_STR_EQ(history_get(1), "STAT:CAL_OK", "calibration completion frame");

    char line[32];
    lcd_line_trimmed(0U, line, sizeof(line));
    ASSERT_STR_EQ(line, "IDLE", "LCD resets to IDLE after calibration");
    lcd_line_trimmed(1U, line, sizeof(line));
    ASSERT_STR_EQ(line, "Ready", "LCD line 1 confirms readiness");

    ASSERT_TRUE(BSP_LED_Get(LED_STATUS) == 1U, "status LED on");
    ASSERT_TRUE(BSP_LED_Get(LED_MEAS) == 0U && BSP_LED_Get(LED_EXCITE) == 0U,
                "cal LEDs off once finished");
    return 0;
}

static int test_measurement_status_and_result_frame(void)
{
    perform_calibration_via_bt();
    MockBoard_BT_ClearHistory();

    BT_MockEnqueueCommand("CMD:MEAS");
    run_ticks(8);

    ASSERT_STATE(STATE_IDLE);
    ASSERT_TRUE(MockBoard_BT_GetHistoryCount() >= 2U, "measurement should emit MEAS + result");
    ASSERT_STR_EQ(history_get(0), "STAT:MEAS", "measurement status frame");
    ASSERT_STR_PREFIX(history_get(1), "DAT:RES:", "result payload frame");

    ASSERT_TRUE(BSP_LED_Get(LED_STATUS) == 1U, "status LED on after measurement");
    ASSERT_TRUE(BSP_LED_Get(LED_MEAS) == 0U && BSP_LED_Get(LED_EXCITE) == 0U,
                "measurement indicators cleared");

    char line[32];
    lcd_line_trimmed(0U, line, sizeof(line));
    ASSERT_STR_EQ(line, "IDLE", "LCD returns to idle heading");
    lcd_line_trimmed(1U, line, sizeof(line));
    ASSERT_STR_EQ(line, "Ready", "LCD indicates ready after measurement");
    return 0;
}

typedef int (*TestFunc)(void);

typedef struct {
    const char *name;
    TestFunc func;
} TestCase;

static const TestCase kTests[] = {
    {"init_reaches_idle", test_init_reaches_idle},
    {"reject_meas_without_cal", test_meas_rejected_without_cal},
    {"button_triggers_calibration", test_button_triggers_calibration},
    {"measurement_flow", test_measurement_flow},
    {"conn_status_leds_lcd", test_conn_status_leds_lcd},
    {"calibration_status_ready_screen", test_calibration_status_and_ready_screen},
    {"measurement_status_result", test_measurement_status_and_result_frame},
};

int main(void)
{
    int failures = 0;
    const size_t count = sizeof(kTests) / sizeof(kTests[0]);

    for (size_t i = 0; i < count; ++i) {
        printf("[ RUN      ] %s\n", kTests[i].name);
        const int rc = kTests[i].func();
        if (rc != 0) {
            printf("[  FAILED  ] %s\n", kTests[i].name);
            ++failures;
        } else {
            printf("[       OK ] %s\n", kTests[i].name);
        }
    }

    if (failures != 0) {
        printf("[ SUMMARY ] %d/%zu tests failed\n", failures, count);
        return EXIT_FAILURE;
    }

    printf("[ SUMMARY ] All %zu tests passed\n", count);
    return EXIT_SUCCESS;
}
