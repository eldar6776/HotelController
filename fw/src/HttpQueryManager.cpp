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
#include <esp_task_wdt.h>  // NOVO: Uključujemo za watchdog reset
#include <cstring>

// Globalni objekat za konfiguraciju (extern)
extern AppConfig g_appConfig; 

// Makro Vrijednosti
#define DEF_HC_OWIFA 31U

// Definišemo specifičan timeout za HTTP upite
#define HTTP_QUERY_TIMEOUT_MS 50

HttpQueryManager::HttpQueryManager() :
    m_rs485_service(NULL),
    m_pending_cmd(NULL),
    m_response_buffer_ptr(NULL),
    m_query_result(false),
    m_retry_count(0), // NOVO: Inicijalizacija brojača
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

    LOG_DEBUG(4, "[HttpQuery] Primljen zahtev za komandu 0x%X na adresu 0x%X\n", cmd->cmd_id, cmd->address);

    m_pending_cmd = cmd;
    m_response_buffer_ptr = responseBuffer;
    m_query_result = false;
    m_retry_count = 0; // NOVO: Resetuj brojač na početku svakog upita

    // Resetuj semafore (za svaki slucaj ako su ostali 'dati' od ranije)
    xSemaphoreTake(m_response_semaphore, 0); 

    bool response_received = false;

    // KORAK 1: Signaliziraj da želimo magistralu.
    // Dispečer će ovo vidjeti preko WantsBus() i prekinuti LogPullManager.
    // Dispečer će pozvati naš Service() metod kada dobijemo pristup.
    // Ovde samo čekamo da se cela transakcija završi.
    
    // KORAK 2: Čekaj da se transakcija završi (uspehom ili neuspehom)
    // Ukupni timeout mora biti veći od svih mogućih retry-a + margine
    // (MAX_RS485_RETRIES * RS485_RESP_TOUT_MS) + margina
    // Primer: (3 * 45ms) + 200ms = 335ms. Koristimo malo veći timeout.
    const TickType_t total_wait_ticks = pdMS_TO_TICKS( (MAX_RS485_RETRIES * RS485_RESP_TOUT_MS) + 200 );
    if (xSemaphoreTake(m_response_semaphore, total_wait_ticks) == pdTRUE)
    {
        response_received = true; 
    }

    if (!response_received)
    {
        LOG_DEBUG(2, "[HttpQuery] TIMEOUT. Nije primljen odgovor na komandu.\n");
        LOG_DEBUG(1, "[HttpQuery] HTTP Blokirajuci upit istekao (timeout)!\n");
        m_query_result = false;
        // KRITIČNO: Ako je semafor istekao, moramo nasilno osloboditi magistralu!
        // Ovo sprečava zaključavanje ako se desi neka nepredviđena greška.
        // Iako Bus Watchdog ovo rešava, dobro je imati i ovde osiguranje.
        m_rs485_service->ReleaseBusAccess(this);
    }    
    
    xSemaphoreGive(m_query_mutex); // Otključaj mutex za sledeći HTTP zahtev
    LOG_DEBUG(4, "[HttpQuery] Završen. Rezultat: %s\n", m_query_result ? "USPEH" : "NEUSPEH");
    return m_query_result;
}


