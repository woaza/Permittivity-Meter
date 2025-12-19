#pragma once

// Hardware mapping for the NUCLEO-L476RG platform used in the Snow Permittivity Meter.
// Values are expressed as symbolic macros so code can remain hardware-agnostic while we
// develop against mocks. Ports use STM32-style lettering and pins are the numeric value.

#define PIN_NOCH_20M_IN_PORT        'A'
#define PIN_NOCH_20M_IN_PIN         0

#define PIN_FRQ_TN_PORT             'A'
#define PIN_FRQ_TN_PIN              4

#define PIN_Q_FACT_TN_PORT          'A'
#define PIN_Q_FACT_TN_PIN           5

#define PIN_NOTCH_AMP_IN_PORT       'C'
#define PIN_NOTCH_AMP_IN_PIN        0
#define ADC_NOTCH_CHANNEL           1

#define PIN_GAIN_SLCT_1_PORT        'C'
#define PIN_GAIN_SLCT_1_PIN         3

#define PIN_GAIN_SLCT_2_PORT        'C'
#define PIN_GAIN_SLCT_2_PIN         1

#define PIN_OP_DIS_PORT             'C'
#define PIN_OP_DIS_PIN              4

#define PIN_BTN_START_PORT          'C'
#define PIN_BTN_START_PIN           13

#define PIN_STATUS_LED_PORT         'B'
#define PIN_STATUS_LED_PIN          8

#define PIN_MEAS_LED_PORT           'C'
#define PIN_MEAS_LED_PIN            9

#define PIN_EXCITE_LED_PORT         'C'
#define PIN_EXCITE_LED_PIN          8

#define PIN_ERR_LED_PORT            'C'
#define PIN_ERR_LED_PIN             6

#define PIN_NINA_TX_PORT            'C'
#define PIN_NINA_TX_PIN             10

#define PIN_NINA_RX_PORT            'C'
#define PIN_NINA_RX_PIN             11

#define PIN_NINA_CTS_PORT           'B'
#define PIN_NINA_CTS_PIN            7

#define PIN_NINA_RTS_PORT           'A'
#define PIN_NINA_RTS_PIN            15

#define PIN_NINA_RST_PORT           'A'
#define PIN_NINA_RST_PIN            11

#define PIN_NINA_STOP_PORT          'A'
#define PIN_NINA_STOP_PIN           12

#define PIN_NINA_DTR_PORT           'B'
#define PIN_NINA_DTR_PIN            11

#define PIN_NINA_DSR_PORT           'B'
#define PIN_NINA_DSR_PIN            12

#define PIN_NINA_LED_RED_PORT       'C'
#define PIN_NINA_LED_RED_PIN        15

#define PIN_NINA_LED_BLUE_PORT      'C'
#define PIN_NINA_LED_BLUE_PIN       14

#define PIN_NINA_LED_GREEN_PORT     'C'
#define PIN_NINA_LED_GREEN_PIN      2

#define PIN_NINA_RTS_MON_PORT       'C'
#define PIN_NINA_RTS_MON_PIN        11

#define TIM_EXCITATION_INSTANCE     1
#define TIM_EXCITATION_CHANNEL      2

#define DAC_FREQ_CHANNEL            1
#define DAC_Q_CHANNEL               2

#define UART_BT_INSTANCE            4

#define PIN_LCD_SCL_PORT            'B'
#define PIN_LCD_SCL_PIN             6

#define PIN_LCD_SDA_PORT            'B'
#define PIN_LCD_SDA_PIN             9
