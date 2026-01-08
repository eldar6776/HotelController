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
    m_address_list_count_L(0),
    m_address_list_count_R(0),
    m_address_index_L(0),
    m_address_index_R(0),
    m_current_bus(0),
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

    if (g_appConfig.enable_dual_bus_mode)
    {
        // Dual bus mode - učitaj obje liste
        Serial.println(F("[LogPullManager] DUAL BUS MODE - Učitavam L i R liste"));
        
        if (!m_eeprom_storage->ReadAddressListL(m_address_list_L, MAX_ADDRESS_LIST_SIZE_PER_BUS, &m_address_list_count_L))
        {
            Serial.println(F("[LogPullManager] GRESKA: LIJEVI bus lista nije učitana!"));
        }
        else
        {
            Serial.printf("[LogPullManager] LIJEVI bus: %d uređaja\n", m_address_list_count_L);
            for(uint16_t i = 0; i < m_address_list_count_L; i++) {
                Serial.printf("  -> L[%d]: %04d (0x%04X)\n", i, m_address_list_L[i], m_address_list_L[i]);
            }
        }
        
        if (!m_eeprom_storage->ReadAddressListR(m_address_list_R, MAX_ADDRESS_LIST_SIZE_PER_BUS, &m_address_list_count_R))
        {
            Serial.println(F("[LogPullManager] GRESKA: DESNI bus lista nije učitana!"));
        }
        else
        {
            Serial.printf("[LogPullManager] DESNI bus: %d uređaja\n", m_address_list_count_R);
            for(uint16_t i = 0; i < m_address_list_count_R; i++) {
                Serial.printf("  -> R[%d]: %04d (0x%04X)\n", i, m_address_list_R[i], m_address_list_R[i]);
            }
        }
        
        m_current_bus = 0; // Počinjemo sa Lijevim busom
        m_address_list_count = 0; // Legacy lista nije u upotrebi
    }
    else
    {
        // Single bus mode - samo Lijeva lista (legacy kompatibilnost)
        Serial.println(F("[LogPullManager] SINGLE BUS MODE - Učitavam samo L listu"));
        
        if (!m_eeprom_storage->ReadAddressList(m_address_list, MAX_ADDRESS_LIST_SIZE, &m_address_list_count))
        {
            Serial.println(F("[LogPullManager] GRESKA: Nije moguće učitati listu adresa!"));
        }
        else
        {
            Serial.printf("[LogPullManager] Lista adresa učitana, %d uređaja.\n", m_address_list_count);
            for(uint16_t i = 0; i < m_address_list_count; i++) {
                Serial.printf("  -> Pročitana Adresa[%d]: %04d (0x%04X)\n", i, m_address_list[i], m_address_list[i]);
            }
        }
        
        m_address_list_count_L = 0;
        m_address_list_count_R = 0;
    }
}

/**
 * @brief Provjerava da li je HILLS protokol aktivan za trenutnu adresu.
 */
bool LogPullManager::IsHillsProtocol()
{
    // KRITIČNO: NE koristiti m_current_bus jer se mijenja prerano u GetNextAddress()!
    // Umjesto toga, odredi protokol na osnovu ADRESE koja se trenutno polluje
    uint8_t current_protocol;
    
    if (g_appConfig.enable_dual_bus_mode) {
        int8_t bus_id = GetBusForAddress(m_current_pull_address);
        if (bus_id == 0) {
            current_protocol = g_appConfig.protocol_version_L;
        } else if (bus_id == 1) {
            current_protocol = g_appConfig.protocol_version_R;
        } else {
            // Fallback ako adresa nije u listama (ne bi se smjelo desiti)
            current_protocol = g_appConfig.protocol_version_L;
        }
    } else {
        // Single bus mode
        current_protocol = g_appConfig.protocol_version_L;
    }
    
    return (static_cast<ProtocolVersion>(current_protocol) == ProtocolVersion::HILLS);
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
                // Timeout - idi na sljedeću adresu
                LOG_DEBUG(4, "[LogPull] Timeout za 0x%X.\n", m_current_pull_address);
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
        // Dual bus mode check
        if (g_appConfig.enable_dual_bus_mode)
        {
            if (m_address_list_count_L == 0 && m_address_list_count_R == 0) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Nema adresa, pauziraj.
                return;
            }
        }
        else
        {
            if (m_address_list_count == 0) {
                vTaskDelay(pdMS_TO_TICKS(100)); // Nema adresa, pauziraj.
                return;
            }
        }
        
        // Uzmi novu adresu
        m_current_pull_address = GetNextAddress();
        
        // Postavi aktivan bus
        if (g_appConfig.enable_dual_bus_mode)
        {
            int8_t bus_id = GetBusForAddress(m_current_pull_address);
            if (bus_id >= 0) {
                m_rs485_service->SelectBus(bus_id);
                LOG_DEBUG(4, "[LogPull] Dual mode: Adresa 0x%04X -> Bus %d\n", m_current_pull_address, bus_id);
            }
        }
        else
        {
            // SINGLE MODE: Koristi m_current_bus (toggle između 0 i 1)
            m_rs485_service->SelectBus(m_current_bus);
            LOG_DEBUG(4, "[LogPull] Single mode: Adresa 0x%04X -> Bus %d\n", m_current_pull_address, m_current_bus);
        }
        
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
 * Implementira sekvencijalnu logiku: prvo SVE adrese sa L liste, pa SVE sa R liste.
 */
