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
// 2. ASCII KONTROLNI KARAKTERI (iz common.h) - SAMO ONI KOJI SE KORISTE
//=============================================================================
// NAPOMENA: Većina ASCII kontrolnih karaktera nije potrebna za ESP32 projekat
// Uključeni su samo SOH, STX, EOT, ACK, NAK koji se koriste u RS485 protokolu
#define SOH     ((char)0x01U)    /* start of header control character   */
#define STX     ((char)0x02U)    /* start of text control character     */
#define EOT     ((char)0x04U)    /* end of transmission control char    */
#define ACK     ((char)0x06U)    /* acknowledge control character       */
#define NAK     ((char)0x15U)    /* negative acknowledge control char   */

//=============================================================================
// 3. GLOBALNE KONSTANTE SISTEMA (Nepromijenjeno)
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
#define UPDATE_PACKET_TIMEOUT_MS    45 // Timeout za DATA pakete, prema specifikaciji starog sistema
#define UPDATE_DATA_CHUNK_SIZE      128
#define APP_START_DEL               12345 // Originalna vrednost: APP_START_DEL (12345U)
#define FWR_COPY_DEL                1567U // Pauza za RC da iskopira novi firmware (iz common.h)
#define IMG_COPY_DEL                4567U // Pauza za RC da iskopira novu sliku (iz common.h)

//=============================================================================
// 4. RS485 KOMANDE (iz common.h)
//=============================================================================

// --- Room Controller Komande ---
#define COPY_DISP_IMG                   ((uint8_t)0x63U)
#define DWNLD_DISP_IMG 		            ((uint8_t)0x63U)    
#define DWNLD_DISP_IMG_1 		        ((uint8_t)0x64U)
#define DWNLD_DISP_IMG_2 		        ((uint8_t)0x65U)
#define DWNLD_DISP_IMG_3 		        ((uint8_t)0x66U)
#define DWNLD_DISP_IMG_4 		        ((uint8_t)0x67U)
#define DWNLD_DISP_IMG_5 		        ((uint8_t)0x68U)
#define DWNLD_DISP_IMG_6 		        ((uint8_t)0x69U)
#define DWNLD_DISP_IMG_7 		        ((uint8_t)0x6AU)
#define DWNLD_DISP_IMG_8 		        ((uint8_t)0x6BU)
#define DWNLD_DISP_IMG_9 		        ((uint8_t)0x6CU)
#define DWNLD_DISP_IMG_10		        ((uint8_t)0x6DU)
#define DWNLD_DISP_IMG_11		        ((uint8_t)0x6EU)
#define DWNLD_DISP_IMG_12		        ((uint8_t)0x6FU)
#define DWNLD_DISP_IMG_13		        ((uint8_t)0x70U)
#define DWNLD_DISP_IMG_14		        ((uint8_t)0x71U)
#define DWNLD_DISP_IMG_15		        ((uint8_t)0x72U)
#define DWNLD_DISP_IMG_16		        ((uint8_t)0x73U)
#define DWNLD_DISP_IMG_17		        ((uint8_t)0x74U)
#define DWNLD_DISP_IMG_18		        ((uint8_t)0x75U)
#define DWNLD_DISP_IMG_19		        ((uint8_t)0x76U)
#define DWNLD_DISP_IMG_20               ((uint8_t)0x77U)
#define DWNLD_DISP_IMG_21               ((uint8_t)0x78U)
#define DWNLD_DISP_IMG_22		        ((uint8_t)0x79U)
#define DWNLD_DISP_IMG_23		        ((uint8_t)0x7AU)
#define DWNLD_DISP_IMG_24               ((uint8_t)0x7BU)
#define DWNLD_DISP_IMG_25               ((uint8_t)0x7CU)

// Aliasi za firmware update komande (iz common.h)
#define DWNLD_FWR_IMG                   DWNLD_DISP_IMG_20
#define DWNLD_BLDR_IMG                  DWNLD_DISP_IMG_21
#define RT_DWNLD_FWR                    DWNLD_DISP_IMG_22
#define RT_DWNLD_BLDR                   DWNLD_DISP_IMG_23  
#define RT_DWNLD_LOGO                   DWNLD_DISP_IMG_24  
#define RT_DWNLD_LANG                   DWNLD_DISP_IMG_25

// Komande za slanje prema Room Controlleru (iz common.h)
#define CMD_DWNLD_FWR_IMG               ((uint8_t)0xBFU) // DWNLD_FWR
#define CMD_DWNLD_BLDR_IMG              ((uint8_t)0xC2U) // UPDATE_BLDR
#define CMD_START_BLDR                  ((uint8_t)0xBCU)
#define CMD_APP_EXE                     ((uint8_t)0xBBU)
#define CMD_RT_DWNLD_FWR                RT_DWNLD_FWR
#define CMD_RT_DWNLD_BLDR               RT_DWNLD_BLDR
#define CMD_RT_DWNLD_LOGO               RT_DWNLD_LOGO

// Opseg slika za Room Controller (koristi se u UpdateManager.h)
#define CMD_IMG_RC_START                DWNLD_DISP_IMG_1 // 0x64
#define CMD_IMG_RC_END                  DWNLD_DISP_IMG_14 // 0x71
#define CMD_IMG_COUNT                   (CMD_IMG_RC_END - CMD_IMG_RC_START + 1)

// CMD-ovi za interne Update Protokol state machine (nisu direktno iz common.h, već interna imena)
// NAPOMENA: Ove komande se koriste za procesiranje odgovora, a ne šalju se direktno
#define CMD_UPDATE_START                0x14 
#define CMD_UPDATE_DATA                 0x15 
#define CMD_UPDATE_FINISH               0x16 

//=============================================================================
// 5. MEMORIJSKA MAPA EEPROM-a (DINAMIČKA - Nepromijenjeno)
//=============================================================================

#define EEPROM_CONFIG_START_ADDR        0x0000
#define EEPROM_CONFIG_SIZE              256
#define EEPROM_ADDRESS_LIST_START_ADDR  (EEPROM_CONFIG_START_ADDR + EEPROM_CONFIG_SIZE)
#define EEPROM_ADDRESS_LIST_SIZE        (MAX_ADDRESS_LIST_SIZE * sizeof(uint16_t))
#define EEPROM_LOG_START_ADDR           (EEPROM_ADDRESS_LIST_START_ADDR + EEPROM_ADDRESS_LIST_SIZE)
#define EEPROM_LOG_AREA_SIZE            (MAX_LOG_ENTRIES * LOG_RECORD_SIZE)

// --- Ping Watchdog (vraćeno na mjesto) ---
#define PING_INTERVAL_MS            60000
#define MAX_PING_FAILURES           10

//=============================================================================
// 6. FILESYSTEM PATHS (uSD kartica - Nepromijenjeno)
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
// 7. DEFAULTNE SISTEMSKE VRIJEDNOSTI (Preuzeto iz common.h)
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
// 8. NAPOMENE ZA RAZVOJ (Stari Obrisani Makroi)
//=============================================================================

// ... (ostatak fajla)