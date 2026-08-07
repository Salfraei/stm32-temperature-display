#include "main.h"
#include "spi.h"
#include "usart.h"
#include "lcd5110.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MQTT_BROKER "tcp://broker.emqx.io:1883"
#define MQTT_CLIENT_ID "stm32-pavlo-a7670e-room1"
#define MQTT_TOPIC "pavlo/esp32/room1"

#define MODEM_RESPONSE_SIZE 1024U
#define MQTT_RECEIVE_SIZE 2048U

static volatile float g_received_temperature = 0.0f;
static volatile uint8_t g_temperature_valid = 0U;

static void SystemClock_Config(void);

static void LED_Init(void);
static void Display_GPIO_Init(void);
static void LED_On(void);
static void LED_Off(void);
static void LED_Blink(uint8_t count, uint32_t delay_ms);
static void Fatal_Error(uint8_t code);

static void UART_Clear(void);

static uint16_t Modem_Read(
    char *buffer,
    uint16_t buffer_size,
    uint32_t timeout_ms
);

static uint8_t Modem_SendCommand(
    const char *command,
    const char *expected,
    uint32_t timeout_ms
);

static uint8_t Modem_InitNetwork(void);
static uint8_t MQTT_Connect(void);
static uint8_t MQTT_Subscribe(void);
static void MQTT_Process(void);

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


/* Main */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    LED_Init();
    Display_GPIO_Init();

    MX_SPI1_Init();
    MX_USART1_UART_Init();

    LED_Off();

    /* Display initialization */

    Nokia5110_Init();
    Nokia5110_Clear();

    Nokia5110_SetCursor(6U, 1U);
    Nokia5110_WriteString("Starting");

    Nokia5110_SetCursor(6U, 3U);
    Nokia5110_WriteString("LTE MQTT");

    HAL_Delay(6000U);

    /* Modem check */

    if (!Modem_SendCommand(
            "AT\r\n",
            "OK",
            3000U
        ))
    {
        Nokia5110_Clear();
        Nokia5110_SetCursor(0U, 2U);
        Nokia5110_WriteString("Modem error");

        Fatal_Error(1U);
    }

    LED_Blink(1U, 500U);
    HAL_Delay(1000U);

    if (!Modem_SendCommand(
            "ATE0\r\n",
            "OK",
            3000U
        ))
    {
        Nokia5110_Clear();
        Nokia5110_SetCursor(0U, 2U);
        Nokia5110_WriteString("ATE0 error");

        Fatal_Error(2U);
    }

    /* LTE connection */

    Nokia5110_Clear();
    Nokia5110_SetCursor(3U, 2U);
    Nokia5110_WriteString("LTE connect");

    if (!Modem_InitNetwork())
    {
        Nokia5110_Clear();
        Nokia5110_SetCursor(0U, 2U);
        Nokia5110_WriteString("Network error");

        Fatal_Error(3U);
    }

    LED_Blink(3U, 500U);
    HAL_Delay(1000U);

    /* MQTT connection */

    Nokia5110_Clear();
    Nokia5110_SetCursor(3U, 2U);
    Nokia5110_WriteString("MQTT connect");

    if (!MQTT_Connect())
    {
        Nokia5110_Clear();
        Nokia5110_SetCursor(0U, 2U);
        Nokia5110_WriteString("MQTT error");

        Fatal_Error(4U);
    }

    LED_Blink(4U, 500U);
    HAL_Delay(1000U);

    /* MQTT subscription */

    Nokia5110_Clear();
    Nokia5110_SetCursor(3U, 2U);
    Nokia5110_WriteString("Subscribe");

    if (!MQTT_Subscribe())
    {
        Nokia5110_Clear();
        Nokia5110_SetCursor(0U, 2U);
        Nokia5110_WriteString("Sub error");

        Fatal_Error(5U);
    }

    LED_Blink(5U, 500U);
    HAL_Delay(500U);

    Nokia5110_Clear();
    Nokia5110_SetCursor(3U, 1U);
    Nokia5110_WriteString("Waiting");

    Nokia5110_SetCursor(8U, 3U);
    Nokia5110_WriteString("temperature");

    while (1)
    {
        MQTT_Process();
    }
}


