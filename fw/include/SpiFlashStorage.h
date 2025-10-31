/**
 ******************************************************************************
 * @file    SpiFlashStorage.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za SpiFlashStorage modul.
 *
 * @note
 * Upravlja eksternim SPI Flash cipom (W25Q128/W25Q512).
 * Koristi fiksnu memorijsku mapu iz ProjectConfig.h.
 ******************************************************************************
 */

#ifndef SPI_FLASH_STORAGE_H
#define SPI_FLASH_STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_SPIFlash.h>
#include "ProjectConfig.h"

class SpiFlashStorage
{
public:
    SpiFlashStorage();
    void Initialize(int8_t sck, int8_t miso, int8_t mosi, int8_t cs);
    
    // API za citanje
    bool BeginRead(uint32_t address);
    int16_t ReadChunk(uint8_t* buffer, uint16_t length);
    void EndRead();

    // API za pisanje
    bool BeginWrite(uint32_t address);
    bool WriteChunk(uint8_t* data, uint16_t length);
    bool EndWrite();
    
    bool EraseSlot(uint32_t slot_address, uint32_t slot_size);

private:
    // Eksterna biblioteka za upravljanje flashom
    Adafruit_FlashTransport_ESP32 m_flash_transport;
    Adafruit_SPIFlash m_flash;
    
    uint32_t m_current_read_address;
    uint32_t m_current_write_address;
};

#endif // SPI_FLASH_STORAGE_H
