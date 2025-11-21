#include "rf_handler.h"
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "driverlib/MSP430F5xx_6xx/driverlib.h"

#include "../main_program.h"
#include "../settings.h"
#include "../util.h"
#include "../hal/hal_init.h"
#include "../hal/hal_defines.h"
#include "../error_codes.h"
#include "../hal/nina/hal_nina.h"
#include "../hal/hal_init.h"
#include "../hal/hal_ISRs.h"

/* ----------------------- // ###### global variables ----------------------- */

extern volatile uint16_t nina_cmd_timeout_countdown;
extern volatile uint16_t nina_boot_timeout_countdown;
extern int cycles_countdown_during_mode_switch;
extern int cycles_countdown_during_mode_switch_wait_response;
extern volatile bool bNinaIsLedRedOn;
extern volatile bool bNinaIsPeerConnected;

extern settings_t settings;

/* ---------------------------- // ###### typdefs --------------------------- */

typedef void (*command_finished_callback)(void);

/* ------------------------ // ###### local functions ----------------------- */

void send_at_command(char *string);

// command queue and its callbacks

void callback__do_nothing();

void init_nina_after_reset();
void RF_BT_init();
void callback__RF_BT_finish_init();
void callback__RF_BT_enable();
void callback__RF_BT_disable();
void callback__RF_WIFI_disable();

static void reset_NINA();

void prepare_sending_AT_command_batch(uint8_t size, command_finished_callback callback);
void send_AT_command_batch();
void prepare_sending_AT_command_single(char *command, command_finished_callback callback);
static void reset_command_queue_logic();

// functions for switching mode
void switch_to_data_mode();
void switch_to_cmd_mode();
void handle_mode_switch();

/* ------------------------ // ###### local variables ----------------------- */

// variables for mode and status
static bool is_BT_enabled = false;
static bool is_WIFI_enabled = false;
static RF_status current_RF_status = RF_STATUS_UNINITIALIZED;
static RF_mode target_mode = RF_DATA_MODE;

// variables for command handling
static const uint8_t max_command_queue_length = 10;
static char *command_queue[max_command_queue_length] = {}; // create a stack of max 10 commands that points to each command
static uint8_t command_queue_index = 0;
static uint8_t command_queue_current_size = 0;
static bool command_queue_has_errored = false;
static command_finished_callback command_callback = &callback__do_nothing;

static bool last_command_returned_ok = false;
static bool last_command_returned_error = false;
static bool last_command_timed_out = false;
static bool is_processing_command_queue = false;

// variables for switching between cmd/data mode
bool isInDataMode = false; // according to config default is command mode
static bool started_switching_modes = false;
static bool is_switching_modes = false;

// variables for sending data through NINA
static bool is_transmitting_message_through_nina = false;
static char message_to_transmit[MAX_RESPONSE_STRING_LENGTH];
static bool was_last_transmission_successfull = false;

// other
static bool has_booted_successfully = false;
static uint8_t rebootCounter = 0;
static const uint8_t maxRebootsUntilError = 10;

// ########################################################################################
// ########################################################################################
// ########################################################################################
// ###### command communication to NINA

/* -------------------------------------------------------------------------- */
/*                                   getters                                  */
/* -------------------------------------------------------------------------- */

bool RF_BT_is_enabled()
{
    return is_BT_enabled;
}

bool RF_is_in_command_mode()
{
    return !isInDataMode;
}

bool RF_is_in_data_mode()
{
    return isInDataMode;
}

bool RF_was_last_message_transmission_successful()
{
    return was_last_transmission_successfull;
}

bool RF_is_transmitting_message_through_nina()
{
    return is_transmitting_message_through_nina;
}

bool RF_is_peer_connected()
{
    return bNinaIsPeerConnected;
}

bool RF_BT_is_initialized()
{
    return current_RF_status == RF_STATUS_BT_INITIALIZED;
}

bool RF_is_booted()
{
    return has_booted_successfully;
}

