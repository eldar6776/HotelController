/**
 ******************************************************************************
 * @file    HttpQueryManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija HttpQueryManager modula.
 ******************************************************************************
 */

#include "HttpQueryManager.h"
#include "DebugConfig.h"   // Uključujemo za LOG_RS485
#include "ProjectConfig.h"
#include "EepromStorage.h" // Za g_appConfig
#include "LogPullManager.h" // Za GetBusForAddress routing
#include <esp_task_wdt.h>  // NOVO: Uključujemo za watchdog reset
#include <cstring>

// Globalni objekat za konfiguraciju (extern)
extern AppConfig g_appConfig; 

// Makro Vrijednosti
#define DEF_HC_OWIFA 31U

// Definišemo specifičan timeout za HTTP upite
#define HTTP_QUERY_TIMEOUT_MS 50

HttpQueryManager::HttpQueryManager() :
    m_rs485_service(NULL)
{
    // Konstruktor
}

void HttpQueryManager::Initialize(Rs485Service* pRs485Service)
{
    m_rs485_service = pRs485Service;
}

// Extern pointer za LogPullManager (routing)
extern LogPullManager* g_logPullManager_ptr;

/**
 * @brief Kreira RS485 paket iz HttpCommand strukture (bazirano na HC_CreateCmdRequest).
 */
uint16_t HttpQueryManager::CreateRs485Packet(HttpCommand* cmd, uint8_t* buffer)
{
    uint16_t rs485_txaddr = cmd->address;
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t rs485_pkt_chksum = 0;
    uint16_t data_length = 1; // Podrazumijevana dužina za CMD bajt
    uint32_t i;
    
    // Osnovno Zaglavlje
    buffer[0] = SOH;
    buffer[1] = (rs485_txaddr >> 8);
    buffer[2] = rs485_txaddr;
    buffer[3] = (rsifa >> 8);
    buffer[4] = rsifa;
    buffer[6] = cmd->cmd_id; // CMD
    
    // Parsiranje Data Polja (tx_buff[7] dalje) na osnovu komande
    switch (cmd->cmd_id)
    {
        // Komande sa 1 bajtom (samo CMD)
        case GET_APPL_STAT:
        case RUBICON_GET_ROOM_STATUS:  // 0x95 za HILLS protokol
        case RESET_SOS_ALARM:
        case RESTART_CTRL:
        case GET_LOG_LIST:
        case DEL_LOG_LIST:
        case RT_DISP_QRC:
        case PREVIEW_DISPL_IMG: // ipr (nema payload)
        {
            data_length = 1; 
            break;
        }
        
        // Komande sa 2 bajta (CMD + 1 data)
        case SET_APPL_STAT: // stg
        case SET_BEDDING_REPL: // sbr
        {
            data_length = 2;
            buffer[7] = (uint8_t)cmd->param1; // Status (stg=val) ili Period (sbr=per)
            break;
        }

        // Komande sa 3 bajta (CMD + 2 data)
        case SET_DISPL_BCKLGHT: // cbr
        {
            data_length = 3;
            // Brightness je uint16_t (param1)
            buffer[7] = (uint8_t)((cmd->param1 >> 8) & 0xFFU); 
            buffer[8] = (uint8_t)(cmd->param1 & 0xFFU);       
            break;
        }
        case SET_SYSTEM_ID: // sid, nid
        {
            data_length = 3;
            uint16_t new_sys_id = (uint16_t)cmd->param1;
            buffer[7] = (uint8_t)((new_sys_id >> 8) & 0xFFU);
            buffer[8] = (uint8_t)(new_sys_id & 0xFFU);      
            break;
        }

        // Komande sa 4 bajta (CMD + 3 data)
        case SET_ROOM_TEMP: // tha
        {
            data_length = 4;
            buffer[7] = (uint8_t)cmd->param1; // Setpoint (spt)
            buffer[8] = (uint8_t)cmd->param2; // Differential (dif)
            buffer[9] = (uint8_t)cmd->param3; // Config byte (sta, mod, ctr, out)
            break;
        }

        // Komande sa string_ptr
        case SET_RS485_CFG: // rsc (7 bajtova)
        {
            data_length = 8; // 7 bajtova + CMD
            memcpy(&buffer[7], cmd->string_ptr, 7); 
            break;
        }
        case CMD_SET_DIN_CFG: // cdi (8 bajtova)
        {
            data_length = 9; // 8 bajtova + CMD
            memcpy(&buffer[7], cmd->string_ptr, 8);
            break;
        }
        case CMD_SET_DOUT_STATE: // cdo (9 bajtova)
        {
            data_length = 10; // 9 bajtova + CMD
            memcpy(&buffer[7], cmd->string_ptr, 9);
            break;
        }
        case SET_PERMITED_GROUP: // pga (16 bajtova)
        {
            data_length = 17; // 16 bajtova + CMD
            memcpy(&buffer[7], cmd->string_ptr, 16);
            break;
        }
        case RT_SET_DISP_STA: // tda (5 bajtova)
        {
            data_length = 6; // 5 bajtova + CMD
            memcpy(&buffer[7], cmd->string_ptr, 5);
            break;
        }

        // Komande sa varijabilnom dužinom (string_len)
        case DWNLD_JRNL: // HSset
        case CMD_RT_DISP_MSG: // txa
        case CMD_RT_UPD_QRC: // qra
        {
            data_length = cmd->string_len + 1; // +1 za CMD
            if (data_length > (MAX_PACKET_LENGTH - 10)) {
                data_length = MAX_PACKET_LENGTH - 10; // Osiguranje od preljeva
            }
            memcpy(&buffer[7], cmd->string_ptr, data_length - 1);
            break;
        }

        // DEFAULT: Za sve nepoznate komande
        default:
        {
            data_length = 1;
            break;
        }
    }

    buffer[5] = (uint8_t)data_length; // Data Length
    uint16_t total_packet_length = data_length + 9;

    // --- LOGIKA ONEWIRE BRIDGE (owa) ---
    //
    if (cmd->owa_addr != 0)
    {
        uint16_t original_data_length = data_length;
        
        // Provjeri ima li dovoljno mjesta za Onewire header (4 bajta)
        if (total_packet_length + 4 > MAX_PACKET_LENGTH) {
             Serial.println(F("[HttpQueryManager] GRESKA: Paket prevelik za Onewire bridge."));
             // Vrati paket bez Onewire-a (ili vrati grešku)
        } else {
            // Pomjeranje originalnog data payload-a (uključujući CMD)
            // Pomjeramo unatrag da ne pregazimo podatke
            for (i = (total_packet_length - 4); i >= 6; i--)
            {
                buffer[i + 4] = buffer[i]; // Dodatnih 4 bajta za BR2OW header
            }

            // Umetni BR2OW Header (CMD + TX_ADDR + IF_ADDR + LEN)
            buffer[6] = SET_BR2OW; 
            buffer[7] = (uint8_t)cmd->owa_addr; // Onewire receiver address
            buffer[8] = DEF_HC_OWIFA;           // Onewire interface address
            buffer[9] = (uint8_t)original_data_length; // Onewire Data Payload Size (ukljucuje originalni CMD)
            
            // Originalni CMD je sada na buffer[10]
            data_length += 4; 
            buffer[5] = (uint8_t)data_length;
            total_packet_length = data_length + 9;
        }
    }
    
    // --- CHECK SUM i EOT ---
    rs485_pkt_chksum = 0;
    for (i = 6; i < (buffer[5] + 6); i++)
    {
        rs485_pkt_chksum += buffer[i];
    }

    buffer[total_packet_length - 3] = (uint8_t)(rs485_pkt_chksum >> 8);
    buffer[total_packet_length - 2] = (uint8_t)rs485_pkt_chksum;
    buffer[total_packet_length - 1] = EOT;

    return total_packet_length;
}


