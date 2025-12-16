#pragma once

#include <stdint.h>
#include <stddef.h>

// Called by main.c to initialize the bridge state
void PC_HostBridge_Init(void);

// Called by the UART/USB ISR/Callback when data arrives
void PC_HostBridge_OnRx(const uint8_t *buf, uint32_t len);

// Called by the application to send data to the PC
void PC_HostBridge_Send(const char *str);

// Optional: Called periodically from the main loop to poll UART RX when IRQ RX
// is unavailable (e.g., during clock-fallback scenarios).
void PC_HostBridge_Poll(void);
