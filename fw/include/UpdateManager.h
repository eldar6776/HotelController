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

// --- RJEŠAVANJE KONFLIKTA MAKROA (FS/SdFat) ---
#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif
// ------------------------------------------------------------

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

// Tipovi Update-a
enum UpdateType
{
    TYPE_FW_RC,     
    TYPE_BLDR_RC,   
    TYPE_IMG_RC,    // Raspon slika 0x64 do 0x71
    TYPE_LOGO_RT,   
    TYPE_FW_TH,     
    TYPE_BLDR_TH    
};

// Makroi za komande slika (preuzeto iz common.h)
#define CMD_IMG_RC_START    0x64U // DWNLD_DISP_IMG_1
#define CMD_IMG_RC_END      0x71U // DWNLD_DISP_IMG_14
#define CMD_IMG_COUNT       (CMD_IMG_RC_END - CMD_IMG_RC_START + 1)

// Sesija iz 'update_manager.c'
struct UpdateSession
{
    UpdateState state;
    UpdateType  type;          
    uint8_t     clientAddress;
    uint32_t    fw_size;       
    uint32_t    fw_crc;        
    uint32_t    fw_address_slot; 
    
    uint32_t    bytesSent;
    uint32_t    currentSequenceNum;
    uint8_t     retryCount;
    unsigned long timeoutStart;
    
    bool        is_read_active;
    uint8_t     read_buffer[UPDATE_DATA_CHUNK_SIZE]; 
    uint16_t    read_chunk_size;
};


class UpdateManager : public IRs485Manager
{
public:
    UpdateManager();
    void Initialize(Rs485Service* pRs485Service, SpiFlashStorage* pSpiStorage);

    SpiFlashStorage* m_spi_storage;

    // Podrzavamo samo jednu sesiju odjednom
    UpdateSession m_session;
    /**
     * @brief Poziva HttpServer da zapocne novu sesiju, koristi Update CMD kod.
     */
    bool StartSession(uint8_t clientAddress, uint8_t updateCmd);

    // Implementacija IRs485Manager interfejsa
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;

private:
    void SendStartRequest(UpdateSession* s);
    void SendDataPacket(UpdateSession* s);
    void SendFinishRequest(UpdateSession* s);
    void CleanupSession(UpdateSession* s);
    
    // Pomoćna funkcija za određivanje Flash adrese i čitanje meta podataka
    bool PrepareSession(UpdateSession* s, uint8_t updateCmd); 

    Rs485Service* m_rs485_service;
};

#endif // UPDATE_MANAGER_H