/* -------------------------------------------------------------------------- */
/*                              NINA power on/off                             */
/* -------------------------------------------------------------------------- */

void RF_NINA_power_off()
{
    NINA_power_off();

    /* ---------------------------- reset everything ---------------------------- */

    is_BT_enabled = false;
    is_WIFI_enabled = false;

    // variables for command handling
    command_queue_index = 0;
    command_queue_current_size = 0;
    command_queue_has_errored = false;
    command_callback = &callback__do_nothing;

    last_command_returned_ok = false;
    last_command_returned_error = false;
    last_command_timed_out = false;
    is_processing_command_queue = false;

    // variables for switching between cmd/data mode
    isInDataMode = false; // according to config default is command mode
    started_switching_modes = false;
    is_switching_modes = false;

    // variables for sending data through NINA
    is_transmitting_message_through_nina = false;
    was_last_transmission_successfull = false;

    // other
    has_booted_successfully = false;
}

void RF_NINA_power_on()
{
    NINA_power_on();
}

bool RF_is_NINA_powered_on()
{
    return NINA_is_powered_on();
}

/* -------------------------------------------------------------------------- */
/*                              command handling                              */
/* -------------------------------------------------------------------------- */

/**
 * empty callback. does nothing.
 */
void callback__do_nothing()
{
}

/* -------------------------------- enable BT ------------------------------- */

static char cmd_setConnectable_yes[] = "AT+UBTCM=2\r\n";
static char cmd_setDiscoverable_general[] = "AT+UBTDM=3\r\n";
static char cmd_setPairable_yes[] = "AT+UBTPM=2\r\n";
static char cmd_disableBLE[] = "AT+UBTLE=0\r\n";
static char cmd_setMasterSlavePolicy_dontcare[] = "AT+UBTMSP=1\r\n";
static char cmd_setName[] = "AT+UBTLN=Sound of Soul (1234567)\r\n";

/**
 * Executes the enable-commands
 * Configures NINA for:
 * -) connectable: yes
 * -) Master/Slave mode: dont care
 * -) discoverable: general discoverability
 * -) pairable: yes
 * -) BLE: disabled
 */
void RF_BT_enable()
{
    if (!is_BT_enabled)
    {
        copyBuffer(settings.serialNumberString, &cmd_setName[24], 7);

        command_queue[0] = cmd_setConnectable_yes;
        command_queue[1] = cmd_setDiscoverable_general;
        command_queue[2] = cmd_setPairable_yes;
        command_queue[3] = cmd_disableBLE;
        command_queue[4] = cmd_setMasterSlavePolicy_dontcare;
        command_queue[5] = cmd_setName;

        prepare_sending_AT_command_batch(6, callback__RF_BT_enable);
    }
}

void callback__RF_BT_enable()
{
    is_BT_enabled = true;
    switch_to_data_mode();
}

/* --------------------------------- init BT -------------------------------- */

/**
 * Initializes the NINA
 * -) sets the BT name NINA should advertise itself
 */
void RF_BT_init()
{
    if (current_RF_status == RF_STATUS_UNINITIALIZED)
    {
        prepare_sending_AT_command_single(cmd_setName, callback__RF_BT_finish_init);
    }
}

void callback__RF_BT_finish_init()
{
    current_RF_status = RF_STATUS_BT_INITIALIZED;
    switch_to_data_mode();
}

/* ------------------------------- disable BT ------------------------------- */

// TODO: first get the list with all connected peers and then select the correct one. Here I'm just guessing that its peer 1 (which is most probably correct)
static char cmd_disconnectPeer[] = "AT+UDCPC=1\r\n";
static char cmd_setConnectable_none[] = "AT+UBTCM=1\r\n";
static char cmd_setDiscoverable_none[] = "AT+UBTDM=1\r\n";

/**
 * Executes the init-commands
 * Configures NINA for:
 * -) connectable: no
 * -) discoverable: no discoverability
 * and also disconnect peer
 */