/**
 * @brief BLOKIRAJUCA funkcija. Ceka dok RS485 ne zavrsi.
 * @return Payload length (broj bajtova), ili -1 ako je greška/timeout
 */
int HttpQueryManager::ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer)
{    
    LOG_DEBUG(4, "[HttpQuery] Primljen zahtev za komandu 0x%X na adresu 0x%X\n", cmd->cmd_id, cmd->address);

    // ========================================================================
    // DUAL BUS ROUTING LOGIC
    // ========================================================================
    bool dual_mode = g_appConfig.enable_dual_bus_mode;
    int8_t target_bus = -1;
    
    if (dual_mode && g_logPullManager_ptr != NULL)
    {
        target_bus = g_logPullManager_ptr->GetBusForAddress(cmd->address);
        
        if (target_bus >= 0)
        {
            // Adresa pronađena u listi, postavi odgovarajući bus
            m_rs485_service->SelectBus(target_bus);
            LOG_DEBUG(4, "[HttpQuery] Adresa 0x%04X -> Bus %d\n", cmd->address, target_bus);
        }
        else
        {
            // Adresa nije u listama - fallback na Bus 0
            LOG_DEBUG(3, "[HttpQuery] Adresa 0x%04X nepoznata, fallback na Bus 0\n", cmd->address);
            m_rs485_service->SelectBus(0);
            target_bus = 0; // Postavi target_bus za protokol selekciju
        }
    }
    else
    {
        target_bus = 0; // Single bus mode koristi Bus 0
    }
    
    // ========================================================================
    // PROTOCOL ADAPTATION
    // ========================================================================
    // Adaptiraj komandu na osnovu protokola za određeni bus
    AdaptCommandForProtocol(cmd, target_bus);
    // ========================================================================

    uint8_t packet[MAX_PACKET_LENGTH];
    uint16_t length = CreateRs485Packet(cmd, packet);
  
    // Odmah zauzmi magistralu i pošalji
    if (!m_rs485_service->SendPacket(packet, length))
    {
        LOG_DEBUG(1, "[HttpQuery] GRESKA: SendPacket nije uspio.\n");
        return -1;  // Greška slanja
    }

    // Čekaj odgovor
    int response_len = m_rs485_service->ReceivePacket(responseBuffer, MAX_PACKET_LENGTH, HTTP_QUERY_TIMEOUT_MS);

    // ========================================================================
    // FALLBACK: Ako timeout, pokušaj drugi bus
    // ========================================================================
    // U DUAL MODE: Ako je adresa bila u listi Bus 0, pokušaj Bus 1
    // U SINGLE MODE: Uvijek pokušaj Bus 1 ako Bus 0 ne odgovori
    if (response_len == 0 && target_bus == 0)
    {
        LOG_DEBUG(3, "[HttpQuery] Timeout na Bus 0, pokušavam Bus 1...\n");
        m_rs485_service->SelectBus(1);
        
        // U SINGLE MODE: Možda trebamo prilagoditi protokol za Bus 1
        if (!dual_mode) {
            // NAPOMENA: U single mode protocol_version_L == protocol_version_R
            // Tako da adaptacija nije kritična, ali je bolje biti eksplicitan
            AdaptCommandForProtocol(cmd, 1);
            // Ponovo kreiraj paket sa (potencijalno) drugačijom komandom
            length = CreateRs485Packet(cmd, packet);
        }
        
        if (m_rs485_service->SendPacket(packet, length))
        {
            response_len = m_rs485_service->ReceivePacket(responseBuffer, MAX_PACKET_LENGTH, HTTP_QUERY_TIMEOUT_MS);
            
            if (response_len > 0) {
                LOG_DEBUG(3, "[HttpQuery] USPJEH na Bus 1!\n");
            } else {
                LOG_DEBUG(3, "[HttpQuery] Timeout i na Bus 1.\n");
            }
        }
    }
    // ========================================================================

    if (response_len > 0)
    {
        LOG_DEBUG(4, "[HttpQuery] Primljen odgovor. Dužina: %d\n", response_len);
        // Parsiranje odgovora - identično kao u starom kodu (httpd_cgi_ssi.c linija 143)
        if (response_len >= 9)
        {
            uint16_t data_field_len = responseBuffer[5];
            if (data_field_len >= 2 && (data_field_len + 7) <= response_len)
            {
                // data_field_len = CMD (1 bajt) + DATA (n bajtova), CRC je van data_field_len
                // Payload je samo DATA, pa treba oduzeti CMD (1 bajt)
                uint16_t payload_len = data_field_len - 1;
                memmove(responseBuffer, &responseBuffer[7], payload_len);
                responseBuffer[payload_len] = '\0';
                return payload_len;
            }
            else
            {
                strcpy((char*)responseBuffer, "OK");
                return 2;
            }
        }
        else
        {
            strcpy((char*)responseBuffer, "ERROR");
            return 5;  // "ERROR" je 5 bajtova
        }
    }
    else if (response_len == 0)
    {
        LOG_DEBUG(2, "[HttpQuery] TIMEOUT. Nije primljen odgovor na komandu.\n");
        return -1;  // Timeout
    }
    else // response_len < 0
    {
        LOG_DEBUG(1, "[HttpQuery] GRESKA: Prijemni bafer je pun (overflow).\n");
        return -1;  // Greška
    }
}

