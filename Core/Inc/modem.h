#ifndef MODEM_H
#define MODEM_H

#include "main.h"

#include <stdint.h>

uint8_t Modem_InitNetwork(void);

uint8_t Modem_SendCommand(
    const char *command,
    const char *expected,
    uint32_t timeout_ms
);

uint16_t Modem_Read(
    char *buffer,
    uint16_t buffer_size,
    uint32_t timeout_ms
);

void Modem_ClearUart(void);

#endif