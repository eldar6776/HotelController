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
#include <pgmspace.h>

// Globalni objekti (extern)
extern AppConfig g_appConfig; 
extern VirtualGpio g_virtualGpio; 

// CMD-ovi za Update (iz httpd_cgi_ssi.c)
#define CMD_DWNLD_FWR_IMG   0x77U // fuf
#define CMD_DWNLD_BLDR_IMG  0x78U // buf
#define CMD_RT_DWNLD_FWR    0x79U // tuf
#define CMD_RT_DWNLD_BLDR   0x7AU // tub
#define CMD_RT_DWNLD_LOGO   0x7BU // tlg
// Stara komanda za update (cud)
#define CMD_OLD_UPDATE_FWR  0x17 // Pretpostavka za "UPDATE_FWR" iz hotel_ctrl.c

HttpServer::HttpServer() : 
    m_server(80) 
{
    // Konstruktor
}

void HttpServer::Initialize(
    HttpQueryManager* pHttpQueryManager,
    UpdateManager* pUpdateManager,
    EepromStorage* pEepromStorage,
    SpiFlashStorage* pSpiFlashStorage
)
{
    Serial.println(F("[HttpServer] Inicijalizacija..."));
    m_http_query_manager = pHttpQueryManager;
    m_update_manager = pUpdateManager;
    m_eeprom_storage = pEepromStorage;
    m_spi_storage = pSpiFlashStorage;

    // 1. Serviranje glavne stranice (Frontend)
    m_server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->HandleRoot(request);
    });

    // 2. Glavni CGI handler (Backend)
    m_server.on("/sysctrl.cgi", HTTP_GET, [this](AsyncWebServerRequest *request) {
        this->HandleSysctrlRequest(request);
    });

    // 3. Handler za upload FW fajlova (POST)
    m_server.on("/upload-firmware", HTTP_POST, 
        [](AsyncWebServerRequest *request){
            request->send(200, "text/plain", "Upload OK. Session finished.");
        }, 
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            this->HandleFileUpload(request, filename, index, data, len, final);
        }
    );

    m_server.onNotFound([this](AsyncWebServerRequest *request){
        this->HandleNotFound(request);
    });

    m_server.begin();
    Serial.println(F("[HttpServer] Server pokrenut."));
}

/**
 * @brief Servira glavnu HTML stranicu iz PROGMEM.
 */
