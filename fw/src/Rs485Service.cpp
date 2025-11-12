/**
 ******************************************************************************
 * @file    Rs485Service.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija Rs485Service modula (Dispecer).
 ******************************************************************************
 */

#include "Rs485Service.h"
#include "DebugConfig.h"   // Uključujemo za LOG_RS485
#include "ProjectConfig.h" 
#include "EepromStorage.h" // Za g_appConfig

// Globalna konfiguracija (treba biti ucitana u EepromStorage::Initialize)
extern AppConfig g_appConfig; 

// RS485 Kontrolni Karakteri
#define SOH 0x01
#define STX 0x02
#define ACK 0x06 // ISPRAVKA: Vraćeno - Neophodno za validaciju odgovora
#define NAK 0x15 // ISPRAVKA: Vraćeno - Neophodno za validaciju odgovora
#define EOT 0x04

// NOVO: Definicija timeout-a za watchdog
#define BUS_WATCHDOG_TIMEOUT_MS 5000

Rs485Service::Rs485Service() :
    m_rs485_serial(2) // Koristi UART2 (Serial2)
{
    m_task_handle = NULL;
    m_http_query_manager = NULL;
    m_update_manager = NULL;
    m_log_pull_manager = NULL;
    m_time_sync = NULL;
    m_current_bus_owner = NULL;
    m_state = Rs485State::IDLE;
    m_bus_watchdog_start = 0;
    m_rx_count = 0;
}

void Rs485Service::Initialize(
    IRs485Manager* pHttpQueryManager,
    IRs485Manager* pUpdateManager,
    IRs485Manager* pLogPullManager,
    IRs485Manager* pTimeSync
)
{
    Serial.println(F("[Rs485Service] Inicijalizacija..."));
    m_http_query_manager = pHttpQueryManager;
    m_update_manager = pUpdateManager;
    m_log_pull_manager = pLogPullManager;
    m_time_sync = pTimeSync;

    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW); // Postavi na RX (prijem)

    m_rs485_serial.begin(RS485_BAUDRATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
}

void Rs485Service::StartTask()
{
    Serial.println(F("[Rs485Service] Pokretanje glavnog zadatka (Dispecera)..."));
    xTaskCreate(
        TaskWrapper,
        "Rs485ServiceTask",
        4096, // Stack size
        this, // Parametar za zadatak (pokazivac na 'this')
        5,    // Prioritet
        &m_task_handle
    );
}

void Rs485Service::TaskWrapper(void* pvParameters)
{
    // Pozivamo stvarnu petlju zadatka unutar klase
    static_cast<Rs485Service*>(pvParameters)->Run();
}

void Rs485Service::Run()
{
    while (true)
    {
        // UVIJEK pozovi dispečer. On će rukovati prioritetima i prekidanjem.
        Dispatch(); 

        // ========================================================================
        // NOVO: BUS WATCHDOG - Apsolutni tajmer za vlasništvo nad magistralom
        // ========================================================================
        if (m_current_bus_owner != NULL && (millis() - m_bus_watchdog_start > BUS_WATCHDOG_TIMEOUT_MS))
        {
            LOG_DEBUG(1, "[Rs485] !!! BUS WATCHDOG TIMEOUT !!!\n");
            LOG_DEBUG(1, "[Rs485] Vlasnik %s je predugo držao magistralu. Nasilno oduzimanje.\n", m_current_bus_owner->Name());
            
            // Nasilno resetuj magistralu. Ovo je sigurnosni mehanizam.
            ResetBus();
        }

        // Obradi trenutno stanje
        switch (m_state) {
        case Rs485State::IDLE:
            // U IDLE stanju, Dispatch() je već odradio posao.
            break;
        case Rs485State::SENDING:
            // Slanje se završava unutar SendPacket() prelazom na WAITING. 
            // Ova tranzicija se ne bi trebala desiti.
            m_state = Rs485State::IDLE; 
            break;
            
        case Rs485State::WAITING:
            HandleReceive();
            // ========================================================================
            // KLJUČNA ISPRAVKA: Koristimo dinamički timeout od vlasnika magistrale.
            // ========================================================================
            if (m_state == Rs485State::WAITING && m_current_bus_owner != NULL)
            {
                uint32_t current_timeout = m_current_bus_owner->GetTimeoutMs();
                if (millis() - m_timeout_start >= current_timeout)
                {
                    LOG_DEBUG(2, "[Rs485] Timeout od %lu ms za '%s'!\n", current_timeout, m_current_bus_owner->Name());
                    m_state = Rs485State::TIMEOUT; // Postavi stanje na TIMEOUT
                    m_current_bus_owner->OnTimeout();
                }
            }
            break;

        case Rs485State::RECEIVING:
            HandleReceive();
            break;
        }

        vTaskDelay(1 / portTICK_PERIOD_MS); 
    }
}

/**
 * @brief DISPECER: Provjerava prioritete (HTTP -> Update -> LogPull -> TimeSync).
 */
