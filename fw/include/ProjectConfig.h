/**
 ******************************************************************************
 * @file    ProjectConfig.h
 * @author  Gemini & [Vase Ime]
 * @brief   Centralna konfiguracija za Hotel Controller ESP32
 * @note    AŽURIRANO: Migracija na LILYGO T-Internet-POE (prema šemi)
 ******************************************************************************
 */

#pragma once

//=============================================================================
// 1. HARDVERSKA MAPA PINOVA (LILYGO T-Internet-POE)
//=============================================================================

// --- RS485 INTERFEJS (Serial2) - Pinovi na P0 konektoru ---
#define RS485_RX_PIN        16  // (P0 Pin 12: IO16 - Hardverski UART2_RX)
// #define RS485_TX_PIN        33  // (P0 Pin 10: IO33)
// #define RS485_DE_PIN        12  // (P0 Pin 9: IO12) // PROBLEM: GPIO12 je "strapping pin" i ometa upload!

#define RS485_TX_PIN        33  // (P0 Pin 10: IO33) - Koristimo GPIO33za TX.
#define RS485_DE_PIN        12  // (P0 Pin 9: IO12) - Koristimo GPIO12za DE, jer nema "strapping" funkciju.

// --- I2C INTERFEJS (EEPROM) - Pinovi na P0 konektoru ---
// PREMJEŠTENO: Koriste se slobodni I/O pinovi koji nisu "strapping" ili input-only
#define I2C_SDA_PIN         4   // (P0 Pin 8: IO4)
#define I2C_SCL_PIN         32  // (P0 Pin 11: IO32)
#define EEPROM_I2C_ADDR     0x50

// --- SPI INTERFEJS (uSD kartica) - Fiksno na ploči ---
// OVI PINOVI SU INTERNO KORIŠTENI (P0 Pin 5, 6, 7) I NE SMIJU SE KORISTITI ZA DRUGE PERIFERIJE
#define SPI_SCK_PIN         14  // (Pin 5: GPIO14/CLK)
#define SPI_MISO_PIN        2   // (Interno J1 / P0 Pin 6)
#define SPI_MOSI_PIN        15  // (Interno J1 / P0 Pin 7)
#define SPI_FLASH_CS_PIN    13  // (Interno J1 - CS pin uSD kartice)

// --- Ethernet (ETH) - Fiksno na ploči ---
#define ETH_MDC_PIN         23  // (Interno U6)
#define ETH_MDIO_PIN        18  // (Interno U6)
#define ETH_POWER_PIN       -1  // NIJE POTREBAN (Riješeno POE), ali je neophodan za potpis funkcije
#define ETH_RESET_PIN       5   // (Interno U6)
#define ETH_PHY_TYPE        ETH_PHY_LAN8720
#ifdef ETH_CLK_MODE
#undef ETH_CLK_MODE
#endif
#define ETH_CLK_MODE        ETH_CLOCK_GPIO17_OUT // ISPRAVKA: LILYGO ploča zahtijeva da ESP32 generiše sat (Uklonjeno upozorenje)

// --- OSTALO ---
// STATUS_LED_PIN se ne može koristiti jer su svi preostali pinovi zauzeti ili nepouzdani.
#define STATUS_LED_PIN      -1  // Onemogućeno
#define WIFI_RST_BTN_PIN    35  // (P0 Pin 13: IO35 - Samo ULAZ)
#define SERIAL_DEBUG_BAUDRATE 115200

// --- DWIN (UART0) - Pinovi na P1 konektoru ---
#define DWIN_RX_PIN         3   // (P1 Pin 1: UARX)
#define DWIN_TX_PIN         1   // (P1 Pin 2: UATX)

//=============================================================================
// 2. GLOBALNE KONSTANTE SISTEMA (Nepromijenjeno)
//=============================================================================

// --- RS485 Protokol ---
#define RS485_BAUDRATE              115200
#define MAX_PACKET_LENGTH           256    
#define RS485_BUFFER_SIZE           MAX_PACKET_LENGTH
#define RS485_TIMEOUT_MS            300
#define MAX_RS485_RETRIES           3    // NOVO: Maksimalan broj ponovnih slanja u slučaju timeout-a
#define RS485_RESP_TOUT_MS          45 // Originalna vrednost: RESP_TOUT (45U)

// --- Polling i Logovanje ---
#define MAX_ADDRESS_LIST_SIZE       500
#define LOG_ENTRY_SIZE              16 // ISPRAVKA: Usklađeno sa starim sistemom (LOG_DSIZE)
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
// 5. DEFAULTNE SISTEMSKE VRIJEDNOSTI (Preuzeto iz common.h)
//=============================================================================
//

// --- Mreža i mDNS ---
#define DEFAULT_MDNS_NAME       "HOTEL_CTRL"    //
#define DEFAULT_IP_ADDR0        192             //
#define DEFAULT_IP_ADDR1        168             //
#define DEFAULT_IP_ADDR2        0               //
#define DEFAULT_IP_ADDR3        199             //
#define DEFAULT_SUBNET_ADDR0    255             //
#define DEFAULT_SUBNET_ADDR1    255             //
#define DEFAULT_SUBNET_ADDR2    255             //
#define DEFAULT_SUBNET_ADDR3    0               //
#define DEFAULT_GW_ADDR0        192             //
#define DEFAULT_GW_ADDR1        168             //
#define DEFAULT_GW_ADDR2        0               //
#define DEFAULT_GW_ADDR3        1               //

// --- RS485 Adrese ---
#define DEFAULT_RS485_BCAST_ADDR    39321       // (DEF_RSBRA)
#define DEFAULT_RS485_GROUP_ADDR    26486       // (DEF_RC_RSGRA)
#define DEFAULT_RS485_IFACE_ADDR    5           // (FST_HC_RSIFA)

// --- Sistem ---
#define DEFAULT_SYSTEM_ID           43962       // (DEF_SYSID)


//=============================================================================
// 6. NAPOMENE ZA RAZVOJ (Stari Obrisani Makroi)
//=============================================================================

// ... (ostatak fajla)