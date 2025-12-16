#include "usb_cdc_bridge.h"

#include <string.h>
#include <stdio.h>

#include "bt_manager.h"
#include "debug_log.h"

#define RX_BUFFER_SIZE 128
#define UART_TX_TIMEOUT_MS 200U

static char s_rx_buffer[RX_BUFFER_SIZE];
static uint32_t s_rx_index = 0;

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

static uint8_t s_uart_rx_byte;

static void report_rx_mode_once(void)
{
    if (s_rx_mode_reported) {
        return;
    }
    s_rx_mode_reported = 1U;
    if (s_use_polling_rx) {
        PC_HostBridge_Send("STAT:UART_RX:POLL");
    } else {
        PC_HostBridge_Send("STAT:UART_RX:IT");
    }
}

static void start_uart_rx_interrupt(void)
{
    /* Clear sticky error flags before (re)starting RX. */
    __HAL_UART_CLEAR_OREFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_NEFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_FEFLAG(&UART_BRIDGE_HANDLE);
    __HAL_UART_CLEAR_PEFLAG(&UART_BRIDGE_HANDLE);

    if (HAL_UART_Receive_IT(&UART_BRIDGE_HANDLE, &s_uart_rx_byte, 1U) != HAL_OK) {
        Debug_LogDriver("UART", "rx_fail");
        s_use_polling_rx = 1U;
    }
}
#endif

void PC_HostBridge_Init(void)
{
    memset(s_rx_buffer, 0, sizeof(s_rx_buffer));
    s_rx_index = 0;
#ifndef UNIT_TESTS
    s_rx_mode_reported = 0U;
    /* When clock fallback is active, keep the RX path as simple as possible:
     * avoid IRQ RX and use polling from the main loop.
     */
    s_use_polling_rx = g_clock_fallback_active ? 1U : 0U;
    if (!s_use_polling_rx) {
        start_uart_rx_interrupt();
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
    if (!s_use_polling_rx) {
        return;
    }

    /* Drain a small amount per call to avoid starving the FSM. */
    for (uint32_t budget = 0U; budget < 256U; ++budget) {
        /* Clear ORE proactively: if an overrun happened, we already lost bytes.
         * Clearing it early helps the UART resume receiving cleanly.
         */
        if (__HAL_UART_GET_FLAG(&UART_BRIDGE_HANDLE, UART_FLAG_ORE) != RESET) {
            __HAL_UART_CLEAR_OREFLAG(&UART_BRIDGE_HANDLE);
        }

        if (__HAL_UART_GET_FLAG(&UART_BRIDGE_HANDLE, UART_FLAG_RXNE) == RESET) {
            break;
        }

        /* Reading RDR clears RXNE. */
        const uint8_t ch = (uint8_t)(UART_BRIDGE_HANDLE.Instance->RDR & 0xFFU);
        PC_HostBridge_OnRx(&ch, 1U);

        /* Clear ORE if it occurred (can happen if host sent while RX wasn't armed). */
        if (__HAL_UART_GET_FLAG(&UART_BRIDGE_HANDLE, UART_FLAG_ORE) != RESET) {
            __HAL_UART_CLEAR_OREFLAG(&UART_BRIDGE_HANDLE);
        }
    }
#endif
}

void PC_HostBridge_OnRx(const uint8_t *buf, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        const char c = (char)buf[i];

        // Handle newline as command terminator
        if (c == '\n' || c == '\r') {
            if (s_rx_index > 0U) {
                s_rx_buffer[s_rx_index] = '\0';
                Debug_LogDriver("UART_RX", s_rx_buffer);
                BT_ProcessIncoming(s_rx_buffer);
                s_rx_index = 0U;
            }
        } else {
            if (s_rx_index < (RX_BUFFER_SIZE - 1U)) {
                s_rx_buffer[s_rx_index++] = c;
            } else {
                // Overflow: discard and reset to keep parser in sync
                s_rx_index = 0U;
                Debug_LogDriver("UART", "overflow");
            }
        }
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
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &UART_BRIDGE_HANDLE) {
        PC_HostBridge_OnRx(&s_uart_rx_byte, 1U);
        start_uart_rx_interrupt();
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart == &UART_BRIDGE_HANDLE) {
        Debug_LogDriver("UART", "err_cb");
        start_uart_rx_interrupt();
    }
}
#endif
