#include "hal_nina.h"
#include <string.h>
#include "driverlib/MSP430F5xx_6xx/driverlib.h"

#include "../../util.h"
#include "../../middleware/rf_handler.h"
#include "../hal_defines.h"
#include "../hal_init.h"
#include "src/hal/hal_init.h"

// ###### global variables

extern volatile char receivedUARTData[];
extern volatile int receivedUARTData_index;

// ###### functions

void NINA_put_received_data_into_buffer(char buffer[])
{
    strcpy(buffer, receivedUARTData);
    reset_uart_buffer();
}

bool NINA_find_delimiter_in_response(const char *delimiter)
{
    return find_substring(receivedUARTData, MAX_UART_BUFFER, delimiter);
}

/**
 * Synchronosly transmits the string via UART.
 * Because it is synchronous make sure the AT cmd is small.
 */
void NINA_send_string(const char *string)
{
    int numBytes = strlen(string);
    NINA_send_bytes(string, numBytes);
}

void NINA_send_bytes(const char *buffer, int length)
{
    if (RF_is_NINA_powered_on())
    {
        int i = 0;
        for (i = 0; i < length; i++)
        {
            USCI_A_UART_transmitData(USCI_A0_BASE, buffer[i]);
            while (USCI_A_UART_queryStatusFlags(USCI_A0_BASE, USCI_A_UART_BUSY))
                ; // wait until complete
        }
    }
}

void reset_uart_buffer()
{
    receivedUARTData_index = 0;
    reset_buffer(receivedUARTData, MAX_UART_BUFFER);
}

void NINA_power_off()
{
    // first power off UART lines
    disable_uart();
    // next put NINA into reset
    GPIO_setOutputLowOnPin(NINA_RESET_PORTPIN);
    DELAY_MS(1);
    // lastly cut off power supply
    GPIO_setOutputHighOnPin(NINA_PWR_OFF_PORT, NINA_PWR_OFF_PIN);
}

void NINA_power_on()
{
    // first supply power
    GPIO_setOutputLowOnPin(NINA_PWR_OFF_PORT, NINA_PWR_OFF_PIN);
    DELAY_MS(1);
    // next init uart
    init_uart();
    // lastly put NINA from reset mode into normal operation
    GPIO_setOutputHighOnPin(NINA_RESET_PORTPIN);
}

bool NINA_is_powered_on()
{
    bool isNotReset = P2IN & GPIO_PIN4;
    bool isPowerOff = P6IN & NINA_PWR_OFF_PIN;
    return isNotReset && !isPowerOff;
}
