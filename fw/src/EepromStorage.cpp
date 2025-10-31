/**
 ******************************************************************************
 * @file    EepromStorage.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija EepromStorage modula.
 ******************************************************************************
 */

#include "EepromStorage.h"

EepromStorage::EepromStorage() :
    m_log_write_index(0),
    m_log_read_index(0),
    m_log_count(0)
{
    // Konstruktor
}

void EepromStorage::Initialize(int8_t sda_pin, int8_t scl_pin)
{
    Serial.printf("[EepromStorage] Inicijalizacija I2C na SDA=%d, SCL=%d\n", sda_pin, scl_pin);
    Wire.begin(sda_pin, scl_pin);
    
    Serial.println(F("[EepromStorage] Skeniranje logova za 'head' i 'tail'..."));
    LoggerInit();
}

//=============================================================================
// API za Konfiguraciju
//=============================================================================

bool EepromStorage::ReadConfig(AppConfig* config)
{
    Serial.println(F("[EepromStorage] Citanje konfiguracije..."));
    return ReadBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

bool EepromStorage::WriteConfig(const AppConfig* config)
{
    Serial.println(F("[EepromStorage] Pisanje konfiguracije..."));
    return WriteBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

//=============================================================================
// API za Listu Adresa
//=============================================================================

bool EepromStorage::ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    Serial.println(F("[EepromStorage] Citanje liste adresa..."));
    // TODO: Implementirati citanje liste
    // Privremeno, vracamo praznu listu
    *actualCount = 0;
    return true;
}

//=============================================================================
// API za Logger (head/tail)
//=============================================================================

/**
 * @brief Skenira EEPROM pri startu da pronadje head, tail i count.
 */
void EepromStorage::LoggerInit()
{
    // TODO: Implementirati logiku skeniranja iz 'logger.c'
    // Za sada, pocinjemo sa praznim logom.
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;
    Serial.printf("[EepromStorage] Logger inicijalizovan. Pronadjeno %d logova.\n", m_log_count);
}

uint16_t EepromStorage::GetLogCount()
{
    return m_log_count;
}

LoggerStatus EepromStorage::WriteLog(const LogEntry* entry)
{
    if (m_log_count >= MAX_LOG_ENTRIES)
    {
        Serial.println(F("[EepromStorage] Log bafer pun. Brisem najstariji unos..."));
        if (DeleteOldestLog() != LoggerStatus::LOGGER_OK)
        {
            return LoggerStatus::LOGGER_ERROR;
        }
    }

    uint16_t write_addr = EEPROM_LOG_START_ADDR + (m_log_write_index * LOG_ENTRY_SIZE);
    
    // Pripremi bafer (Status + Podaci)
    uint8_t write_buffer[LOG_ENTRY_SIZE];
    write_buffer[0] = STATUS_BYTE_VALID;
    memcpy(&write_buffer[1], entry, sizeof(LogEntry));

    if (!WriteBytes(write_addr, write_buffer, LOG_ENTRY_SIZE))
    {
        Serial.println(F("[EepromStorage] GRESKA pri upisu loga!"));
        return LoggerStatus::LOGGER_ERROR;
    }

    m_log_write_index = (m_log_write_index + 1) % MAX_LOG_ENTRIES;
    m_log_count++;

    return LoggerStatus::LOGGER_OK;
}

LoggerStatus EepromStorage::GetOldestLog(LogEntry* entry)
{
    if (m_log_count == 0)
    {
        return LoggerStatus::LOGGER_EMPTY;
    }

    // Adresa podataka (preskacemo status bajt)
    uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_ENTRY_SIZE) + 1;

    if (!ReadBytes(read_addr, (uint8_t*)entry, sizeof(LogEntry)))
    {
        Serial.println(F("[EepromStorage] GRESKA pri citanju loga!"));
        return LoggerStatus::LOGGER_ERROR;
    }

    return LoggerStatus::LOGGER_OK;
}

LoggerStatus EepromStorage::DeleteOldestLog()
{
    if (m_log_count == 0)
    {
        return LoggerStatus::LOGGER_EMPTY;
    }

    // Adresa status bajta
    uint16_t status_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_ENTRY_SIZE);
    uint8_t empty_byte = STATUS_BYTE_EMPTY;

    // Prebrisi samo status bajt
    if (!WriteBytes(status_addr, &empty_byte, 1))
    {
        Serial.println(F("[EepromStorage] GRESKA pri brisanju loga!"));
        return LoggerStatus::LOGGER_ERROR;
    }

    m_log_read_index = (m_log_read_index + 1) % MAX_LOG_ENTRIES;
    m_log_count--;

    return LoggerStatus::LOGGER_OK;
}

LoggerStatus EepromStorage::ClearAllLogs()
{
    Serial.println(F("[EepromStorage] Formatiranje log particije..."));
    // TODO: Implementirati brisanje svih blokova
    
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;
    return LoggerStatus::LOGGER_OK;
}


//=============================================================================
// Privatne I2C Helper Funkcije (za 24C1024)
//=============================================================================

/**
 * @brief Pise bajtove na 16-bitnu adresu (za 24C1024).
 */
bool EepromStorage::WriteBytes(uint16_t address, uint8_t* data, uint16_t length)
{
    // TODO: Implementirati logiku za pisanje na 24C1024,
    // ukljucujuci podjelu na stranice (page write).
    // Ovo je samo placeholder.
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((uint8_t)(address >> 8));   // MSB adrese
    Wire.write((uint8_t)(address & 0xFF)); // LSB adrese
    
    uint16_t bytes_written = 0;
    for (uint16_t i = 0; i < length; i++)
    {
        Wire.write(data[i]);
        bytes_written++;
        // TODO: Implementirati 'page boundary' logiku ovdje
    }
    
    if (Wire.endTransmission() != 0)
    {
        Serial.println(F("[EepromStorage] I2C Write Error"));
        return false;
    }
    delay(5); // Obavezna pauza za EEPROM upis
    return true;
}

/**
 * @brief Cita bajtove sa 16-bitne adrese (za 24C1024).
 */
bool EepromStorage::ReadBytes(uint16_t address, uint8_t* data, uint16_t length)
{
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((uint8_t)(address >> 8));   // MSB adrese
    Wire.write((uint8_t)(address & 0xFF)); // LSB adrese
    if (Wire.endTransmission() != 0)
    {
        Serial.println(F("[EepromStorage] I2C Read Error (set address)"));
        return false;
    }

    Wire.requestFrom(EEPROM_I2C_ADDR, (uint8_t)length);
    if (Wire.available() != length)
    {
        Serial.println(F("[EepromStorage] I2C Read Error (request)"));
        return false;
    }
    
    for (uint16_t i = 0; i < length; i++)
    {
        data[i] = Wire.read();
    }
    return true;
}