/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void HttpQueryManager::Service()
{
    // Proveri da li smo dobili magistralu i da li imamo komandu
    if (m_pending_cmd != NULL && m_rs485_service->GetCurrentBusOwner() == this)
    {
        // Ako je ovo prvi pokušaj (retry_count == 0), pošalji paket.
        // U suprotnom, OnTimeout() će se pobrinuti za ponovno slanje.
        if (m_retry_count == 0) {
            uint8_t packet[MAX_PACKET_LENGTH]; 
            uint16_t length = CreateRs485Packet(m_pending_cmd, packet);
            
            if (!m_rs485_service->SendPacket(packet, length))
            {
                // KRITIČNA ISPRAVKA: Ako slanje ne uspije, moramo osloboditi magistralu i odblokirati HTTP zahtjev.
                LOG_DEBUG(1, "[HttpQuery] GRESKA: SendPacket nije uspio. Prekidam upit.\n");
                m_query_result = false;
                m_pending_cmd = NULL; // Obriši zahtjev
                m_rs485_service->ReleaseBusAccess(this); // Oslobodi magistralu
                xSemaphoreGive(m_response_semaphore); // Odblokiraj ExecuteBlockingQuery
            }
        }
    }
    else
    {
        // Ako smo pozvani, a nemamo komandu, odmah oslobodi magistralu.
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Callback - Stigao je odgovor.
 */
void HttpQueryManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    m_retry_count = 0; // Resetuj brojač nakon uspešnog odgovora
    LOG_DEBUG(4, "[HttpQuery] Primljen odgovor. Dužina: %d\n", length);
    if (m_response_buffer_ptr != NULL)
    {
        // =================================================================================
        // KLJUČNA ISPRAVKA: Implementacija parsiranja identična originalnom kodu.
        // memcpy(pcInsert, &rx_buff[7], rx_buff[5]-2U);
        // =================================================================================
        if (length >= 9) // Minimalna dužina paketa
        {
            uint16_t data_field_len = packet[5];
            if (data_field_len >= 2 && (data_field_len + 7) <= length)
            {
                uint16_t payload_len = data_field_len - 2; // Tačna logika iz starog koda
                memcpy(m_response_buffer_ptr, &packet[7], payload_len);
                m_response_buffer_ptr[payload_len] = '\0'; // Osiguraj null-terminator
            }
            else
            {
                // Ako je odgovor samo ACK bez payload-a
                strcpy((char*)m_response_buffer_ptr, "OK");
            }
        }
        else
        {
            // Ako je paket neispravan ili prekratak
            strcpy((char*)m_response_buffer_ptr, "ERROR");
        }
        m_query_result = true;
    }
    
    // =================================================================================
    // KONAČNO REŠENJE: Pravilan redosled oslobađanja resursa.
    // 1. Resetuj stanje (da WantsBus() vrati false).
    // 2. Odblokiraj HTTP server.
    // 3. Tek na kraju oslobodi magistralu.
    // =================================================================================
    m_pending_cmd = NULL;
    xSemaphoreGive(m_response_semaphore); // Obavijesti ExecuteBlockingQuery da je posao gotov
    m_rs485_service->ReleaseBusAccess(this);
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void HttpQueryManager::OnTimeout()
{
    m_retry_count++;
    LOG_DEBUG(2, "[HttpQuery] Timeout. Pokušaj %d od %d...\n", m_retry_count, MAX_RS485_RETRIES);

    if (m_retry_count >= MAX_RS485_RETRIES)
    {
        LOG_DEBUG(2, "[HttpQuery] Dostignut maksimalan broj pokušaja. Odustajem.\n");
        m_query_result = false;
        // KONAČNO REŠENJE: Pravilan redosled i ovde.
        m_pending_cmd = NULL;
        xSemaphoreGive(m_response_semaphore); // Odblokiraj ExecuteBlockingQuery sa rezultatom 'false'
        m_rs485_service->ReleaseBusAccess(this); // Oslobodi magistralu nakon neuspjeha
    }
    else
    {
        // Ponovo pošalji isti paket
        uint8_t packet[MAX_PACKET_LENGTH];
        uint16_t length = CreateRs485Packet(m_pending_cmd, packet);
        if (!m_rs485_service->SendPacket(packet, length))
        {
            LOG_DEBUG(1, "[HttpQuery] GRESKA: Ponovno slanje paketa nije uspjelo. Prekidam upit.\n");
            m_query_result = false;
            xSemaphoreGive(m_response_semaphore); // Odblokiraj HTTP sa greškom
        }
    }
}

/**
 * @brief NOVO: Signalizira da HttpQueryManager ima hitan posao.
 * @return true ako postoji komanda na čekanju, u suprotnom false.
 */
bool HttpQueryManager::WantsBus()
{
    return (m_pending_cmd != NULL);
}

const char* HttpQueryManager::Name() const
{
    return "HttpQueryManager";
}

uint32_t HttpQueryManager::GetTimeoutMs() const
{
    return HTTP_QUERY_TIMEOUT_MS;
}