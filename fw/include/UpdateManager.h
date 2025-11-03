/**
 ******************************************************************************
 * @file    UpdateManager.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header za UpdateManager modul.
 *
 * @note
 * Upravlja logikom update-a (FWR, BLDR, Slike).
 * REFAKTORISAN: Čita fajlove sa uSD kartice umjesto SPI Flash-a.
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
#include "SdCardManager.h"
#include "ProjectConfig.h" // DODATO: Da bi APP_START_DEL bio dostupan
#include <SD.h>

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
    S_PENDING_RESTART_CMD, // NOVO: Stanje za slanje START_BLDR nakon UPDATE_FWR
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

// Sesija iz 'update_manager.c' - REFAKTORISANA
struct UpdateSession
{
    UpdateState state;
    UpdateType  type;          
    uint8_t     clientAddress;
    uint32_t    fw_size;       
    uint32_t    fw_crc;        
    
    // REMOVED: uint32_t fw_address_slot; - više ne koristimo Flash adrese
    // NEW: Filesystem support
    String      filename;      // Ime fajla na uSD kartici (npr. "/NEW.BIN")
    File        fw_file;       // File objekat za čitanje tokom update-a
    
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
    void Initialize(Rs485Service* pRs485Service, SdCardManager* pSdCardManager);

    // CHANGED: Pokazivač na SdCardManager umjesto SpiFlashStorage
    SdCardManager* m_sd_card_manager;

    // Podržavamo samo jednu sesiju odjednom
    UpdateSession m_session;
    
    /**
     * @brief Poziva HttpServer da započne novu sesiju, koristi Update CMD kod.
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
    void SendRestartCommand(UpdateSession* s); // DODATO: Deklaracija funkcije
    void CleanupSession(UpdateSession* s);
    
    // REFAKTORISANA: Određuje ime fajla, otvara ga i čita metadatu
    bool PrepareSession(UpdateSession* s, uint8_t updateCmd); 

    // NEW: Pomoćne funkcije za CRC32
    uint32_t CalculateCRC32(File& file);
    bool ReadMetadataFromFile(const String& metaFilePath, uint32_t* size, uint32_t* crc);

    Rs485Service* m_rs485_service;
};

#endif // UPDATE_MANAGER_H