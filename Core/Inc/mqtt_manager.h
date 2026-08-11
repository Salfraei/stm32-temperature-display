#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include "main.h"

#include <stdint.h>

uint8_t MQTT_Connect(void);
uint8_t MQTT_Subscribe(void);

uint8_t MQTT_Process(
    float *temperature
);

#endif