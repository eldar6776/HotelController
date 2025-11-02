/**
 ******************************************************************************
 * @file    ProjectConfig.h
 * @author  Gemini & [Vase Ime]
 * @brief   Centralna konfiguracija za Hotel Controller ESP32
 * 
 * @note    UPDATED: Dinamička EEPROM mapa i povećana lista adresa na 500
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

// --- SPI INTERFEJS (uSD kartica) ---
#define SPI_SCK_PIN         14
#define SPI_MISO_PIN        19
#define SPI_MOSI_PIN        18
#define SPI_FLASH_CS_PIN    23  // Koristi se za CS pin uSD kartice

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
#define SERIAL_DEBUG_BAUDRATE 115200

//=============================================================================
// 2. GLOBALNE KONSTANTE SISTEMA
//=============================================================================

// --- RS485 Protokol ---
#define RS485_BAUDRATE              115200
#define MAX_PACKET_LENGTH           256    
#define RS485_BUFFER_SIZE           MAX_PACKET_LENGTH
#define RS485_TIMEOUT_MS            300
#define RS485_RESP_TOUT_MS          RS485_TIMEOUT_MS

// --- Polling i Logovanje ---
#define MAX_ADDRESS_LIST_SIZE       500  // UPDATED: Povećano sa 64 na 500
#define LOG_ENTRY_SIZE              20
#define MAX_LOG_ENTRIES             512
#define STATUS_BYTE_VALID           0x55
#define STATUS_BYTE_EMPTY           0xFF

// Jedan zapis u EEPROM log oblasti = status byte + LOG_ENTRY_SIZE
#define LOG_RECORD_SIZE             (LOG_ENTRY_SIZE + 1)

// --- TimeSync / NTP ---
#define TIME_BROADCAST_INTERVAL_MS  3600000
#define TIMEZONE_STRING             "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define NTP_SERVER_1                "hr.pool.ntp.org"
#define NTP_SERVER_2                "ba.pool.ntp.org"

// --- Update Manager ---
#define MAX_UPDATE_RETRIES          3
#define UPDATE_PACKET_TIMEOUT_MS    5000
#define UPDATE_DATA_CHUNK_SIZE      128

// --- Ping Watchdog ---
#define PING_INTERVAL_MS            60000
#define MAX_PING_FAILURES           10

//=============================================================================
// 3. MEMORIJSKA MAPA EEPROM-a (DINAMIČKA - UPDATED)
//=============================================================================

// Struktura AppConfig (definisana u EepromStorage.h)
// Za kalkulaciju veličine koristimo sizeof(AppConfig) u runtime-u

// FIKSNA POČETNA ADRESA
#define EEPROM_CONFIG_START_ADDR        0x0000

// DINAMIČKA KALKULACIJA ADRESA (kompajler će izračunati u compile-time)
// NAPOMENA: sizeof(AppConfig) = ~20 bytes, ali koristimo 256 za rezervu

#define EEPROM_CONFIG_SIZE                  256  // Rezerva za buduća proširenja

// Lista adresa slijedi odmah poslije konfiguracije
#define EEPROM_ADDRESS_LIST_START_ADDR      (EEPROM_CONFIG_START_ADDR + EEPROM_CONFIG_SIZE)
#define EEPROM_ADDRESS_LIST_SIZE            (MAX_ADDRESS_LIST_SIZE * sizeof(uint16_t))  // 500 * 2 = 1000 bytes

// Log oblast slijedi odmah poslije liste adresa
#define EEPROM_LOG_START_ADDR               (EEPROM_ADDRESS_LIST_START_ADDR + EEPROM_ADDRESS_LIST_SIZE)
#define EEPROM_LOG_AREA_SIZE                (MAX_LOG_ENTRIES * LOG_RECORD_SIZE)  // 512 * 21 = 10752 bytes

// UKUPNA KORIŠTENA MEMORIJA: 256 + 1000 + 10752 = 12008 bytes (~12KB od 128KB)

//=============================================================================
// 4. FILESYSTEM PATHS (uSD kartica)
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
// 5. NAPOMENE ZA RAZVOJ
//=============================================================================

// OBRISANE KONSTANTE (Više se ne koriste):
// - SLOT_SIZE_* (sve Flash slot konstante)
// - SLOT_ADDR_* (sve Flash adrese)
// - VERS_INF_OFFSET (metadata offset u Flash-u)
// 
// RAZLOG: Prešli smo sa SPI Flash-a na FAT32 filesystem (uSD kartica)