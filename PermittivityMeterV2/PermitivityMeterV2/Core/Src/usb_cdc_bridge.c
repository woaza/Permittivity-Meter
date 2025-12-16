#include "usb_cdc_bridge.h"

#include <string.h>

#include "bt_manager.h"
#include "debug_log.h"

#define RX_BUFFER_SIZE 128
#define UART_TX_TIMEOUT_MS 200U

#define RX_LINE_QUEUE_SIZE 32U

#define RX_BYTE_RING_SIZE 512U

#define RX_TO_IDLE_BUF_SIZE 256U

static char s_rx_buffer[RX_BUFFER_SIZE];
static uint32_t s_rx_index = 0;

static char s_rx_line_queue[RX_LINE_QUEUE_SIZE][RX_BUFFER_SIZE];
static volatile uint8_t s_rx_line_head = 0U;
static volatile uint8_t s_rx_line_tail = 0U;

static uint8_t s_rx_to_idle_buf[RX_TO_IDLE_BUF_SIZE];

static uint8_t s_rx_byte_ring[RX_BYTE_RING_SIZE];
static volatile uint16_t s_rx_byte_head = 0U;
static volatile uint16_t s_rx_byte_tail = 0U;

static uint8_t s_use_polling_rx = 0U;
static uint8_t s_rx_mode_reported = 0U;

#ifndef UNIT_TESTS
#if defined(__has_include)
#  if __has_include("main.h")
#    include "main.h"
#  elif __has_include("../Core/Inc/main.h")
#    include "../Core/Inc/main.h"
#  endif
#else
#  include "main.h"
#endif

#ifndef STM32L4xx_HAL_H
typedef enum {
    HAL_OK       = 0x00U,
    HAL_ERROR    = 0x01U,
    HAL_BUSY     = 0x02U,
    HAL_TIMEOUT  = 0x03U
} HAL_StatusTypeDef;

typedef struct __UART_HandleTypeDef UART_HandleTypeDef;

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size, uint32_t Timeout);
#endif

#ifndef UART_BRIDGE_HANDLE
extern UART_HandleTypeDef huart2;
#define UART_BRIDGE_HANDLE huart2
#endif

extern volatile uint8_t g_clock_fallback_active;

static void rx_ring_push_byte(uint8_t b)
{
    const uint16_t tail = s_rx_byte_tail;
    const uint16_t next_tail = (uint16_t)((tail + 1U) % RX_BYTE_RING_SIZE);
    if (next_tail == s_rx_byte_head) {
        /* Drop oldest byte. */
        s_rx_byte_head = (uint16_t)((s_rx_byte_head + 1U) % RX_BYTE_RING_SIZE);
        Debug_LogDriver("UART", "rxring_ovf");
    }
    s_rx_byte_ring[tail] = b;
    s_rx_byte_tail = next_tail;
}

static uint8_t rx_ring_pop_byte(uint8_t *out)
{
    if (out == NULL) {
        return 0U;
    }
    const uint16_t head = s_rx_byte_head;
    if (head == s_rx_byte_tail) {
        return 0U;
    }
    *out = s_rx_byte_ring[head];
    s_rx_byte_head = (uint16_t)((head + 1U) % RX_BYTE_RING_SIZE);
    return 1U;
}

static void enqueue_rx_line(const char *line)
{
    if (line == NULL || line[0] == '\0') {
        return;
    }

    const uint8_t tail = s_rx_line_tail;
    const uint8_t next_tail = (uint8_t)((tail + 1U) % RX_LINE_QUEUE_SIZE);
    if (next_tail == s_rx_line_head) {
        /* Drop oldest. */
        s_rx_line_head = (uint8_t)((s_rx_line_head + 1U) % RX_LINE_QUEUE_SIZE);
        Debug_LogDriver("UART", "rxq_ovf");
    }

    (void)strncpy(s_rx_line_queue[tail], line, RX_BUFFER_SIZE - 1U);
    s_rx_line_queue[tail][RX_BUFFER_SIZE - 1U] = '\0';
    s_rx_line_tail = next_tail;
}

static uint8_t dequeue_rx_line(char *out, uint32_t out_len)
{
    if (out == NULL || out_len == 0U) {
        return 0U;
    }

    const uint8_t head = s_rx_line_head;
    if (head == s_rx_line_tail) {
        return 0U;
    }

    (void)strncpy(out, s_rx_line_queue[head], out_len - 1U);
    out[out_len - 1U] = '\0';
    s_rx_line_head = (uint8_t)((head + 1U) % RX_LINE_QUEUE_SIZE);
    return 1U;
}

static void report_rx_mode_once(void)
{
    if (s_rx_mode_reported) {
        return;
    }
    s_rx_mode_reported = 1U;
    if (s_use_polling_rx) {
        PC_HostBridge_Send("STAT:UART_RX:POLL");
    } else {
        if (UART_BRIDGE_HANDLE.hdmarx != NULL) {
            PC_HostBridge_Send("STAT:UART_RX:DMA_IDLE");
        } else {
            PC_HostBridge_Send("STAT:UART_RX:IT_IDLE");
        }
    }
}