void Rs485Service::Dispatch()
{
    // =================================================================================
    // KONAČNA ISPRAVKA: Provjera da li je trenutni vlasnik završio svoj posao.
    // Ako vlasnik postoji, ali više ne želi magistralu (WantsBus() == false),
    // to znači da je završio i da magistralu treba osloboditi.
    // =================================================================================
    if (m_current_bus_owner != NULL && !m_current_bus_owner->WantsBus())
    {
        LOG_DEBUG(4, "[Rs485] Vlasnik %s je završio. Oslobađam magistralu.\n", m_current_bus_owner->Name());
        ResetBus();
    }


    // 1. Provjera da li menadžer visokog prioriteta želi da prekine trenutnog vlasnika
    IRs485Manager* high_priority_managers[] = { m_http_query_manager, m_update_manager };
    for (IRs485Manager* high_prio_manager : high_priority_managers) {
        if (high_prio_manager != NULL && high_prio_manager->WantsBus()) {
            // Ako je magistrala zauzeta od strane menadžera nižeg prioriteta (bilo koga ko nije on sam)
            if (m_current_bus_owner != NULL && m_current_bus_owner != high_prio_manager) {
                LOG_DEBUG(3, "[Rs485] PREKID: %s preuzima magistralu od %s\n", high_prio_manager->Name(), m_current_bus_owner->Name());
                ResetBus(); // Agresivno resetuj stanje, oslobađa magistralu
            }
            // Ako je magistrala sada slobodna, odmah pokušaj da je dodijeliš
            if (m_state == Rs485State::IDLE) {
                if (RequestBusAccess(high_prio_manager)) {
                    high_prio_manager->Service();
                    return; // Prekini dispečer, posao za ovaj ciklus je gotov
                }
            }
            // Ako je magistrala dodijeljena, izađi iz petlje prioriteta
            break; 
        }
    }

    // 2. Ako je magistrala slobodna, dodijeli je prvom menadžeru koji je treba
    if (m_state == Rs485State::IDLE) {
        // Redoslijed definira prioritet: HTTP -> Update -> LogPull -> TimeSync
        IRs485Manager* all_managers[] = { m_http_query_manager, m_update_manager, m_log_pull_manager, m_time_sync }; 
        for (IRs485Manager* manager : all_managers) {
            if (manager != NULL)
            {
                // Provjeri da li menadžer želi magistralu i da li je ona slobodna
                if (manager->WantsBus() && RequestBusAccess(manager))
                {
                    manager->Service();
                    // Menadžer je preuzeo kontrolu, izađi iz Dispatch-a za ovaj ciklus
                    return; 
                }
            }
        }
    }
}

/**
 * @brief Izračunava Checksum na Data Polju paketa (počinje od indeksa 6).
 */
uint16_t Rs485Service::CalculateChecksum(uint8_t* buffer, uint16_t data_length)
{
    uint16_t checksum = 0;
    for (uint16_t i = 6; i < (6 + data_length); i++) 
    {
        checksum += buffer[i]; 
    }
    return checksum;
}

/**
 * @brief Validira SOH/STX, Adrese, Dužinu i Checksum paketa. (Replicira logiku iz hotel_ctrl.c)
 */
bool Rs485Service::ValidatePacket(uint8_t* buffer, uint16_t length)
{
    // Minimalna dužina paketa je 10 bajtova
    if (length < 10) { // Svi paketi, uključujući ACK/NAK odgovore, imaju minimalnu dužinu
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Prekratak paket: %d)\n", length);
        return false;
    }

    uint16_t data_length = buffer[5];
    uint16_t expectedLength = data_length + 9; 

    if (length != expectedLength) { // Provjera konzistentnosti dužine
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Pogrešna dužina. Očekivano: %d, Primljeno: %d)\n", expectedLength, length);
        return false;
    }
    
    // 1. Provjera početnog i krajnjeg bajta (SOH/STX/ACK/NAK i EOT)
    // Ovo je ključna ispravka koja prihvata ACK kao validan početak.
    bool is_valid_start = (buffer[0] == SOH || buffer[0] == STX || buffer[0] == ACK || buffer[0] == NAK);
    if (!is_valid_start) {
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Pogrešan početni bajt: 0x%02X)\n", buffer[0]);
        return false;
    }
    if (buffer[length - 1] != EOT) { // Svi paketi se moraju završiti sa EOT
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Nedostaje EOT)\n");
        return false;
    }

    // 2. Provjera Adrese Cilja (Target Address) - mora biti naša adresa (rsifa)
    uint16_t target_addr = (buffer[1] << 8) | buffer[2];
    uint16_t my_addr = g_appConfig.rs485_iface_addr;
    
    if (target_addr != my_addr) { // Paket mora biti adresiran na nas
        LOG_DEBUG(4, "[Rs485] ValidatePacket -> INFO (Paket za drugog: 0x%04X != 0x%04X)\n", target_addr, my_addr);
        return false;
    }

    // 3. Provjera Checksum-a
    uint16_t received_checksum = (buffer[expectedLength - 3] << 8) | buffer[expectedLength - 2];
    uint16_t calculated_checksum = CalculateChecksum(buffer, data_length);
    
    if (received_checksum != calculated_checksum) {
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Checksum neispravan. Primljen: 0x%X, Očekivan: 0x%X)\n", received_checksum, calculated_checksum);
        return false;
    }
    
    return true;
}