/* LTE network */

static uint8_t Modem_InitNetwork(void)
{
    char response[MODEM_RESPONSE_SIZE];

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+CPIN?\r\n",
            strlen("AT+CPIN?\r\n"),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        3000U
    );

    if (strstr(response, "+CPIN: READY") == NULL)
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

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+NETOPEN?\r\n",
            strlen("AT+NETOPEN?\r\n"),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        5000U
    );

    if ((strstr(response, "+NETOPEN: 1") == NULL) &&
        (strstr(response, "+NETOPEN:1") == NULL))
    {
        return 0U;
    }

    return 1U;
}


/* MQTT connection */

static uint8_t MQTT_Connect(void)
{
    char command[192];
    char response[MODEM_RESPONSE_SIZE];

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

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)"AT+CMQTTSTART\r\n",
            strlen("AT+CMQTTSTART\r\n"),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        10000U
    );

    if ((strstr(response, "+CMQTTSTART: 0") == NULL) &&
        (strstr(response, "OK") == NULL))
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

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)command,
            strlen(command),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        30000U
    );

    if (strstr(response, "+CMQTTCONNECT: 0,0") == NULL)
    {
        return 0U;
    }

    return 1U;
}


/* MQTT subscription */

static uint8_t MQTT_Subscribe(void)
{
    char command[96];
    char response[MODEM_RESPONSE_SIZE];

    snprintf(
        command,
        sizeof(command),
        "AT+CMQTTSUB=0,%u,0\r\n",
        (unsigned int)strlen(MQTT_TOPIC)
    );

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)command,
            strlen(command),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        5000U
    );

    if (strchr(response, '>') == NULL)
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

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        15000U
    );

    if (strstr(response, "+CMQTTSUB: 0,0") == NULL)
    {
        return 0U;
    }

    return 1U;
}


/* MQTT receive */

static void MQTT_Process(void)
{
    static char mqtt_buffer[MQTT_RECEIVE_SIZE];
    static uint16_t mqtt_position = 0U;

    uint8_t received_byte;
    float parsed_temperature = 0.0f;

    while (HAL_UART_Receive(
               &huart1,
               &received_byte,
               1U,
               20U
           ) == HAL_OK)
    {
        if (mqtt_position < sizeof(mqtt_buffer) - 1U)
        {
            mqtt_buffer[mqtt_position] =
                (char)received_byte;

            mqtt_position++;

            mqtt_buffer[mqtt_position] = '\0';
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
                g_received_temperature =
                    parsed_temperature;

                g_temperature_valid = 1U;

                Nokia5110_ShowTemperature(
                    g_received_temperature
                );

                LED_Blink(2U, 500U);
            }
            else
            {
                Nokia5110_Clear();

                Nokia5110_SetCursor(0U, 1U);
                Nokia5110_WriteString(
                    "Payload error"
                );

                Nokia5110_SetCursor(0U, 3U);
                Nokia5110_WriteString(
                    "No number"
                );

                LED_Blink(4U, 150U);
            }

            mqtt_position = 0U;

            memset(
                mqtt_buffer,
                0,
                sizeof(mqtt_buffer)
            );

            break;
        }

        if (strstr(
                mqtt_buffer,
                "+CMQTTCONNLOST:"
            ) != NULL)
        {
            Nokia5110_Clear();

            Nokia5110_SetCursor(0U, 1U);
            Nokia5110_WriteString("MQTT lost");

            Nokia5110_SetCursor(0U, 3U);
            Nokia5110_WriteString(
                "Restart board"
            );

            LED_Blink(7U, 150U);

            mqtt_position = 0U;

            memset(
                mqtt_buffer,
                0,
                sizeof(mqtt_buffer)
            );

            break;
        }
    }

    HAL_Delay(10U);
}


