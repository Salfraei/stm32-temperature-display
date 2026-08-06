#ifndef LCD5110_H
#define LCD5110_H

#include "main.h"

#include <stdint.h>

void Nokia5110_Init(void);
void Nokia5110_Clear(void);
void Nokia5110_SetCursor(uint8_t x, uint8_t y);
void Nokia5110_WriteChar(char character);
void Nokia5110_WriteString(const char *text);
void Nokia5110_ShowTemperature(float temperature);

#endif