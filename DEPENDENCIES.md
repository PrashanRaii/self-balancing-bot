# Project Dependencies

This document lists the software and libraries needed to compile and upload the project on another computer.

## Required software

| Requirement | Notes |
|---|---|
| Arduino IDE | Used to open, compile, and upload the sketch |
| ESP32 board package | Provides ESP32 board support and the ESP32-specific libraries |
| USB driver for the ESP32 board | May be required by some computers to detect the board |

## ESP32 board package

Install the ESP32 board package through the Arduino IDE Boards Manager.

In Arduino IDE:

1. Open **Tools → Board → Boards Manager**.
2. Search for `esp32`.
3. Install the ESP32 package published by **Espressif Systems**.
4. Select the appropriate ESP32 board under **Tools → Board**.

The project was developed for an ESP32 DevKit V1. The exact board name may appear slightly differently depending on the installed ESP32 package version.

## External Arduino library

Install this library through the Arduino IDE Library Manager:

| Library | Header used by the project | Publisher / maintainer |
|---|---|---|
| WebSockets | `WebSocketsServer.h` | Markus Sattler / Links2004 |

In Arduino IDE:

1. Open **Sketch → Include Library → Manage Libraries**.
2. Search for `WebSockets`.
3. Install **WebSockets by Markus Sattler**.

## Libraries provided by the ESP32 package

The following headers are used by the project but do not need separate installation:

| Header | Purpose |
|---|---|
| `Arduino.h` | Core Arduino functions and types |
| `Wire.h` | I2C communication with the MPU6050 |
| `WiFi.h` | ESP32 Wi-Fi access point |
| `WebServer.h` | HTTP web server |
| `freertos/FreeRTOS.h` | ESP32 real-time operating system support |
| `freertos/task.h` | ESP32 task functions |

## Quick setup checklist

On a new computer:

- [ ] Install Arduino IDE.
- [ ] Install the ESP32 board package by Espressif Systems.
- [ ] Install WebSockets by Markus Sattler.
- [ ] Open `Self_Balancing_Bot/Self_Balancing_Bot.ino`.
- [ ] Select the correct ESP32 board and USB port.
- [ ] Compile the sketch before uploading it to the robot.

## Important note

Arduino does not have a universal dependency file like Python's `requirements.txt`. This document serves the same practical purpose for this project, but the listed board package and library must be installed manually through the Arduino IDE.
