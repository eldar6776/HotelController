/**
 ******************************************************************************
 * @file    ProjectConfig.h
 * @author  Gemini & [Vase Ime]
 * @brief   Centralna konfiguracija za Hotel Controller ESP32
 *
 * @note
 * Ovdje definiramo nasu zakljucanu hardversku mapu pinova (v11)
 * i fiksnu memorijsku mapu za EEPROM i SPI Flash,
 * kao i sve globalne konstante projekta.
 ******************************************************************************
 */

#pragma once // Osigurava da se fajl ukljuci samo jednom

//=============================================================================
// 1. HARDVERSKA MAPA PINOVA (v11 - DWIN + 1x Flash + LED)
//=============================================================================
// ----------------------------------------------------------------------------
// ZAUZETI SISTEMSKI PINOVI (NE KORISTITI!)
// ----------------------------------------------------------------------------
#define PIN_EMAC_CLK        0   // IO0 (Kriticki sat sa LAN8720A)

// ----------------------------------------------------------------------------
// RS485 INTERFEJS (Serial2)
// ----------------------------------------------------------------------------
#define RS485_RX_PIN        5   // IO5 (RXD) -> Serial2 RX
#define RS485_TX_PIN        17  // IO17 (TXD) -> Serial2 TX
#define RS485_DE_PIN        33  // IO33 (485_EN) -> Driver Enable

// ----------------------------------------------------------------------------
// I2C INTERFEJS (EEPROM 24C1024)
// ----------------------------------------------------------------------------
#define I2C_SDA_PIN         14  // IO14
#define I2C_SCL_PIN         15  // IO15

// ----------------------------------------------------------------------------
// DWIN DISPLEJ / FAZA 1 DEBUG PORT (Serial0)
// ----------------------------------------------------------------------------
#define DWIN_TX_PIN         1   // IO1 (TX0) -> Serial0 TX
#define DWIN_RX_PIN         3   // IO3 (RX0) -> Serial0 RX

// ----------------------------------------------------------------------------
// SPI INTERFEJS (ZA W25Q512 - 64MB Flash)
// ----------------------------------------------------------------------------
#define SPI_SCK_PIN         12  // IO12 (SCK)
#define SPI_MOSI_PIN        4   // IO4  (MOSI)
#define SPI_MISO_PIN        36  // IO36 (MISO) (Input-Only)
#define SPI_FLASH_CS_PIN    32  // IO32 (CFG) (Chip Select)

// ----------------------------------------------------------------------------
// OSTALI GPIO
// ----------------------------------------------------------------------------
#define STATUS_LED_PIN      2   // IO2 (On-board LED za dijagnostiku)
#define WLAN_RST_BTN_PIN    39  // IO39 (Input-Only pin za WiFi reset)

// ----------------------------------------------------------------------------
// VIRTUELNI PINOVI (Upravljani preko I2C Expandera)
// ----------------------------------------------------------------------------
#define VIRTUAL_LIGHT_PIN   254 // Relej za rasvjetu
#define VIRTUAL_LED_PIN_A   253 // Neka druga LED dioda
// ...

//=============================================================================
// 2. MEMORIJSKA MAPA (EEPROM 24C1024 - 128KB)
//=============================================================================
#define EEPROM_SIZE_BYTES       (128 * 1024)
#define EEPROM_I2C_ADDR         0x50 // Standardna adresa za 24C1024

// BLOK 1: Konfiguracija Sistema (Rezervisemo 1KB)
#define EEPROM_CONFIG_START_ADDR    0x0000 // Pocetak EEPROM-a
#define EEPROM_CONFIG_SIZE          1024

// BLOK 2: Lista Adresa (Rezervisemo 2KB za max 1000 adresa)
#define EEPROM_ADDR_LIST_START_ADDR 0x0400 // (Nakon 1KB konfiguracije)
#define EEPROM_ADDR_LIST_SIZE       2048

// BLOK 3: Log Događaja (Ostatak memorije, cca 125KB)
#define EEPROM_LOG_START_ADDR       0x0C00 // (Nakon 1KB + 2KB)
#define EEPROM_LOG_END_ADDR         (EEPROM_SIZE_BYTES - 1) // Do kraja
#define LOG_ENTRY_SIZE              16 // Definisacemo tacnu velicinu strukture loga
#define MAX_LOG_ENTRIES             ((EEPROM_LOG_END_ADDR - EEPROM_LOG_START_ADDR) / LOG_ENTRY_SIZE)
#define STATUS_BYTE_VALID           0x55 // Marker za validan log
#define STATUS_BYTE_EMPTY           0xFF // Marker za prazan slot

//=============================================================================
// 3. MEMORIJSKA MAPA (SPI FLASH W25Q512 - 64MB)
// (Bazirano na common.h fajlu [cite: 418-453])
//=============================================================================
// Definisemo "slotove" sa fiksnim adresama
// Koristimo velike blokove (Sektori na SPI flashu su cesto 64KB)
#define SLOT_SIZE_64K           (64 * 1024)
#define SLOT_SIZE_128K          (128 * 1024)
#define SLOT_SIZE_1M            (1024 * 1024)
#define SLOT_SIZE_12M           (12 * 1024 * 1024)

#define SLOT_ADDR_FW_RC         0x00000000 // Slot za FW Kontrolera Sobe (Rezervisemo 128K)
#define SLOT_ADDR_BL_RC         0x00020000 // Slot za BL Kontrolera Sobe (Rezervisemo 64K)
#define SLOT_ADDR_FW_TH_1M      0x00030000 // Slot za FW Termostata (Rezervisemo 1MB)
#define SLOT_ADDR_BL_TH         0x00130000 // Slot za BL Termostata (Rezervisemo 64K)
#define SLOT_ADDR_IMG_LOGO      0x00140000 // Slot za Logo (Rezervisemo 128K)
#define SLOT_ADDR_IMG_RC_START  0x00160000 // Pocetak bloka za 14 slika (svaka po 256K)
// ... (14 * 256K = 3.5MB) ...
#define SLOT_ADDR_QSPI_ER_12M   0x00500000 // Veliki slot od 12MB za ER_QSPI1 fajl

//=============================================================================
// 4. OSTALE KONSTANTE PROJEKTA
//=============================================================================
#define SERIAL_DEBUG_BAUDRATE   115200 // Za Fazu 1
#define RS485_BAUDRATE          115200 // Prema Procitaj.txt [cite: 292-323]
#define RS485_RESP_TOUT_MS      45     // Definisano u common.h [cite: 418-453]
#define RS485_RX2TX_DELAY_MS    3      // Definisano u common.h [cite: 418-453]

// Nomenklatura (podsjetnik)
// Fajlovi/Klase:   PascalCase
// Funkcije/Metode: PascalCase
// Globalne Var:    g_snake_case
// Clanske Var:     m_snake_case
// Lokalne Var:     camelCase
// Konstante:       ALL_CAPS_SNAKE_CASE
