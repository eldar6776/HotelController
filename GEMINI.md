# GEMINI.md - Hotel Controller Project

This document provides a comprehensive overview of the Hotel Controller project, intended to be used as a quick-start guide and instructional context for development.

## 1. Project Overview

This project is a complete firmware migration of a "Hotel Controller" system from an older STM32F429 platform to a modern **ESP32 (WT32-ETH01)** board. The primary goal is to achieve this migration while maintaining **100% backward compatibility** with the existing RS485 communication protocol and HTTP CGI command interface.

The original monolithic "superloop" architecture has been refactored into a modular, multi-tasking system based on **FreeRTOS**. Each core functionality is encapsulated in a dedicated "Manager" service that runs as an independent task.

- **Platform**: ESP32 (WT32-ETH01)
- **Framework**: Arduino, managed by PlatformIO
- **RTOS**: FreeRTOS
- **Primary Technologies**:
    - **Networking**: Ethernet (primary), Wi-Fi (backup) via `AsyncWebServer`.
    - **Communication**: RS485, I2C, SPI, UART.
    - **Storage**: I2C EEPROM for configuration/logs, and external SPI Flash for large firmware update files.

## 2. Building and Running

The project is configured to be built and managed using [PlatformIO](https://platformio.org/).

- **Build the project:**
  ```shell
  pio run
  ```
- **Upload the firmware to the device:**
  (Ensure `upload_port` in `platformio.ini` is set to the correct COM port)
  ```shell
  pio run -t upload
  ```
- **Monitor serial output:**
  (Ensure `monitor_port` in `platformio.ini` is set correctly)
  ```shell
  pio device monitor
  ```
- **Run tests:**
  (Note: No tests are currently defined in the `test` directory)
  ```shell
  pio test
  ```

## 3. Software Architecture & Key Modules

The firmware is divided into several independent services, each managing a specific aspect of the system.

- **`main.cpp`**: The entry point of the application. It initializes all manager modules and starts the FreeRTOS scheduler. The main `loop()` is kept non-blocking.
- **`NetworkManager`**: Manages the Ethernet and Wi-Fi connections, including NTP time synchronization.
- **`Rs485Service`**: The core of the application. It's a dispatcher task that serializes access to the RS485 bus, validates packets, and routes them to the appropriate manager.
- **`HttpServer`**: Runs an `AsyncWebServer` to handle incoming HTTP requests. It replicates the legacy CGI command interface documented in `doc/Procitaj !!!.txt`.
- **`HttpQueryManager`**: Implements the blocking logic required to handle HTTP requests that need a response from an RS485 device. It uses FreeRTOS semaphores to synchronize between the HTTP server and the RS485 service.
- **`EepromStorage` / `SdCardManager`**: Manages persistent storage for configuration, address lists, and event logs.
- **`UpdateManager`**: A state machine that handles the firmware update process for connected devices over the RS485 bus.

## 4. Core Functionality & Protocols

### HTTP CGI Interface

The system is controlled via a specific set of HTTP GET requests that mimic a legacy CGI interface. A **complete and detailed list of all supported commands** is documented in:

- **`doc/Procitaj !!!.txt`**

This file is the primary reference for all system control commands, including device state requests, configuration changes, and firmware updates. It also contains important information about default RS485 and One-Wire addresses.

### Development Workflow

Development is structured in two phases:

1.  **Phase 1: Core Development**: The device is connected via USB, and `Serial.println()` is used for debugging output to the VS Code serial monitor. The DWIN display is disconnected during this phase.
2.  **Phase 2: Graphics Integration**: The USB cable is disconnected, and a DWIN serial display is connected to the `TX0`/`RX0` pins. All `Serial.println()` output is automatically routed to the display.

## 5. Coding Conventions

The project follows a strict set of naming conventions to maintain code clarity and consistency.

| Category | Style (Format) | Example |
| :--- | :--- | :--- |
| **Files (Modules)** | `PascalCase` | `Rs485Service.h` |
| **Types (Classes, Structs)** | `PascalCase` | `class EepromStorage` |
| **Functions / Methods** | `PascalCase` | `void Initialize()` |
| **Global Variables** | `g_snake_case` | `g_rs485_task_handle` |
| **Class Member Variables**| `m_snake_case` | `uint16_t m_write_index;` |
| **Local Variables** | `camelCase` | `int localCounter` |
| **Constants / Macros** | `ALL_CAPS_SNAKE_CASE` | `RS485_DE_PIN` |
