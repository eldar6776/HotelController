/**
 ******************************************************************************
 * @file    HttpServer.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za HttpServer modul.
 *
 * @note
 * Obradjuje sve HTTP CGI GET/POST komande.
 * Replicira `httpd_cgi_ssi.c`.
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
#include "index_html.h" // Uključujemo naš novi frontend

// Proslijedjujemo forward-deklaracije nasih Menadzera
class HttpQueryManager;
class UpdateManager;
class EepromStorage;
class SpiFlashStorage; // Za Upload

class HttpServer
{
public:
    HttpServer();
    void Initialize(
        HttpQueryManager* pHttpQueryManager,
        UpdateManager* pUpdateManager,
        EepromStorage* pEepromStorage,
        SpiFlashStorage* pSpiFlashStorage 
    );

private:
    void HandleRoot(AsyncWebServerRequest *request); // Servira frontend
    void HandleSysctrlRequest(AsyncWebServerRequest *request);
    void HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void HandleNotFound(AsyncWebServerRequest *request);
    
    // Funkcije za parsiranje
    String ParseMacros(String input); 
    uint32_t IpStringToUint(const String& ipString);
    bool StartUpdateSession(AsyncWebServerRequest *request, uint8_t updateCmd, const String& addrParam, const String& lastAddrParam);

    // Pomoćne funkcije za parsiranje listi (iz httpd_cgi_ssi.c)
    void BuildAddressList(const String& firstAddrStr, const String& lastAddrStr, uint16_t* list, uint16_t* count);
    void BuildFileList(const String& firstFileStr, const String& lastFileStr, uint8_t* list, uint16_t* count);


    AsyncWebServer m_server;

    // Pokazivaci na module koje pozivamo
    HttpQueryManager* m_http_query_manager;
    UpdateManager* m_update_manager;
    EepromStorage* m_eeprom_storage;
    SpiFlashStorage* m_spi_storage; // Za file upload
};

#endif // HTTP_SERVER_H