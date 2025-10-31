/**
 ******************************************************************************
 * @file    HttpServer.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija HttpServer modula.
 ******************************************************************************
 */

#include "HttpServer.h"
#include "HttpQueryManager.h"
#include "UpdateManager.h"
#include "EepromStorage.h"

HttpServer::HttpServer() : 
    m_server(80) // Inicijalizacija servera na portu 80
{
    // Konstruktor
}

void HttpServer::Initialize(
    HttpQueryManager* pHttpQueryManager,
    UpdateManager* pUpdateManager,
    EepromStorage* pEepromStorage
)
{
    Serial.println(F("[HttpServer] Inicijalizacija..."));
    m_http_query_manager = pHttpQueryManager;
    m_update_manager = pUpdateManager;
    m_eeprom_storage = pEepromStorage;

    // --- Povezivanje (Binding) URL-ova ---

    // Glavni CGI handler
    m_server.on("/sysctrl.cgi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->HandleSysctrlRequest(request);
    });

    // Handler za upload FW fajlova (POST)
    m_server.on("/upload-firmware", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Upload OK");
        }, 
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            this->HandleFileUpload(request, filename, index, data, len, final);
        }
    );

    // TODO: Dodati web stranicu za konfiguraciju (HTML/CSS)
    m_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Hotel Controller ESP32</h1><p>Konfiguraciona stranica (TODO)</p>");
    });

    // 404 Handler
    m_server.onNotFound([this](AsyncWebServerRequest *request){
        this->HandleNotFound(request);
    });

    m_server.begin();
    Serial.println(F("[HttpServer] Server pokrenut."));
}

/**
 * @brief Parsira CGI komande iz 'Procitaj !!!.txt'.
 */
void HttpServer::HandleSysctrlRequest(AsyncWebServerRequest *request)
{
    Serial.println(F("[HttpServer] Primljen /sysctrl.cgi zahtjev..."));

    // TODO: Implementirati kompletan parser iz 'Procitaj !!!.txt'
    
    if (request->hasParam("cst"))
    {
        // Ovo je BLOKIRAJUCI upit
        String adresa = request->getParam("cst")->value();
        Serial.printf("[HttpServer] BLOKIRAJUCI UPIT: cst=%s\n", adresa.c_str());

        // TODO: Kreirati HttpCommand objekat
        // HttpCommand cmd;
        // ...
        
        uint8_t response_buffer[256];
        if (m_http_query_manager->ExecuteBlockingQuery(NULL, response_buffer))
        {
            // Uspjeh
            request->send(200, "text/plain", (char*)response_buffer);
        }
        else
        {
            // Timeout
            request->send(504, "text/plain", "TIMEOUT");
        }
    }
    else if (request->hasParam("fwu"))
    {
        // Ovo je NE-BLOKIRAJUCI upit
        Serial.println(F("[HttpServer] NE-BLOKIRAJUCI UPIT: fwu"));
        
        // m_updateManager->StartSession(...)
        request->send(200, "text/plain", "Update pokrenut...");
    }
    else
    {
        request->send(400, "text/plain", "Nepoznata CGI komanda.");
    }
}

/**
 * @brief Obradjuje upload fajla na SPI Flash.
 */
void HttpServer::HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (index == 0)
    {
        Serial.printf("[HttpServer] Zapoceo upload fajla: %s\n", filename.c_str());
        // TODO: Odrediti slot na osnovu imena fajla
        // m_spiFlashStorage->BeginWrite(SLOT_ADDR_FW_RC);
    }

    // m_spiFlashStorage->WriteChunk(data, len);

    if (final)
    {
        // m_spiFlashStorage->EndWrite();
        Serial.printf("[HttpServer] Zavrsen upload fajla: %s, Ukupno: %d bajtova\n", filename.c_str(), index + len);
    }
}

void HttpServer::HandleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not Found");
}

String HttpServer::ParseMacros(String input)
{
    // TODO: Implementirati zamjenu za RCgra, RSbra...
    return input;
}
