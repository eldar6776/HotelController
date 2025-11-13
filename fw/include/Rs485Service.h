/**
 ******************************************************************************
 * @file    Rs485Service.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za Rs485Service modul (Dispecer).
 ******************************************************************************
 */

#ifndef RS485_SERVICE_H
#define RS485_SERVICE_H

#include <Arduino.h>
#include <freertos/task.h> 
#include "ProjectConfig.h" 
#include "EepromStorage.h" // Za pristup AppConfig (rsifa)

enum class Rs485State
{
    IDLE,
    SENDING,
    WAITING,
    RECEIVING,
    TIMEOUT
};

class Rs485Service
{
public:
    Rs485Service();
    void Initialize();
    bool SendPacket(const uint8_t* data, uint16_t length);
    int ReceivePacket(uint8_t* buffer, uint16_t buffer_size, uint32_t timeout_ms);

private:
    bool ValidatePacket(uint8_t* buffer, uint16_t length);
    uint16_t CalculateChecksum(uint8_t* buffer, uint16_t data_length); 

    HardwareSerial m_rs485_serial;

};

#endif // RS485_SERVICE_H