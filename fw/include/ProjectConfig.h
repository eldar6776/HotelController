/**
 ******************************************************************************
 * @file    ProjectConfig.h
 * @author  Gemini & [Vase Ime]
 * @brief   Centralna konfiguracija za Hotel Controller ESP32
 * * @note    Pinout ažuriran na v11. Strategija skladištenja ostaje uSD kartica.
 ******************************************************************************
 */

#pragma once

//=============================================================================
// 1. HARDVERSKA MAPA PINOVA (v11 - Preuzeto iz README.md)
//=============================================================================

// --- RS485 INTERFEJS (Serial2) ---
#define RS485_RX_PIN        5    //
#define RS485_TX_PIN        17   //
#define RS485_DE_PIN        33   //

// --- I2C INTERFEJS (EEPROM) ---
#define I2C_SDA_PIN         14   //
#define I2C_SCL_PIN         15   //
#define EEPROM_I2C_ADDR     0x50

// --- SPI INTERFEJS (uSD kartica - Koristeći v11 pinove za Flash) ---
#define SPI_SCK_PIN         12   //
#define SPI_MOSI_PIN        4    //
#define SPI_MISO_PIN        36   //
#define SPI_FLASH_CS_PIN    32   // (Koristi se za CS pin uSD kartice)

// --- Ethernet (ETH) ---
#define ETH_MDC_PIN         23
#define ETH_MDIO_PIN        12   // Konflikt sa SPI_SCK_PIN (Upravlja se softverski)
#define ETH_POWER_PIN       16
#define ETH_PHY_ADDR        1
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN //

// --- OSTALO ---
#define STATUS_LED_PIN      2    //
#define WIFI_RST_BTN_PIN    39   //
#define SERIAL_DEBUG_BAUDRATE 115200

//=============================================================================
// 2. GLOBALNE KONSTANTE SISTEMA (Nepromijenjeno)
//=============================================================================

// --- RS485 Protokol ---
#define RS485_BAUDRATE              115200
#define MAX_PACKET_LENGTH           256    
#define RS485_BUFFER_SIZE           MAX_PACKET_LENGTH
#define RS485_TIMEOUT_MS            300
#define RS485_RESP_TOUT_MS          45 // Originalna vrednost: RESP_TOUT (45U)

// --- Polling i Logovanje ---
#define MAX_ADDRESS_LIST_SIZE       500
#define LOG_ENTRY_SIZE              20
#define MAX_LOG_ENTRIES             512
#define STATUS_BYTE_VALID           0x55
#define STATUS_BYTE_EMPTY           0xFF
#define LOG_RECORD_SIZE             (LOG_ENTRY_SIZE + 1)

// --- TimeSync / NTP ---
#define TIME_BROADCAST_INTERVAL_MS  6789 // Originalna vrednost: RTC_UPD_TIME (6789U)
#define TIMEZONE_STRING             "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define NTP_SERVER_1                "hr.pool.ntp.org"
#define NTP_SERVER_2                "ba.pool.ntp.org"

// --- Update Manager ---
#define MAX_UPDATE_RETRIES          30 // Originalna vrednost: MAXREP_CNT (30U)
#define UPDATE_PACKET_TIMEOUT_MS    5000
#define UPDATE_DATA_CHUNK_SIZE      128
#define APP_START_DEL               12345 // Originalna vrednost: APP_START_DEL (12345U)

// --- Ping Watchdog ---
#define PING_INTERVAL_MS            60000
#define MAX_PING_FAILURES           10

//=============================================================================
// 3. MEMORIJSKA MAPA EEPROM-a (DINAMIČKA - Nepromijenjeno)
//=============================================================================

#define EEPROM_CONFIG_START_ADDR        0x0000
#define EEPROM_CONFIG_SIZE              256
#define EEPROM_ADDRESS_LIST_START_ADDR  (EEPROM_CONFIG_START_ADDR + EEPROM_CONFIG_SIZE)
#define EEPROM_ADDRESS_LIST_SIZE        (MAX_ADDRESS_LIST_SIZE * sizeof(uint16_t))
#define EEPROM_LOG_START_ADDR           (EEPROM_ADDRESS_LIST_START_ADDR + EEPROM_ADDRESS_LIST_SIZE)
#define EEPROM_LOG_AREA_SIZE            (MAX_LOG_ENTRIES * LOG_RECORD_SIZE)

//=============================================================================
// 4. FILESYSTEM PATHS (uSD kartica - Nepromijenjeno)
//=============================================================================

// Firmware fajlovi
#define PATH_FW_RC          "/NEW.BIN"
#define PATH_BLDR_RC        "/BOOTLOADER.BIN"
#define PATH_FW_TH          "/TH_FW.BIN"
#define PATH_BLDR_TH        "/TH_BL.BIN"

// Slike
#define PATH_LOGO           "/LOGO.RAW"
// IMG1.RAW do IMG14.RAW (generiše se dinamički)

// Konfiguracija
#define PATH_CTRL_ADD_LIST  "/CTRL_ADD.TXT"
#define PATH_UPDATE_CFG     "/UPDATE.CFG"

//=============================================================================
// 5. NAPOMENE ZA RAZVOJ (Ažurirano)
//=============================================================================
//
// - AŽURIRANO: Mapa pinova (v11).
// - ZADRŽANO: Strategija skladištenja na uSD kartici (FAT32).
// - UPOZORENJE: GPIO12 (ETH_MDIO) i GPIO12 (SPI_SCK) su u konfliktu.
//