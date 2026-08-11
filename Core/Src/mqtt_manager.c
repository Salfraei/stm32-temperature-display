#include "mqtt_manager.h"
#include "modem.h"
#include "usart.h"

#include <stdio.h>
#include <string.h>

#define MQTT_BROKER \
    "tcp://broker.emqx.io:1883"

#define MQTT_CLIENT_ID \
    "stm32-pavlo-a7670e-room1"

#define MQTT_TOPIC \
    "pavlo/esp32/room1"

#define MQTT_RECEIVE_SIZE 2048U

static uint8_t IsDigit(char character);

static uint8_t ParseFloatManual(
    const char *start,
    const char *end,
    float *value
);

static uint8_t MQTT_ExtractTemperature(
    const char *buffer,
    float *temperature
);

uint8_t MQTT_Connect(void)
{
    char command[192];
    char response[1024];

    Modem_SendCommand(
        "AT+CMQTTDISC=0,60\r\n",
        NULL,
        3000U
    );

    Modem_SendCommand(
        "AT+CMQTTREL=0\r\n",
        NULL,
        3000U
    );

    Modem_SendCommand(
        "AT+CMQTTSTOP\r\n",
        NULL,
        5000U
    );

    HAL_Delay(1000U);

    Modem_ClearUart();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+CMQTTSTART\r\n",
            strlen("AT+CMQTTSTART\r\n"),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(
        response,
        0,
        sizeof(response)
    );

    Modem_Read(
        response,
        sizeof(response),
        10000U
    );

    if ((strstr(
             response,
             "+CMQTTSTART: 0"
         ) == NULL) &&
        (strstr(
             response,
             "OK"
         ) == NULL))
    {
        return 0U;
    }

    snprintf(
        command,
        sizeof(command),
        "AT+CMQTTACCQ=0,\"%s\",0\r\n",
        MQTT_CLIENT_ID
    );

    if (!Modem_SendCommand(
            command,
            "OK",
            5000U
        ))
    {
        return 0U;
    }

    snprintf(
        command,
        sizeof(command),
        "AT+CMQTTCONNECT=0,\"%s\",60,1\r\n",
        MQTT_BROKER
    );

    Modem_ClearUart();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)command,
            strlen(command),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(
        response,
        0,
        sizeof(response)
    );

    Modem_Read(
        response,
        sizeof(response),
        30000U
    );

    if (strstr(
            response,
            "+CMQTTCONNECT: 0,0"
        ) == NULL)
    {
        return 0U;
    }

    return 1U;
}

uint8_t MQTT_Subscribe(void)
{
    char command[96];
    char response[1024];

    snprintf(
        command,
        sizeof(command),
        "AT+CMQTTSUB=0,%u,0\r\n",
        (unsigned int)strlen(MQTT_TOPIC)
    );

    Modem_ClearUart();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)command,
            strlen(command),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(
        response,
        0,
        sizeof(response)
    );

    Modem_Read(
        response,
        sizeof(response),
        5000U
    );

    if (strchr(
            response,
            '>'
        ) == NULL)
    {
        return 0U;
    }

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)MQTT_TOPIC,
            strlen(MQTT_TOPIC),
            2000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(
        response,
        0,
        sizeof(response)
    );

    Modem_Read(
        response,
        sizeof(response),
        15000U
    );

    if (strstr(
            response,
            "+CMQTTSUB: 0,0"
        ) == NULL)
    {
        return 0U;
    }

    return 1U;
}

