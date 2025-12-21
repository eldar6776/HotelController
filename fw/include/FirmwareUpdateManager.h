/**
 ******************************************************************************
 * @file    FirmwareUpdateManager.h
 * @author  Gemini Code Assist
 * @brief   Header za FirmwareUpdateManager - ISKLJUČIVO za fuf/buf komande.
 *
 * @note
 * Ovaj modul je potpuno odvojen od UpdateManager-a da bi se osiguralo
 * da logika za transfer slika (iuf) ostane netaknuta.
 ******************************************************************************
 */

#ifndef FIRMWARE_UPDATE_MANAGER_H
#define FIRMWARE_UPDATE_MANAGER_H

#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif

#include "Rs485Service.h"
#include "SdCardManager.h"
#include "ProjectConfig.h"
#include <SD.h>

// Stanja su slična UpdateManager-u, ali specifična za ovaj modul
enum FufUpdateState
{
    FUF_S_IDLE,
    FUF_S_STARTING,
    FUF_S_WAITING_FOR_START_ACK,
    FUF_S_SENDING_DATA,
    FUF_S_WAITING_FOR_DATA_ACK,
    FUF_S_SENDING_RESTART_CMD,
    FUF_S_PENDING_APP_START,
    FUF_S_SENDING_APP_EXE,
    FUF_S_FAILED
};

// Tipovi transfera koje ovaj menadžer podržava
enum FufUpdateType
{
    FUF_TYPE_FIRMWARE, // fuf - IMG20.RAW
    FUF_TYPE_BOOTLOADER // buf - IMG21.RAW
};

// Struktura za praćenje sekvence
struct FufUpdateSequence
{
    bool is_active;
    uint16_t first_addr;
    uint16_t last_addr;
    uint16_t current_addr;
    FufUpdateType type;
};

// Struktura za praćenje sesije (jedan transfer)
struct FufUpdateSession
{
    FufUpdateState state;
    uint16_t clientAddress;
    String filename;
    File file_handle;
    uint32_t file_size;
    uint32_t file_crc;
    uint32_t bytesSent;
    uint32_t currentSequenceNum;
    uint8_t retryCount;
    uint8_t read_buffer[UPDATE_DATA_CHUNK_SIZE];
    uint16_t read_chunk_size;
};

class FirmwareUpdateManager
{
public:
    /**
     * @brief Konstruktor.
     */
    FirmwareUpdateManager();

    /**
     * @brief Inicijalizuje menadžera sa potrebnim servisima.
     * @param pRs485Service Pointer na RS485 servis.
     * @param pSdCardManager Pointer na SD Card menadžera.
     */
    void Initialize(Rs485Service* pRs485Service, SdCardManager* pSdCardManager);

    /**
     * @brief Postavlja referencu na HTTP server.
     * @param pHttpServer Pointer na HTTP server.
     */
    void SetHttpServer(class HttpServer* pHttpServer) { m_http_server = pHttpServer; }
    
    /**
     * @brief Pokreće sekvencu ažuriranja firmvera za opseg adresa.
     * @param first_addr Prva adresa u opsegu.
     * @param last_addr Zadnja adresa u opsegu.
     * @param type Tip ažuriranja (FIRMWARE ili BOOTLOADER).
     */
    void StartFirmwareUpdateSequence(uint16_t first_addr, uint16_t last_addr, FufUpdateType type);

    /**
     * @brief Glavna petlja menadžera. Treba se pozivati periodično.
     */
    void Run();

    /**
     * @brief Provjerava da li je proces ažuriranja aktivan.
     * @return true ako je aktivan, false inače.
     */
    bool IsActive();

    /**
     * @brief Zaustavlja trenutnu sekvencu ažuriranja.
     */
    void StopSequence();

private:
    bool StartSession(uint16_t clientAddress, FufUpdateType type);
    void CleanupSession(bool failed);
    void ProcessResponse(const uint8_t* packet, uint16_t length);
    void OnTimeout();

    void SendStartRequest();
    void SendDataPacket();
    void SendRestartCommand();
    void SendAppExeCommand();

    FufUpdateSequence m_sequence;
    FufUpdateSession m_session;
    Rs485Service* m_rs485_service;
    SdCardManager* m_sd_card_manager;
    class HttpServer* m_http_server;
};

#endif // FIRMWARE_UPDATE_MANAGER_H
