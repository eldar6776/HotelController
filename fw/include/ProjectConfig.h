/**
 ******************************************************************************
 * @file    ProjectConfig.h
 * @author  Gemini & [Vase Ime]
 * @brief   Centralna konfiguracija za Hotel Controller ESP32
 ******************************************************************************
 */

#pragma once

//=============================================================================
// 1. HARDVERSKA MAPA PINOVA (v11 - WT32-ETH01)
//=============================================================================

// --- RS485 INTERFEJS (Serial2) ---
#define RS485_RX_PIN        5
#define RS485_TX_PIN        17
#define RS485_DE_PIN        33

// --- I2C INTERFEJS (EEPROM) ---
#define I2C_SDA_PIN         21
#define I2C_SCL_PIN         22
#define EEPROM_I2C_ADDR     0x50

// --- SPI INTERFEJS (Eksterni Flash - Pinovi koji su nedostajali u main.cpp) ---
#define SPI_SCK_PIN         14
#define SPI_MISO_PIN        19
#define SPI_MOSI_PIN        18
#define SPI_FLASH_CS_PIN    23

// --- Ethernet (ETH) ---
#define ETH_MDC_PIN         23  // Konflikt sa Flash CS (Upravlja se softverski)
#define ETH_MDIO_PIN        12
#define ETH_POWER_PIN       16
#define ETH_PHY_ADDR        1
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN

// --- OSTALO ---
#define STATUS_LED_PIN      2
#define WIFI_RST_BTN_PIN    32
#define SERIAL_DEBUG_BAUDRATE 115200 // Rjesava gresku u main.cpp

//=============================================================================
// 2. GLOBALNE KONSTANTE SISTEMA (Rjesavanje svih 'was not declared' errora)
//=============================================================================

// --- RS485 Protokol ---
#define RS485_BAUDRATE              115200
#define MAX_PACKET_LENGTH           256    
#define RS485_BUFFER_SIZE           MAX_PACKET_LENGTH   // Rjesava gresku u Rs485Service.h
#define RS485_TIMEOUT_MS            300
#define RS485_RESP_TOUT_MS          RS485_TIMEOUT_MS    // Rjesava gresku u Rs485Service.cpp

// --- Polling i Logovanje ---
#define MAX_ADDRESS_LIST_SIZE       64                  // Rjesava gresku u LogPullManager.h
#define LOG_ENTRY_SIZE              20                  // Fiksna velicina za LogEntry (rjesava buffer overflow warning)
#define MAX_LOG_ENTRIES             512
#define STATUS_BYTE_VALID           0x55                // Rjesava gresku u EepromStorage.cpp
#define STATUS_BYTE_EMPTY           0xFF                // Rjesava gresku u EepromStorage.cpp

// Jedan zapis u EEPROM log oblasti = status byte + LOG_ENTRY_SIZE
#define LOG_RECORD_SIZE              (LOG_ENTRY_SIZE + 1)

// --- TimeSync / NTP ---
#define TIME_BROADCAST_INTERVAL_MS  3600000             // Rjesava gresku u TimeSync.cpp
#define TIMEZONE_STRING             "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define NTP_SERVER_1                "hr.pool.ntp.org"
#define NTP_SERVER_2                "ba.pool.ntp.org"

// --- Update Manager ---
#define MAX_UPDATE_RETRIES          3                   // Rjesava gresku u UpdateManager.cpp
#define UPDATE_PACKET_TIMEOUT_MS    5000                // Rjesava gresku u UpdateManager.cpp
#define UPDATE_DATA_CHUNK_SIZE      128

// --- Ping Watchdog ---
#define PING_INTERVAL_MS            60000

//=============================================================================
// 3. MEMORIJSKA MAPA EEPROM-a
//=============================================================================

#define EEPROM_CONFIG_START_ADDR        0x0000
#define EEPROM_ADDRESS_LIST_START_ADDR  0x0100
#define EEPROM_ADDRESS_LIST_SIZE        (MAX_ADDRESS_LIST_SIZE * sizeof(uint16_t))
#define EEPROM_LOG_START_ADDR           0x0200      // Rjesava gresku u EepromStorage.cpp
#define EEPROM_LOG_AREA_SIZE            (MAX_LOG_ENTRIES * LOG_RECORD_SIZE)

//=============================================================================
// 4. MEMORIJSKA MAPA SPI FLASH-a (16MB)
//=============================================================================

#define SLOT_SIZE_128K          (128 * 1024)
#define SLOT_SIZE_1M            (1024 * 1024)
#define SLOT_SIZE_12M           (12 * 1024 * 1024)

#define SLOT_ADDR_FW_RC         0x00000000 // Slot za FW Kontrolera Sobe (128K)
#define SLOT_ADDR_BL_RC         0x00020000 // Slot za BL Kontrolera Sobe (64K)
#define SLOT_ADDR_FW_TH_1M      0x00030000 // Slot za FW Termostata (1MB)
#define SLOT_ADDR_BL_TH         0x00130000 // Slot za BL Termostata (64K)
#define SLOT_ADDR_IMG_LOGO      0x00140000 // Slot za Logo (128K)
#define SLOT_ADDR_IMG_RC_START  0x00160000 // Pocetak bloka za 14 slika (3.5MB)
#define SLOT_ADDR_QSPI_ER_12M   0x00500000 // Veliki slot od 12MB za ER_QSPI1 fajl