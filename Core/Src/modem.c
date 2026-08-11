#include "modem.h"
#include "usart.h"

#include <string.h>

#define MODEM_RESPONSE_SIZE 1024U

uint8_t Modem_InitNetwork(void)
{
    char response[MODEM_RESPONSE_SIZE];

    Modem_ClearUart();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+CPIN?\r\n",
            strlen("AT+CPIN?\r\n"),
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
        3000U
    );

    if (strstr(
            response,
            "+CPIN: READY"
        ) == NULL)
    {
        return 0U;
    }

    if (!Modem_SendCommand(
            "AT+CGDCONT=1,\"IP\",\"internet\"\r\n",
            "OK",
            5000U
        ))
    {
        return 0U;
    }

    Modem_SendCommand(
        "AT+NETOPEN\r\n",
        NULL,
        15000U
    );

    HAL_Delay(1000U);

    Modem_ClearUart();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+NETOPEN?\r\n",
            strlen("AT+NETOPEN?\r\n"),
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

    if ((strstr(
             response,
             "+NETOPEN: 1"
         ) == NULL) &&
        (strstr(
             response,
             "+NETOPEN:1"
         ) == NULL))
    {
        return 0U;
    }

    return 1U;
}

uint8_t Modem_SendCommand(
    const char *command,
    const char *expected,
    uint32_t timeout_ms
)
{
    char response[MODEM_RESPONSE_SIZE];

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
        timeout_ms
    );

    if (expected == NULL)
    {
        return 1U;
    }

    return strstr(
        response,
        expected
    ) != NULL;
}

uint16_t Modem_Read(
    char *buffer,
    uint16_t buffer_size,
    uint32_t timeout_ms
)
{
    uint8_t received_byte;
    uint16_t position = 0U;
    uint32_t start_time;

    if ((buffer == NULL) ||
        (buffer_size < 2U))
    {
        return 0U;
    }

    start_time = HAL_GetTick();

    while ((HAL_GetTick() - start_time) <
           timeout_ms)
    {
        if (HAL_UART_Receive(
                &huart1,
                &received_byte,
                1U,
                50U
            ) == HAL_OK)
        {
            if (position <
                buffer_size - 1U)
            {
                buffer[position] =
                    (char)received_byte;

                position++;

                buffer[position] = '\0';
            }
        }
    }

    buffer[position] = '\0';

    return position;
}

void Modem_ClearUart(void)
{
    uint8_t received_byte;

    while (HAL_UART_Receive(
               &huart1,
               &received_byte,
               1U,
               10U
           ) == HAL_OK)
    {
    }

    __HAL_UART_CLEAR_OREFLAG(
        &huart1
    );
}