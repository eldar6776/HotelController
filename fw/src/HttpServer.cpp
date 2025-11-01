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
#include "SpiFlashStorage.h"
#include "VirtualGpio.h"
#include <cstring>
#include <time.h> 

// Globalni objekti (extern)
extern AppConfig g_appConfig; 
extern VirtualGpio g_virtualGpio; 

// Makro Definicije (Premješteno i ispravljeno)
#define SOH 0x01
#define EOT 0x04
#define SET_ROOM_TEMP 0xD6
#define GET_APPL_STAT 0xA1
#define RESET_SOS_ALARM 0xD4
#define SET_DISPL_BCKLGHT 0xD7
#define RESTART_CTRL 0xC0
#define SET_SYSTEM_ID 0xD8
#define SET_RS485_CFG 0xD1
#define SET_BEDDING_REPL 0xDA
#define GET_LOG_LIST 0xA3
#define DEL_LOG_LIST 0xD3

// RS485 Komande koje su nedostajale (iz HttpQueryManager.h/common.h)
#define CMD_SET_DIN_CFG 0xC6 // cdi
#define CMD_SET_DOUT_STATE 0xD2 // cdo
#define CMD_RT_DISP_MSG 0x48 // txa
#define CMD_RT_UPD_QRC 0x52 // qra

// CMD-ovi za Update
#define CMD_DWNLD_FWR_IMG   0x77U 
#define CMD_DWNLD_BLDR_IMG  0x78U 
#define CMD_RT_DWNLD_LOGO   0x7BU 
#define CMD_IMG_RC_START    0x64U 


HttpServer::HttpServer() : 
    m_server(80) 
{
    // Konstruktor
}

void HttpServer::Initialize(
    HttpQueryManager* pHttpQueryManager,
    UpdateManager* pUpdateManager,
    EepromStorage* pEepromStorage,
    SpiFlashStorage* pSpiFlashStorage // Dodato da se poklopi sa HttpServer.h
)
{
    Serial.println(F("[HttpServer] Inicijalizacija..."));
    m_http_query_manager = pHttpQueryManager;
    m_update_manager = pUpdateManager;
    m_eeprom_storage = pEepromStorage;
    m_spi_storage = pSpiFlashStorage;

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

    m_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", "<h1>Hotel Controller ESP32</h1><p>Konfiguraciona stranica (TODO)</p>");
    });

    m_server.onNotFound([this](AsyncWebServerRequest *request){
        this->HandleNotFound(request);
    });

    m_server.begin();
    Serial.println(F("[HttpServer] Server pokrenut."));
}

/**
 * @brief Pretvara IP string u uint32_t (Host Endian).
 */
uint32_t HttpServer::IpStringToUint(const String& ipString) {
    uint32_t ip = 0;
    int octet = 0;
    int shift = 24;
    for (size_t i = 0; i < ipString.length(); ++i) {
        char c = ipString.charAt(i);
        if (c == '.') {
            ip |= (uint32_t)octet << shift;
            octet = 0;
            shift -= 8;
        } else if (c >= '0' && c <= '9') {
            octet = octet * 10 + (c - '0');
        }
    }
    ip |= (uint32_t)octet << shift;
    return ip;
}

/**
 * @brief Zamjenjuje makroe (RCgra, RSbra, itd.) aktuelnim adresama.
 */
String HttpServer::ParseMacros(String input) 
{
    if (input.equalsIgnoreCase("RSbra")) return String(g_appConfig.rs485_bcast_addr);
    if (input.equalsIgnoreCase("HCgra")) return String(g_appConfig.rs485_group_addr); 
    if (input.equalsIgnoreCase("HCifa")) return String(g_appConfig.rs485_iface_addr);
    if (input.equalsIgnoreCase("RTgra")) return String(30855); // DEF_RT_RSGRA
    if (input.equalsIgnoreCase("RCgra")) return String(26486); // DEF_RC_RSGRA
    if (input.equalsIgnoreCase("OWbra")) return String(127); // DEF_OWBRA
    if (input.equalsIgnoreCase("RTgraOW")) return String(10); // DEF_RT_OWGRA
    
    return input; 
}

