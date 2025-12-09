/**
 * @file    hal_gpio.h
 * @brief   Hardware Abstraction Layer for GPIO (LEDs, Buttons, Control Signals)
 * @version 1.0
 * @author  Majdedin Al Rashid
 * @date    2025-12-09
 */

/**
 * @page hal_gpio_usage How to Use HAL GPIO Module
 * 
 * @section overview Overview
 * This module provides a hardware-independent interface for controlling
 * GPIOs (LEDs, Buttons, Control Signals). You do NOT need to know the physical
 * pin assignments (PA6, PB1, etc.) - just use the logical names.
 * 
 * @section including Including the Header
 * @code
 * #include "hl/hal_gpio.h"
 * @endcode
 * 
 * @section init Initialization
 * GPIO pins are automatically configured by STM32CubeMX in main.c.
 * You can optionally call HL_GPIO_Init() for any additional setup:
 * @code
 * HL_GPIO_Init();  // Optional - already done in MX_GPIO_Init()
 * @endcode
 * 
 * @section leds Controlling LEDs
 * There are 4 LEDs available:
 * - HL_GPIO_LED_INIT   (Green)  - System initialized / Idle
 * - HL_GPIO_LED_MEAS   (Blue)   - Measurement in progress
 * - HL_GPIO_LED_EXCITE (Yellow) - RF excitation active
 * - HL_GPIO_LED_ERR    (Red)    - Error state
 * 
 * @subsection led_on Turn LED ON
 * @code
 * HL_GPIO_Write(HL_GPIO_LED_INIT, HL_GPIO_HIGH);   // Green LED ON
 * HL_GPIO_Write(HL_GPIO_LED_ERR, HL_GPIO_HIGH);    // Red LED ON
 * @endcode
 * 
 * @subsection led_off Turn LED OFF
 * @code
 * HL_GPIO_Write(HL_GPIO_LED_MEAS, HL_GPIO_LOW);    // Blue LED OFF
 * @endcode
 * 
 * @subsection led_toggle Toggle LED
 * @code
 * HL_GPIO_Toggle(HL_GPIO_LED_EXCITE);  // Flip Yellow LED state
 * @endcode
 * 
 * @section button Reading the User Button
 * The User Button (B1 on Nucleo board) is active-low internally,
 * but BSP_Button_GetState() in bsp_ui.c handles the inversion for you.
 * 
 * Direct read (raw, active-low):
 * @code
 * HL_GPIO_State_t btn_state;
 * HL_GPIO_Read(HL_GPIO_BTN_USER, &btn_state);
 * // btn_state == HL_GPIO_LOW means button IS pressed
 * // btn_state == HL_GPIO_HIGH means button is NOT pressed
 * @endcode
 * 
 * Recommended: Use BSP layer instead:
 * @code
 * #include "bsp_ui.h"
 * uint8_t pressed = BSP_Button_GetState();  // 1 = pressed, 0 = not pressed
 * @endcode
 * 
 * @section rf_control RF Gain Control
 * Two GPIO pins control RF amplifier gain (binary encoding):
 * @code
 * // Set gain level 0 (both LOW)
 * HL_GPIO_Write(HL_GPIO_RF_GAIN_0, HL_GPIO_LOW);
 * HL_GPIO_Write(HL_GPIO_RF_GAIN_1, HL_GPIO_LOW);
 * 
 * // Set gain level 3 (both HIGH)
 * HL_GPIO_Write(HL_GPIO_RF_GAIN_0, HL_GPIO_HIGH);
 * HL_GPIO_Write(HL_GPIO_RF_GAIN_1, HL_GPIO_HIGH);
 * @endcode
 * 
 * @section errors Error Handling
 * All functions return HL_GPIO_Status_t:
 * @code
 * HL_GPIO_Status_t result = HL_GPIO_Write(HL_GPIO_LED_INIT, HL_GPIO_HIGH);
 * if (result != GPIO_OK) {
 *     // Handle error (e.g., invalid pin)
 * }
 * @endcode
 * 
 * @section pin_table Pin Reference Table
 * | Logical Name       | Physical Pin | Direction | Description          |
 * |--------------------|--------------|-----------|----------------------|
 * | HL_GPIO_LED_INIT   | PA6          | Output    | Init/Idle LED (Green)|
 * | HL_GPIO_LED_MEAS   | PA7          | Output    | Measure LED (Blue)   |
 * | HL_GPIO_LED_EXCITE | PC7          | Output    | Excite LED (Yellow)  |
 * | HL_GPIO_LED_ERR    | PB1          | Output    | Error LED (Red)      |
 * | HL_GPIO_BTN_USER   | PC13         | Input     | User Button          |
 * | HL_GPIO_RF_GAIN_0  | PC8          | Output    | RF Gain Bit 0        |
 * | HL_GPIO_RF_GAIN_1  | PC9          | Output    | RF Gain Bit 1        |
 * | HL_GPIO_NINA_RST   | PA11         | Output    | BLE Module Reset     |
 * | HL_GPIO_NINA_STOP  | PA12         | Output    | BLE Module Stop      |
 */

