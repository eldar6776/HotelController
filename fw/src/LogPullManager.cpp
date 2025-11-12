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
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void LogPullManager::Service()
{
    if (m_address_list_count == 0)
    {
        // Prazna lista, ne radimo ništa. Odmah oslobodi magistralu.
        m_rs485_service->ReleaseBusAccess(this);
        return;
    }
    
    // Ako smo dobili magistralu, a nismo već u sred transakcije
    if (m_state == PullState::IDLE && m_rs485_service->GetCurrentBusOwner() == this)
    {
        // Uzmi sljedecu adresu i posalji upit za status (Polling)
        LOG_DEBUG(4, "[LogPull] Stanje: IDLE. Uzimam sljedeću adresu...\n");
        m_current_pull_address = GetNextAddress();
        m_retry_count = 0; // NOVO: Resetuj brojač za novi uređaj
        SendStatusRequest(m_current_pull_address);
    }
    else if (m_state == PullState::IDLE)
    {
        // Nismo vlasnici, a pozvani smo? To ne bi trebalo da se desi, ali za svaki slučaj oslobodi.
        m_rs485_service->ReleaseBusAccess(this);
    }
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
        LOG_DEBUG(5, "[LogPull] -> Stanje prešlo u WAITING_FOR_STATUS_ACK\n");
        m_state = PullState::WAITING_FOR_STATUS_ACK;
    }
    else
    {
        LOG_DEBUG(1, "[LogPull] GRESKA: SendPacket nije uspio. Resetujem stanje na IDLE.\n");
        m_state = PullState::IDLE;
        m_rs485_service->ReleaseBusAccess(this); // Oslobodi magistralu
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
        LOG_DEBUG(5, "[LogPull] -> Stanje prešlo u WAITING_FOR_LOG_ACK\n");
        m_state = PullState::WAITING_FOR_LOG_ACK;
    }
    else
    {
        LOG_DEBUG(1, "[LogPull] GRESKA: SendPacket (za log) nije uspio. Resetujem stanje na IDLE.\n");
        m_state = PullState::IDLE;
        m_rs485_service->ReleaseBusAccess(this); // Oslobodi magistralu
    }
}


/**
 * @brief Callback koji poziva Rs485Service kada stigne odgovor.
 */
void LogPullManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    m_retry_count = 0; // Resetuj brojač nakon uspešnog odgovora
    // Pretpostavljamo da je paket validiran od strane Rs485Service
    uint8_t response_cmd = packet[6];
    
    if (m_state == PullState::WAITING_FOR_STATUS_ACK)
    {
        if (response_cmd == GET_SYS_STAT)
        {
            // Provjera statusa: HC logika je često koristila rx_buff[7]=='1' ili rx_buff[8]=='1'
            // za provjeru pending loga. Pretpostavljamo da je Log Status na indeksu 7 (rx_buff[7]).
            uint8_t log_pending_flag = packet[7]; 
            
            LOG_DEBUG(4, "[LogPull] Primljen odgovor na STAT sa adrese 0x%X. Log pending: %d\n", m_current_pull_address, log_pending_flag);

            if (log_pending_flag > 0) // Ako postoji pending log
            {
                // Uredjaj ima log, trazi ga. Ostajemo vlasnici magistrale.
                SendLogRequest(m_current_pull_address);
                return; // Ne oslobađamo bus!
            }
        }
    }
    else if (m_state == PullState::WAITING_FOR_LOG_ACK)
    {
        uint8_t ack_nack = packet[0];
        uint16_t data_len = packet[5];

        if (ack_nack == ACK && response_cmd == GET_LOG_LIST)
        {
            // Očekujemo da odgovor sadrži LogEntry + status/overhead (LOG_RECORD_SIZE)
            // LogEntry = 16 bajtova, LOG_RECORD_SIZE = 20 (u memoriji EEPROM-a)
            if (data_len > 1) // Svaki log ima bar 1 bajt payload-a
            {
                LOG_DEBUG(3, "[LogPull] Primljen LOG (Dužina: %d) sa adrese 0x%X\n", data_len, m_current_pull_address);
                
                LogEntry newLog; 
                // Podaci počinju od paketa[7] (poslije CMD bajta)
                memcpy((uint8_t*)&newLog, &packet[7], sizeof(LogEntry));
                
                // Zapiši log u EEPROM
                if (m_eeprom_storage->WriteLog(&newLog) == LoggerStatus::LOGGER_OK)
                {
                    LOG_DEBUG(3, "[LogPull] Log uspješno zapisan u EEPROM.\n");
                }
            }
            else
            {
                LOG_DEBUG(4, "[LogPull] Uredjaj 0x%X potvrdio praznu log listu.\n", m_current_pull_address);
            }
        }
    }

    m_state = PullState::IDLE;
    m_rs485_service->ReleaseBusAccess(this);
}

/**
 * @brief Callback koji poziva Rs485Service ako uredjaj nije odgovorio.
 */
void LogPullManager::OnTimeout()
{
    m_retry_count++;
    LOG_DEBUG(2, "[LogPull] Timeout za 0x%X. Pokušaj %d od %d...\n", m_current_pull_address, m_retry_count, MAX_RS485_RETRIES);

    if (m_retry_count >= MAX_RS485_RETRIES)
    {
        LOG_DEBUG(2, "[LogPull] Uredjaj 0x%X nedostupan nakon %d pokušaja. Oslobađam magistralu.\n", m_current_pull_address, MAX_RS485_RETRIES);
        m_state = PullState::IDLE;
        m_rs485_service->ReleaseBusAccess(this);
    }
    else
    {
        // Ponovo pošalji isti zahtev (Status ili Log)
        if (m_state == PullState::WAITING_FOR_STATUS_ACK) {
            SendStatusRequest(m_current_pull_address);
        }
        else if (m_state == PullState::WAITING_FOR_LOG_ACK) {
            SendLogRequest(m_current_pull_address);
        } else {
            // Neočekivano stanje, oslobodi magistralu
            m_rs485_service->ReleaseBusAccess(this);
        }
    }
}

bool LogPullManager::WantsBus()
{
    // LogPullManager uvijek želi magistralu ako je slobodna,
    // jer je to pozadinski posao koji se stalno vrti.
    // Dispečer će ga prekinuti ako se pojavi zadatak višeg prioriteta.
    // Dodatna provjera da ne tražimo bus ako je lista prazna.
    return (m_address_list_count > 0);
}

const char* LogPullManager::Name() const
{
    return "LogPullManager";
}

uint32_t LogPullManager::GetTimeoutMs() const
{
    return RS485_RESP_TOUT_MS; // Koristimo standardni timeout za polling
}
