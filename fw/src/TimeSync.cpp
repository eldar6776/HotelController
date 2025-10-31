/**
 ******************************************************************************
 * @file    TimeSync.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija TimeSync modula.
 ******************************************************************************
 */

#include "TimeSync.h"
#include "ProjectConfig.h"
#include <time.h> // Za time()

TimeSync::TimeSync() :
    m_rs485_service(NULL),
    m_last_sync_time(0)
{
    // Konstruktor
}

void TimeSync::Initialize(Rs485Service* pRs485Service)
{
    m_rs485_service = pRs485Service;
}

/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void TimeSync::Service()
{
    if (millis() - m_last_sync_time > TIME_BROADCAST_INTERVAL_MS)
    {
        m_last_sync_time = millis();
        SendTimeBroadcast();
    }
    else
    {
        // Jos nije vrijeme, oslobodi magistralu
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Salje SET_RTC_DATE_TIME paket.
 */
void TimeSync::SendTimeBroadcast()
{
    Serial.println(F("[TimeSync] Slanje broadcast vremena..."));
    
    // TODO: Kreirati paket po uzoru na 'HC_CreateTimeUpdatePacket'
    uint8_t packet[32]; // Placeholder
    uint16_t length = 0;

    // m_rs485_service->SendPacket(packet, length);
    
    // Posto je broadcast, ne cekamo odgovor
    m_rs485_service->ReleaseBusAccess(this);
}

/**
 * @brief Broadcast ne ocekuje odgovor.
 */
void TimeSync::ProcessResponse(uint8_t* packet, uint16_t length)
{
    // Ignorise se
}

/**
 * @brief Broadcast ne ocekuje odgovor, tako da je i timeout ignorisan.
 */
void TimeSync::OnTimeout()
{
    // Ignorise se
}
