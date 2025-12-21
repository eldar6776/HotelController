/**
 ******************************************************************************
 * @file    EepromStorage.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za EepromStorage modul.
 *
 * @note
 * Upravlja I2C EEPROM-om (Logovi, Konfig, Lista Adresa).
 * Implementira 'head/tail' loger.
 ******************************************************************************
 */

#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <Arduino.h>
#include <Wire.h>
#include "ProjectConfig.h"

/**
 * @brief Definicija strukture za dodatni TimeSync paket.
 */
struct AdditionalSyncPacket
{
    uint8_t enabled;           ///< 0 = onemogućeno, 1 = omogućeno
    uint8_t protocol_version;  ///< ProtocolVersion enum
    uint16_t broadcast_addr;   ///< Broadcast adresa za ovaj paket
};

/**
 * @brief Definicija strukture za konfiguraciju aplikacije.
 */
struct AppConfig
{
    uint32_t ip_address;                     ///< IP adresa uredjaja
    uint32_t subnet_mask;                    ///< Subnet maska
    uint32_t gateway;                        ///< Default gateway
    uint16_t rs485_iface_addr;               ///< RS485 adresa interfejsa
    uint16_t rs485_group_addr;               ///< RS485 grupna adresa
    uint16_t rs485_bcast_addr;               ///< RS485 broadcast adresa
    uint16_t system_id;                      ///< Sistemski ID
    char mdns_name[32];                      ///< mDNS ime uredjaja
    uint8_t protocol_version;                ///< Glavni ProtocolVersion enum
    AdditionalSyncPacket additional_sync[3]; ///< Do 3 dodatna TimeSync paketa
};

/**
 * @brief Definicija strukture za log unos (16 bajtova).
 */
struct LogEntry
{
    uint16_t log_id;      ///< Jedinstveni ID loga
    uint8_t  event_code;  ///< Kod dogadjaja
    uint16_t device_addr; ///< Adresa uredjaja vezanog za dogadjaj
    uint8_t  log_type;    ///< Tip loga
    uint8_t  rf_card_id[4]; ///< ID RF kartice
    uint32_t timestamp;   ///< Unix vremenski pecat
    uint8_t  reserved[2]; ///< Rezervisano za poravnanje (padding)
};

/**
 * @brief Status logger operacija.
 */
enum class LoggerStatus
{
    LOGGER_OK,    ///< Operacija uspjesna
    LOGGER_ERROR, ///< Greska tokom operacije
    LOGGER_EMPTY  ///< Logger je prazan
};

/**
 * @brief Klasa za upravljanje EEPROM memorijom.
 */
class EepromStorage
{
public:
    /**
     * @brief Konstruktor.
     */
    EepromStorage();

    /**
     * @brief Inicijalizuje EEPROM modul.
     * @param sda_pin Pin za SDA I2C liniju.
     * @param scl_pin Pin za SCL I2C liniju.
     */
    void Initialize(int8_t sda_pin, int8_t scl_pin);
    
    // --- API za Konfiguraciju ---
    
    /**
     * @brief Cita konfiguraciju iz EEPROM-a.
     * @param config Pointer na strukturu gdje ce se upisati procitana konfiguracija.
     * @return true ako je citanje uspjesno, false inace.
     */
    bool ReadConfig(AppConfig* config);

    /**
     * @brief Pise konfiguraciju u EEPROM.
     * @param config Pointer na strukturu sa konfiguracijom za upis.
     * @return true ako je upis uspjesan, false inace.
     */
    bool WriteConfig(const AppConfig* config);

    // --- API za Listu Adresa ---

    /**
     * @brief Cita listu adresa iz EEPROM-a.
     * @param listBuffer Buffer za smjestanje procitanih adresa.
     * @param maxCount Maksimalan broj adresa koji stane u buffer.
     * @param actualCount Pointer gdje ce se upisati stvarni broj procitanih adresa.
     * @return true ako je citanje uspjesno, false inace.
     */
    bool ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount);

    /**
     * @brief Pise listu adresa u EEPROM.
     * @param listBuffer Buffer sa adresama za upis.
     * @param count Broj adresa za upis.
     * @return true ako je upis uspjesan, false inace.
     */
    bool WriteAddressList(const uint16_t* listBuffer, uint16_t count);
    
    // --- API za Logger (head/tail) ---

    /**
     * @brief Upisuje novi log unos u EEPROM.
     * @param entry Pointer na LogEntry strukturu.
     * @return Status operacije.
     */
    LoggerStatus WriteLog(const LogEntry* entry);

    /**
     * @brief Dohvata najstariji log unos.
     * @param entry Pointer na LogEntry strukturu gdje ce se upisati podaci.
     * @return Status operacije.
     */
    LoggerStatus GetOldestLog(LogEntry* entry);

    /**
     * @brief Cita blok logova kao hex string.
     * @return String sa hex podacima.
     */
    String ReadLogBlockAsHexString();

    /**
     * @brief Brise blok logova (pomjera tail).
     * @return Status operacije.
     */
    LoggerStatus DeleteLogBlock();

    /**
     * @brief Brise sve logove (resetuje head i tail).
     * @return Status operacije.
     */
    LoggerStatus ClearAllLogs();

    /**
     * @brief Vraca trenutni broj logova.
     * @return Broj logova.
     */
    uint16_t GetLogCount();

private:
    /**
     * @brief Inicijalizuje logger varijable skeniranjem EEPROM-a.
     */
    void LoggerInit(); 

    /**
     * @brief Ucitava podrazumijevanu konfiguraciju.
     */
    void LoadDefaultConfig();

    /**
     * @brief Pise niz bajtova na zadatu adresu u EEPROM-u.
     * @param address Pocetna adresa u EEPROM-u.
     * @param data Pointer na podatke.
     * @param length Duzina podataka.
     * @return true ako je upis uspjesan, false inace.
     */
    bool WriteBytes(uint16_t address, const uint8_t* data, uint16_t length);

    /**
     * @brief Cita niz bajtova sa zadate adrese iz EEPROM-a.
     * @param address Pocetna adresa u EEPROM-u.
     * @param data Buffer za smjestanje procitanih podataka.
     * @param length Duzina podataka za citanje.
     * @return true ako je citanje uspjesno, false inace.
     */
    bool ReadBytes(uint16_t address, uint8_t* data, uint16_t length);

    uint16_t m_log_write_index; ///< 'head' index
    uint16_t m_log_read_index;  ///< 'tail' index
    uint16_t m_log_count;       ///< Broj aktivnih logova
};

#endif // EEPROM_STORAGE_H