/**
 * @brief Provjerava da li je protokol za određeni bus HILLS.
 */
bool HttpQueryManager::IsHillsProtocolForBus(int8_t bus_id)
{
    uint8_t protocol = (bus_id == 0) 
        ? g_appConfig.protocol_version_L 
        : g_appConfig.protocol_version_R;
    
    return (static_cast<ProtocolVersion>(protocol) == ProtocolVersion::HILLS);
}

/**
 * @brief Adaptira cmd_id za protokol na odgovarajućem bus-u.
 * 
 * @note Neki protokoli koriste različite komandne bajtove:
 *       - HILLS:    GET_APPL_STAT -> RUBICON_GET_ROOM_STATUS (0xA1 -> 0x95)
 *       - Standardni: Koristi originalne komande
 */
void HttpQueryManager::AdaptCommandForProtocol(HttpCommand* cmd, int8_t bus_id)
{
    bool is_hills = IsHillsProtocolForBus(bus_id);
    
    // Adaptiraj GET_APPL_STAT (cst) komandu
    if (cmd->cmd_id == GET_APPL_STAT && is_hills)
    {
        cmd->cmd_id = RUBICON_GET_ROOM_STATUS; // 0xA1 -> 0x95
        LOG_DEBUG(4, "[HttpQuery] Protokol adaptacija: GET_APPL_STAT -> RUBICON_GET_ROOM_STATUS (HILLS)\n");
    }
    else if (cmd->cmd_id == RUBICON_GET_ROOM_STATUS && !is_hills)
    {
        cmd->cmd_id = GET_APPL_STAT; // 0x95 -> 0xA1
        LOG_DEBUG(4, "[HttpQuery] Protokol adaptacija: RUBICON_GET_ROOM_STATUS -> GET_APPL_STAT (Standard)\n");
    }
    
    // Ovdje se mogu dodati adaptacije za druge komande po potrebi
}