/**
 * @brief Pomaže pri pokretanju Update sesije za seriju slika/uređaja.
 */
bool HttpServer::StartUpdateSession(AsyncWebServerRequest *request, uint8_t updateCmd, const String& addrParam, const String& lastAddrParam)
{
    String clientAddrStr = ParseMacros(addrParam);
    uint8_t clientAddr = clientAddrStr.toInt();
    
    if (clientAddr == 0) return false;
    
    return m_update_manager->StartSession(clientAddr, updateCmd);
}

/**
 * @brief Parsira CGI komande iz 'Procitaj !!!.txt'.
 */
void HttpServer::HandleSysctrlRequest(AsyncWebServerRequest *request)
{
    Serial.println(F("[HttpServer] Primljen /sysctrl.cgi zahtjev..."));

    // Provera da li je update već u toku
    if (m_update_manager->m_session.state != S_IDLE)
    {
        request->send(503, "text/plain", "BUSY: Update session already in progress.");
        return;
    }

    // --- Inicijalizacija ---
    char buffer_data[256] = {0}; 
    HttpCommand cmd = {};
    cmd.string_ptr = (uint8_t*)buffer_data;
    String targetAddrStr;
    bool is_blocking = false;
    
    // --- LOKALNE/KONFIGURACIJSKE KOMANDE ---
    if (request->hasParam("ipa"))
    {
        if (request->hasParam("ipa") && request->hasParam("snm") && request->hasParam("gwa"))
        {
            g_appConfig.ip_address = IpStringToUint(request->getParam("ipa")->value());
            g_appConfig.subnet_mask = IpStringToUint(request->getParam("snm")->value());
            g_appConfig.gateway = IpStringToUint(request->getParam("gwa")->value());
            if (m_eeprom_storage->WriteConfig(&g_appConfig)) {
                request->send(200, "text/plain", "OK. Restart required for network changes."); return;
            }
        }
        request->send(400, "text/plain", "ERROR: Invalid IP/Subnet/Gateway parameters."); return;
    }
    
    if (request->hasParam("HCled"))
    {
        int led_state = request->getParam("HCled")->value().toInt();
        if (led_state >= 0 && led_state <= 3) {
            g_virtualGpio.SetStatusLed((LedState)led_state); 
            request->send(200, "text/plain", "OK"); return;
        }
    }

    // --- IMPLEMENTACIJA tdu / DTset ---
    if (request->hasParam("tdu") || request->hasParam("DTset"))
    {
        String dt = request->hasParam("tdu") ? request->getParam("tdu")->value() : request->getParam("DTset")->value();
        
        if (dt.length() == 15)
        {
            int weekday = dt.substring(0, 1).toInt();
            int day = dt.substring(1, 3).toInt();
            int month = dt.substring(3, 5).toInt();
            int year = dt.substring(5, 7).toInt() + 2000;
            int hour = dt.substring(7, 9).toInt();
            int minute = dt.substring(9, 11).toInt();
            int second = dt.substring(11, 13).toInt();
            
            struct tm tm_time = {};
            tm_time.tm_year = year - 1900; 
            tm_time.tm_mon = month - 1;    
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;
            tm_time.tm_isdst = -1; 
            
            time_t t = mktime(&tm_time);
            if (t != (time_t)-1)
            {
                struct timeval now = {.tv_sec = t};
                settimeofday(&now, nullptr);
                request->send(200, "text/plain", "RTC Date & Time Set OK."); return;
            }
        }
        request->send(400, "text/plain", "Invalid DTset/tdu format."); return;
    }
    
    // --- NE-BLOKIRAJUĆE KOMANDE (Update/FW) ---
    if (request->hasParam("fuf") || request->hasParam("buf") || request->hasParam("tlg") || request->hasParam("iuf"))
    {
        uint8_t updateCmd = 0;
        String addrParam = "";
        String lastAddrParam = "";
        
        if (request->hasParam("fuf") && request->hasParam("ful")) {
            updateCmd = CMD_DWNLD_FWR_IMG; addrParam = request->getParam("fuf")->value(); lastAddrParam = request->getParam("ful")->value();
        } else if (request->hasParam("buf") && request->hasParam("bul")) {
            updateCmd = CMD_DWNLD_BLDR_IMG; addrParam = request->getParam("buf")->value(); lastAddrParam = request->getParam("bul")->value();
        } else if (request->hasParam("tlg")) {
            updateCmd = CMD_RT_DWNLD_LOGO; addrParam = request->getParam("tlg")->value(); lastAddrParam = request->getParam("tlg")->value();
        }
        else if (request->hasParam("iuf") && request->hasParam("ifa")) {
            int img_index = request->getParam("ifa")->value().toInt();
            if (img_index >= 1 && img_index <= CMD_IMG_COUNT)
            {
                updateCmd = CMD_IMG_RC_START + img_index - 1; 
                addrParam = request->getParam("iuf")->value();
                lastAddrParam = request->getParam("iul")->value();
            }
        }

        if (updateCmd != 0 && StartUpdateSession(request, updateCmd, addrParam, lastAddrParam))
        {
             request->send(200, "text/plain", "Update Session Started: " + addrParam); return;
        }
        request->send(400, "text/plain", "Update CMD Invalid or Missing Parameters."); return;
    }


    // --- BLOKIRAJUĆE KOMANDE (RS485 Komunikacija) ---
    
    if (request->hasParam("cst"))
    {
        targetAddrStr = request->getParam("cst")->value();
        cmd.cmd_id = GET_APPL_STAT; is_blocking = true;
    }
    else if (request->hasParam("tha"))
    {
        targetAddrStr = request->getParam("tha")->value();
        cmd.cmd_id = SET_ROOM_TEMP;
        if (request->hasParam("spt")) cmd.param1 = request->getParam("spt")->value().toInt();
        if (request->hasParam("dif")) cmd.param2 = request->getParam("dif")->value().toInt();
        cmd.param3 = 0xFF; // Config byte placeholder
        is_blocking = true;
    }
    else if (request->hasParam("rud"))
    {
        targetAddrStr = request->getParam("rud")->value();
        cmd.cmd_id = RESET_SOS_ALARM; is_blocking = true;
    }
    else if (request->hasParam("sbr"))
    {
        if (request->hasParam("per"))
        {
            targetAddrStr = request->getParam("sbr")->value();
            cmd.cmd_id = SET_BEDDING_REPL;
            cmd.param1 = request->getParam("per")->value().toInt();
            is_blocking = true;
        }
    }
    else if (request->hasParam("cdo"))
    {
        targetAddrStr = request->getParam("cdo")->value();
        cmd.cmd_id = CMD_SET_DOUT_STATE;
        
        // Skupljanje 9 bajtova (do0 do do7 + ctrl) u string_ptr
        int len = 0;
        for (int i = 0; i <= 7; i++) { 
             if (request->hasParam("do" + String(i))) len += sprintf(buffer_data + len, "%s", request->getParam("do" + String(i))->value().c_str()); 
        }
        if (request->hasParam("ctrl")) len += sprintf(buffer_data + len, "%s", request->getParam("ctrl")->value().c_str());
        is_blocking = true;
    }
    else if (request->hasParam("cdi"))
    {
        targetAddrStr = request->getParam("cdi")->value();
        cmd.cmd_id = CMD_SET_DIN_CFG;
        
        // Skupljanje 8 bajtova (di0 do di7) u string_ptr
        int len = 0;
        for (int i = 0; i <= 7; i++) { 
            if (request->hasParam("di" + String(i))) len += sprintf(buffer_data + len, "%s", request->getParam("di" + String(i))->value().c_str()); 
        }
        is_blocking = true;
    }
    else if (request->hasParam("txa"))
    {
        targetAddrStr = request->getParam("txa")->value();
        cmd.cmd_id = CMD_RT_DISP_MSG;
        // Za txa (tekstualna poruka) se očekuje kompleksno formatiranje u string_ptr
        // Prosljeđujemo raw argumente, pretpostavljajući da HttpQueryManager barata formatom
        if (request->hasParam("txt")) strcpy(buffer_data, request->getParam("txt")->value().c_str());
        is_blocking = true;
    }
    else if (request->hasParam("qra"))
    {
        targetAddrStr = request->getParam("qra")->value();
        cmd.cmd_id = CMD_RT_UPD_QRC;
        if (request->hasParam("qrc")) {
            strcpy(buffer_data, request->getParam("qrc")->value().c_str());
        }
        is_blocking = true;
    }
    else if (request->hasParam("sid"))
    {
        if (request->hasParam("nid")) 
        {
            targetAddrStr = request->getParam("sid")->value();
            cmd.cmd_id = SET_SYSTEM_ID;
            cmd.param1 = request->getParam("nid")->value().toInt();
            is_blocking = true;
        }
    }
    else if (request->hasParam("rsc"))
    {
        if (request->hasParam("rsa") && request->hasParam("rga") && request->hasParam("rba") && request->hasParam("rib"))
        {
            targetAddrStr = request->getParam("rsc")->value();
            cmd.cmd_id = SET_RS485_CFG;
            // Skupljanje RS485 parametara u string_ptr
            int len = 0;
            len += sprintf(buffer_data + len, "%s", ParseMacros(request->getParam("rsa")->value()).c_str());
            len += sprintf(buffer_data + len, "%s", ParseMacros(request->getParam("rga")->value()).c_str());
            len += sprintf(buffer_data + len, "%s", ParseMacros(request->getParam("rba")->value()).c_str());
            len += sprintf(buffer_data + len, "%s", ParseMacros(request->getParam("rib")->value()).c_str());
            is_blocking = true;
        }
    }
    else if (request->hasParam("log"))
    {
        String log_op = request->getParam("log")->value();
        if (log_op == "3" || log_op.equalsIgnoreCase("RDlog")) cmd.cmd_id = GET_LOG_LIST;
        else if (log_op == "4" || log_op.equalsIgnoreCase("DLlog")) cmd.cmd_id = DEL_LOG_LIST;
        else { request->send(400, "text/plain", "Invalid Log CMD"); return; }
        
        targetAddrStr = String(g_appConfig.rs485_iface_addr); 
        is_blocking = true;
    }


    // 2. Izvršenje blokirajućeg upita
    if (is_blocking)
    {
        cmd.address = ParseMacros(targetAddrStr).toInt();
        if (request->hasParam("owa")) cmd.owa_addr = ParseMacros(request->getParam("owa")->value()).toInt();

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        memset(response_buffer, 0, MAX_PACKET_LENGTH);

        if (m_http_query_manager->ExecuteBlockingQuery(&cmd, response_buffer))
        {
            String response_str = (char*)response_buffer;
            
            if (cmd.cmd_id == GET_APPL_STAT) {
                 request->send(200, "text/plain", "RC Status: " + response_str);
            } else if (cmd.cmd_id == GET_LOG_LIST) {
                 request->send(200, "text/plain", "LOG: " + response_str);
            } else {
                 request->send(200, "text/plain", response_str.length() > 1 ? response_str : "ACK");
            }
        }
        else
        {
            request->send(504, "text/plain", "TIMEOUT");
        }
    }
    else
    {
        request->send(400, "text/plain", "Nepoznata CGI komanda ili nedostaju parametri.");
    }
}

/**
 * @brief Obradjuje upload fajla na SPI Flash.
 */
void HttpServer::HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    uint32_t flash_address = SLOT_ADDR_FW_RC; 
    
    if (index == 0)
    {
        Serial.printf("[HttpServer] Zapoceo upload fajla: %s\n", filename.c_str());
        m_spi_storage->BeginWrite(flash_address);
    }

    if (m_spi_storage->WriteChunk(data, len))
    {
        // Pisanje OK
    }
    else
    {
        Serial.println(F("[HttpServer] GRESKA: Pisanje u SPI Flash neuspjesno."));
    }

    if (final)
    {
        m_spi_storage->EndWrite();
        Serial.printf("[HttpServer] Zavrsen upload fajla: %s, Ukupno: %lu bajtova\n", filename.c_str(), index + len);
    }
}

void HttpServer::HandleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not Found");
}