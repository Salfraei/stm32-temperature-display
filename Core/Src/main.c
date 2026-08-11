#include "main.h"
#include "spi.h"
#include "usart.h"
#include "lcd5110.h"
#include "modem.h"
#include "mqtt_manager.h"

#include <stdint.h>

static volatile float g_received_temperature = 0.0f;
static volatile uint8_t g_temperature_valid = 0U;

static void SystemClock_Config(void);

static void LED_Init(void);
static void Display_GPIO_Init(void);

static void LED_On(void);
static void LED_Off(void);

static void LED_Blink(
    uint8_t count,
    uint32_t delay_ms
);

static void Fatal_Error(uint8_t code);


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

    /* Wait for A7670E startup */

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

    /* Main loop */

    while (1)
    {
        float new_temperature = 0.0f;

        if (MQTT_Process(&new_temperature))
        {
            g_received_temperature =
                new_temperature;

            g_temperature_valid = 1U;

            Nokia5110_ShowTemperature(
                g_received_temperature
            );

            LED_Blink(2U, 500U);
        }

        HAL_Delay(10U);
    }
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

    gpio_config.Pin =
        GPIO_PIN_13;

    gpio_config.Mode =
        GPIO_MODE_OUTPUT_PP;

    gpio_config.Pull =
        GPIO_NOPULL;

    gpio_config.Speed =
        GPIO_SPEED_FREQ_LOW;

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

    gpio_config.Mode =
        GPIO_MODE_OUTPUT_PP;

    gpio_config.Pull =
        GPIO_NOPULL;

    gpio_config.Speed =
        GPIO_SPEED_FREQ_LOW;

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