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

// Makro Vrijednosti Glavnih Update Paketa (za Switch case logiku)
#define CMD_SET_DIN_CFG 0xC6 // Set Digital Input Configuration
#define CMD_SET_DOUT_STATE 0xD2 // Set Digital Output State
#define DEF_HC_OWIFA 31U

HttpQueryManager::HttpQueryManager() :
    m_rs485_service(NULL),
    m_pending_cmd(NULL),
    m_response_buffer_ptr(NULL),
    m_query_result(false)
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
        case GET_APPL_STAT:
        case RESET_SOS_ALARM:
        case RESTART_CTRL:
        {
            // Nema dodatnog payload-a
            data_length = 1; 
            break;
        }
        case SET_ROOM_TEMP: // tha
        {
            data_length = 4;
            buffer[7] = (uint8_t)cmd->param1; // Setpoint (spt)
            buffer[8] = (uint8_t)cmd->param2; // Differential (dif)
            buffer[9] = (uint8_t)cmd->param3; // Config byte (sta, mod, ctr, out)
            break;
        }
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
        case SET_BEDDING_REPL: // sbr (Data polje sadrži samo vrijednost)
        {
            data_length = 2;
            buffer[7] = (uint8_t)cmd->param1; // Period zamjene
            break;
        }
        case CMD_SET_DIN_CFG: // cdi: 8 config bajtova
        {
            // Očekuje se da string_ptr ukazuje na 8 bajtova (dix=x) + NULL separatore
            data_length = 9; 
            memcpy(&buffer[7], cmd->string_ptr, 8); // Kopiraj 8 bajtova konfiguracije
            break;
        }
        case CMD_SET_DOUT_STATE: // cdo: 8 config bajtova + 1 ctrl bajt
        {
            // Očekuje se da string_ptr ukazuje na 9 bajtova (doX=x i ctrl=x)
            data_length = 10; 
            memcpy(&buffer[7], cmd->string_ptr, 9); // Kopiraj 9 bajtova konfiguracije
            break;
        }
        case SET_RS485_CFG: // rsc: rsa, rga, rba, rib
        {
            // Podaci su sekvencijalno spakovani u cmd->string_ptr
            data_length = 8; // rsa(2) + rga(2) + rba(2) + rib(1) + CMD(1)
            memcpy(&buffer[7], cmd->string_ptr, 7); // Kopiraj 7 bajtova konfiguracije (bez cmd bajta)
            break;
        }
        // DEFAULT: Za sve nepoznate/jednostavne komande pretpostavimo 1 data bajt
        default:
        {
            data_length = 1;
            break;
        }
    }

    buffer[5] = (uint8_t)data_length; // Data Length
    uint16_t total_packet_length = data_length + 9;

    // --- LOGIKA ONEWIRE BRIDGE (owa) ---
    if (cmd->owa_addr != 0)
    {
        uint16_t original_data_length = data_length;
        
        // Pomjeranje originalnog data payload-a
        for (i = total_packet_length; i > 5; i--)
        {
            buffer[i + 4] = buffer[i]; // Dodatnih 4 bajta za BR2OW header
        }

        // Umetni BR2OW Header (CMD + TX_ADDR + IF_ADDR + LEN)
        buffer[6] = SET_BR2OW; 
        buffer[7] = cmd->owa_addr;       // Onewire receiver address
        buffer[8] = DEF_HC_OWIFA;        // Onewire interface address (iz common.h/ProjectConfig.h)
        buffer[9] = original_data_length; // Onewire Data Payload Size (ukljucuje originalni CMD)
        
        // Originalni CMD je sada na buffer[10] (data_len se mora ažurirati)
        data_length += 4; 
        buffer[5] = (uint8_t)data_length;
        total_packet_length = data_length + 9;
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

    if (!m_rs485_service->RequestBusAccess(this))
    {
        m_pending_cmd = NULL; 
        xSemaphoreGive(m_query_mutex);
        return false;
    }
    
    // Čekanje na semafor do 10 sekundi (TIMEOUT)
    if (xSemaphoreTake(m_response_semaphore, pdMS_TO_TICKS(10000)) == pdFALSE)
    {
        Serial.println(F("[HttpQueryManager] HTTP Blokirajuci upit istekao (10s Timeout)!"));
        m_query_result = false;
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
            m_rs485_service->ReleaseBusAccess(this);
            xSemaphoreGive(m_response_semaphore);
        }
    }
    else
    {
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
        // Kopira se samo DATA payload (počinje od indeksa 7)
        uint16_t data_length = packet[5];
        memcpy(m_response_buffer_ptr, &packet[7], data_length);
        m_response_buffer_ptr[data_length] = '\0'; 
    }
    m_query_result = true;
    
    xSemaphoreGive(m_response_semaphore);
    // Rs485Service automatski oslobađa magistralu.
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void HttpQueryManager::OnTimeout()
{
    m_query_result = false;
    xSemaphoreGive(m_response_semaphore);
}