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

// Definicija strukture za konfiguraciju
struct AppConfig
{
    uint32_t ip_address;
    uint32_t subnet_mask;
    uint32_t gateway;
    uint16_t rs485_iface_addr;
    uint16_t rs485_group_addr;
    uint16_t rs485_bcast_addr;
    uint16_t system_id;
    char mdns_name[32];
};

// Definicija strukture za log (16 bajtova)
struct LogEntry
{
    uint16_t log_id;
    uint8_t  event_code;
    uint16_t device_addr;
    uint8_t  log_type;
    uint8_t  rf_card_id[4];
    uint32_t timestamp; // Unix vrijeme
    uint8_t  reserved[1];
};

enum class LoggerStatus
{
    LOGGER_OK,
    LOGGER_ERROR,
    LOGGER_EMPTY
};

class EepromStorage
{
public:
    EepromStorage();
    void Initialize(int8_t sda_pin, int8_t scl_pin);
    
    // --- API za Konfiguraciju ---
    bool ReadConfig(AppConfig* config);
    bool WriteConfig(const AppConfig* config);

    // --- API za Listu Adresa ---
    bool ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount);
    bool WriteAddressList(const uint16_t* listBuffer, uint16_t count);
    
    // --- API za Logger (head/tail) ---
    LoggerStatus WriteLog(const LogEntry* entry);
    LoggerStatus GetOldestLog(LogEntry* entry);
    LoggerStatus DeleteOldestLog();
    LoggerStatus ClearAllLogs();
    uint16_t GetLogCount();

private:
    void LoggerInit(); // Skenira EEPROM da pronadje head/tail
    void LoadDefaultConfig();
    // Privatne funkcije za I2C komunikaciju sa 24C1024 (16-bitne adrese)
    bool WriteBytes(uint16_t address, const uint8_t* data, uint16_t length);
    bool ReadBytes(uint16_t address, uint8_t* data, uint16_t length);

    uint16_t m_log_write_index; // 'head'
    uint16_t m_log_read_index;  // 'tail'
    uint16_t m_log_count;
};

#endif // EEPROM_STORAGE_H
