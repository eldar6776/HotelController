/**
 ******************************************************************************
 * @file    HttpQueryManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija HttpQueryManager modula.
 ******************************************************************************
 */

#include "HttpQueryManager.h"
#include "ProjectConfig.h"
#include "EepromStorage.h" // Za g_appConfig
#include <cstring>

// Globalni objekat za konfiguraciju (extern)
extern AppConfig g_appConfig; 

// Makro Vrijednosti
#define DEF_HC_OWIFA 31U

HttpQueryManager::HttpQueryManager() :
    m_rs485_service(NULL),
    m_pending_cmd(NULL),
    m_response_buffer_ptr(NULL),
    m_query_result(false),
    m_query_mutex(NULL),
    m_response_semaphore(NULL)
{
    // Konstruktor
}

void HttpQueryManager::Initialize(Rs485Service* pRs485Service)
{
    m_rs485_service = pRs485Service;
    
    m_query_mutex = xSemaphoreCreateMutex();
    m_response_semaphore = xSemaphoreCreateBinary();
}

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
 */
bool HttpQueryManager::ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer)
{
    if (xSemaphoreTake(m_query_mutex, pdMS_TO_TICKS(100)) == pdFALSE)
    {
        Serial.println(F("[HttpQueryManager] Mutex zauzet, HTTP upit odbijen."));
        return false;
    }

    m_pending_cmd = cmd;
    m_response_buffer_ptr = responseBuffer;
    m_query_result = false;

    // Resetuj semafor (za svaki slucaj ako je ostao 'dat' od ranije)
    xSemaphoreTake(m_response_semaphore, 0); 

    if (!m_rs485_service->RequestBusAccess(this))
    {
        Serial.println(F("[HttpQueryManager] Nije moguce dobiti pristup magistrali."));
        m_pending_cmd = NULL; 
        xSemaphoreGive(m_query_mutex);
        return false;
    }
    
    // Uspješno smo zatražili magistralu. Rs485Service će pozvati naš Service()
    // Service() će poslati paket i mi ćemo čekati da ProcessResponse/OnTimeout
    // oslobode semafor.

    // Čekanje na semafor do 10 sekundi (TIMEOUT)
    if (xSemaphoreTake(m_response_semaphore, pdMS_TO_TICKS(10000)) == pdFALSE)
    {
        Serial.println(F("[HttpQueryManager] HTTP Blokirajuci upit istekao (10s Timeout)!"));
        m_query_result = false;
        // Ako je semafor istekao, mi smo i dalje vlasnici busa
        m_rs485_service->ReleaseBusAccess(this);
    }

    m_pending_cmd = NULL;
    m_response_buffer_ptr = NULL;

    xSemaphoreGive(m_query_mutex);

    return m_query_result;
}


/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void HttpQueryManager::Service()
{
    if (m_pending_cmd != NULL)
    {
        uint8_t packet[MAX_PACKET_LENGTH]; 
        uint16_t length = CreateRs485Packet(m_pending_cmd, packet);
        
        if (!m_rs485_service->SendPacket(packet, length))
        {
            Serial.println(F("[HttpQueryManager] GRESKA: Nije moguce poslati paket."));
            m_query_result = false;
            m_rs485_service->ReleaseBusAccess(this);
            xSemaphoreGive(m_response_semaphore); // Odblokiraj ExecuteBlockingQuery
        }
        // Ako je slanje uspjelo, SendPacket() je prebacio stanje u WAITING.
        // Mi samo čekamo ProcessResponse ili OnTimeout.
    }
    else
    {
        // Ovo se ne bi smjelo desiti ako je RequestBusAccess bio uspješan
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Callback - Stigao je odgovor.
 */
void HttpQueryManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    if (m_response_buffer_ptr != NULL)
    {
        uint16_t data_length = packet[5];

        if (packet[0] == ACK) {
             // Za ACK, možemo vratiti "ACK" ili payload ako postoji
             if (data_length > 0) {
                // Kopira se ceo DATA payload (počinje od indeksa 6)
                memcpy(m_response_buffer_ptr, &packet[6], data_length);
             } else {
                strcpy((char*)m_response_buffer_ptr, "ACK");
             }
        } else {
            // Za NACK ili druge odgovore, kopiraj ceo paket radi analize
            memcpy(m_response_buffer_ptr, packet, length);
        }
    }
    m_query_result = true;
    
    xSemaphoreGive(m_response_semaphore);
    // Rs485Service automatski oslobađa magistralu nakon ProcessResponse.
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void HttpQueryManager::OnTimeout()
{
    m_query_result = false;
    xSemaphoreGive(m_response_semaphore);
    // Rs485Service automatski oslobađa magistralu nakon OnTimeout.
}