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

LogPullManager::LogPullManager() :
    m_rs485_service(NULL),
    m_eeprom_storage(NULL),
    m_state(PullState::IDLE),
    m_current_address_index(0),
    m_current_pull_address(0),
    m_address_list_count(0),
    m_retry_count(0),
    m_hills_query_attempts(0),
    m_last_activity_time(0)
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
 * @brief Provjerava da li je HILLS protokol aktivan.
 */
bool LogPullManager::IsHillsProtocol()
{
    return (static_cast<ProtocolVersion>(g_appConfig.protocol_version) == ProtocolVersion::HILLS);
}

/**
 * @brief Vraća protokol-specifičnu komandu za status request.
 */
uint8_t LogPullManager::GetStatusCommand()
{
    return IsHillsProtocol() ? HILLS_GET_SYS_STATUS : GET_SYS_STAT;
}

/**
 * @brief Vraća protokol-specifičnu komandu za log request.
 */
uint8_t LogPullManager::GetLogCommand()
{
    return IsHillsProtocol() ? HILLS_GET_LOG_LIST : GET_LOG_LIST;
}

/**
 * @brief Vraća protokol-specifičnu komandu za delete request.
 */
uint8_t LogPullManager::GetDeleteCommand()
{
    return IsHillsProtocol() ? HILLS_DELETE_LOG_LIST : DEL_LOG_LIST;
}

/**
 * @brief Vraća protokol-specifičan response timeout.
 */
uint32_t LogPullManager::GetResponseTimeout()
{
    return IsHillsProtocol() ? HILLS_RESPONSE_TIMEOUT_MS : RS485_RESP_TOUT_MS;
}

/**
 * @brief Vraća protokol-specifičan RX-TX delay.
 */
uint32_t LogPullManager::GetRxTxDelay()
{
    return IsHillsProtocol() ? HILLS_RX_TO_TX_DELAY_MS : RX2TX_DEL_MS;
}

/**
 * @brief Glavna funkcija koju poziva state-mašina. Obavlja jedan puni ciklus pollinga.
 */
void LogPullManager::Run()
{
    // ========================================================================
    // --- PROVJERA: DA LI JE LOGGER OMOGUĆEN? ---
    // ========================================================================
    if (!g_appConfig.logger_enable)
    {
        return; // Logger je onemogućen, ne radi ništa
    }
    // ========================================================================
    
    // ========================================================================
    // --- IMPLEMENTACIJA OBAVEZNE RX->TX PAUZE (protokol-specifična) ---
    // ========================================================================
    uint32_t rx_tx_delay = GetRxTxDelay();
    if (millis() - m_last_activity_time < rx_tx_delay)
    {
        return; // Još nije prošla potrebna pauza
    }
    // ========================================================================

    // KORAK 2: Ako čekamo odgovor, provjeri da li je stigao.
    if (m_state == PullState::WAITING_FOR_RESPONSE || m_state == PullState::WAITING_FOR_DELETE_CONFIRMATION)
    {
        uint8_t response_buffer[MAX_PACKET_LENGTH];
        uint32_t timeout = GetResponseTimeout();
        int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, timeout);

        if (response_len > 0) {
            // Imamo odgovor, obradi ga i promijeni stanje.
            ProcessResponse(response_buffer, response_len);
        } else {
            // Timeout
            if (IsHillsProtocol() && m_state == PullState::WAITING_FOR_DELETE_CONFIRMATION)
            {
                // HILLS: Timeout na DELETE confirmation
                m_hills_query_attempts++;
                LOG_DEBUG(3, "[LogPull-HILLS] Timeout na DELETE (attempt %d/%d)\n", 
                    m_hills_query_attempts, HILLS_MAX_QUERY_ATTEMPTS);
                
                if (m_hills_query_attempts >= HILLS_MAX_QUERY_ATTEMPTS)
                {
                    LOG_DEBUG(3, "[LogPull-HILLS] Max attempts dostignut za 0x%X\n", m_current_pull_address);
                    m_state = PullState::IDLE;
                    m_hills_query_attempts = 0;
                }
                else
                {
                    // Pokušaj ponovo GET_LOG_LIST
                    m_state = PullState::SENDING_LOG_REQUEST;
                }
            }
            else
            {
                LOG_DEBUG(4, "[LogPull] Timeout za 0x%X. Sljedeća adresa.\n", m_current_pull_address);
                m_state = PullState::IDLE;
                m_hills_query_attempts = 0;
            }
            m_last_activity_time = millis();
        }
        return;
    }
    
    // KORAK 3: Ako smo slobodni (IDLE), započni novi ciklus anketiranja.
    if (m_state == PullState::IDLE)
    {
        if (m_address_list_count == 0) {
            vTaskDelay(pdMS_TO_TICKS(100)); // Nema adresa, pauziraj.
            return;
        }
        
        m_current_pull_address = GetNextAddress();
        m_retry_count = 0;
        m_hills_query_attempts = 0; // Reset counter za novu adresu
        m_state = PullState::SENDING_STATUS_REQUEST; // Pripremi se za slanje statusnog upita.
    }

    // KORAK 4: Izvrši akciju slanja (ako je stanje postavljeno u prethodnom koraku).
    // Ove funkcije samo pošalju paket i odmah se završe.
    // NAPOMENA: Pauza između paketa (CONST_PAUSE) je implementirana u Rs485Service::SendPacket()
    
    switch (m_state)
    {
        case PullState::SENDING_STATUS_REQUEST:
            SendStatusRequest(m_current_pull_address);
            break;
        case PullState::SENDING_LOG_REQUEST:
            SendLogRequest(m_current_pull_address);
            break;
        default:
            break;
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
    uint8_t cmd = GetStatusCommand();
    LOG_DEBUG(4, "[LogPull] -> Šaljem STATUS(0x%02X) na 0x%X\n", cmd, address);
    
    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint8_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    packet[6] = cmd;
    
    uint16_t checksum = cmd;
    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        m_state = PullState::WAITING_FOR_RESPONSE;
    }
}

