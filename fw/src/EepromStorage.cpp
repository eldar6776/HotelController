/**
 ******************************************************************************
 * @file    EepromStorage.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija EepromStorage modula.
 ******************************************************************************
 */

#include "EepromStorage.h"

// Konstante za EEPROM
#define EEPROM_PAGE_SIZE 128 
#define EEPROM_WRITE_DELAY 5 

// Globalni objekat za konfiguraciju
AppConfig g_appConfig; 

EepromStorage::EepromStorage() :
    m_log_write_index(0),
    m_log_read_index(0),
    m_log_count(0)
{
    // Konstruktor
}

void EepromStorage::Initialize(int8_t sda_pin, int8_t scl_pin)
{
    Serial.printf("[EepromStorage] Inicijalizacija I2C na SDA=%d, SCL=%d\r\n", sda_pin, scl_pin);
    Wire.begin(sda_pin, scl_pin);
    
    // Učitaj globalnu konfiguraciju
    if (ReadConfig(&g_appConfig))
    {
        Serial.printf("[EepromStorage] Konfiguracija ucitana. RS485 Adresa: 0x%X\r\n", g_appConfig.rs485_iface_addr);
    }
    else
    {
        Serial.println(F("[EepromStorage] GRESKA: Nije moguce ucitati konfiguraciju."));
    }

    Serial.println(F("[EepromStorage] Inicijalizacija Logera...\r\n"));
    LoggerInit();
}

//=============================================================================
// API za Konfiguraciju
//=============================================================================

bool EepromStorage::ReadConfig(AppConfig* config)
{
    return ReadBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

bool EepromStorage::WriteConfig(const AppConfig* config)
{
    return WriteBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

//=============================================================================
// I2C Drajver - Implementacija Page Write logike (24C1024)
//=============================================================================

bool EepromStorage::WriteBytes(uint16_t address, const uint8_t* data, uint16_t length)
{
    uint16_t current_addr = address;
    uint16_t bytes_remaining = length;
    uint16_t data_offset = 0;

    while (bytes_remaining > 0)
    {
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t bytes_to_end_of_page = EEPROM_PAGE_SIZE - page_offset;
        uint16_t chunk_size = min((uint16_t)bytes_remaining, bytes_to_end_of_page);
        
        Wire.beginTransmission(EEPROM_I2C_ADDR);
        Wire.write((uint8_t)(current_addr >> 8));   
        Wire.write((uint8_t)(current_addr & 0xFF)); 
        
        Wire.write(data + data_offset, chunk_size);
        
        if (Wire.endTransmission() != 0)
        {
            return false;
        }
        
        delay(EEPROM_WRITE_DELAY); 

        current_addr += chunk_size;
        data_offset += chunk_size;
        bytes_remaining -= chunk_size;
    }
    
    return true;
}

bool EepromStorage::ReadBytes(uint16_t address, uint8_t* data, uint16_t length)
{
    Wire.beginTransmission(EEPROM_I2C_ADDR);
    Wire.write((uint8_t)(address >> 8));   
    Wire.write((uint8_t)(address & 0xFF)); 
    
    if (Wire.endTransmission(false) != 0) 
    {
        return false;
    }
    
    // Rjesava I2C Ambiguity Warning (Wire.requestFrom)
    if (Wire.requestFrom((uint8_t)EEPROM_I2C_ADDR, (size_t)length) != length)
    {
        return false;
    }

    for (uint16_t i = 0; i < length; i++)
    {
        if (Wire.available())
        {
            data[i] = Wire.read();
        }
    }
    
    return true;
}

//=============================================================================
// API za Logger (Rjesava greske u Loger funkcijama)
//=============================================================================

void EepromStorage::LoggerInit()
{
    // Ovdje bi se trebala citati memorija za pronalazenje head/tail,
    // za sada pretpostavljamo default:
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;
    // TODO: Implementirati skeniranje (kao u hotel_ctrl.c lin. 204-367)
}

LoggerStatus EepromStorage::WriteLog(const LogEntry* entry)
{
    if (m_log_count >= MAX_LOG_ENTRIES)
    {
        // Puni smo, brisemo najstariji prije upisa
        DeleteOldestLog();
    }

    // Dodatni bajt za status (0x55/0xFF), stoga koristimo LOG_RECORD_SIZE
    uint8_t write_buffer[LOG_RECORD_SIZE]; 
    
    uint16_t write_addr = EEPROM_LOG_START_ADDR + (m_log_write_index * LOG_RECORD_SIZE);

    // 1. Status Byte
    write_buffer[0] = STATUS_BYTE_VALID; 
    
    // 2. Kopiraj LogEntry (sigurno koristimo stvarnu veličinu strukture i popunimo ostatak nulama)
    size_t entry_copy_size = min((size_t)LOG_ENTRY_SIZE, sizeof(LogEntry));
    memcpy(&write_buffer[1], (const uint8_t*)entry, entry_copy_size);
    if (LOG_ENTRY_SIZE > entry_copy_size)
    {
        memset(&write_buffer[1 + entry_copy_size], 0, LOG_ENTRY_SIZE - entry_copy_size);
    }

    if (!WriteBytes(write_addr, write_buffer, LOG_RECORD_SIZE))
    {
        return LoggerStatus::LOGGER_ERROR;
    }

    // Azuriraj indekse
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
    
    // Citamo 1 bajt (status) + LOG_ENTRY_SIZE bajtova podataka
    uint8_t read_buffer[LOG_RECORD_SIZE];

    uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE); 
    
    if (!ReadBytes(read_addr, read_buffer, LOG_RECORD_SIZE))
    {
        return LoggerStatus::LOGGER_ERROR;
    }

    // Status Byte mora biti VALID
    if (read_buffer[0] != STATUS_BYTE_VALID)
    {
        return LoggerStatus::LOGGER_ERROR;
    }

    // Kopiraj log (podaci pocinju od read_buffer[1])
    size_t entry_copy_size = min((size_t)LOG_ENTRY_SIZE, sizeof(LogEntry));
    memcpy((uint8_t*)entry, &read_buffer[1], entry_copy_size);
    if (sizeof(LogEntry) > entry_copy_size)
    {
        // Zero remaining bytes in structure if any
        memset(((uint8_t*)entry) + entry_copy_size, 0, sizeof(LogEntry) - entry_copy_size);
    }

    return LoggerStatus::LOGGER_OK;
}

LoggerStatus EepromStorage::DeleteOldestLog()
{
    if (m_log_count == 0)
    {
        return LoggerStatus::LOGGER_EMPTY;
    }

    uint16_t status_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
    
    // Brisemo samo statusni bajt
    uint8_t empty_byte = STATUS_BYTE_EMPTY; 
    
    if (!WriteBytes(status_addr, &empty_byte, 1))
    {
        return LoggerStatus::LOGGER_ERROR;
    }

    m_log_read_index = (m_log_read_index + 1) % MAX_LOG_ENTRIES;
    m_log_count--;

    return LoggerStatus::LOGGER_OK;
}

// Implementacija WriteAddressList
bool EepromStorage::WriteAddressList(const uint16_t* listBuffer, uint16_t count)
{
    // Pretvaramo count uint16_t elemenata u bajtove
    uint16_t bytes_to_write = count * sizeof(uint16_t);

    if (bytes_to_write > EEPROM_ADDRESS_LIST_SIZE) {
        bytes_to_write = EEPROM_ADDRESS_LIST_SIZE; // Osiguranje od preljeva
        Serial.println(F("[EepromStorage] Upozorenje: Lista adresa je skraćena zbog ograničenja EEPROM-a."));
    }
    
    // Upisujemo bajtove u EEPROM na definisanoj početnoj adresi
    if (WriteBytes(EEPROM_ADDRESS_LIST_START_ADDR, (const uint8_t*)listBuffer, bytes_to_write))
    {
        Serial.printf("[EepromStorage] Uspješno zapisano %u adresa (%u B).\n", count, bytes_to_write);
        return true;
    }
    Serial.println(F("[EepromStorage] GREŠKA: Pisanje liste adresa neuspješno."));
    return false;
}

bool EepromStorage::ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    uint16_t bytes_to_read = min((uint16_t)EEPROM_ADDRESS_LIST_SIZE, (uint16_t)(maxCount * sizeof(uint16_t)));
    
    if (ReadBytes(EEPROM_ADDRESS_LIST_START_ADDR, (uint8_t*)listBuffer, bytes_to_read))
    {
        *actualCount = bytes_to_read / sizeof(uint16_t);
        return true;
    }
    *actualCount = 0;
    return false;
}

