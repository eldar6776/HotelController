/**
 ******************************************************************************
 * @file    HttpServer.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za HttpServer modul.
 *
 * @note
 * Obrađuje sve HTTP CGI GET/POST komande.
 * UPDATED: SSI odgovori, cad=load, file browser.
 ******************************************************************************
 */

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// --- RJEŠAVANJE KONFLIKTA MAKROA (FS/SdFat) ---
#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif
// ------------------------------------------------------------

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "ProjectConfig.h"
#include "index_html.h" // Uključujemo naš frontend

// Forward-deklaracije naših Menadžera
class HttpQueryManager;
class FirmwareUpdateManager; // NOVO
class UpdateManager;
class EepromStorage;
class SdCardManager; // CHANGED: Zamjenjen SpiFlashStorage

class HttpServer
{
public:
    HttpServer();
    void Initialize(
        HttpQueryManager* pHttpQueryManager,
        FirmwareUpdateManager* pFufUpdateManager, // NOVO
        UpdateManager* pUpdateManager,
        EepromStorage* pEepromStorage,
        SdCardManager* pSdCardManager  // CHANGED
    );
    void Start(); // Metoda za pokretanje servera
    void Stop();  // Metoda za potpuno zaustavljanje servera

private:
    void HandleRoot(AsyncWebServerRequest *request); // Servira frontend
    void HandleSysctrlRequest(AsyncWebServerRequest *request);
    void HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void HandleNotFound(AsyncWebServerRequest *request);
    
    // NEW: SSI Response Helper
    void SendSSIResponse(AsyncWebServerRequest *request, const String& message);
    
    // Funkcije za parsiranje
    String ParseMacros(String input); 
    uint32_t IpStringToUint(const String& ipString);
    bool StartUpdateSession(AsyncWebServerRequest *request, uint8_t updateCmd, const String& addrParam, const String& lastAddrParam);

    // Pomoćne funkcije za parsiranje listi
    void BuildAddressList(const String& firstAddrStr, const String& lastAddrStr, uint16_t* list, uint16_t* count);
    void BuildFileList(const String& firstFileStr, const String& lastFileStr, uint8_t* list, uint16_t* count);

    // Helper za Autentifikaciju (EEPROM + Backdoor)
    bool IsAuthenticated(AsyncWebServerRequest *request);

    AsyncWebServer m_server;

    // Pokazivači na module
    HttpQueryManager* m_http_query_manager;
    FirmwareUpdateManager* m_fuf_update_manager; // NOVO
    UpdateManager* m_update_manager;
    EepromStorage* m_eeprom_storage;
    SdCardManager* m_sd_card_manager; // CHANGED
};

#endif // HTTP_SERVER_H