static void start_uart_rx_to_idle(void)
{
    /* Clear sticky error flags before (re)starting RX. */
    __HAL_UART_CLEAR_OREFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_NEFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_FEFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_PEFLAG(&UART_BRIDGE_HANDLE);

    /* Prefer Receive-to-Idle so we handle bursts efficiently and avoid per-byte re-arming overhead. */
    HAL_StatusTypeDef st = HAL_ERROR;
    if (UART_BRIDGE_HANDLE.hdmarx != NULL) {
        st = HAL_UARTEx_ReceiveToIdle_DMA(&UART_BRIDGE_HANDLE, s_rx_to_idle_buf, (uint16_t)sizeof(s_rx_to_idle_buf));
        if (st == HAL_OK) {
            /* Don't spam half-transfer callbacks. */
            __HAL_DMA_DISABLE_IT(UART_BRIDGE_HANDLE.hdmarx, DMA_IT_HT);
        }
    } else {
        st = HAL_UARTEx_ReceiveToIdle_IT(&UART_BRIDGE_HANDLE, s_rx_to_idle_buf, (uint16_t)sizeof(s_rx_to_idle_buf));
    }

    if (st != HAL_OK) {
        Debug_LogDriver("UART", "rx_idle_fail");
        /* Last-resort: fall back to polling RX. */
        s_use_polling_rx = 1U;
    }
}
#endif

void PC_HostBridge_Init(void)
{
    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_rx_index = 0;
    s_rx_line_head = 0U;
    s_rx_line_tail = 0U;
    s_rx_byte_head = 0U;
    s_rx_byte_tail = 0U;
#ifndef UNIT_TESTS
    s_rx_mode_reported = 0U;
    /* RX mode selection:
     * - Prefer DMA receive-to-idle whenever available (robust under slow clocks and bursts).
     * - If no RX DMA is configured, fall back to polling under clock-fallback (slow core clock).
     * - Under normal clocking (no fallback), interrupt receive-to-idle is acceptable.
     */
    s_use_polling_rx = (g_clock_fallback_active && (UART_BRIDGE_HANDLE.hdmarx == NULL)) ? 1U : 0U;
    if (!s_use_polling_rx) {
        start_uart_rx_to_idle();
    }
    report_rx_mode_once();
    Debug_LogDriver("UART", "init");
#else
    Debug_LogDriver("HOST", "init");
#endif
}

void PC_HostBridge_Poll(void)
{
#ifndef UNIT_TESTS
    /* If polling RX is enabled, drain UART into the byte ring. */
    if (s_use_polling_rx) {
        for (uint32_t budget = 0U; budget < 256U; ++budget) {
            if (__HAL_UART_GET_FLAG(&UART_BRIDGE_HANDLE, UART_FLAG_ORE) != RESET) {
                __HAL_UART_CLEAR_OREFLAG(&UART_BRIDGE_HANDLE);
            }
            if (__HAL_UART_GET_FLAG(&UART_BRIDGE_HANDLE, UART_FLAG_RXNE) == RESET) {
                break;
            }
            const uint8_t ch = (uint8_t)(UART_BRIDGE_HANDLE.Instance->RDR & 0xFFU);
            rx_ring_push_byte(ch);
        }
    }

    /* Assemble lines from the byte ring in main context. */
    for (uint32_t budget = 0U; budget < 256U; ++budget) {
        uint8_t ch;
        if (!rx_ring_pop_byte(&ch)) {
            break;
        }

        const char c = (char)ch;
        if (c == '\n' || c == '\r') {
            if (s_rx_index > 0U) {
                s_rx_buffer[s_rx_index] = '\0';
                enqueue_rx_line(s_rx_buffer);
                s_rx_index = 0U;
                s_rx_buffer[0] = '\0';
            }
        } else {
            if (s_rx_index < (RX_BUFFER_SIZE - 1U)) {
                s_rx_buffer[s_rx_index++] = c;
            } else {
                s_rx_index = 0U;
                Debug_LogDriver("UART", "overflow");
            }
        }
    }

    /* Always drain any complete lines into BT processing (even in IRQ RX mode). */
    for (uint32_t i = 0U; i < RX_LINE_QUEUE_SIZE; ++i) {
        char line[RX_BUFFER_SIZE];
        if (!dequeue_rx_line(line, sizeof(line))) {
            break;
        }
        BT_ProcessIncoming(line);
    }
#endif
}

void PC_HostBridge_OnRx(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        rx_ring_push_byte(buf[i]);
    }
}

void PC_HostBridge_Send(const char *str)
{
    if (str == NULL) {
        return;
    }

    char buffer[160];
    size_t len = strlen(str);
    if (len >= sizeof(buffer) - 2U) {
        len = sizeof(buffer) - 2U;
    }
    memcpy(buffer, str, len);
    if (len == 0U || buffer[len - 1U] != '\n') {
        buffer[len++] = '\n';
    }
    buffer[len] = '\0';

#ifndef UNIT_TESTS
    if (HAL_UART_Transmit(&UART_BRIDGE_HANDLE, (uint8_t *)buffer, (uint16_t)len, UART_TX_TIMEOUT_MS) != HAL_OK) {
        Debug_LogDriver("UART", "tx_fail");
    }
#else
    Debug_LogDriver("HOST_TX", buffer);
#endif
}

#ifndef UNIT_TESTS
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == &UART_BRIDGE_HANDLE) {
        if (Size > 0U && Size <= (uint16_t)sizeof(s_rx_to_idle_buf)) {
            PC_HostBridge_OnRx(s_rx_to_idle_buf, (uint32_t)Size);
        }

        /* Re-arm receive-to-idle unless we're in polling mode fallback. */
        if (!s_use_polling_rx) {
            start_uart_rx_to_idle();
        }
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &UART_BRIDGE_HANDLE) {
        Debug_LogDriver("UART", "err_cb");
        if (!s_use_polling_rx) {
            start_uart_rx_to_idle();
        }
    }
}
#endif