uint16_t LogPullManager::GetNextAddress()
{
    if (g_appConfig.enable_dual_bus_mode)
    {
        // SEKVENCIJALNA LOGIKA: Prvo SVE adrese sa Lijeve, pa SVE sa Desne
        if (m_current_bus == 0) // Lijevi bus je TRENUTNO aktivan
        {
            if (m_address_list_count_L == 0)
            {
                // Lijeva lista prazna, prebaci na Desnu
                m_current_bus = 1;
                m_address_index_R = 0; // Resetuj index za Desnu listu
                if (m_address_list_count_R == 0) return 0;
                // Uzmi prvu adresu sa Desne liste
                uint16_t address = m_address_list_R[m_address_index_R];
                m_address_index_R++;
                return address;
            }
            
            // Uzmi adresu sa Lijeve liste koristeći njen index
            uint16_t current_address = m_address_list_L[m_address_index_L];
            m_address_index_L++;
            
            // PROVJERA: Da li smo završili Lijevu listu?
            if (m_address_index_L >= m_address_list_count_L)
            {
                // Završili smo Lijevu listu - prebaci na Desnu
                m_address_index_L = 0; // Resetuj index za sledeći ciklus
                m_current_bus = 1;     // Prebaci na Desni bus
                m_address_index_R = 0; // Resetuj index Desne liste
            }
            
            return current_address;
        }
        else // Desni bus je TRENUTNO aktivan (m_current_bus == 1)
        {
            if (m_address_list_count_R == 0)
            {
                // Desna lista prazna, prebaci na Lijevu
                m_current_bus = 0;
                m_address_index_L = 0; // Resetuj index za Lijevu listu
                if (m_address_list_count_L == 0) return 0;
                // Uzmi prvu adresu sa Lijeve liste
                uint16_t address = m_address_list_L[m_address_index_L];
                m_address_index_L++;
                return address;
            }
            
            // Uzmi adresu sa Desne liste koristeći njen index
            uint16_t current_address = m_address_list_R[m_address_index_R];
            m_address_index_R++;
            
            // PROVJERA: Da li smo završili Desnu listu?
            if (m_address_index_R >= m_address_list_count_R)
            {
                // Završili smo Desnu listu - prebaci na Lijevu
                m_address_index_R = 0; // Resetuj index za sledeći ciklus
                m_current_bus = 0;     // Prebaci na Lijevi bus
                m_address_index_L = 0; // Resetuj index Lijeve liste
            }
            
            return current_address;
        }
    }
    else
    {
        // Single mode: prvo SVE adrese na Bus 0, zatim SVE na Bus 1
        if (m_address_list_count == 0) return 0;
        
        uint16_t current_address = m_address_list[m_current_address_index];
        m_current_address_index++;
        
        // Kada završimo cijelu listu, prebaci na drugi bus
        if (m_current_address_index >= m_address_list_count)
        {
            m_current_address_index = 0;
            m_current_bus = (m_current_bus == 0) ? 1 : 0; // Toggle Bus 0 <-> Bus 1
            LOG_DEBUG(4, "[LogPull] Single mode: Završena lista, prebacujem na Bus %d\n", m_current_bus);
        }
        
        return current_address;
    }
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
             m_state = PullState::IDLE;
             m_last_activity_time = millis();
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

/**
 * @brief Vraća bus ID za datu adresu.
 * @param address Adresa uređaja.
 * @return 0 (Lijevi bus), 1 (Desni bus), ili -1 (nije pronađeno).
 */
int8_t LogPullManager::GetBusForAddress(uint16_t address)
{
    // Provjera u Lijevoj listi
    for (uint16_t i = 0; i < m_address_list_count_L; i++)
    {
        if (m_address_list_L[i] == address) {
            return 0; // Lijevi bus
        }
    }
    
    // Provjera u Desnoj listi
    for (uint16_t i = 0; i < m_address_list_count_R; i++)
    {
        if (m_address_list_R[i] == address) {
            return 1; // Desni bus
        }
    }
    
    // Adresa nije pronađena ni u jednoj listi
    return -1;
}
