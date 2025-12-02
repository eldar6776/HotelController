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
 * @brief Glavna funkcija koju poziva state-mašina. Obavlja jedan puni ciklus pollinga.
 */
void LogPullManager::Run()
{
    // ========================================================================
    // --- IMPLEMENTACIJA OBAVEZNE RX->TX PAUZE (3ms) ---
    // ========================================================================
    if (millis() - m_last_activity_time < RX2TX_DEL_MS)
    {
        return; // Još nije prošlo 3ms, ne radi ništa, izađi odmah.
    }
    // ========================================================================

    // KORAK 2: Ako čekamo odgovor, provjeri da li je stigao.
    if (m_state == PullState::WAITING_FOR_RESPONSE)
    {
        uint8_t response_buffer[MAX_PACKET_LENGTH];
        int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, RS485_RESP_TOUT_MS);

        if (response_len > 0) {
            // Imamo odgovor, obradi ga i promijeni stanje.
            ProcessResponse(response_buffer, response_len);
        } else {
            // Timeout. Vrati se u IDLE stanje i pripremi za sljedeću adresu.
            LOG_DEBUG(4, "[LogPull] Timeout za adresu 0x%X. Prelazim na sljedeću.\n", m_current_pull_address);
            m_state = PullState::IDLE; // Vrati se u IDLE
            m_last_activity_time = millis(); // Zabilježi vrijeme neuspjeha
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
        m_state = PullState::SENDING_STATUS_REQUEST; // Pripremi se za slanje statusnog upita.
    }

    // KORAK 4: Izvrši akciju slanja (ako je stanje postavljeno u prethodnom koraku).
    // Ove funkcije samo pošalju paket i odmah se završe.
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
        m_state = PullState::WAITING_FOR_RESPONSE;
    }
}

/**
 * @brief Kreira i salje DEL_LOG_LIST paket.
 */
void LogPullManager::SendDeleteLogRequest(uint16_t address)
{
    LOG_DEBUG(4, "[LogPull] -> Šaljem DEL_LOG_LIST na adresu 0x%X\n", address);
    
    uint8_t packet[10]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint8_t cmd = DEL_LOG_LIST;
    
    packet[0] = SOH;
    packet[1] = (address >> 8);
    packet[2] = (address & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = 1; // Data Length
    packet[6] = cmd;
    
    uint16_t checksum = cmd;
    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    m_rs485_service->SendPacket(packet, 10);
    // Ne čekamo odgovor na komandu za brisanje, samo je pošaljemo.
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
    
    // Obrada odgovora na GET_SYS_STAT (0xA0)
    if (response_cmd == GET_SYS_STAT)
    {
        // Provjera "Log Pending" flaga (prema izvještaju, bajtovi 7 i 8).
        if (packet[7] == '1' || (length > 8 && packet[8] == '1'))
        {
            LOG_DEBUG(3, "[LogPull] Uređaj 0x%X ima log. Pripremam zahtjev...\n", m_current_pull_address);
            m_state = PullState::SENDING_LOG_REQUEST; // Postavi stanje za slanje zahtjeva za logom u sljedećem ciklusu.
            return; // Izađi i čekaj sljedeći `Run()` poziv (nakon RX2TX_DEL pauze).
        }
        else {
             LOG_DEBUG(4, "[LogPull] Uređaj 0x%X nema logova.\n", m_current_pull_address);
        }
    }
    // Obrada odgovora na GET_LOG_LIST (0xA3)
    else if (response_cmd == GET_LOG_LIST)
    {
        uint16_t data_len = packet[5];
        // Provjera da li paket sadrži kompletan log od 16 bajtova.
        // Format odgovora je [CMD] [LOG_ID] [LOG_DATA...], ukupno 18 bajtova (2+16)
        if (data_len >= (LOG_RECORD_SIZE + 2)) 
        {
            LOG_DEBUG(3, "[LogPull] Primljen LOG sa adrese 0x%X\n", m_current_pull_address);
            LogEntry newLog; 
            // Kopiramo kompletan 16-bajtni log zapis koji počinje od 7. bajta paketa.
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
                LOG_DEBUG(3, "[LogPull] -> Upisan log ID: %u sa kontrolera 0x%X\n", newLog.log_id, m_current_pull_address);
                // =================================================================================
                // --- KLJUČNA ISPRAVKA: Odmah pošalji komandu za brisanje loga nazad na uređaj ---
                // Ovo sprečava beskonačnu petlju.
                // =================================================================================
                SendDeleteLogRequest(m_current_pull_address);
            }
            // ========================================================================
            // --- KLJUČNA ISPRAVKA: Odmah provjeri ponovo da li ima još logova ---
            // ========================================================================
            m_state = PullState::SENDING_STATUS_REQUEST; // Vrati se na provjeru statusa za ISTI uređaj
            return;
        }
    }

    // Nakon obrade (ili ako nije bilo loga), vrati se u IDLE stanje i zabilježi vrijeme.
    m_state = PullState::IDLE;
    m_last_activity_time = millis();
}