void RF_BT_disable()
{
    if (is_BT_enabled)
    {

        command_queue[0] = cmd_disconnectPeer;
        command_queue[1] = cmd_setConnectable_none;
        command_queue[2] = cmd_setDiscoverable_none;

        prepare_sending_AT_command_batch(3, callback__RF_BT_disable);
    }
}

void callback__RF_BT_disable()
{
    is_BT_enabled = false;
}

/* ------------------------------- disable WIFI ----------------------------- */

/**
 * Deactivates the wifi interface 0
 */
void RF_WIFI_disable()
{
    if (is_WIFI_enabled)
    {
        char *cmd_disableInterface0 = "AT+UWSCA=0,4\r\n";

        command_queue[0] = cmd_disableInterface0;

        prepare_sending_AT_command_batch(1, callback__RF_WIFI_disable);
    }
}

void callback__RF_WIFI_disable()
{
    is_WIFI_enabled = false;
}

// ########################################################################################
// ########################################################################################
// ########################################################################################
// ############# sending string through nina (to eg PC)

/**
 * Transmits a string through NINA to a connected device (eg PC)
 * The passed string parameter must not have a length greater than MAX_RESPONSE_STRING_LENGTH
 * This method is async because we may need to switch to data mode -> check for RF_was_last_message_transmission_successfull()
 *
 * @param string: The string to be sent. Must not be longer than MAX_RESPONSE_STRING_LENGTH
 * @param override_last_transmission: May be used for important messages like errors to override the current transmission
 */
cmd_response RF_send_string(char *string, bool override_last_transmission)
{
    if (current_RF_status != RF_STATUS_BT_INITIALIZED)
    {
        is_transmitting_message_through_nina = false;
        return create_cmd_response(false, ERRORCODE_NINA_UNABLE_TO_INITIALIZE);
    }

    //    if (strlen(string) == 0)
    //    {
    //        return create_cmd_response(true, ERRORCODE_NINA_NO_MESSAGE_GIVEN); // empty message was successfully not transmitted
    //    }

    if (is_transmitting_message_through_nina && !override_last_transmission)
    {
        return create_cmd_response(false, ERRORCODE_NINA_UNABLE_TO_SEND_STILL_TRANSMITTING); // still transmitting old message
    }

    was_last_transmission_successfull = false;
    is_transmitting_message_through_nina = true;

    switch_to_data_mode();
    copyBuffer(string, message_to_transmit, MAX_RESPONSE_STRING_LENGTH);
    return create_cmd_response(true, ERRORCODE_OK_NO_ERROR);
}

void RF_send_bytes_blocking(char *buffer, int length)
{
    if (bNinaIsPeerConnected && isInDataMode)
        NINA_send_bytes(buffer, length);
}

// ########################################################################################
// ########################################################################################
// ########################################################################################
// ############# handling of cyclic, state changes and all other things

/**
 * This function shall be called periodically.
 * It reads the UART buffer and detects responses from NINA
 *  and also handles the batch sending of commands
 */