void HttpServer::HandleRoot(AsyncWebServerRequest *request)
{
    request->send_P(200, "text/html", INDEX_HTML);
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
 *
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
 * @brief Pomoćna funkcija za parsiranje listi adresa (iz httpd_cgi_ssi.c)
 *
 */
void HttpServer::BuildAddressList(const String& firstAddrStr, const String& lastAddrStr, uint16_t* list, uint16_t* count)
{
    // TODO: Implementirati punu logiku za čitanje adresa iz EepromStorage (addr_list)
    // kao što to radi httpd_cgi_ssi.c.
    // Trenutno, samo podržavamo jednu adresu ili puni opseg (ako je lista prazna).
    
    *count = 0;
    uint16_t first_addr = ParseMacros(firstAddrStr).toInt();
    uint16_t last_addr = ParseMacros(lastAddrStr).toInt();

    if (first_addr == 0 || last_addr == 0 || last_addr < first_addr) {
        return;
    }

    // Za sada, samo vraćamo prvu adresu
    list[0] = first_addr;
    *count = 1;
    
    // Ako je opseg, dodaj sve adrese u opsegu (simplifikovano)
    // Oprez: Ovo može biti velika lista!
    /*
    for (uint16_t addr = first_addr; addr <= last_addr; addr++) {
        if (*count < MAX_ADDRESS_LIST_SIZE) { // Koristi neku definisanu max veličinu
            list[*count] = addr;
            (*count)++;
        } else {
            break; 
        }
    }
    */
}

/**
 * @brief Pomoćna funkcija za parsiranje listi fajlova (iz httpd_cgi_ssi.c)
 *
 */
void HttpServer::BuildFileList(const String& firstFileStr, const String& lastFileStr, uint8_t* list, uint16_t* count)
{
    *count = 0;
    uint8_t first_file = firstFileStr.toInt();
    uint8_t last_file = lastFileStr.toInt();

    if (first_file == 0 || last_file == 0 || last_file < first_file) {
        return;
    }

    for (uint8_t img = first_file; img <= last_file; img++) {
        if (*count < 25) { // Ograničenje
            list[*count] = img;
            (*count)++;
        } else {
            break;
        }
    }
}


/**
 * @brief Pomaže pri pokretanju Update sesije.
 * Trenutno podržava samo jednu po jednu adresu.
 */
bool HttpServer::StartUpdateSession(AsyncWebServerRequest *request, uint8_t updateCmd, const String& addrParam, const String& lastAddrParam)
{
    uint16_t address_list[MAX_ADDRESS_LIST_SIZE];
    uint16_t address_count = 0;
    
    BuildAddressList(addrParam, lastAddrParam, address_list, &address_count);
    
    if (address_count == 0) return false;

    // TODO: Pokreni sesiju za svaku adresu u address_list.
    // Za sada, pokrećemo samo za prvu.
    uint8_t clientAddr = (uint8_t)address_list[0];
    
    return m_update_manager->StartSession(clientAddr, updateCmd);
}

// ============================================================================
// GLAVNI CGI HANDLER
// Replicira logiku iz httpd_cgi_ssi.c
// ============================================================================
void HttpServer::HandleSysctrlRequest(AsyncWebServerRequest *request)
{
    Serial.println(F("[HttpServer] Primljen /sysctrl.cgi zahtjev..."));

    // --- TODO: Komande koje NISU u httpd_cgi_ssi.c ali JESU u Procitaj.txt ---
    // set (postavljanje password-a)
    // wgi (wiegand)
    // ...i ostale

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
    cmd.string_len = 0;
    String targetAddrStr;
    bool is_blocking = false;
    
    // ========================================================================
    // --- LOKALNE KOMANDE (Ne idu na RS485 bus) ---
    // ========================================================================

    // --- HC led control: HCled ---
    //
    if (request->hasParam("HCled"))
    {
        int led_state = request->getParam("HCled")->value().toInt();
        if (led_state >= 0 && led_state <= 3) {
            g_virtualGpio.SetStatusLed((LedState)led_state); 
            request->send(200, "text/plain", "OK (HCled)"); return;
        }
    }

    // --- HC set IP addresse: ipa, snm, gwa ---
    //
    if (request->hasParam("ipa") && request->hasParam("snm") && request->hasParam("gwa"))
    {
        g_appConfig.ip_address = IpStringToUint(request->getParam("ipa")->value());
        g_appConfig.subnet_mask = IpStringToUint(request->getParam("snm")->value());
        g_appConfig.gateway = IpStringToUint(request->getParam("gwa")->value());
        if (m_eeprom_storage->WriteConfig(&g_appConfig)) {
            request->send(200, "text/plain", "OK (ipa). Restart required."); return;
        }
        request->send(500, "text/plain", "ERROR: EEPROM write failed."); return;
    }

    // --- HC update rtc date & time: tdu / DTset ---
    //
    if (request->hasParam("tdu") || request->hasParam("DTset"))
    {
        String dt = request->hasParam("tdu") ? request->getParam("tdu")->value() : request->getParam("DTset")->value();
        
        // Format: WDDMMYYYYHHMMSS (15 karaktera)
        if (dt.length() == 15) 
        {
            int weekday = dt.substring(0, 1).toInt(); // W
            int day = dt.substring(1, 3).toInt();     // DD
            int month = dt.substring(3, 5).toInt();   // MM
            int year = dt.substring(5, 9).toInt();    // YYYY
            int hour = dt.substring(9, 11).toInt();   // HH
            int minute = dt.substring(11, 13).toInt(); // MM
            int second = dt.substring(13, 15).toInt(); // SS
            
            struct tm tm_time = {};
            tm_time.tm_year = year - 1900; 
            tm_time.tm_mon = month - 1;    
            tm_time.tm_mday = day;
            tm_time.tm_hour = hour;
            tm_time.tm_min = minute;
            tm_time.tm_sec = second;
            
            time_t t = mktime(&tm_time);
            if (t != (time_t)-1)
            {
                struct timeval now = {.tv_sec = t};
                settimeofday(&now, nullptr);
                request->send(200, "text/plain", "OK (tdu)"); return;
            }
        }
        request->send(400, "text/plain", "Invalid DTset/tdu format. Expected WDDMMYYYYHHMMSS."); return;
    }

    // --- HC log list request: log ---
    //
    if (request->hasParam("log") || request->hasParam("RQlog"))
    {
        String log_op = request->getParam(request->hasParam("log") ? "log" : "RQlog")->value();
        if (log_op == "3" || log_op.equalsIgnoreCase("RDlog"))
        {
            // TODO: Implementirati čitanje *svih* logova. Za sada čitamo samo najstariji.
            LogEntry entry;
            if (m_eeprom_storage->GetOldestLog(&entry) == LoggerStatus::LOGGER_OK)
            {
                char log_str[128];
                snprintf(log_str, 128, "Najstariji Log: ID: %u, Event: 0x%X, Addr: 0x%X, Time: %lu", 
                    entry.log_id, entry.event_code, entry.device_addr, entry.timestamp);
                request->send(200, "text/plain", log_str);
            }
            else { request->send(200, "text/plain", "Log je prazan (EMPTY)."); }
            return;
        }
        else if (log_op == "4" || log_op.equalsIgnoreCase("DLlog"))
        {
            if (m_eeprom_storage->DeleteOldestLog() == LoggerStatus::LOGGER_OK) {
                request->send(200, "text/plain", "OK (log=4) - Najstariji log obrisan.");
            } else { request->send(200, "text/plain", "Log je već prazan (EMPTY)."); }
            return;
        }
        else if (log_op == "5" || log_op.equalsIgnoreCase("DLlst"))
        {
             if (m_eeprom_storage->ClearAllLogs() == LoggerStatus::LOGGER_OK) {
                request->send(200, "text/plain", "OK (log=5) - Svi logovi obrisani.");
            } else { request->send(500, "text/plain", "ERROR: Brisanje logova nije uspjelo."); }
            return;
        }
        request->send(400, "text/plain", "Invalid Log CMD"); return;
    }

    // --- HC update firmware: fwu ---
    //
    if (request->hasParam("fwu") || request->hasParam("HCfwu"))
    {
        // TODO: Implementirati BLDR_Clear() i BLDR_Enable() logiku
        Serial.println("[HttpServer] Restarting system for update (fwu=hc)...");
        request->send(200, "text/plain", "OK (fwu=hc). Restarting...");
        delay(100);
        ESP.restart();
        return;
    }
    
    // --- HC load address list: cad ---
    //
    if (request->hasParam("cad") || request->hasParam("HClst"))
    {
        // TODO: Implementirati HC_LoadAddrList() (citanje sa SD/SPIFFS)
        request->send(501, "text/plain", "Not Implemented: (cad) - SD/FS read"); return;
    }

    // --- update weather forecast: WFset ---
    //
    if (request->hasParam("WFset"))
    {
        // TODO: Implementirati logiku za pisanje u EEPROM (EE_FORECAST_ADD)
        request->send(501, "text/plain", "Not Implemented: (WFset)"); return;
    }
    
    // ========================================================================
    // --- NE-BLOKIRAJUĆE KOMANDE (Update) ---
    // ========================================================================

    // --- RC update old firmware: cud ---
    //
    if (request->hasParam("cud"))
    {
        if (StartUpdateSession(request, CMD_OLD_UPDATE_FWR, request->getParam("cud")->value(), request->getParam("cud")->value()))
        {
             request->send(200, "text/plain", "OK (cud) - Update Session Started."); return;
        }
    }

    // --- RC update firmware: fuf, ful ---
    //
    if (request->hasParam("fuf") && request->hasParam("ful"))
    {
        if (StartUpdateSession(request, CMD_DWNLD_FWR_IMG, request->getParam("fuf")->value(), request->getParam("ful")->value()))
        {
             request->send(200, "text/plain", "OK (fuf) - Update Session Started."); return;
        }
    }

    // --- RC update bootloader: buf, bul ---
    //
    if (request->hasParam("buf") && request->hasParam("bul"))
    {
        if (StartUpdateSession(request, CMD_DWNLD_BLDR_IMG, request->getParam("buf")->value(), request->getParam("bul")->value()))
        {
             request->send(200, "text/plain", "OK (buf) - Update Session Started."); return;
        }
    }

    // --- RT update firmware: tuf, owa ---
    //
    if (request->hasParam("tuf") && request->hasParam("owa"))
    {
        if (StartUpdateSession(request, CMD_RT_DWNLD_FWR, request->getParam("tuf")->value(), request->getParam("tuf")->value()))
        {
             request->send(200, "text/plain", "OK (tuf) - Update Session Started."); return;
        }
    }

    // --- upload RT display user logo image: tlg, owa ---
    //
    if (request->hasParam("tlg") && request->hasParam("owa"))
    {
        if (StartUpdateSession(request, CMD_RT_DWNLD_LOGO, request->getParam("tlg")->value(), request->getParam("tlg")->value()))
        {
             request->send(200, "text/plain", "OK (tlg) - Update Session Started."); return;
        }
    }

    // --- RC update display image: iuf, iul, ifa, ila ---
    //
    if (request->hasParam("iuf") && request->hasParam("iul") && request->hasParam("ifa") && request->hasParam("ila"))
    {
        // TODO: Implementirati petlju za više adresa (iul) i više slika (ila)
        int img_index = request->getParam("ifa")->value().toInt();
        if (img_index >= 1 && img_index <= 14) // 14 slika
        {
            uint8_t updateCmd = CMD_IMG_RC_START + img_index - 1; 
            if (StartUpdateSession(request, updateCmd, request->getParam("iuf")->value(), request->getParam("iul")->value()))
            {
                request->send(200, "text/plain", "OK (iuf) - Update Session Started."); return;
            }
        }
    }

    // ========================================================================
    // --- BLOKIRAJUĆE KOMANDE (RS485 Upiti) ---
    // ========================================================================

    // --- HC RC RT reset controller: rst ---
    //
    if (request->hasParam("rst"))
    {
        targetAddrStr = request->getParam("rst")->value();
        if (targetAddrStr == "0" || ParseMacros(targetAddrStr) == String(g_appConfig.rs485_iface_addr))
        {
            request->send(200, "text/plain", "OK (rst=0). Restarting HC...");
            delay(100);
            ESP.restart();
            return;
        }
        cmd.cmd_id = RESTART_CTRL; 
        is_blocking = true;
    }
    // --- update hotel status: HSset ---
    //
    else if (request->hasParam("HSset"))
    {
        targetAddrStr = "RSbra"; // Broadcast
        cmd.cmd_id = DWNLD_JRNL;
        cmd.string_len = request->getParam("HSset")->value().length();
        strncpy((char*)cmd.string_ptr, request->getParam("HSset")->value().c_str(), 255);
        is_blocking = true;
    }
    // --- HC set system ID: sid, nid ---
    //
    else if (request->hasParam("sid") && request->hasParam("nid"))
    {
        targetAddrStr = request->getParam("sid")->value();
        uint16_t target_addr = ParseMacros(targetAddrStr).toInt();
        uint16_t new_id = request->getParam("nid")->value().toInt();
        
        if (target_addr == g_appConfig.rs485_iface_addr)
        {
            g_appConfig.system_id = new_id;
            m_eeprom_storage->WriteConfig(&g_appConfig);
            
            targetAddrStr = "RSbra"; // Broadcast
            cmd.address = g_appConfig.rs485_bcast_addr;
            cmd.cmd_id = SET_SYSTEM_ID;
            cmd.param1 = new_id;
            is_blocking = true;
        }
        else
        {
            cmd.cmd_id = SET_SYSTEM_ID;
            cmd.param1 = new_id;
            is_blocking = true;
        }
    }
    // --- RC set room status: stg, val ---
    //
    else if (request->hasParam("stg") && request->hasParam("val"))
    {
        targetAddrStr = request->getParam("stg")->value();
        cmd.cmd_id = SET_APPL_STAT;
        cmd.param1 = request->getParam("val")->value().toInt();
        is_blocking = true;
    }
    // --- RC set bedding period: sbr, per ---
    //
    else if (request->hasParam("sbr") && request->hasParam("per"))
    {
        targetAddrStr = request->getParam("sbr")->value();
        cmd.cmd_id = SET_BEDDING_REPL;
        cmd.param1 = request->getParam("per")->value().toInt();
        is_blocking = true;
    }
    // --- RC room status request: cst ---
    //
    else if (request->hasParam("cst"))
    {
        targetAddrStr = request->getParam("cst")->value();
        cmd.cmd_id = GET_APPL_STAT; 
        is_blocking = true;
    }
    // --- RC preview display image: ipr ---
    //
    else if (request->hasParam("ipr"))
    {
        targetAddrStr = request->getParam("ipr")->value();
        cmd.cmd_id = PREVIEW_DISPL_IMG; 
        is_blocking = true;
    }
    // --- RC set digital output: cdo ---
    //
    else if (request->hasParam("cdo") && request->hasParam("ctrl"))
    {
        targetAddrStr = request->getParam("cdo")->value();
        cmd.cmd_id = CMD_SET_DOUT_STATE;
        for (int i = 0; i <= 7; i++) { 
             cmd.string_ptr[i] = request->getParam("do" + String(i))->value().toInt(); 
        }
        cmd.string_ptr[8] = request->getParam("ctrl")->value().toInt();
        cmd.string_len = 9;
        is_blocking = true;
    }
    // --- RC set digital input: cdi ---
    //
    else if (request->hasParam("cdi"))
    {
        targetAddrStr = request->getParam("cdi")->value();
        cmd.cmd_id = CMD_SET_DIN_CFG;
        for (int i = 0; i <= 7; i++) { 
            cmd.string_ptr[i] = request->getParam("di" + String(i))->value().toInt(); 
        }
        cmd.string_len = 8;
        is_blocking = true;
    }
    // --- RC set display brightness: cbr ---
    //
    else if (request->hasParam("cbr") && request->hasParam("br"))
    {
        targetAddrStr = request->getParam("cbr")->value();
        cmd.cmd_id = SET_DISPL_BCKLGHT;
        cmd.param1 = request->getParam("br")->value().toInt();
        is_blocking = true;
    }
    // --- RC set permited group: pga, pgu ---
    //
    else if (request->hasParam("pga") && request->hasParam("pgu"))
    {
        targetAddrStr = request->getParam("pga")->value();
        cmd.cmd_id = SET_PERMITED_GROUP;
        strncpy((char*)cmd.string_ptr, request->getParam("pgu")->value().c_str(), 16);
        cmd.string_len = 16;
        is_blocking = true;
    }
    // --- RC SOS alarm reset request: rud ---
    //
    else if (request->hasParam("rud"))
    {
        targetAddrStr = request->getParam("rud")->value();
        cmd.cmd_id = RESET_SOS_ALARM; 
        is_blocking = true;
    }
    // --- set room display thermostat: tha ---
    //
    else if (request->hasParam("tha"))
    {
        targetAddrStr = request->getParam("tha")->value();
        cmd.cmd_id = SET_ROOM_TEMP;
        uint8_t configByte = 0x00; // Po defaultu 0
        
        // Logika za pakovanje configByte iz httpd_cgi_ssi.c (lin. 830-873)
        // Ako parametar nije poslan, ne postavljamo "NewSet" flag
        if (request->hasParam("spt")) cmd.param1 = request->getParam("spt")->value().toInt();
        else cmd.param1 = 0; // Ovdje treba bolja default vrijednost

        if (request->hasParam("dif")) cmd.param2 = request->getParam("dif")->value().toInt();
        else cmd.param2 = 0; // Default

        if (request->hasParam("sta")) {
            configByte |= (1 << 4); // NewStaSet
            if (request->getParam("sta")->value() == "ON") configByte |= (1 << 0); // TempRegOn
            else configByte &= ~(1 << 0); // TempRegOff
        }
        if (request->hasParam("mod")) {
            configByte |= (1 << 5); // NewModSet
            if (request->getParam("mod")->value() == "HEAT") configByte |= (1 << 1); // TempRegHeating
            else configByte &= ~(1 << 1); // TempRegCooling
        }
        if (request->hasParam("ctr")) {
            configByte |= (1 << 6); // NewCtrSet
            if (request->getParam("ctr")->value() == "ENA") configByte |= (1 << 2);
            else configByte &= ~(1 << 2);
        }
        if (request->hasParam("out")) {
            configByte |= (1 << 7); // NewOutSet
            if (request->getParam("out")->value() == "ON") configByte |= (1 << 3);
            else configByte &= ~(1 << 3);
        }
        
        cmd.param3 = configByte;
        is_blocking = true;
    }
    // --- RC set rs485 address: rsc, rsa, rga, rba, rib ---
    //
    else if (request->hasParam("rsc") && request->hasParam("rsa") && request->hasParam("rga") && request->hasParam("rba") && request->hasParam("rib"))
    {
        targetAddrStr = request->getParam("rsc")->value();
        uint16_t target_addr = ParseMacros(targetAddrStr).toInt();
        
        uint16_t new_rsa = ParseMacros(request->getParam("rsa")->value()).toInt();
        uint16_t new_rga = ParseMacros(request->getParam("rga")->value()).toInt();
        uint16_t new_rba = ParseMacros(request->getParam("rba")->value()).toInt();
        uint8_t new_rib = request->getParam("rib")->value().toInt();

        // Pakovanje 7 bajtova: rsa(2) + rga(2) + rba(2) + rib(1)
        cmd.string_ptr[0] = (new_rsa >> 8) & 0xFF; cmd.string_ptr[1] = new_rsa & 0xFF;
        cmd.string_ptr[2] = (new_rga >> 8) & 0xFF; cmd.string_ptr[3] = new_rga & 0xFF;
        cmd.string_ptr[4] = (new_rba >> 8) & 0xFF; cmd.string_ptr[5] = new_rba & 0xFF;
        cmd.string_ptr[6] = new_rib;
        cmd.string_len = 7;
        
        if (target_addr == g_appConfig.rs485_iface_addr)
        {
            g_appConfig.rs485_iface_addr = new_rsa;
            g_appConfig.rs485_group_addr = new_rga;
            g_appConfig.rs485_bcast_addr = new_rba;
            m_eeprom_storage->WriteConfig(&g_appConfig);
            
            // TODO: Pozvati Rs485Service->Reinitialize(new_baud_rate)
            request->send(200, "text/plain", "OK (rsc). RS485 Config written to local EEPROM.");
            return;
        }
        else
        {
            cmd.cmd_id = SET_RS485_CFG;
            is_blocking = true;
        }
    }
    // --- RT display text on screen: txa ---
    //
    else if (request->hasParam("txa") && request->hasParam("txt"))
    {
        targetAddrStr = request->getParam("txa")->value();
        cmd.cmd_id = CMD_RT_DISP_MSG;
        
        uint16_t tx0 = request->getParam("tx0")->value().toInt();
        uint16_t ty0 = request->getParam("ty0")->value().toInt();
        uint16_t tx1 = request->getParam("tx1")->value().toInt();
        uint16_t ty1 = request->getParam("ty1")->value().toInt();

        cmd.string_ptr[0] = request->getParam("trc")->value().toInt();
        cmd.string_ptr[1] = (tx0 >> 8) & 0xFF; cmd.string_ptr[2] = tx0 & 0xFF;
        cmd.string_ptr[3] = (ty0 >> 8) & 0xFF; cmd.string_ptr[4] = ty0 & 0xFF;
        cmd.string_ptr[5] = (tx1 >> 8) & 0xFF; cmd.string_ptr[6] = tx1 & 0xFF;
        cmd.string_ptr[7] = (ty1 >> 8) & 0xFF; cmd.string_ptr[8] = ty1 & 0xFF;
        cmd.string_ptr[9] = request->getParam("txc")->value().toInt();
        cmd.string_ptr[10] = request->getParam("txf")->value().toInt();
        cmd.string_ptr[11] = request->getParam("txh")->value().toInt();
        cmd.string_ptr[12] = request->getParam("txv")->value().toInt();
        
        String txt = request->getParam("txt")->value();
        strncpy((char*)&cmd.string_ptr[13], txt.c_str(), 255 - 13);
        cmd.string_len = 13 + txt.length();
        
        is_blocking = true;
    }
    // --- RT set display state: tda ---
    //
    else if (request->hasParam("tda") && request->hasParam("tdn"))
    {
        targetAddrStr = request->getParam("tda")->value();
        cmd.cmd_id = RT_SET_DISP_STA;
        cmd.string_ptr[0] = request->getParam("tdn")->value().toInt();
        cmd.string_ptr[1] = request->getParam("tdi")->value().toInt();
        cmd.string_ptr[2] = request->getParam("tdt")->value().toInt();
        cmd.string_ptr[3] = request->getParam("tbm")->value().toInt();
        cmd.string_ptr[4] = request->getParam("tbt")->value().toInt();
        cmd.string_len = 5;
        is_blocking = true;
    }
    // --- RT update/display qr code: qra ---
    //
    else if (request->hasParam("qra"))
    {
        targetAddrStr = request->getParam("qra")->value();
        if (request->hasParam("qrc")) // Upload QR
        {
            cmd.cmd_id = CMD_RT_UPD_QRC;
            String qrc = request->getParam("qrc")->value();
            // Zamjena '-' sa '&' i '+' sa '='
            qrc.replace('-', '&');
            qrc.replace('+', '=');
            strncpy((char*)cmd.string_ptr, qrc.c_str(), 255);
            cmd.string_len = qrc.length();
        }
        else if (request->hasParam("qrd")) // Display QR
        {
            cmd.cmd_id = RT_DISP_QRC;
            cmd.string_len = 0;
        }
        is_blocking = true;
    }

    // ========================================================================
    // --- IZVRŠENJE BLOKIRAJUĆEG UPITA ---
    // ========================================================================
    if (is_blocking)
    {
        cmd.address = ParseMacros(targetAddrStr).toInt();
        cmd.owa_addr = 0;
        if (request->hasParam("owa")) {
            cmd.owa_addr = ParseMacros(request->getParam("owa")->value()).toInt();
        }

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        memset(response_buffer, 0, MAX_PACKET_LENGTH);

        if (m_http_query_manager->ExecuteBlockingQuery(&cmd, response_buffer))
        {
            String response_str = (char*)response_buffer;
            request->send(200, "text/plain", response_str.length() > 0 ? response_str : "OK");
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
    // TODO: Ova funkcija treba biti proširena da koristi query parametre (npr. ?slot=fwrc)
    // za određivanje adrese pisanja, kao što je opisano u Planu Rada.
    
    uint32_t flash_address = SLOT_ADDR_FW_RC; // Privremeno fiksirano
    uint32_t slot_size = SLOT_SIZE_128K; // Privremeno fiksirano

    if (index == 0)
    {
        Serial.printf("[HttpServer] Zapoceo upload fajla: %s\n", filename.c_str());

        // ISPRAVKA: Uklonjena pogrešna linija: request->_tempFile = (File)flash_address;
        
        // OBRISI SLOT PRIJE PISANJA
        if (!m_spi_storage->EraseSlot(flash_address, slot_size)) {
            Serial.println(F("[HttpServer] GRESKA: Neuspjelo brisanje slota."));
            return;
        }

        if (!m_spi_storage->BeginWrite(flash_address)) {
             Serial.println(F("[HttpServer] GRESKA: Neuspjeo BeginWrite."));
             return;
        }
    }

    // Pisanje chunk-a
    if (len > 0)
    {
        if (!m_spi_storage->WriteChunk(data, len))
        {
            Serial.println(F("[HttpServer] GRESKA: Pisanje u SPI Flash neuspjesno."));
        }
    }

    // Završni paket
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