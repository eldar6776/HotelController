/**
 ******************************************************************************
 * @file    HttpServer.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija HttpServer modula.
 *
 * @note
 * POTPUNO REFAKTORISAN:
 * - SSI Response Pattern (V1 Kompatibilnost)
 * - cad=load handler (Hibridni model lista adresa)
 * - File Upload na uSD karticu
 * - File Browser (list/delete)
 ******************************************************************************
 */
#include "NetworkManager.h"
#include "HttpServer.h"
#include "HttpQueryManager.h"
#include "UpdateManager.h"
#include "EepromStorage.h"
#include "SdCardManager.h"
#include "VirtualGpio.h"
#include "log_html.h" // SSI Template
#include <cstring>
#include <time.h>
#include <pgmspace.h>

// Globalni objekti (extern)
extern AppConfig g_appConfig;
extern VirtualGpio g_virtualGpio;
extern NetworkManager g_networkManager; // Potrebno za Eth/RS485 restart

// CMD-ovi za Update (iz httpd_cgi_ssi.c)
#define CMD_DWNLD_FWR_IMG 0x77U  // fuf
#define CMD_DWNLD_BLDR_IMG 0x78U // buf
#define CMD_RT_DWNLD_FWR 0x79U   // tuf
#define CMD_RT_DWNLD_BLDR 0x7AU  // tub
#define CMD_RT_DWNLD_LOGO 0x7BU  // tlg
#define CMD_OLD_UPDATE_FWR 0x17  // cud

HttpServer::HttpServer() : m_server(80)
{
    // Konstruktor
}

void HttpServer::Initialize(
    HttpQueryManager *pHttpQueryManager,
    UpdateManager *pUpdateManager,
    EepromStorage *pEepromStorage,
    SdCardManager *pSdCardManager)
{
    Serial.println(F("[HttpServer] Inicijalizacija..."));
    m_http_query_manager = pHttpQueryManager;
    m_update_manager = pUpdateManager;
    m_eeprom_storage = pEepromStorage;
    m_sd_card_manager = pSdCardManager;

    // 1. Serviranje glavne stranice (Frontend)
    m_server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
                { this->HandleRoot(request); });

    // 2. Glavni CGI handler (Backend)
    m_server.on("/sysctrl.cgi", HTTP_GET, [this](AsyncWebServerRequest *request)
                { this->HandleSysctrlRequest(request); });

    // 3. Handler za upload FW fajlova (POST)
    m_server.on("/upload-firmware", HTTP_POST, [](AsyncWebServerRequest *request)
                { request->send(200, "text/plain", "Upload OK"); }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                { this->HandleFileUpload(request, filename, index, data, len, final); });

    // 4. NEW: File Browser Endpoints
    m_server.on("/list_files", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
        if (m_sd_card_manager->IsCardMounted())
        {
            String json = m_sd_card_manager->ListFiles("/");
            request->send(200, "application/json", json);
        }
        else
        {
            request->send(503, "application/json", "{\"error\":\"Card not mounted\"}");
        } });

    m_server.on("/delete_file", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
        if (request->hasParam("file"))
        {
            String filename = request->getParam("file")->value();
            if (m_sd_card_manager->DeleteFile(filename.c_str()))
            {
                request->send(200, "text/plain", "File deleted: " + filename);
            }
            else
            {
                request->send(500, "text/plain", "Failed to delete file");
            }
        }
        else
        {
            request->send(400, "text/plain", "Missing 'file' parameter");
        } });

    m_server.onNotFound([this](AsyncWebServerRequest *request)
                        { this->HandleNotFound(request); });

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
 * @brief POMOĆNA: Šalje SSI odgovor (V1 Kompatibilnost).
 */
void HttpServer::SendSSIResponse(AsyncWebServerRequest *request, const String &message)
{
    String html = String(FPSTR(LOG_HTML));
    // Koristimo logiku $MESSAGE$
    String formatted = "$" + message + "$";
    html.replace("$$", formatted);
    request->send(200, "text/html", html);
}

/**
 * @brief Pretvara IP string u uint32_t (Host Endian).
 */