void RF_cyclic()
{
    /* --------------------------- first handle bootup -------------------------- */
    if (!has_booted_successfully)
    {
        if (nina_boot_timeout_countdown == 1)
        {
            // we did not detect the welcome message from NINA -> boot unsuccessful -> reboot
            rebootCounter++;
            if (rebootCounter >= maxRebootsUntilError)
                throw_fatal_error(ERRORCODE_NINA_MAX_BOOT_TRIES_EXCEEDED);
            else
                reset_NINA();
        }
        else if (NINA_find_delimiter_in_response("+STARTUP\r"))
        {
            has_booted_successfully = true;
            current_RF_status = RF_STATUS_BT_INITIALIZED;
        }
        return; // until we have booted successfully we do nothing else
    }

    /* ----------------- normal init and cmd execution handling ----------------- */

    if (nina_cmd_timeout_countdown == 1)
    {
        nina_cmd_timeout_countdown = 0;
        last_command_timed_out = true;
    }

    // override isInDataMode when possible from reliable source
    if (!bNinaIsPeerConnected)
    {
        // if no peer is connected we can be sure that NINA is in data or command mode (see datasheet pg 13)
        isInDataMode = bNinaIsLedRedOn;
    }

    // TODO: maybe switch if/else branch and remove if cond?
    if (!isInDataMode || (is_switching_modes || started_switching_modes))
    {
        // detect if last command was ok or not and set flags
        if (NINA_find_delimiter_in_response("ERROR\r\n"))
        {
            last_command_returned_error = true;
            last_command_returned_ok = false;
        }
        else if (NINA_find_delimiter_in_response("OK\r\n"))
        {
            last_command_returned_error = false;
            last_command_returned_ok = true;
        }
        else if (NINA_find_delimiter_in_response("+UUDPC:"))
        {
            // TODO: we got connected. maybe save the peer handle address?
        }
        else if (NINA_find_delimiter_in_response("+UUDPD:"))
        {
            // TODO: we got disconnected. maybe do something with this peer handle?
        }
    }
    else
    {
        if (is_transmitting_message_through_nina)
        {
            NINA_send_string(message_to_transmit);
            was_last_transmission_successfull = true;
            is_transmitting_message_through_nina = false;
            reset_buffer(message_to_transmit, MAX_RESPONSE_STRING_LENGTH);
        }
    }

    // call async handlers
    if (is_switching_modes || started_switching_modes)
    {
        handle_mode_switch();
    }
    else if (is_processing_command_queue)
    {
        send_AT_command_batch();
    }
    else if (current_RF_status == RF_STATUS_UNINITIALIZED)
    {
        // after booting we issued that command already and we do not want to execute it twice
        if (command_callback != callback__RF_BT_finish_init)
            RF_BT_init();
    }
    else
    {
        if (!isInDataMode)
            switch_to_data_mode(); // go into default state (if not already in there)
    }
}

/**
 * If a command was send over BT it will be placed on the buffer
 */
void RF_fetch_BT_cmd(char buffer[])
{
    NINA_put_received_data_into_buffer(buffer);
}

void prepare_sending_AT_command_single(char *command, command_finished_callback callback)
{
    command_queue[0] = command;
    prepare_sending_AT_command_batch(1, callback);
}

void prepare_sending_AT_command_batch(uint8_t size, command_finished_callback callback)
{
    reset_command_queue_logic();
    if (size > max_command_queue_length)
    {
        command_queue_has_errored = true;
        return;
    }
    command_queue_current_size = size;
    command_callback = callback;

    // empty the unused fields
    int i = 0;
    for (i = size; i < max_command_queue_length; i++)
        command_queue[i] = "";

    is_processing_command_queue = true;
    send_AT_command_batch();
}

static void reset_command_queue_logic()
{
    is_processing_command_queue = false;
    command_queue_has_errored = false;
    command_queue_index = 0;
    command_queue_current_size = 0;
}

void send_AT_command_batch()
{
    if (command_queue_has_errored)
        return;

    if (!isInDataMode)
    {
        if (command_queue_index == 0)
        {
            reset_uart_buffer(); // clear buffer before sending stuff
            // first send doesn't check the last cmd status
            send_at_command(command_queue[command_queue_index]);
            command_queue_index++;
        }

        if (last_command_returned_ok)
        {
            if (command_queue_index < command_queue_current_size)
            {
                // there is another command to be send
                send_at_command(command_queue[command_queue_index]);
                command_queue_index++;
            }
            else
            {
                // we are finished sending commands
                switch_to_data_mode(); // let's get back to default mode
                // reset queue execution logic
                reset_command_queue_logic();
                (*command_callback)(); // exec callback
            }
        }
        else if (last_command_returned_error)
        {
            // last command errored
            // TODO: try again a few times instead of instant fail

            switch_to_data_mode(); // let's get back to default mode
            // reset queue execution logic
            reset_command_queue_logic();
        }
        else if (last_command_timed_out)
        {
            // TODO: how many retries until error?
            command_queue_index--; // retry at last not-sent command
        }
    }
    else
    {
        switch_to_cmd_mode();
    }
}

