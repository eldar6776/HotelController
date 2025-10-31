/**
 ******************************************************************************
 * @file    SpiFlashStorage.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija SpiFlashStorage modula.
 ******************************************************************************
 */

#include "SpiFlashStorage.h"

SpiFlashStorage::SpiFlashStorage() : 
    m_flash_transport(SPI_FLASH_CS_PIN, SPI), // Koristi CS pin iz Config-a
    m_flash(&m_flash_transport),
    m_current_read_address(0),
    m_current_write_address(0)
{
    // Konstruktor
}

void SpiFlashStorage::Initialize(int8_t sck, int8_t miso, int8_t mosi, int8_t cs)
{
    Serial.println(F("[SpiFlashStorage] Inicijalizacija SPI..."));
    
    // Inicijalizuj SPI magistralu
    SPI.begin(sck, miso, mosi, -1); // -1 za CS, jer Adafruit biblioteka sama upravlja sa CS
    
    if (!m_flash.begin())
    {
        Serial.println(F("[SpiFlashStorage] GRESKA: Nije pronadjen SPI Flash cip!"));
        // TODO: Prijaviti kriticnu gresku
    }
    else
    {
        Serial.printf("[SpiFlashStorage] Pronadjen Flash cip. JEDEC ID: 0x%lX\n", m_flash.getJEDECID());
        Serial.printf("[SpiFlashStorage] Kapacitet: %d MB\n", m_flash.size() / (1024 * 1024));
    }
}

/**
 * @brief Brise sektor/blok na SPI Flashu.
 */
bool SpiFlashStorage::EraseSlot(uint32_t slot_address, uint32_t slot_size)
{
    Serial.printf("[SpiFlashStorage] Brisanje slota na adresi 0x%lX, velicina %d KB...\n", slot_address, slot_size / 1024);
    // TODO: Implementirati brisanje. Paziti na sektore/blokove.
    // m_flash.eraseSector(sector_number);
    return true;
}

/**
 * @brief Priprema za citanje fajla (kao f_open).
 */
bool SpiFlashStorage::BeginRead(uint32_t address)
{
    Serial.printf("[SpiFlashStorage] Zapoceo citanje sa adrese 0x%lX\n", address);
    m_current_read_address = address;
    // Za SPI flash, nema potrebe za 'open'
    return true;
}

/**
 * @brief Cita dio podataka (kao f_read).
 */
int16_t SpiFlashStorage::ReadChunk(uint8_t* buffer, uint16_t length)
{
    uint32_t bytes_read = m_flash.readBuffer(m_current_read_address, buffer, length);
    if (bytes_read == 0)
    {
        Serial.println(F("[SpiFlashStorage] GRESKA pri citanju!"));
        return -1;
    }
    
    m_current_read_address += bytes_read;
    return bytes_read;
}

void SpiFlashStorage::EndRead()
{
    m_current_read_address = 0;
}

// TODO: Implementirati BeginWrite, WriteChunk, EndWrite...
bool SpiFlashStorage::BeginWrite(uint32_t address) { return false; }
bool SpiFlashStorage::WriteChunk(uint8_t* data, uint16_t length) { return false; }
bool SpiFlashStorage::EndWrite() { return false; }
