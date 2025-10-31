/**
 ******************************************************************************
 * @file    SpiFlashStorage.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija SpiFlashStorage modula.
 ******************************************************************************
 */

#include "SpiFlashStorage.h"

// Fiksirano: Koristimo SPI instancu direktno u initializer listi sa CS pinom
SpiFlashStorage::SpiFlashStorage() : 
    // Koristimo defaultni konstruktor transporta (kompatibilno sa Adafruit SPIFlash verzijom u projektu)
    m_flash_transport(),
    m_flash(&m_flash_transport),
    m_current_read_address(0),
    m_current_write_address(0)
{
    // Konstruktor
}

void SpiFlashStorage::Initialize(int8_t sck, int8_t miso, int8_t mosi, int8_t cs)
{
    Serial.println(F("[SpiFlashStorage] Inicijalizacija SPI...\r\n"));
    
    // Inicijalizuj SPI magistralu
    SPI.begin(sck, miso, mosi, -1); 
    
    if (!m_flash.begin())
    {
        Serial.println(F("[SpiFlashStorage] GRESKA: Nije pronadjen SPI Flash cip!"));
    }
    else
    {
        Serial.printf("[SpiFlashStorage] Pronadjen Flash cip. JEDEC ID: 0x%X, Velicina: %d MB.\r\n", 
            m_flash.getJEDECID(), m_flash.size() / (1024 * 1024));
    }
}

bool SpiFlashStorage::BeginRead(uint32_t address)
{
    Serial.printf("[SpiFlashStorage] Zapoceo citanje sa adrese 0x%lX\r\n", address);
    m_current_read_address = address;
    return true;
}

int16_t SpiFlashStorage::ReadChunk(uint8_t* buffer, uint16_t length)
{
    uint32_t bytes_read = m_flash.readBuffer(m_current_read_address, buffer, length);
    m_current_read_address += bytes_read;
    return (int16_t)bytes_read;
}

void SpiFlashStorage::EndRead()
{
    // Nema posebnog zatvaranja
}

bool SpiFlashStorage::BeginWrite(uint32_t address)
{
    Serial.printf("[SpiFlashStorage] Zapoceo pisanje na adresu 0x%lX\r\n", address);
    m_current_write_address = address;
    return true;
}

bool SpiFlashStorage::WriteChunk(uint8_t* data, uint16_t length)
{
    if (m_flash.writeBuffer(m_current_write_address, data, length))
    {
        m_current_write_address += length;
        return true;
    }
    return false;
}

bool SpiFlashStorage::EndWrite()
{
    Serial.println(F("[SpiFlashStorage] Pisanje zavrseno."));
    return true;
}

bool SpiFlashStorage::EraseSlot(uint32_t slot_address, uint32_t slot_size)
{
    Serial.printf("[SpiFlashStorage] Brisanje slota na adresi 0x%lX, velicina %d KB...\r\n", slot_address, slot_size / 1024);
    // Napomena: trenutna verzija Adafruit_SPIFlash biblioteke u projektu ne izgleda
    // da izlaže metodu `erase(address, size)` direktno. Implementacija brisanja
    // ovisi o nativnim metodama biblioteke (npr. eraseSector/eraseBlock).
    // Za sada izbjegavamo pozvati nepostojeću metodu i vraćamo false kako bi
    // pozivitelj znao da brisanje nije izvedeno.
    Serial.println(F("[SpiFlashStorage] EraseSlot: not implemented for this library version."));
    (void)slot_address;
    (void)slot_size;
    return false;
}