void Rs485Service::HandleReceive()
{
    while (m_rs485_serial.available())
    {
        if (m_rx_count < RS485_BUFFER_SIZE)
        {
            uint8_t incoming_byte = m_rs485_serial.read();
            m_rx_buffer[m_rx_count++] = incoming_byte;
            m_state = Rs485State::RECEIVING;

            // Provjera kraja paketa nakon svakog primljenog bajta
            if (incoming_byte == EOT)
            {
                if (ValidatePacket(m_rx_buffer, m_rx_count))
                {
                    if (m_current_bus_owner != NULL)
                    {   
                        LOG_DEBUG(4, "[Rs485] Primljen validan paket. Prosleđujem vlasniku: %p\n", m_current_bus_owner);
                        m_current_bus_owner->ProcessResponse(m_rx_buffer, m_rx_count);
                    }
                }
                else
                {
                    // Detaljan ispis nevalidnog paketa za dijagnostiku
                    Serial.print(F("[Rs485Service] ODBAČEN PAKET! Sadržaj (HEX):\n -> "));
                    for(uint16_t i = 0; i < m_rx_count; i++) {
                        Serial.printf("%02X ", m_rx_buffer[i]);
                    }
                    Serial.println();
                }

                // Resetuj bafer za prijem. Stanje će biti resetovano od strane vlasnika
                // pozivom ReleaseBusAccess().
                m_rx_count = 0;
                if (m_current_bus_owner == NULL) {
                    m_state = Rs485State::IDLE; // Ako nema vlasnika, idi u idle
                }
            }
        }
        else
        {
            // Buffer overflow, resetuj sve
            m_rx_count = 0; 
            m_state = Rs485State::IDLE;
            return;
        }
    }
}

bool Rs485Service::SendPacket(uint8_t* data, uint16_t length)
{
    // =================================================================================
    // KLJUČNA ISPRAVKA: Dozvoli slanje ako je stanje IDLE ILI ako je poziv došao
    // od vlasnika koji pokušava ponovo (stanje je WAITING ili TIMEOUT).
    // =================================================================================
    if (m_current_bus_owner == NULL)
    {
        LOG_DEBUG(1, "[Rs485] GRESKA: SendPacket pozvan bez vlasnika magistrale!\n");
        return false;
    }
    if (m_state != Rs485State::IDLE && m_state != Rs485State::WAITING && m_state != Rs485State::TIMEOUT && m_state != Rs485State::SENDING) {
        LOG_DEBUG(1, "[Rs485] GRESKA: SendPacket pozvan u neispravnom stanju (%d)!\n", (int)m_state);
        return false;
    }
    
    LOG_DEBUG(4, "[Rs485] Slanje paketa -> Dužina: %d, Sadržaj: %02X %02X %02X %02X %02X %02X %02X...\n", length, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    m_state = Rs485State::SENDING;
    
    digitalWrite(RS485_DE_PIN, HIGH); 
    delayMicroseconds(50); 

    m_rs485_serial.write(data, length);
    m_rs485_serial.flush(); 
    
    delayMicroseconds(50);
    digitalWrite(RS485_DE_PIN, LOW); 
    
    m_timeout_start = millis();
    m_state = Rs485State::WAITING;
    m_rx_count = 0;

    return true;
}

bool Rs485Service::RequestBusAccess(IRs485Manager* manager)
{
    // Dozvoli preuzimanje samo ako je magistrala slobodna
    if (m_state == Rs485State::IDLE && m_current_bus_owner == NULL)
    {
        m_current_bus_owner = manager;
        m_bus_watchdog_start = millis(); // NOVO: Pokreni watchdog za ovog vlasnika
        LOG_DEBUG(5, "[Rs485] Magistrala dodijeljena -> %s\n", manager->Name());
        return true;
    }
    return false;
}

void Rs485Service::ReleaseBusAccess(IRs485Manager* manager)
{
    if (m_current_bus_owner == manager)
    {
        LOG_DEBUG(5, "[Rs485] Magistralu oslobodio -> %s\n", manager->Name());
        ResetBus();
    } else if (m_current_bus_owner != NULL) {
        LOG_DEBUG(2, "[Rs485] UPOZORENJE: %s pokušao osloboditi magistralu, ali je vlasnik %s.\n", manager->Name(), m_current_bus_owner->Name());
    }
}

/**
 * @brief AGRESIVNI RESET: Bezuslovno resetuje stanje magistrale.
 * Ovo je jedina tačka odgovornosti za oslobađanje magistrale.
 */
void Rs485Service::ResetBus()
{
    m_current_bus_owner = NULL;
    m_state = Rs485State::IDLE;
    m_rx_count = 0;
    digitalWrite(RS485_DE_PIN, LOW); // Osiguraj da je u RX modu
}

IRs485Manager* Rs485Service::GetCurrentBusOwner()
{
    return m_current_bus_owner;
}