#ifndef HL_HAL_GPIO_H_
#define HL_HAL_GPIO_H_

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/*                              Pin Definitions                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Application-level GPIO pin identifiers.
 *        These abstract the physical pin mapping from higher layers.
 */
typedef enum 
{
    /* LEDs */
    HL_GPIO_LED_INIT   = 0,  /**< Init/Idle LED (Green) - PA6 */
    HL_GPIO_LED_MEAS   = 1,  /**< Measurement LED (Blue) - PA7 */
    HL_GPIO_LED_EXCITE = 2,  /**< RF Excitation LED (Yellow) - PC7 */
    HL_GPIO_LED_ERR    = 3,  /**< Error LED (Red) - PB1 */

    /* Inputs */
    HL_GPIO_BTN_USER   = 4,  /**< User Button - PC13 */

    /* RF Control */
    HL_GPIO_RF_GAIN_0  = 5,  /**< RF Gain Select Bit 0 - PC8 */
    HL_GPIO_RF_GAIN_1  = 6,  /**< RF Gain Select Bit 1 - PC9 */

    /* NINA Module Control */
    HL_GPIO_NINA_RST   = 7,  /**< NINA Reset - PA11 */
    HL_GPIO_NINA_STOP  = 8,  /**< NINA Stop - PA12 */

    HL_GPIO_PIN_COUNT        /**< Total number of defined pins */
} HL_GPIO_Pin_t;

/**
 * @brief GPIO logic levels.
 */
typedef enum 
{
    HL_GPIO_LOW  = 0,
    HL_GPIO_HIGH = 1
} HL_GPIO_State_t;

/**
 * @brief Return status codes.
 */
typedef enum 
{
    GPIO_OK    = 0,
    GPIO_ERROR = 1
} HL_GPIO_Status_t;

/* -------------------------------------------------------------------------- */
/*                             Function Prototypes                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialize the GPIO module.
 * @note   This function is a placeholder; actual pin initialization is done
 *         in MX_GPIO_Init() generated by STM32CubeMX.
 * @retval GPIO_OK on success.
 */
HL_GPIO_Status_t HL_GPIO_Init(void);

/**
 * @brief  Write a logic level to an output pin.
 * @param  pin   The application-level pin identifier.
 * @param  state The desired logic level (HL_GPIO_LOW or HL_GPIO_HIGH).
 * @retval GPIO_OK on success, GPIO_ERROR if pin is invalid or not an output.
 */
HL_GPIO_Status_t HL_GPIO_Write(HL_GPIO_Pin_t pin, HL_GPIO_State_t state);

/**
 * @brief  Toggle the state of an output pin.
 * @param  pin The application-level pin identifier.
 * @retval GPIO_OK on success, GPIO_ERROR if pin is invalid or not an output.
 */
HL_GPIO_Status_t HL_GPIO_Toggle(HL_GPIO_Pin_t pin);

/**
 * @brief  Read the current state of an input pin.
 * @param  pin   The application-level pin identifier.
 * @param  state Pointer to store the read state.
 * @retval GPIO_OK on success, GPIO_ERROR if pin is invalid.
 */
HL_GPIO_Status_t HL_GPIO_Read(HL_GPIO_Pin_t pin, HL_GPIO_State_t *state);

#endif /* HL_HAL_GPIO_H_ */
