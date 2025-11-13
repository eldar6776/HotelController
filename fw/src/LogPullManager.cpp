/**
 ******************************************************************************
 * @file    LogPullManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija LogPullManager modula.
 ******************************************************************************
 */
#include "DebugConfig.h" 
#include "LogPullManager.h"
#include "ProjectConfig.h"
#include <cstring> 

// Globalna konfiguracija (extern)
extern AppConfig g_appConfig; 

// RS485 Kontrolni Karakteri i Komande
#define SOH 0x01
#define EOT 0x04
#define ACK 0x06 // Primjer ACK statusa
#define GET_SYS_STAT        0xA0 // CMD za Status Kontrolera (Polling)
#define GET_LOG_LIST        0xA3 // CMD za Dohvat Loga (Log Pull)


LogPullManager::LogPullManager() :
    m_rs485_service(NULL),
    m_eeprom_storage(NULL),
    m_state(PullState::IDLE),
    m_current_address_index(0),
    m_current_pull_address(0),
    m_address_list_count(0),
    m_retry_count(0) // NOVO: Inicijalizacija brojača
{
    // Konstruktor
}

void LogPullManager::Initialize(Rs485Service* pRs485Service, EepromStorage* pEepromStorage)
{
    m_rs485_service = pRs485Service;
    m_eeprom_storage = pEepromStorage;

    // Ucitaj listu adresa iz EEPROM-a u RAM
    // Koristi MAX_ADDRESS_LIST_SIZE iz ProjectConfig.h
    if (!m_eeprom_storage->ReadAddressList(m_address_list, MAX_ADDRESS_LIST_SIZE, &m_address_list_count))
    {
        Serial.println(F("[LogPullManager] GRESKA: Nije moguce ucitati listu adresa!"));
    }
    else
    {
        Serial.printf("[LogPullManager] Lista adresa ucitana, %d uredjaja.\n", m_address_list_count);
        // DEBUG ISPIS: Prikaži sve adrese koje su pročitane
        for(uint16_t i = 0; i < m_address_list_count; i++) {
            Serial.printf("  -> Pročitana Adresa[%d]: %04d (0x%04X)\n", i, m_address_list[i], m_address_list[i]);
        }
    }
}

/**
 * @brief Glavna funkcija koju poziva state-mašina. Obavlja jedan puni ciklus pollinga.
 */
void LogPullManager::Run()
{
    if (m_address_list_count == 0)
    {
        // Prazna lista, ne radimo ništa.
        vTaskDelay(pdMS_TO_TICKS(100)); // Mala pauza da ne opteretimo CPU
        return;
    }
    
    // Uzmi sljedecu adresu i posalji upit za status (Polling)
    m_current_pull_address = GetNextAddress();
    m_retry_count = 0; 

    SendStatusRequest(m_current_pull_address);

    uint8_t response_buffer[MAX_PACKET_LENGTH];
    int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, RS485_RESP_TOUT_MS);

    if (response_len > 0) {
        ProcessResponse(response_buffer, response_len);
    } // Ako je timeout, ne radimo ništa, samo prelazimo na sljedeću adresu u idućem pozivu Run()
}

/**
 * @brief Vraća sljedeću adresu za polling (Replicira HC_GetNextAddr).
 */
uint16_t LogPullManager::GetNextAddress()
{
    if (m_address_list_count == 0) return 0;
    
    // Uzimamo trenutnu adresu
    uint16_t current_address = m_address_list[m_current_address_index];

    // Pomjeramo indeks na sljedeću adresu
    m_current_address_index = (m_current_address_index + 1);

    // Ako smo došli do kraja liste, vraćamo se na početak
    if (m_current_address_index >= m_address_list_count)
    {
        m_current_address_index = 0;
    }
    
    return current_address;
}

/**
 * @brief Kreira i salje GET_SYS_STAT paket (Polling).
 */
void LogPullManager::SendStatusRequest(uint16_t address)
{
    LOG_DEBUG(4, "[LogPull] -> Šaljem GET_SYS_STAT na adresu 0x%X\n", address);
    
    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint8_t cmd = GET_SYS_STAT;
    uint8_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    packet[6] = cmd;
    
    // Checksum (samo na cmd bajtu)
    uint16_t checksum = cmd; 

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        m_state = PullState::WAITING_FOR_STATUS_ACK;
    }
}

/**
 * @brief Kreira i salje GET_LOG_LIST paket (Log Pull).
 */
void LogPullManager::SendLogRequest(uint16_t address)
{
    LOG_DEBUG(4, "[LogPull] -> Uređaj ima log. Šaljem GET_LOG_LIST na adresu 0x%X\n", address);

    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint8_t cmd = GET_LOG_LIST;
    uint8_t data_len = 1;
    
    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    packet[6] = cmd;
    
    // Checksum (samo na cmd bajtu)
    uint16_t checksum = cmd;

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        m_state = PullState::WAITING_FOR_LOG_ACK;
    }
}


/**
 * @brief Obrađuje odgovor primljen od uređaja.
 */
void LogPullManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    uint8_t response_cmd = packet[6];
    
    if (m_state == PullState::WAITING_FOR_STATUS_ACK)
    {
        if (response_cmd == GET_SYS_STAT)
        {
            uint8_t log_pending_flag = packet[7]; 
            
            LOG_DEBUG(4, "[LogPull] Primljen odgovor na STAT sa adrese 0x%X. Log pending: %d\n", m_current_pull_address, log_pending_flag);

            if (log_pending_flag > 0) // Ako postoji pending log
            {
                SendLogRequest(m_current_pull_address);
                
                // Čekaj odgovor na log request
                uint8_t log_response_buffer[MAX_PACKET_LENGTH];
                int log_response_len = m_rs485_service->ReceivePacket(log_response_buffer, MAX_PACKET_LENGTH, RS485_RESP_TOUT_MS);
                if (log_response_len > 0) {
                    // Proslijedi odgovor na obradu (rekurzivni poziv sa promijenjenim stanjem)
                    ProcessResponse(log_response_buffer, log_response_len);
                }
            }
        }
    }
    else if (m_state == PullState::WAITING_FOR_LOG_ACK)
    {
        if (response_cmd == GET_LOG_LIST)
        {
            uint16_t data_len = packet[5];
            if (data_len > 1) // Svaki log ima bar 1 bajt payload-a
            {
                LOG_DEBUG(3, "[LogPull] Primljen LOG (Dužina: %d) sa adrese 0x%X\n", data_len, m_current_pull_address);
                
                LogEntry newLog; 
                memcpy((uint8_t*)&newLog, &packet[7], sizeof(LogEntry));
                
                if (m_eeprom_storage->WriteLog(&newLog) == LoggerStatus::LOGGER_OK)
                {
                    LOG_DEBUG(3, "[LogPull] Log uspješno zapisan u EEPROM.\n");
                }
            }
        }
    }

    m_state = PullState::IDLE;
}