/**
 * @brief Kreira i salje DEL_LOG_LIST paket.
 */
void LogPullManager::SendDeleteLogRequest(uint16_t address)
{
    uint8_t cmd = GetDeleteCommand();
    LOG_DEBUG(4, "[LogPull] -> Šaljem DELETE(0x%02X) na 0x%X\n", cmd, address);
    
    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    
    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = 1;
    packet[6] = cmd;
    
    uint16_t checksum = cmd;
    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        if (IsHillsProtocol())
        {
            // HILLS: Čekamo ACK na DELETE
            m_state = PullState::WAITING_FOR_DELETE_CONFIRMATION;
            LOG_DEBUG(4, "[LogPull-HILLS] Čekam DELETE ACK...\n");
        }
        // Standardni: Fire-and-forget
    }
}

/**
 * @brief Kreira i salje GET_LOG_LIST paket (Log Pull).
 */
void LogPullManager::SendLogRequest(uint16_t address)
{
    uint8_t cmd = GetLogCommand();
    LOG_DEBUG(4, "[LogPull] -> Šaljem GET_LOG(0x%02X) na 0x%X\n", cmd, address);

    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint8_t data_len = 1;
    
    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    packet[6] = cmd;
    
    uint16_t checksum = cmd;
    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        m_state = PullState::WAITING_FOR_RESPONSE;
    }
}


/**
 * @brief Obrađuje odgovor primljen od uređaja.
 */
void LogPullManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    uint8_t response_cmd = packet[6];
    uint16_t sender_addr = (packet[3] << 8) | packet[4]; // Adresa uređaja koji je poslao odgovor

    // ========================================================================
    // --- KLJUČNA ISPRAVKA I DEBUG LOG ---
    // ========================================================================
    LOG_DEBUG(4, "[LogPull] Primljen paket od 0x%X (očekujem od 0x%X). CMD: 0x%02X, Dužina: %d\n", 
              sender_addr, m_current_pull_address, response_cmd, length);

    // Striktna provjera: Da li je ovo odgovor od uređaja koji smo pitali?
    if (sender_addr != m_current_pull_address) {
        LOG_DEBUG(4, "[LogPull] -> ODBACUJEM. Paket nije od očekivanog uređaja.\n");
        m_state = PullState::IDLE;
        m_last_activity_time = millis();
        return;
    }

    // ========================================================================
    // --- NOVI DEBUG LOG: Ispis sadržaja primljenog paketa ---
    // ========================================================================
    char payload_str[128] = {0};
    for (int i = 0; i < min((int)length - 9, 40); i++) { // Ispisujemo do 40 bajtova payload-a
        sprintf(payload_str + strlen(payload_str), "%02X ", packet[i + 7]);
    }
    LOG_DEBUG(3, "[LogPull] -> Odgovor od 0x%X: [ %s]\n", sender_addr, payload_str);
    
    // ========================================================================
    // Obrada STATUS odgovora (0xA0 ili 0xBA)
    // ========================================================================
    uint8_t expected_status_cmd = GetStatusCommand();
    if (response_cmd == expected_status_cmd)
    {
        // Provjera "Log Pending" flaga
        if (packet[7] == '1' || (length > 8 && packet[8] == '1'))
        {
            LOG_DEBUG(3, "[LogPull] 0x%X ima log(ove)\n", m_current_pull_address);
            m_state = PullState::SENDING_LOG_REQUEST;
            return;
        }
        else {
             LOG_DEBUG(4, "[LogPull] 0x%X nema logova\n", m_current_pull_address);
        }
    }
    // ========================================================================
    // Obrada GET_LOG odgovora (0xA3 ili 0xB4)
    // ========================================================================
    uint8_t expected_log_cmd = GetLogCommand();
    if (response_cmd == expected_log_cmd)
    {
        uint16_t data_len = packet[5];
        
        // HILLS: Provjera za "prazna lista" (data_len == 1)
        if (IsHillsProtocol() && data_len == 1)
        {
            LOG_DEBUG(3, "[LogPull-HILLS] Prazna lista na 0x%X. Sljedeća adresa.\n", m_current_pull_address);
            m_state = PullState::IDLE;
            m_hills_query_attempts = 0;
            m_last_activity_time = millis();
            return;
        }
        
        // Log paket (18 bajtova: CMD + 16B log + checksum)
        if (data_len >= (LOG_RECORD_SIZE + 2)) 
        {
            LOG_DEBUG(3, "[LogPull] Primljen LOG sa 0x%X\n", m_current_pull_address);
            LogEntry newLog; 
            memcpy((uint8_t*)&newLog, &packet[7], LOG_RECORD_SIZE);
            
            // ========================================================================
            // --- KONAČNA ISPRAVKA: TAČNA REPLIKACIJA LOGIKE IZ hotel_ctrl.c ---
            // Stari kod je radio `i2c_ee_buffer[3] = adr_H; i2c_ee_buffer[4] = adr_L;`
            // NAKON što je kopirao log. Ovo je prepisivalo bajtove na tim pozicijama.
            // Naša LogEntry struktura je niz od 16 bajtova.
            // Event se nalazi na 3. bajtu (indeks 2).
            // Adresa se upisuje na 4. i 5. bajt (indeksi 3 i 4).
            
            // Kasting u niz bajtova za direktnu manipulaciju
            uint8_t* log_bytes = (uint8_t*)&newLog;
            log_bytes[3] = (m_current_pull_address >> 8) & 0xFF; // Upis adrese (MSB) na 4. bajt
            log_bytes[4] = m_current_pull_address & 0xFF;        // Upis adrese (LSB) na 5. bajt
            // Originalni event na log_bytes[2] ostaje netaknut, što je ispravno ponašanje.
            // ========================================================================
            
            // NOVI DEBUG LOG: Ispis heksadecimalnog sadržaja strukture newLog prije upisa
            char log_hex_buffer[LOG_RECORD_SIZE * 3 + 1];
            log_hex_buffer[0] = '\0';
            uint8_t* log_ptr = (uint8_t*)&newLog;
            for (int i = 0; i < LOG_RECORD_SIZE; i++) {
                sprintf(log_hex_buffer + strlen(log_hex_buffer), "%02X ", log_ptr[i]);
            }
            LOG_DEBUG(3, "[LogPull] -> Pripremljen Log za upis: [ %s]\n", log_hex_buffer);

            if (m_eeprom_storage->WriteLog(&newLog) == LoggerStatus::LOGGER_OK)
            {
                LOG_DEBUG(3, "[LogPull] -> Log upisan (ID:%u, addr:0x%X)\n", 
                    newLog.log_id, m_current_pull_address);
                SendDeleteLogRequest(m_current_pull_address);
                
                // HILLS vs Standardni
                if (IsHillsProtocol())
                {
                    // HILLS: State već postavljen u SendDeleteLogRequest()
                    // Čekamo DELETE ACK pa nastavljamo ping-pong
                    LOG_DEBUG(4, "[LogPull-HILLS] Ping-pong: čekam DELETE ACK\n");
                }
                else
                {
                    // Standardni: Vrati se na status check
                    m_state = PullState::SENDING_STATUS_REQUEST;
                }
            }
            return;
        }
    }
    // ========================================================================
    // HILLS: Obrada DELETE ACK
    // ========================================================================
    uint8_t expected_delete_cmd = GetDeleteCommand();
    if (IsHillsProtocol() && m_state == PullState::WAITING_FOR_DELETE_CONFIRMATION)
    {
        // Provjeri ACK i komandu
        if (packet[0] == ACK && response_cmd == expected_delete_cmd)
        {
            LOG_DEBUG(3, "[LogPull-HILLS] DELETE ACK primljen. Nastavljam ping-pong.\n");
            m_hills_query_attempts++;
            
            if (m_hills_query_attempts >= HILLS_MAX_QUERY_ATTEMPTS)
            {
                LOG_DEBUG(3, "[LogPull-HILLS] Max ping-pong ciklusa za 0x%X\n", m_current_pull_address);
                m_state = PullState::IDLE;
                m_hills_query_attempts = 0;
            }
            else
            {
                // Nastavi ping-pong: šalji novi GET_LOG_LIST
                m_state = PullState::SENDING_LOG_REQUEST;
            }
            m_last_activity_time = millis();
            return;
        }
    }

    // Default: Vrati se u IDLE
    m_state = PullState::IDLE;
    m_hills_query_attempts = 0;
    m_last_activity_time = millis();
}