/* Temperature parsing */

static uint8_t IsDigit(char character)
{
    return ((character >= '0') &&
            (character <= '9'));
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
        char character = *position;

        if (IsDigit(character))
        {
            uint8_t digit =
                (uint8_t)(character - '0');

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

                fraction_multiplier *= 0.1f;
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

    number_start = payload_area;

    while (number_start < rx_end)
    {
        if (IsDigit(*number_start))
        {
            break;
        }

        if (((*number_start == '-') ||
             (*number_start == '+')) &&
            ((number_start + 1) < rx_end) &&
            IsDigit(*(number_start + 1)))
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


/* Modem communication */

static uint8_t Modem_SendCommand(
    const char *command,
    const char *expected,
    uint32_t timeout_ms
)
{
    char response[MODEM_RESPONSE_SIZE];

    UART_Clear();

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)command,
            strlen(command),
            1000U
        ) != HAL_OK)
    {
        return 0U;
    }

    memset(response, 0, sizeof(response));

    Modem_Read(
        response,
        sizeof(response),
        timeout_ms
    );

    if (expected == NULL)
    {
        return 1U;
    }

    return strstr(response, expected) != NULL;
}


static uint16_t Modem_Read(
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
            if (position < buffer_size - 1U)
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


static void UART_Clear(void)
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

    __HAL_UART_CLEAR_OREFLAG(&huart1);
}


/* LED */

static void Fatal_Error(uint8_t code)
{
    while (1)
    {
        LED_Blink(code, 150U);
        HAL_Delay(1500U);
    }
}


static void LED_Blink(
    uint8_t count,
    uint32_t delay_ms
)
{
    uint8_t index;

    for (index = 0U;
         index < count;
         index++)
    {
        LED_On();
        HAL_Delay(delay_ms);

        LED_Off();
        HAL_Delay(delay_ms);
    }
}


static void LED_On(void)
{
    HAL_GPIO_WritePin(
        GPIOC,
        GPIO_PIN_13,
        GPIO_PIN_RESET
    );
}


static void LED_Off(void)
{
    HAL_GPIO_WritePin(
        GPIOC,
        GPIO_PIN_13,
        GPIO_PIN_SET
    );
}


static void LED_Init(void)
{
    GPIO_InitTypeDef gpio_config = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();

    HAL_GPIO_WritePin(
        GPIOC,
        GPIO_PIN_13,
        GPIO_PIN_SET
    );

    gpio_config.Pin = GPIO_PIN_13;
    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_NOPULL;
    gpio_config.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOC,
        &gpio_config
    );
}


/* Nokia 5110 GPIO */

static void Display_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio_config = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_1,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_10,
        GPIO_PIN_SET
    );

    gpio_config.Pin =
        GPIO_PIN_0 |
        GPIO_PIN_1 |
        GPIO_PIN_10;

    gpio_config.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_config.Pull = GPIO_NOPULL;
    gpio_config.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOB,
        &gpio_config
    );
}


/* System clock */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef oscillator_config = {0};
    RCC_ClkInitTypeDef clock_config = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    oscillator_config.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    oscillator_config.HSEState =
        RCC_HSE_ON;

    oscillator_config.PLL.PLLState =
        RCC_PLL_ON;

    oscillator_config.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    oscillator_config.PLL.PLLM = 25;
    oscillator_config.PLL.PLLN = 200;
    oscillator_config.PLL.PLLP =
        RCC_PLLP_DIV2;
    oscillator_config.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(
            &oscillator_config
        ) != HAL_OK)
    {
        Error_Handler();
    }

    clock_config.ClockType =
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    clock_config.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    clock_config.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    clock_config.APB1CLKDivider =
        RCC_HCLK_DIV2;

    clock_config.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &clock_config,
            FLASH_LATENCY_3
        ) != HAL_OK)
    {
        Error_Handler();
    }
}


void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}