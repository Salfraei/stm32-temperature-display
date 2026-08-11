# STM32 LTE MQTT Temperature Display

STM32F411CEU6-based device that receives temperature values from an MQTT broker through an LTE modem and displays the received temperature on a Nokia 5110 LCD.

The project is the receiving part of a remote temperature monitoring system. Temperature measurements are published by a separate ESP32 device and received by the STM32 through MQTT over an LTE connection.

## Hardware

- STM32F411CEU6 Black Pill
- A7670E Cat-1 LTE modem
- Nokia 5110 LCD
- SIM card with mobile data

## Software

- C
- STM32 HAL
- CMake
- Visual Studio Code

## System Architecture

```text
DS18B20
   |
   | 1-Wire
   v
 ESP32
   |
   | Wi-Fi
   v
MQTT Broker
   |
   | LTE
   v
 A7670E
   |
   | UART
   v
STM32F411CEU6
   |
   | SPI
   v
Nokia 5110
```

The ESP32 measures the room temperature using a DS18B20 sensor and publishes the value to an MQTT broker over Wi-Fi.

The STM32 communicates with the A7670E LTE modem using UART. The modem connects to the same MQTT broker through the mobile network. The STM32 subscribes to the temperature topic, receives the MQTT payload, extracts and stores the temperature value, and displays it on the Nokia 5110 LCD.

## MQTT Configuration

Broker:

```text
broker.emqx.io:1883
```

Topic:

```text
pavlo/esp32/room1
```

Example payload:

```text
29.19
```

The ESP32 publishes temperature values to this topic, while the STM32 subscribes to the same topic.

## A7670E Connection

The STM32 communicates with the A7670E LTE modem using USART1.

| STM32F411CEU6 | A7670E |
|---|---|
| PA9 (USART1 TX) | RX |
| PA10 (USART1 RX) | TX |
| GND | GND |

UART configuration:

```text
Baud rate: 115200
Data bits: 8
Stop bits: 1
Parity: None
Flow control: None
```

The modem is controlled using AT commands. It establishes the mobile data connection and handles communication with the MQTT broker.

## Nokia 5110 Connection

The Nokia 5110 LCD is connected to the STM32 through SPI1.

| Nokia 5110 | STM32F411CEU6 |
|---|---|
| CLK | PA5 |
| DIN | PA7 |
| DC | PB0 |
| SCE / CS | PB1 |
| RST | PB10 |
| VCC | 3.3V |
| GND | GND |

The display is used to show connection status during startup and the received temperature during normal operation.

## Operation

After startup, the STM32 performs the following sequence:

1. Initializes the STM32 peripherals.
2. Initializes the Nokia 5110 display.
3. Initializes communication with the A7670E modem.
4. Checks whether the SIM card is ready.
5. Configures and opens the LTE data connection.
6. Starts the MQTT service on the modem.
7. Connects to the MQTT broker.
8. Subscribes to `pavlo/esp32/room1`.
9. Waits for temperature messages.
10. Extracts and stores the received temperature value.
11. Displays the temperature on the Nokia 5110.

Example display output:

```text
Temperature

29.19 C
```

## Communication Flow

```text
ESP32
  |
  | MQTT Publish
  | Topic: pavlo/esp32/room1
  v
broker.emqx.io
  |
  | MQTT Subscribe
  v
A7670E
  |
  | UART / AT commands
  v
STM32F411CEU6
  |
  | SPI
  v
Nokia 5110
```


Additional modules can be used to separate LTE modem and MQTT functionality from the main application logic.

## Build

The project uses CMake and can be built from the Visual Studio Code terminal.

```powershell
cmake --build --preset Debug
```

After a successful build, the generated firmware can be flashed to the STM32F411CEU6 using the configured STM32 programming/debugging tools.

## Repository

This repository contains the STM32 receiving device of the remote temperature monitoring system.

The ESP32 transmitter is maintained in a separate repository:

https://github.com/Salfraei/esp32-temperature-node