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
    S_SENDING_RESTART_CMD,       // Stanje za slanje START_BLDR komande
    S_WAITING_FOR_RESTART_ACK,   // NOVO: Čekanje na ACK nakon START_BLDR
    S_PENDING_FW_UPDATE,         // NOVO: Stanje koje prethodi slanju DWNLD_FWR
    S_PENDING_APP_START,         // Stanje za pauzu prije slanja APP_EXE
    S_COMPLETED_OK,
    S_FAILED,
    S_PENDING_CLEANUP
};

// NOVO: Struktura za praćenje sekvence ažuriranja slika
struct ImageUpdateSequence
{
    bool is_active;
    uint16_t first_addr;
    uint16_t last_addr;
    uint8_t first_img;
    uint8_t last_img;
    uint16_t current_addr;
    uint8_t current_img;
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

// Sesija iz 'update_manager.c' - REFAKTORISANA
struct UpdateSession
{
    UpdateState state;
    UpdateType  type;
    uint8_t     original_cmd; // NOVO: Čuva originalnu komandu (npr. CMD_DWNLD_FWR_IMG)
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
    // NAPOMENA: Buffer koristi maksimalnu veličinu chunk-a (128) da podrži oba protokola
    // STARI protokol (64B) koristi samo prvu polovinu, NOVI protokol (128B) koristi cijeli buffer
    uint8_t     read_buffer[UPDATE_DATA_CHUNK_SIZE]; 
    uint16_t    read_chunk_size;
};


class UpdateManager
{
public:
    UpdateManager();
    void Initialize(Rs485Service* pRs485Service, SdCardManager* pSdCardManager);
    void SetHttpServer(class HttpServer* pHttpServer) { m_http_server = pHttpServer; }

    /**
     * @brief Poziva HttpServer da započne novu sesiju, koristi Update CMD kod.
     */
    bool StartSession(uint8_t clientAddress, uint8_t updateCmd);

    /**
     * @brief NOVO: Pokreće sekvencu ažuriranja za više adresa i slika.
     * @param first_addr Prva adresa kontrolera.
     * @param last_addr Zadnja adresa kontrolera.
     * @param first_img Indeks prve slike.
     * @param last_img Indeks zadnje slike.
     */
    void StartImageUpdateSequence(uint16_t first_addr, uint16_t last_addr, uint8_t first_img, uint8_t last_img);

    // Glavna funkcija koju poziva state-mašina
    void Run();
    bool IsActive();
    bool was_interrupted; // Flag za nastavak

    /**
     * @brief NOVO: Provjerava da li je sekvencijalni update aktivan.
     */
    bool IsSequenceActive();
    
    /**
     * @brief NOVO: Zaustavlja sekvencu i ponovo pokreće HTTP server.
     */
    void StopSequence();

public:
    // Podržavamo samo jednu sesiju odjednom
    UpdateSession m_session; // Javno zbog HttpServer-a

    ImageUpdateSequence m_sequence; // NOVO: Stanje sekvence

private:
    void ProcessResponse(const uint8_t* packet, uint16_t length);
    void OnTimeout();
    
    void SendFirmwareStartRequest(); // NOVO: Za FW/BLDR
    void SendFileStartRequest();     // NOVO: Za Slike/Logo
    void SendDataPacket();
    void SendFinishRequest();
    void SendRestartCommand();
    void SendAppExeCommand(); // Deklaracija za novu funkciju
    void CleanupSession(bool failed = false);
    
    // REFAKTORISANA: Određuje ime fajla, otvara ga i čita metadatu
    bool PrepareSession(UpdateSession* s, uint8_t updateCmd); 

    // NEW: Pomoćne funkcije za CRC32
    uint32_t CalculateCRC32(File& file);
    bool ReadMetadataFromFile(const String& metaFilePath, uint32_t* size, uint32_t* crc);

    /**
     * @brief Vraća veličinu chunk-a (payload) za trenutni protokol.
     * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): 64 bajta
     *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): 128 bajtova (default)
     * @return Veličina chunk-a u bajtovima (64 ili 128)
     * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
     */
    uint16_t GetChunkSizeForProtocol();

    /**
     * @brief Vraća timeout za čekanje odgovora tokom update-a za trenutni protokol.
     * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): 78ms
     *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): 45ms (default)
     * @return Timeout u milisekundama (78 ili 45)
     * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
     */
    uint32_t GetUpdateTimeoutForProtocol();

    /**
     * @brief Provjerava da li trenutni protokol koristi single-byte ACK/NAK odgovore.
     * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): true (1 bajt ACK/NAK)
     *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): false (puni ACK paket sa headerom)
     * @return true ako protokol koristi single-byte ACK/NAK, false inače
     * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
     */
    bool UseSingleByteAckForProtocol();

    Rs485Service* m_rs485_service;
    SdCardManager* m_sd_card_manager;
    class HttpServer* m_http_server;
    uint8_t m_last_sent_sub_cmd; // NOVO: Čuva zadnju poslanu sub-komandu (npr. 0x64)
    
    // Zastavice za sekvencijalnu logiku
    bool m_first_image_in_sequence;
    bool m_session_in_progress;
};

#endif // UPDATE_MANAGER_H