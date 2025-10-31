/**
 ******************************************************************************
 * @file    UpdateManager.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header za UpdateManager modul.
 *
 * @note
 * Upravlja logikom update-a (FWR, BLDR, Slike).
 * Bazirano na 'update_manager.c'.
 * Implementira IRs485Manager interfejs.
 ******************************************************************************
 */

#ifndef UPDATE_MANAGER_H
#define UPDATE_MANAGER_H

#include "Rs485Service.h"
#include "SpiFlashStorage.h"

// Stanja iz 'update_manager.c'
enum UpdateState
{
    S_IDLE,
    S_STARTING,
    S_WAITING_FOR_START_ACK,
    S_SENDING_DATA,
    S_WAITING_FOR_DATA_ACK,
    S_FINISHING,
    S_WAITING_FOR_FINISH_ACK,
    S_COMPLETED_OK,
    S_FAILED,
    S_PENDING_CLEANUP
};

// Sesija iz 'update_manager.c'
struct UpdateSession
{
    UpdateState state;
    uint8_t     clientAddress;
    uint32_t    fw_size;
    uint32_t    fw_crc;
    uint32_t    fw_address_slot; // Adresa na SPI Flashu odakle citamo
    
    uint32_t    bytesSent;
    uint32_t    currentSequenceNum;
    uint8_t     retryCount;
    unsigned long timeoutStart;
};


class UpdateManager : public IRs485Manager
{
public:
    UpdateManager();
    void Initialize(Rs485Service* pRs485Service, SpiFlashStorage* pSpiStorage);

    /**
     * @brief Poziva HttpServer da zapocne novu sesiju. Ne-blokirajuce.
     */
    bool StartSession(uint8_t clientAddress, uint32_t fwAddressSlot);

    // Implementacija IRs485Manager interfejsa
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;

private:
    void SendStartRequest(UpdateSession* s);
    void SendDataPacket(UpdateSession* s);
    void SendFinishRequest(UpdateSession* s);
    void CleanupSession(UpdateSession* s);

    Rs485Service* m_rs485_service;
    SpiFlashStorage* m_spi_storage;

    // Podrzavamo samo jednu sesiju odjednom da pojednostavimo
    UpdateSession m_session;
};

#endif // UPDATE_MANAGER_H