uint8_t MQTT_Process(
    float *temperature
)
{
    static char mqtt_buffer[
        MQTT_RECEIVE_SIZE
    ];

    static uint16_t mqtt_position = 0U;

    uint8_t received_byte;
    float parsed_temperature = 0.0f;

    if (temperature == NULL)
    {
        return 0U;
    }

    while (HAL_UART_Receive(
               &huart1,
               &received_byte,
               1U,
               20U
           ) == HAL_OK)
    {
        if (mqtt_position <
            sizeof(mqtt_buffer) - 1U)
        {
            mqtt_buffer[mqtt_position] =
                (char)received_byte;

            mqtt_position++;

            mqtt_buffer[mqtt_position] =
                '\0';
        }
        else
        {
            mqtt_position = 0U;

            memset(
                mqtt_buffer,
                0,
                sizeof(mqtt_buffer)
            );
        }

        if (strstr(
                mqtt_buffer,
                "+CMQTTRXEND:"
            ) != NULL)
        {
            if (MQTT_ExtractTemperature(
                    mqtt_buffer,
                    &parsed_temperature
                ))
            {
                *temperature =
                    parsed_temperature;

                mqtt_position = 0U;

                memset(
                    mqtt_buffer,
                    0,
                    sizeof(mqtt_buffer)
                );

                return 1U;
            }

            mqtt_position = 0U;

            memset(
                mqtt_buffer,
                0,
                sizeof(mqtt_buffer)
            );

            return 0U;
        }

        if (strstr(
                mqtt_buffer,
                "+CMQTTCONNLOST:"
            ) != NULL)
        {
            mqtt_position = 0U;

            memset(
                mqtt_buffer,
                0,
                sizeof(mqtt_buffer)
            );

            return 0U;
        }
    }

    return 0U;
}

static uint8_t IsDigit(
    char character
)
{
    return (
        (character >= '0') &&
        (character <= '9')
    );
}

static uint8_t ParseFloatManual(
    const char *start,
    const char *end,
    float *value
)
{
    const char *position;

    float result = 0.0f;
    float fraction_multiplier = 0.1f;

    uint8_t negative = 0U;
    uint8_t found_digit = 0U;
    uint8_t decimal_found = 0U;

    if ((start == NULL) ||
        (end == NULL) ||
        (value == NULL) ||
        (start >= end))
    {
        return 0U;
    }

    position = start;

    if (*position == '-')
    {
        negative = 1U;
        position++;
    }
    else if (*position == '+')
    {
        position++;
    }

    while (position < end)
    {
        char character =
            *position;

        if (IsDigit(character))
        {
            uint8_t digit =
                (uint8_t)(
                    character - '0'
                );

            found_digit = 1U;

            if (!decimal_found)
            {
                result =
                    (result * 10.0f) +
                    (float)digit;
            }
            else
            {
                result +=
                    (float)digit *
                    fraction_multiplier;

                fraction_multiplier *=
                    0.1f;
            }
        }
        else if ((character == '.') ||
                 (character == ','))
        {
            if (decimal_found)
            {
                break;
            }

            decimal_found = 1U;
        }
        else
        {
            break;
        }

        position++;
    }

    if (!found_digit)
    {
        return 0U;
    }

    if (negative)
    {
        result = -result;
    }

    if ((result < -100.0f) ||
        (result > 150.0f))
    {
        return 0U;
    }

    *value = result;

    return 1U;
}

static uint8_t MQTT_ExtractTemperature(
    const char *buffer,
    float *temperature
)
{
    const char *payload_marker;
    const char *payload_area;
    const char *rx_end;
    const char *number_start;

    if ((buffer == NULL) ||
        (temperature == NULL))
    {
        return 0U;
    }

    payload_marker = strstr(
        buffer,
        "+CMQTTRXPAYLOAD"
    );

    if (payload_marker == NULL)
    {
        return 0U;
    }

    rx_end = strstr(
        payload_marker,
        "+CMQTTRXEND"
    );

    if (rx_end == NULL)
    {
        return 0U;
    }

    payload_area = strchr(
        payload_marker,
        '\n'
    );

    if (payload_area != NULL)
    {
        payload_area++;
    }
    else
    {
        payload_area = strchr(
            payload_marker,
            ':'
        );

        if (payload_area == NULL)
        {
            return 0U;
        }

        payload_area++;
    }

    if (payload_area >= rx_end)
    {
        return 0U;
    }

    number_start =
        payload_area;

    while (number_start < rx_end)
    {
        if (IsDigit(
                *number_start
            ))
        {
            break;
        }

        if (((*number_start == '-') ||
             (*number_start == '+')) &&
            ((number_start + 1) < rx_end) &&
            IsDigit(
                *(number_start + 1)
            ))
        {
            break;
        }

        number_start++;
    }

    if (number_start >= rx_end)
    {
        return 0U;
    }

    return ParseFloatManual(
        number_start,
        rx_end,
        temperature
    );
}