#ifndef SRC_RF_HANDLER_H_
#define SRC_RF_HANDLER_H_

#include <stdbool.h>
#include "../command_handler.h"

typedef enum
{
    RF_STATUS_ERROR,         //
    RF_STATUS_UNINITIALIZED, //
    RF_STATUS_BT_INITIALIZED
} RF_status;

typedef enum
{
    RF_DATA_MODE, //
    RF_CMD_MODE
} RF_mode;

void RF_cyclic();

void RF_fetch_BT_cmd(char buffer[]);

cmd_response RF_send_string(char *string, bool override_last_transmission);
void RF_send_bytes_blocking(char *buffer, int length);

// general
void RF_NINA_power_off();
void RF_NINA_power_on();
bool RF_is_NINA_powered_on();
bool RF_is_in_command_mode();
bool RF_is_in_data_mode();
bool RF_was_last_message_transmission_successful();
bool RF_is_transmitting_message_through_nina();
bool RF_is_booted();
bool RF_is_peer_connected();

// BT
bool RF_BT_is_enabled();
bool RF_BT_is_initialized();
void RF_BT_disable();
void RF_BT_enable();

// WIFI
void RF_WIFI_disable();

#endif /* SRC_RF_HANDLER_H_ */