// ============================================================================
// --- ISPRAVKA: DODATA FUNKCIJA KOJA NEDOSTAJE ---
// ============================================================================
LoggerStatus EepromStorage::ClearAllLogs()
{
    Serial.println(F("[EepromStorage] Brisanje svih logova..."));

    // Pripremi buffer sa 0xFF (STATUS_BYTE_EMPTY)
    uint8_t empty_buffer[EEPROM_PAGE_SIZE];
    memset(empty_buffer, STATUS_BYTE_EMPTY, EEPROM_PAGE_SIZE);

    uint32_t bytes_to_clear = EEPROM_LOG_AREA_SIZE;
    uint16_t current_address = EEPROM_LOG_START_ADDR;

    while (bytes_to_clear > 0)
    {
        // Odredi koliko pisati u ovom ciklusu (do kraja stranice)
        uint16_t page_offset = current_address % EEPROM_PAGE_SIZE;
        uint16_t bytes_to_end_of_page = EEPROM_PAGE_SIZE - page_offset;
        uint16_t chunk_size = min((uint16_t)bytes_to_clear, bytes_to_end_of_page);
        
        if (!WriteBytes(current_address, empty_buffer, chunk_size))
        {
            Serial.println(F("[EepromStorage] GRESKA pri brisanju logova."));
            return LoggerStatus::LOGGER_ERROR;
        }
        
        bytes_to_clear -= chunk_size;
        current_address += chunk_size;
    }

    // Resetuj head/tail pokazivače
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;

    Serial.println(F("[EepromStorage] Svi logovi obrisani."));
    return LoggerStatus::LOGGER_OK;
}