void handle_mode_switch()
{
    if (isInDataMode && target_mode == RF_CMD_MODE)
    {
        if (started_switching_modes)
        {
            // first entry -> init
            GPIO_setOutputHighOnPin(UCA0DSR_PIN);
            started_switching_modes = false;
            is_switching_modes = true;
            last_command_returned_error = false;
            last_command_returned_ok = false;
            last_command_timed_out = false;
            cycles_countdown_during_mode_switch = MS_TO_CYCLES(2);               // time by trial and error
            cycles_countdown_during_mode_switch_wait_response = MS_TO_CYCLES(2); // time by trial and error
        }
        else
        {
            if (cycles_countdown_during_mode_switch <= 0)
            {
                GPIO_setOutputLowOnPin(UCA0DSR_PIN);
                if (cycles_countdown_during_mode_switch_wait_response <= 0)
                {
                    if (last_command_returned_ok)
                    {
                        // finished
                        last_command_timed_out = false;
                        is_switching_modes = false;
                        cycles_countdown_during_mode_switch = -1;
                        cycles_countdown_during_mode_switch_wait_response = -1;
                        isInDataMode = false;
                        if (is_processing_command_queue)
                            send_AT_command_batch(); // we were sending something -> resume that
                    }
                    else
                    {
                        // we tried switching to cmd mode, but there was no 'OK' response after the switching timeout
                        // we should try switching again
                        last_command_timed_out = false;
                        is_switching_modes = false;
                        switch_to_cmd_mode();
                    }
                }
            }
        }
    }
    else if (!isInDataMode && target_mode == RF_DATA_MODE)
    {
        if (started_switching_modes)
        {
            // first entry -> init
            started_switching_modes = false;
            is_switching_modes = true;
            send_at_command("ATO1\r\n");
        }
        else
        {
            if (last_command_returned_ok)
            {
                // finished
                last_command_timed_out = false;
                nina_cmd_timeout_countdown = 0; // reset timer because everything went as planed
                is_switching_modes = false;
                isInDataMode = true;
            }
            else if (last_command_returned_error)
            {
                // try again (TODO: how many times until error?)
                send_at_command("ATO1\r\n");
            }
            else if (last_command_timed_out)
            {
                // the last command took too long. maybe we are already in data mode?
                // current_BT_mode = RF_BT_DATA_MODE;
                // maybe try switching again? idk
                switch_to_data_mode();
            }
        }
    }
    else
    {
        // we have no reason to change the mode -> seems like we are finished here
        is_switching_modes = false;
        started_switching_modes = false;
    }
}

void switch_to_data_mode()
{
    if (!isInDataMode && RF_is_peer_connected()) // data mode only makes sense when a peer is connected
    {
        target_mode = RF_DATA_MODE;
        started_switching_modes = true;
    }
}

void switch_to_cmd_mode()
{
    if (isInDataMode)
    {
        target_mode = RF_CMD_MODE;
        started_switching_modes = true;
    }
}

static void reset_NINA()
{
    nina_boot_timeout_countdown = NINA_BOOT_TIME;
    GPIO_setOutputLowOnPin(NINA_RESET_PORTPIN);
    DELAY_MS(1);
    GPIO_setOutputHighOnPin(NINA_RESET_PORTPIN);
}

/**
 * synchronous. calls NINA send function with additional stuff
 */
void send_at_command(char *string)
{
    last_command_returned_ok = false;
    last_command_returned_error = false;
    last_command_timed_out = false;

    // small performance boost: we know that more time is needed during booting, but later a shorter time until error is enough
    nina_cmd_timeout_countdown = has_booted_successfully ? MS_TO_CYCLES(400) : MS_TO_CYCLES(160); // numbers gained through trial and error

    NINA_send_string(string);
}
