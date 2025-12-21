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

    /**
     * @brief Aktivira single-byte ACK/NAK mod za STARI protokol (SAX/HILLS/itd).
     * @details Kada je aktiviran, ReceivePacket() odmah prihvata ACK (0x06) ili NAK (0x15)
     *          kao validne single-byte odgovore bez čekanja na puni paket.
     * @note Koristi se SAMO za transfer fajlova sa STARIM protokolom u UpdateManager-u.
     */
    void EnableSingleByteMode();

    /**
     * @brief Deaktivira single-byte ACK/NAK mod i vraća normalan način rada.
     * @details Poziva se nakon završetka transfera ili greške da vrati Rs485Service
     *          u normalno stanje za LogPullManager i ostale funkcije.
     */
    void DisableSingleByteMode();

private:
    bool ValidatePacket(uint8_t* buffer, uint16_t length);
    uint16_t CalculateChecksum(uint8_t* buffer, uint16_t data_length); 

    HardwareSerial m_rs485_serial;
    
    /**
     * @brief Flag koji omogućava single-byte ACK/NAK prijem za STARI protokol.
     * @details Kada je true, ReceivePacket() prihvata 1-byte ACK/NAK odmah.
     *          Kada je false, radi normalno (čeka puni paket) - za LogPullManager i ostale.
     */
    bool m_single_byte_mode;

};

#endif // RS485_SERVICE_H