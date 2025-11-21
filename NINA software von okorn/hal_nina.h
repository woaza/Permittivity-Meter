#ifndef SRC_HAL_HAL_NINA_H_
#define SRC_HAL_HAL_NINA_H_

#include <stdbool.h>
#include <stdint.h>

void reset_uart_buffer();
void NINA_send_string(const char *data);
void NINA_send_bytes(const char* buffer, int length);

bool NINA_find_delimiter_in_response(const char* delimiter);
void NINA_put_received_data_into_buffer(char buffer[]);

void NINA_power_off();
void NINA_power_on();
bool NINA_is_powered_on();

#endif /* SRC_HAL_HAL_NINA_H_ */
