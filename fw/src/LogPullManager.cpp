/**
 ******************************************************************************
 * @file    LogPullManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija LogPullManager modula.
 ******************************************************************************
 */

#include "LogPullManager.h"

LogPullManager::LogPullManager() :
    m_rs485_service(NULL),
    m_eeprom_storage(NULL),
    m_state(PullState::IDLE),
    m_current_address_index(0),
    m_current_pull_address(0),
    m_address_list_count(0)
{
    // Konstruktor
}

void LogPullManager::Initialize(Rs485Service* pRs485Service, EepromStorage* pEepromStorage)
{
    m_rs485_service = pRs485Service;
    m_eeprom_storage = pEepromStorage;

    // Ucitaj listu adresa iz EEPROM-a u RAM
    if (!m_eeprom_storage->ReadAddressList(m_address_list, MAX_ADDRESS_LIST_SIZE, &m_address_list_count))
    {
        Serial.println(F("[LogPullManager] GRESKA: Nije moguce ucitati listu adresa!"));
    }
    else
    {
        Serial.printf("[LogPullManager] Lista adresa ucitana, %d uredjaja.\n", m_address_list_count);
    }
}

/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void LogPullManager::Service()
{
    if (m_address_list_count == 0)
    {
        Serial.println(F("[LogPullManager] Nema adresa za polling."));
        m_rs485_service->ReleaseBusAccess(this);
        delay(1000); // Sacekaj prije nego sto pitas ponovo
        return;
    }
    
    // Uzmi sljedecu adresu i posalji upit
    m_current_pull_address = GetNextAddress();
    SendStatusRequest(m_current_pull_address);
}

uint16_t LogPullManager::GetNextAddress()
{
    if (m_current_address_index >= m_address_list_count)
    {
        m_current_address_index = 0;
    }
    return m_address_list[m_current_address_index++];
}

void LogPullManager::SendStatusRequest(uint16_t address)
{
    Serial.printf("[LogPullManager] Slanje GET_SYS_STAT na adresu %d\n", address);
    
    // TODO: Kreirati 'GET_SYS_STAT' paket
    uint8_t packet[32]; // Placeholder
    uint16_t length = 0;
    
    if (m_rs485_service->SendPacket(packet, length))
    {
        m_state = PullState::WAITING_FOR_STATUS_ACK;
    }
    else
    {
        // Nije uspio poslati, magistrala zauzeta?
        m_state = PullState::IDLE;
        m_rs485_service->ReleaseBusAccess(this);
    }
}

void LogPullManager::SendLogRequest(uint16_t address)
{
    Serial.printf("[LogPullManager] Slanje GET_LOG_LIST na adresu %d\n", address);
    // TODO: Kreirati 'GET_LOG_LIST' paket
    uint8_t packet[32]; // Placeholder
    uint16_t length = 0;
    
    if (m_rs485_service->SendPacket(packet, length))
    {
        m_state = PullState::WAITING_FOR_LOG_ACK;
    }
    else
    {
        m_state = PullState::IDLE;
        m_rs485_service->ReleaseBusAccess(this);
    }
}


/**
 * @brief Callback koji poziva Rs485Service kada stigne odgovor.
 */
void LogPullManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    if (m_state == PullState::WAITING_FOR_STATUS_ACK)
    {
        // Stigao je odgovor na GET_SYS_STAT
        Serial.printf("[LogPullManager] Primljen odgovor na STAT sa adrese %d\n", m_current_pull_address);

        // TODO: Parsirati odgovor
        bool imaLog = false; // (packet[7] == '1')

        if (imaLog)
        {
            // Uredjaj ima log, trazi ga
            SendLogRequest(m_current_pull_address);
            // Ostajemo vlasnici magistrale, cekamo sljedeci odgovor
            return;
        }
    }
    else if (m_state == PullState::WAITING_FOR_LOG_ACK)
    {
        // Stigao je odgovor na GET_LOG_LIST
        Serial.printf("[LogPullManager] Primljen LOG sa adrese %d\n", m_current_pull_address);
        
        // TODO: Parsirati log iz 'packet' u 'LogEntry' strukturu
        LogEntry newLog; 
        
        // m_eeprom_storage->WriteLog(&newLog);
    }

    // U svakom slucaju, zavrsili smo sa ovim uredjajem
    m_state = PullState::IDLE;
    m_rs485_service->ReleaseBusAccess(this);
}

/**
 * @brief Callback koji poziva Rs485Service ako uredjaj nije odgovorio.
 */
void LogPullManager::OnTimeout()
{
    Serial.printf("[LogPullManager] Uredjaj %d nije odgovorio (Timeout).\n", m_current_pull_address);
    m_state = PullState::IDLE;
    // Rs485Service ce automatski osloboditi magistralu
}
