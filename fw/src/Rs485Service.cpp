/**
 ******************************************************************************
 * @file    Rs485Service.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija Rs485Service modula (Dispecer).
 ******************************************************************************
 */

#include "Rs485Service.h"
#include "ProjectConfig.h" 
#include "EepromStorage.h" // Za g_appConfig

// Globalna konfiguracija (treba biti ucitana u EepromStorage::Initialize)
extern AppConfig g_appConfig; 

// RS485 Kontrolni Karakteri
#define SOH 0x01
#define STX 0x02
#define EOT 0x04

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
        switch (m_state)
        {
        case Rs485State::IDLE:
            Dispatch();
            break;
        
        case Rs485State::SENDING:
            // Slanje se završava unutar SendPacket() prelazom na WAITING. 
            // Ova tranzicija se ne bi trebala desiti.
            m_state = Rs485State::IDLE; 
            break;
            
        case Rs485State::WAITING:
        case Rs485State::RECEIVING:
            HandleReceive();
            // Provjera timeout-a (aktivna samo u stanju WAITING)
            if (m_state == Rs485State::WAITING && (millis() - m_timeout_start) > RS485_RESP_TOUT_MS)
            {
                m_state = Rs485State::TIMEOUT;
            }
            break;

        case Rs485State::TIMEOUT:
            Serial.println(F("[Rs485Service] Timeout!"));
            if (m_current_bus_owner != NULL)
            {
                m_current_bus_owner->OnTimeout();
            }
            m_current_bus_owner = NULL;
            m_state = Rs485State::IDLE;
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
    IRs485Manager* managers[] = {
        m_http_query_manager,
        m_update_manager,
        m_log_pull_manager,
        m_time_sync
    };

    for (IRs485Manager* manager : managers)
    {
        if (manager != NULL)
        {
            m_current_bus_owner = manager;
            
            // Pozivamo Service(). Ako pošalje paket, prelazimo u WAITING i prekidamo Dispatch.
            manager->Service();

            if (m_state != Rs485State::IDLE)
            {
                return;
            }
            m_current_bus_owner = NULL; 
        }
    }
    
    m_current_bus_owner = NULL; 
    m_state = Rs485State::IDLE;
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
    if (length < 10) return false;

    uint16_t data_length = buffer[5];
    uint16_t expectedLength = data_length + 9; 

    if (length != expectedLength) return false;
    
    // 1. Provjera SOH/STX i EOT
    if ((buffer[0] != SOH) && (buffer[0] != STX)) return false; 
    if (buffer[length - 1] != EOT) return false; 

    // 2. Provjera Adrese Cilja (Target Address) - mora biti naša adresa (rsifa)
    uint16_t target_addr = (buffer[1] << 8) | buffer[2];
    uint16_t my_addr = g_appConfig.rs485_iface_addr;
    
    if (target_addr != my_addr) return false; 

    // 3. Provjera Checksum-a
    uint16_t received_checksum = (buffer[expectedLength - 3] << 8) | buffer[expectedLength - 2];
    uint16_t calculated_checksum = CalculateChecksum(buffer, data_length);
    
    if (received_checksum != calculated_checksum)
    {
        Serial.printf("[Rs485Service] GRESKA: Checksum neispravan. Primljen: 0x%X, Očekivan: 0x%X\r\n", 
            received_checksum, calculated_checksum);
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
                        m_current_bus_owner->ProcessResponse(m_rx_buffer, m_rx_count);
                    }
                }
                // Bez obzira na validaciju, paket je gotov. Resetuj stanje.
                m_current_bus_owner = NULL;
                m_state = Rs485State::IDLE;
                m_rx_count = 0;
                return; // Izađi iz funkcije čim je paket obrađen
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
    // Provjera da li je SendPacket pozvan od strane trenutnog vlasnika magistrale i da li je IDLE
    if (m_current_bus_owner == NULL && m_state == Rs485State::IDLE)
    {
        Serial.println(F("[Rs485Service] GRESKA: Pokušaj slanja bez vlasnika/zauzeta magistrala."));
        return false;
    }
    
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
    if (m_state == Rs485State::IDLE)
    {
        m_current_bus_owner = manager;
        return true;
    }
    return false;
}

void Rs485Service::ReleaseBusAccess(IRs485Manager* manager)
{
    if (m_current_bus_owner == manager)
    {
        m_current_bus_owner = NULL;
        m_state = Rs485State::IDLE;
    }
}