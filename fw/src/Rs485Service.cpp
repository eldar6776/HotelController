/**
 ******************************************************************************
 * @file    Rs485Service.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija Rs485Service modula (Dispecer).
 ******************************************************************************
 */

#include "Rs485Service.h"

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

/**
 * @brief Glavna petlja FreeRTOS zadatka.
 */
void Rs485Service::Run()
{
    while (true)
    {
        switch (m_state)
        {
        case Rs485State::IDLE:
            // Ovo je Dispecer. Provjerava prioritete.
            Dispatch();
            break;
        
        case Rs485State::SENDING:
            // Salje paket koji je postavio m_current_bus_owner
            // TODO: Implementirati slanje
            break;
            
        case Rs485State::WAITING:
            // Ceka na odgovor ili timeout
            HandleReceive();
            if ((millis() - m_timeout_start) > RS485_RESP_TOUT_MS)
            {
                m_state = Rs485State::TIMEOUT;
            }
            break;
            
        case Rs485State::RECEIVING:
            // Nastavlja da prima dok se paket ne kompletira
            HandleReceive();
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

        vTaskDelay(1 / portTICK_PERIOD_MS); // Mali delay
    }
}

/**
 * @brief DISPECER: Provjerava prioritete i dodjeljuje magistralu.
 */
void Rs485Service::Dispatch()
{
    // TODO: Implementirati logiku provjere prioriteta
    // (Ovdje cemo samo pozvati LogPullManager radi jednostavnosti)
    
    // 1. Provjeri HTTP Upit
    // if (m_http_query_manager->IsPending()) ...

    // 2. Provjeri Update
    // if (m_update_manager->IsPending()) ...

    // 3. Pokreni redovni Polling
    // (Za sada, samo pozivamo njega)
    if (m_log_pull_manager != NULL)
    {
        m_current_bus_owner = m_log_pull_manager;
        m_log_pull_manager->Service(); // Ovo ce pozvati SendPacket()
    }
}

void Rs485Service::HandleReceive()
{
    while (m_rs485_serial.available())
    {
        if (m_rx_count < RS485_BUFFER_SIZE)
        {
            m_rx_buffer[m_rx_count++] = m_rs485_serial.read();
            // TODO: Ovdje ide logika za detekciju kraja paketa (EOT)
            // i provjeru SOH/Adrese/CRC
            
            // Ako je paket kompletan i validan:
            if (ValidatePacket(m_rx_buffer, m_rx_count))
            {
                if (m_current_bus_owner != NULL)
                {
                    m_current_bus_owner->ProcessResponse(m_rx_buffer, m_rx_count);
                }
                m_current_bus_owner = NULL; // Oslobodi magistralu
                m_state = Rs485State::IDLE;
                m_rx_count = 0;
            }
        }
        else
        {
            // Prekoracenje bafera
            m_rx_count = 0; 
        }
    }
}

bool Rs485Service::ValidatePacket(uint8_t* buffer, uint16_t length)
{
    // TODO: Implementirati SOH, EOT, CRC i provjeru adrese
    return false; // Privremeno
}


bool Rs485Service::SendPacket(uint8_t* data, uint16_t length)
{
    if (m_state != Rs485State::IDLE)
    {
        // Magistrala je zauzeta, odbij slanje
        return false;
    }
    
    m_state = Rs485State::SENDING;
    
    digitalWrite(RS485_DE_PIN, HIGH); // Ukljuci slanje
    delayMicroseconds(50); // Kratka pauza

    m_rs485_serial.write(data, length);
    m_rs485_serial.flush(); // Sacekaj da se svi bajtovi posalju

    delayMicroseconds(50);
    digitalWrite(RS485_DE_PIN, LOW); // Vrati na prijem
    
    m_timeout_start = millis();
    m_state = Rs485State::WAITING;
    m_rx_count = 0;

    return true;
}

// ... Implementacija RequestBusAccess i ReleaseBusAccess ...
bool Rs485Service::RequestBusAccess(IRs485Manager* manager) { return false; }
void Rs485Service::ReleaseBusAccess(IRs485Manager* manager) {}
