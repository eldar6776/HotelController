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


// --- RJEŠAVANJE KONFLIKTA MAKROA (za SdFat/FS redefinicije) ---
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

// Proslijedjujemo forward-deklaracije nasih Menadzera
class HttpQueryManager;
class UpdateManager;
class EepromStorage;

class HttpServer
{
public:
    HttpServer();
    void Initialize(
        HttpQueryManager* pHttpQueryManager,
        UpdateManager* pUpdateManager,
        EepromStorage* pEepromStorage
    );

private:
    void HandleSysctrlRequest(AsyncWebServerRequest *request);
    void HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void HandleNotFound(AsyncWebServerRequest *request);
    String ParseMacros(String input); // Za `RCgra`, `RSbra` itd.

    AsyncWebServer m_server;

    // Pokazivaci na module koje pozivamo
    HttpQueryManager* m_http_query_manager;
    UpdateManager* m_update_manager;
    EepromStorage* m_eeprom_storage;
};

#endif // HTTP_SERVER_H
