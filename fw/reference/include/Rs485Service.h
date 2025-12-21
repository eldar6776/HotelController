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

struct ProtocolSettings {
    uint32_t rx_tx_delay_ms;      // 3ms (Novi) vs 10ms (Stari)
    uint32_t response_timeout_ms; // 45ms (Novi) vs 78ms (Stari)
    uint16_t packet_payload_size; // 128 (Novi) vs 64 (Stari)
    bool use_single_byte_response;// false (Novi) vs true (Stari)
};

class Rs485Service
{
public:
    Rs485Service();
    void Initialize();
    void SetProtocol(ProtocolVersion version); // Konfiguracija protokola
    
    // Getters
    const ProtocolSettings& GetProtocolSettings() const { return m_settings; }
    uint16_t GetChunkSize() const { return m_settings.packet_payload_size; }

    bool SendPacket(const uint8_t* data, uint16_t length);
    int ReceivePacket(uint8_t* buffer, uint16_t buffer_size, uint32_t timeout_ms = 0); // 0 = koristi default iz settings

private:
    bool ValidatePacket(uint8_t* buffer, uint16_t length);
    uint16_t CalculateChecksum(uint8_t* buffer, uint16_t data_length); 

    HardwareSerial m_rs485_serial;
    ProtocolSettings m_settings;
};

#endif // RS485_SERVICE_H