uint32_t HttpServer::IpStringToUint(const String &ipString)
{
    uint32_t ip = 0;
    int octet = 0;
    int shift = 24;
    for (size_t i = 0; i < ipString.length(); ++i)
    {
        char c = ipString.charAt(i);
        if (c == '.')
        {
            ip |= (uint32_t)octet << shift;
            octet = 0;
            shift -= 8;
        }
        else if (c >= '0' && c <= '9')
        {
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
    if (input.equalsIgnoreCase("RSbra"))
        return String(g_appConfig.rs485_bcast_addr);
    if (input.equalsIgnoreCase("HCgra"))
        return String(g_appConfig.rs485_group_addr);
    if (input.equalsIgnoreCase("HCifa"))
        return String(g_appConfig.rs485_iface_addr);
    if (input.equalsIgnoreCase("RTgra"))
        return String(30855);
    if (input.equalsIgnoreCase("RCgra"))
        return String(26486);
    if (input.equalsIgnoreCase("OWbra"))
        return String(127);
    if (input.equalsIgnoreCase("RTgraOW"))
        return String(10);

    return input;
}

/**
 * @brief Pomoćna funkcija za parsiranje listi adresa.
 */
void HttpServer::BuildAddressList(const String &firstAddrStr, const String &lastAddrStr, uint16_t *list, uint16_t *count)
{
    *count = 0;
    uint16_t first_addr = ParseMacros(firstAddrStr).toInt();
    uint16_t last_addr = ParseMacros(lastAddrStr).toInt();

    if (first_addr == 0 || last_addr == 0 || last_addr < first_addr)
    {
        return;
    }

    list[0] = first_addr;
    *count = 1;
}

/**
 * @brief Pomoćna funkcija za parsiranje listi fajlova.
 */
void HttpServer::BuildFileList(const String &firstFileStr, const String &lastFileStr, uint8_t *list, uint16_t *count)
{
    *count = 0;
    uint8_t first_file = firstFileStr.toInt();
    uint8_t last_file = lastFileStr.toInt();

    if (first_file == 0 || last_file == 0 || last_file < first_file)
    {
        return;
    }

    for (uint8_t img = first_file; img <= last_file; img++)
    {
        if (*count < 25)
        {
            list[*count] = img;
            (*count)++;
        }
        else
        {
            break;
        }
    }
}

/**
 * @brief Pomaže pri pokretanju Update sesije.
 */
bool HttpServer::StartUpdateSession(AsyncWebServerRequest *request, uint8_t updateCmd, const String &addrParam, const String &lastAddrParam)
{
    uint16_t address_list[MAX_ADDRESS_LIST_SIZE];
    uint16_t address_count = 0;

    BuildAddressList(addrParam, lastAddrParam, address_list, &address_count);

    if (address_count == 0)
        return false;

    uint8_t clientAddr = (uint8_t)address_list[0];

    return m_update_manager->StartSession(clientAddr, updateCmd);
}

// ============================================================================
// GLAVNI CGI HANDLER - POTPUNO REFAKTORISAN
// ============================================================================
void HttpServer::HandleSysctrlRequest(AsyncWebServerRequest *request)
{
    Serial.println(F("[HttpServer] Primljen /sysctrl.cgi zahtjev..."));

    // Provjera da li je update već u toku
    if (m_update_manager->m_session.state != S_IDLE)
    {
        SendSSIResponse(request, "BUSY");
        return;
    }

    // Inicijalizacija
    char buffer_data[256] = {0};
    HttpCommand cmd = {};
    cmd.string_ptr = (uint8_t *)buffer_data;
    cmd.string_len = 0;
    String targetAddrStr;
    bool is_blocking = false;

    // ========================================================================
    // --- LOKALNE KOMANDE (Ne idu na RS485 bus) ---
    // ========================================================================

    // --- HC led control: HCled ---
    if (request->hasParam("HCled"))
    {
        int led_state = request->getParam("HCled")->value().toInt();
        if (led_state >= 0 && led_state <= 3)
        {
            g_virtualGpio.SetStatusLed((LedState)led_state);
            SendSSIResponse(request, "OK");
            return;
        }
    }

    // --- HC set IP addresses: ipa, snm, gwa ---
    if (request->hasParam("ipa") && request->hasParam("snm") && request->hasParam("gwa"))
    {
        g_appConfig.ip_address = IpStringToUint(request->getParam("ipa")->value());
        g_appConfig.subnet_mask = IpStringToUint(request->getParam("snm")->value());
        g_appConfig.gateway = IpStringToUint(request->getParam("gwa")->value());
        if (m_eeprom_storage->WriteConfig(&g_appConfig))
        {
            SendSSIResponse(request, "OK (Restart required)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- HC update rtc date & time: tdu / DTset ---
    if (request->hasParam("tdu") || request->hasParam("DTset"))
    {
        String dt = request->hasParam("tdu") ? request->getParam("tdu")->value() : request->getParam("DTset")->value();

        if (dt.length() == 15)
        {
            // int weekday = dt.substring(0, 1).toInt(); // Stari format
            int day = dt.substring(1, 3).toInt();
            int month = dt.substring(3, 5).toInt();
            int year = dt.substring(5, 9).toInt();
            int hour = dt.substring(9, 11).toInt();
            int minute = dt.substring(11, 13).toInt();
            int second = dt.substring(13, 15).toInt();

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
                SendSSIResponse(request, "OK");
                return;
            }
        }
        SendSSIResponse(request, "ERROR");
        return;
    }

    // --- HC log list request: log ---
    if (request->hasParam("log") || request->hasParam("RQlog"))
    {
        String log_op = request->getParam(request->hasParam("log") ? "log" : "RQlog")->value();

        if (log_op == "3" || log_op.equalsIgnoreCase("RDlog"))
        {
            LogEntry entry;
            if (m_eeprom_storage->GetOldestLog(&entry) == LoggerStatus::LOGGER_OK)
            {
                char log_str[256];
                snprintf(log_str, 256, "Log ID: %u\nEvent: 0x%02X\nAddr: 0x%04X\nType: %u\nTime: %lu\nCard: %02X%02X%02X%02X",
                         entry.log_id, entry.event_code, entry.device_addr, entry.log_type, entry.timestamp,
                         entry.rf_card_id[0], entry.rf_card_id[1], entry.rf_card_id[2], entry.rf_card_id[3]);
                SendSSIResponse(request, String(log_str));
            }
            else
            {
                SendSSIResponse(request, "EMPTY");
            }
            return;
        }
        else if (log_op == "4" || log_op.equalsIgnoreCase("DLlog"))
        {
            if (m_eeprom_storage->DeleteOldestLog() == LoggerStatus::LOGGER_OK)
            {
                SendSSIResponse(request, "DELETED");
            }
            else
            {
                SendSSIResponse(request, "EMPTY");
            }
            return;
        }
        else if (log_op == "5" || log_op.equalsIgnoreCase("DLlst"))
        {
            if (m_eeprom_storage->ClearAllLogs() == LoggerStatus::LOGGER_OK)
            {
                SendSSIResponse(request, "OK");
            }
            else
            {
                SendSSIResponse(request, "ERROR");
            }
            return;
        }
        SendSSIResponse(request, "ERROR");
        return;
    }

    // --- NEW: HC load address list: cad=load ---
    if (request->hasParam("cad") && request->getParam("cad")->value() == "load")
    {
        Serial.println(F("[HttpServer] Učitavam listu adresa sa uSD kartice..."));

        if (!m_sd_card_manager->IsCardMounted())
        {
            SendSSIResponse(request, "ERROR (Card not mounted)");
            return;
        }

        String content = m_sd_card_manager->ReadTextFile(PATH_CTRL_ADD_LIST);
        if (content.length() == 0)
        {
            SendSSIResponse(request, "ERROR (File not found)");
            return;
        }

        // Parsiranje CTRL_ADD.TXT (formati: "101,102,103;" ili "101\n102\n103")
        uint16_t address_list[MAX_ADDRESS_LIST_SIZE];
        uint16_t count = 0;
        
        content.replace("\n", ","); // Zamijeni nove redove zarezima
        content.replace(";", ",");  // Zamijeni tačka-zarez zarezima

        int start = 0;
        int end = content.indexOf(',');

        while (end != -1 && count < MAX_ADDRESS_LIST_SIZE)
        {
            String addrStr = content.substring(start, end);
            addrStr.trim();

            if (addrStr.length() > 0 && addrStr.charAt(0) != '#') 
            {
                uint16_t addr = addrStr.toInt();
                if (addr > 0)
                {
                    address_list[count++] = addr;
                }
            }
            start = end + 1;
            end = content.indexOf(',', start);
        }

        // Procesuj zadnji element ako postoji
        if (start < content.length() && count < MAX_ADDRESS_LIST_SIZE)
        {
            String addrStr = content.substring(start);
            addrStr.trim();
            if (addrStr.length() > 0 && addrStr.charAt(0) != '#')
            {
                uint16_t addr = addrStr.toInt();
                if (addr > 0)
                {
                    address_list[count++] = addr;
                }
            }
        }

        // Upiši u EEPROM keš
        if (m_eeprom_storage->WriteAddressList(address_list, count))
        {
            String response = "OK (" + String(count) + " addresses loaded)";
            SendSSIResponse(request, response);
            // TODO: Treba signalizirati LogPullManageru da ponovo učita listu
        }
        else
        {
            SendSSIResponse(request, "ERROR (EEPROM write failed)");
        }
        return;
    }

    // --- HC update firmware: fwu ---
    if (request->hasParam("fwu") || request->hasParam("HCfwu"))
    {
        Serial.println("[HttpServer] Restarting system for update (fwu=hc)...");
        SendSSIResponse(request, "OK (Restarting...)");
        delay(100);
        ESP.restart();
        return;
    }

    // ========================================================================
    // --- NE-BLOKIRAJUĆE KOMANDE (Update) ---
    // ========================================================================

    // --- RC update old firmware: cud ---
    if (request->hasParam("cud"))
    {
        if (StartUpdateSession(request, CMD_OLD_UPDATE_FWR, request->getParam("cud")->value(), request->getParam("cud")->value()))
        {
            SendSSIResponse(request, "OK (Update started)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- RC update firmware: fuf, ful ---
    if (request->hasParam("fuf") && request->hasParam("ful"))
    {
        if (StartUpdateSession(request, CMD_DWNLD_FWR_IMG, request->getParam("fuf")->value(), request->getParam("ful")->value()))
        {
            SendSSIResponse(request, "OK (Update started)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- RC update bootloader: buf, bul ---
    if (request->hasParam("buf") && request->hasParam("bul"))
    {
        if (StartUpdateSession(request, CMD_DWNLD_BLDR_IMG, request->getParam("buf")->value(), request->getParam("bul")->value()))
        {
            SendSSIResponse(request, "OK (Update started)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- RT update firmware: tuf, owa ---
    if (request->hasParam("tuf") && request->hasParam("owa"))
    {
        if (StartUpdateSession(request, CMD_RT_DWNLD_FWR, request->getParam("tuf")->value(), request->getParam("tuf")->value()))
        {
            SendSSIResponse(request, "OK (Update started)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- upload RT display user logo image: tlg, owa ---
    if (request->hasParam("tlg") && request->hasParam("owa"))
    {
        if (StartUpdateSession(request, CMD_RT_DWNLD_LOGO, request->getParam("tlg")->value(), request->getParam("tlg")->value()))
        {
            SendSSIResponse(request, "OK (Update started)");
        }
        else
        {
            SendSSIResponse(request, "ERROR");
        }
        return;
    }

    // --- RC update display image: iuf, iul, ifa, ila ---
    if (request->hasParam("iuf") && request->hasParam("iul") && request->hasParam("ifa") && request->hasParam("ila"))
    {
        // UPOZORENJE: Ova logika je limitirana na JEDNU adresu (iuf),
        // ali će sada ispravno iterirati kroz sve slike od 'ifa' do 'ila'.
        uint16_t first_addr = request->getParam("iuf")->value().toInt();
        // uint16_t last_addr = request->getParam("iul")->value().toInt(); // Trenutno se ne koristi
        int first_img = request->getParam("ifa")->value().toInt();
        int last_img = request->getParam("ila")->value().toInt();

        if (first_img < 1 || last_img > 14 || first_img > last_img || first_addr == 0) {
            SendSSIResponse(request, "ERROR (Invalid image range)");
            return;
        }

        // Petlja koja prolazi kroz sve slike za PRVU adresu
        for (int img_idx = first_img; img_idx <= last_img; ++img_idx)
        {
            // Sačekaj da se prethodna sesija završi
            while (m_update_manager->m_session.state != S_IDLE) {
                delay(100); // Ne blokiraj sistem potpuno
            }

            Serial.printf("[HttpServer] Pokretanje update-a slike %d za adresu %d\n", img_idx, first_addr);
            uint8_t updateCmd = CMD_IMG_RC_START + img_idx - 1;
            
            // Pokreni sesiju za jednu adresu i jednu sliku
            if (!m_update_manager->StartSession(first_addr, updateCmd))
            {
                // Ako sesija ne može ni da počne (npr. fajl ne postoji), prekini sve.
                String errorMsg = "ERROR (Failed to start session for image " + String(img_idx) + ")";
                SendSSIResponse(request, errorMsg);
                return; // Prekini ako jedna sesija ne uspe
            }
        }

        SendSSIResponse(request, "OK (Image update sequence started)");
        return;
    }

    // ========================================================================
    // --- BLOKIRAJUĆE KOMANDE (RS485 Upiti) ---
    // ========================================================================

    // --- Implementacija SET_PERMITED_GROUP (pga) ---
    if (request->hasParam("pga") && request->hasParam("pgu"))
    {
        targetAddrStr = request->getParam("pga")->value();
        cmd.cmd_id = SET_PERMITED_GROUP;

        String groups = request->getParam("pgu")->value();
        memset(buffer_data, ' ', 16);
        strncpy(buffer_data, groups.c_str(), 16);
        cmd.string_len = 16;
        is_blocking = true;
    }

    // --- Implementacija CMD_SET_DIN_CFG (cdi) ---
    else if (request->hasParam("cdi") && request->hasParam("di0")) 
    {
        targetAddrStr = request->getParam("cdi")->value();
        cmd.cmd_id = CMD_SET_DIN_CFG;

        for (int i = 0; i < 8; ++i)
        {
            String paramName = "di" + String(i);
            if (request->hasParam(paramName)) {
                buffer_data[i] = request->getParam(paramName)->value().toInt() + '0';
            } else {
                buffer_data[i] = '0';
            }
        }
        cmd.string_len = 8;
        is_blocking = true;
    }

    // --- Implementacija CMD_RT_DISP_MSG (txa) ---
    else if (request->hasParam("txa") && request->hasParam("txt"))
    {
        targetAddrStr = request->getParam("txa")->value();
        cmd.cmd_id = CMD_RT_DISP_MSG;
        
        uint8_t i = 0; // Index unutar buffer_data (Payload)
        
        cmd.string_ptr[i++] = request->getParam("trc")->value().toInt() & 0xFFU;

        uint16_t tx0 = request->getParam("tx0")->value().toInt();
        uint16_t ty0 = request->getParam("ty0")->value().toInt();
        cmd.string_ptr[i++] = (uint8_t)((tx0 >> 8) & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)(tx0 & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)((ty0 >> 8) & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)(ty0 & 0xFFU);
        
        uint16_t tx1 = request->getParam("tx1")->value().toInt();
        uint16_t ty1 = request->getParam("ty1")->value().toInt();
        cmd.string_ptr[i++] = (uint8_t)((tx1 >> 8) & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)(tx1 & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)((ty1 >> 8) & 0xFFU);
        cmd.string_ptr[i++] = (uint8_t)(ty1 & 0xFFU);
        
        cmd.string_ptr[i++] = request->getParam("txc")->value().toInt() & 0xFFU; 
        cmd.string_ptr[i++] = request->getParam("txf")->value().toInt() & 0xFFU; 
        cmd.string_ptr[i++] = request->getParam("txh")->value().toInt() & 0xFFU; 
        cmd.string_ptr[i++] = request->getParam("txv")->value().toInt() & 0xFFU; 
        
        String text = request->getParam("txt")->value(); 
        size_t text_len = text.length();
        size_t max_text_len = MAX_PACKET_LENGTH - 10 - 1 - i; 
        size_t final_text_len = min((size_t)text_len, max_text_len);
        
        memcpy(&cmd.string_ptr[i], text.c_str(), final_text_len); 
        cmd.string_len = i + final_text_len; 
        is_blocking = true;
    }

    // --- Implementacija RT_SET_DISP_STA (tda) ---
    else if (request->hasParam("tda") && request->hasParam("tdi")) 
    {
        targetAddrStr = request->getParam("tda")->value();
        cmd.cmd_id = RT_SET_DISP_STA;

        buffer_data[0] = request->getParam("tdn")->value().toInt();
        buffer_data[1] = request->getParam("tdi")->value().toInt();
        buffer_data[2] = request->getParam("tdt")->value().toInt();
        buffer_data[3] = request->getParam("tbm")->value().toInt();
        buffer_data[4] = request->getParam("tbt")->value().toInt();
        cmd.string_len = 5;
        is_blocking = true;
    }

    // --- Implementacija CMD_RT_UPD_QRC / RT_DISP_QRC (qra) ---
    else if (request->hasParam("qra"))
    {
        targetAddrStr = request->getParam("qra")->value();
        
        if (request->hasParam("qrc")) // Upload (CMD_RT_UPD_QRC)
        {
            cmd.cmd_id = CMD_RT_UPD_QRC;
            String qrText = request->getParam("qrc")->value();
            qrText.replace('+', '=');
            qrText.replace('-', '&');
            strncpy(buffer_data, qrText.c_str(), 255);
            cmd.string_len = qrText.length();
            is_blocking = true;
        }
        else if (request->hasParam("qrd")) // Display (RT_DISP_QRC)
        {
            cmd.cmd_id = RT_DISP_QRC;
            is_blocking = true;
        }
    }
    // --- Implementacija SET_ROOM_TEMP (tha) ---
    else if (request->hasParam("tha")) 
    {
        targetAddrStr = request->getParam("tha")->value();
        cmd.cmd_id = SET_ROOM_TEMP;
        
        uint8_t config_byte = 0;
        cmd.param1 = 0; // spt
        cmd.param2 = 1; // dif (Default: 1)

        for (int i = 1; i < request->params(); ++i) 
        {
            String pName = request->getParam(i)->name();
            String pValue = request->getParam(i)->value();
            
            if (pName == "spt") 
                cmd.param1 = pValue.toInt();
            else if (pName == "dif") 
                cmd.param2 = pValue.toInt(); 
            else if (pName == "sta")
            {
                if (pValue.equalsIgnoreCase("ON"))      config_byte |= (1 << 0) | (1 << 4);
                else if (pValue.equalsIgnoreCase("OFF")) config_byte |= (1 << 4);
            }
            else if (pName == "mod")
            {
                if (pValue.equalsIgnoreCase("HEAT"))    config_byte |= (1 << 1) | (1 << 5);
                else if (pValue.equalsIgnoreCase("COOL")) config_byte |= (1 << 5);
            }
            else if (pName == "ctr")
            {
                if (pValue.equalsIgnoreCase("ENA"))     config_byte |= (1 << 2) | (1 << 6);
                else if (pValue.equalsIgnoreCase("DIS")) config_byte |= (1 << 6);
            }
            else if (pName == "out")
            {
                if (pValue.equalsIgnoreCase("ON"))      config_byte |= (1 << 3) | (1 << 7);
                else if (pValue.equalsIgnoreCase("OFF")) config_byte |= (1 << 7);
            }
        }
        cmd.param3 = config_byte;
        is_blocking = true;
    }
    
    // ========================================================================
    // --- NOVO: Implementacija preostalih CGI komandi ---
    // ========================================================================

    // --- RC set room status: stg, val ---
    else if (request->hasParam("stg") && request->hasParam("val"))
    {
        targetAddrStr = request->getParam("stg")->value();
        cmd.cmd_id = SET_APPL_STAT;
        cmd.param1 = request->getParam("val")->value().toInt();
        is_blocking = true;
    }
    // --- RC set bedding period: sbr, per ---
    else if (request->hasParam("sbr") && request->hasParam("per"))
    {
        targetAddrStr = request->getParam("sbr")->value();
        cmd.cmd_id = SET_BEDDING_REPL;
        cmd.param1 = request->getParam("per")->value().toInt();
        is_blocking = true;
    }
    // --- RC room status request: cst ---
    else if (request->hasParam("cst"))
    {
        targetAddrStr = request->getParam("cst")->value();
        cmd.cmd_id = GET_APPL_STAT;
        is_blocking = true;
    }
    // --- RC preview display image: ipr ---
    else if (request->hasParam("ipr"))
    {
        targetAddrStr = request->getParam("ipr")->value();
        cmd.cmd_id = PREVIEW_DISPL_IMG;
        is_blocking = true;
    }
    // --- RC set digital output: cdo ---
    else if (request->hasParam("cdo") && request->hasParam("ctrl"))
    {
        targetAddrStr = request->getParam("cdo")->value();
        cmd.cmd_id = CMD_SET_DOUT_STATE;
        
        // Payload je 9 bajtova (do0-do7 + ctrl) -
        buffer_data[0] = request->hasParam("do0") ? (request->getParam("do0")->value().toInt() + '0') : '0';
        buffer_data[1] = request->hasParam("do1") ? (request->getParam("do1")->value().toInt() + '0') : '0';
        buffer_data[2] = request->hasParam("do2") ? (request->getParam("do2")->value().toInt() + '0') : '0';
        buffer_data[3] = request->hasParam("do3") ? (request->getParam("do3")->value().toInt() + '0') : '0';
        buffer_data[4] = request->hasParam("do4") ? (request->getParam("do4")->value().toInt() + '0') : '0';
        buffer_data[5] = request->hasParam("do5") ? (request->getParam("do5")->value().toInt() + '0') : '0';
        buffer_data[6] = request->hasParam("do6") ? (request->getParam("do6")->value().toInt() + '0') : '0';
        buffer_data[7] = request->hasParam("do7") ? (request->getParam("do7")->value().toInt() + '0') : '0';
        buffer_data[8] = request->getParam("ctrl")->value().toInt() + '0';
        
        cmd.string_len = 9;
        is_blocking = true;
    }
    // --- RC set display brightness: cbr, br ---
    else if (request->hasParam("cbr") && request->hasParam("br")) 
    {
        targetAddrStr = request->getParam("cbr")->value();
        cmd.cmd_id = SET_DISPL_BCKLGHT;
        cmd.param1 = request->getParam("br")->value().toInt();
        is_blocking = true;
    }
    // --- RC SOS alarm reset request: rud ---
    else if (request->hasParam("rud"))
    {
        targetAddrStr = request->getParam("rud")->value();
        cmd.cmd_id = RESET_SOS_ALARM;
        is_blocking = true;
    }
    // --- RC set rs485 address: rsc, rsa, rga, rba, rib ---
    else if (request->hasParam("rsc") && request->hasParam("rsa") && request->hasParam("rga") && request->hasParam("rba") && request->hasParam("rib"))
    {
        targetAddrStr = request->getParam("rsc")->value();
        
        uint16_t rsa = request->getParam("rsa")->value().toInt();
        uint16_t rga = request->getParam("rga")->value().toInt();
        uint16_t rba = request->getParam("rba")->value().toInt();
        uint8_t rib = request->getParam("rib")->value().toInt();

        // 1. Lokalna promjena (ako je HC adresa)
        if (ParseMacros(targetAddrStr).toInt() == g_appConfig.rs485_iface_addr)
        {
            Serial.println(F("[HttpServer] Primljena lokalna RS485 konfiguracija..."));
            g_appConfig.rs485_iface_addr = rsa;
            g_appConfig.rs485_group_addr = rga;
            g_appConfig.rs485_bcast_addr = rba;
            
            if (m_eeprom_storage->WriteConfig(&g_appConfig)) {
                SendSSIResponse(request, "OK (HC Config saved. Restart required for baudrate change.)");
            } else {
                SendSSIResponse(request, "ERROR (EEPROM Write)");
            }
            return; 
        }
        
        // 2. Slanje komande na daljinu
        cmd.cmd_id = SET_RS485_CFG;
        
        buffer_data[0] = (uint8_t)((rsa >> 8) & 0xFFU);
        buffer_data[1] = (uint8_t)(rsa & 0xFFU);
        buffer_data[2] = (uint8_t)((rga >> 8) & 0xFFU);
        buffer_data[3] = (uint8_t)(rga & 0xFFU);
        buffer_data[4] = (uint8_t)((rba >> 8) & 0xFFU);
        buffer_data[5] = (uint8_t)(rba & 0xFFU);
        buffer_data[6] = (uint8_t)(rib + '0'); // Baudrate se u starom kodu slao kao char
        
        cmd.string_len = 7;
        is_blocking = true;
    }
    // --- HC set system ID: sid, nid ---
    else if (request->hasParam("sid") && request->hasParam("nid"))
    {     
        targetAddrStr = request->getParam("sid")->value();
        uint16_t new_id = request->getParam("nid")->value().toInt();
        
        // 1. Lokalna promjena (HC) -
        if (ParseMacros(targetAddrStr).toInt() == g_appConfig.rs485_iface_addr)
        {
            Serial.println(F("[HttpServer] Primljen lokalni System ID..."));
            g_appConfig.system_id = new_id;
            if (!m_eeprom_storage->WriteConfig(&g_appConfig)) {
                    SendSSIResponse(request, "ERROR (EEPROM Write)");
                    return;
            }
            targetAddrStr = "RSbra"; 
        }
        
        // 2. Slanje komande (P2P ili Broadcast)
        cmd.cmd_id = SET_SYSTEM_ID;
        cmd.param1 = new_id;
        is_blocking = true;
    }
    
    // --- HC RC RT reset controller: rst ---
    else if (request->hasParam("rst"))
    {
        targetAddrStr = request->getParam("rst")->value();
        if (targetAddrStr == "0" || ParseMacros(targetAddrStr) == String(g_appConfig.rs485_iface_addr))
        {
            SendSSIResponse(request, "OK (Restarting HC...)");
            delay(100);
            ESP.restart();
            return;
        }
        cmd.cmd_id = RESTART_CTRL;
        is_blocking = true;
    }
    // --- update hotel status: HSset ---
    else if (request->hasParam("HSset"))
    {
        targetAddrStr = "RSbra";
        cmd.cmd_id = DWNLD_JRNL;
        cmd.string_len = request->getParam("HSset")->value().length();
        strncpy((char *)cmd.string_ptr, request->getParam("HSset")->value().c_str(), 255);
        is_blocking = true;
    }


    // ========================================================================
    // --- IZVRŠENJE BLOKIRAJUĆEG UPITA ---
    // ========================================================================
    if (is_blocking)
    {
        cmd.address = ParseMacros(targetAddrStr).toInt();
        cmd.owa_addr = 0;
        if (request->hasParam("owa"))
        {
            cmd.owa_addr = ParseMacros(request->getParam("owa")->value()).toInt();
        }

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        memset(response_buffer, 0, MAX_PACKET_LENGTH);

        if (m_http_query_manager->ExecuteBlockingQuery(&cmd, response_buffer))
        {
            String response_str = (char *)response_buffer;
            SendSSIResponse(request, response_str.length() > 0 ? response_str : "OK");
        }
        else
        {
            SendSSIResponse(request, "TIMEOUT");
        }
    }
    else
    {
        SendSSIResponse(request, "ERROR (Unknown command)");
    }
}

/**
 * @brief Obrađuje upload fajla na uSD karticu.
 */
void HttpServer::HandleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
    if (!m_sd_card_manager->IsCardMounted())
    {
        Serial.println(F("[HttpServer] uSD kartica nije dostupna!"));
        return;
    }

    String destPath = "/";
    if (request->hasParam("file", true))
    {
        destPath += request->getParam("file", true)->value();
    }
    else
    {
        destPath += filename;
    }

    static File uploadFile;

    if (index == 0)
    {
        Serial.printf("[HttpServer] Započeo upload: %s -> %s\n", filename.c_str(), destPath.c_str());

        uploadFile = m_sd_card_manager->CreateFile(destPath.c_str());

        if (!uploadFile)
        {
            Serial.println(F("[HttpServer] Greška pri kreiranju fajla!"));
            return;
        }
    }

    if (len > 0 && uploadFile)
    {
        size_t written = uploadFile.write(data, len);
        if (written != len)
        {
            Serial.printf("[HttpServer] Greška pisanja: %d/%d bytes\n", written, len);
        }
    }

    if (final && uploadFile)
    {
        uploadFile.close();
        Serial.printf("[HttpServer] Upload završen: %s (%lu bytes)\n", destPath.c_str(), index + len);
    }
}

void HttpServer::HandleNotFound(AsyncWebServerRequest *request)
{
    request->send(404, "text/plain", "Not Found");
}