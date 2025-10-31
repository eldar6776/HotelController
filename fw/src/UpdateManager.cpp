/**
 ******************************************************************************
 * @file    UpdateManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija UpdateManager modula.
 ******************************************************************************
 */

#include "UpdateManager.h"

UpdateManager::UpdateManager()
{
    m_session.state = UpdateState::S_IDLE;
    m_rs485_service = NULL;
    m_spi_storage = NULL;
}

void UpdateManager::Initialize(Rs485Service* pRs485Service, SpiFlashStorage* pSpiStorage)
{
    m_rs485_service = pRs485Service;
    m_spi_storage = pSpiStorage;
}

bool UpdateManager::StartSession(uint8_t clientAddress, uint32_t fwAddressSlot)
{
    if (m_session.state != UpdateState::S_IDLE)
    {
        Serial.println(F("[UpdateManager] Vec je u toku druga sesija. Odbijeno."));
        return false; // Zauzet
    }

    Serial.printf("[UpdateManager] Pokretanje nove sesije za klijenta %d sa slota 0x%lX\n", clientAddress, fwAddressSlot);
    
    // TODO: Procitati velicinu, CRC itd. sa SPI Flasha
    // m_spi_storage->BeginRead(fwAddressSlot + OFFSET_METADATA);
    // m_spi_storage->ReadChunk(...) -> procitaj 'pecat'

    m_session.clientAddress = clientAddress;
    m_session.fw_address_slot = fwAddressSlot;
    // m_session.fw_size = ... (procitano sa flasha)
    // m_session.fw_crc = ... (procitano sa flasha)
    m_session.bytesSent = 0;
    m_session.currentSequenceNum = 0;
    m_session.retryCount = MAX_UPDATE_RETRIES;
    m_session.state = UpdateState::S_STARTING;
    
    // Obavijesti Rs485Service da imamo prioritetni zadatak
    // m_rs485_service->RequestBusAccess(this);

    return true;
}


/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void UpdateManager::Service()
{
    if (m_session.state == UpdateState::S_IDLE)
    {
        m_rs485_service->ReleaseBusAccess(this);
        return;
    }

    // Provjera tajmera
    if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK ||
        m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK ||
        m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
    {
        if ((millis() - m_session.timeoutStart) > UPDATE_PACKET_TIMEOUT_MS)
        {
            OnTimeout(); // Nas interni timeout
        }
    }


    switch (m_session.state)
    {
    case UpdateState::S_STARTING:
        SendStartRequest(&m_session);
        break;
    case UpdateState::S_SENDING_DATA:
        SendDataPacket(&m_session);
        break;
    case UpdateState::S_FINISHING:
        SendFinishRequest(&m_session);
        break;
    case UpdateState::S_PENDING_CLEANUP:
        CleanupSession(&m_session);
        m_rs485_service->ReleaseBusAccess(this);
        break;
    case UpdateState::S_COMPLETED_OK:
    case UpdateState::S_FAILED:
        // Cekamo da nas dispecer pozove ponovo da bi presli u PENDING_CLEANUP
        m_session.state = UpdateState::S_PENDING_CLEANUP;
        break;
    default:
        // Cekamo (S_IDLE, S_WAITING_*)
        break;
    }
}

/**
 * @brief Callback - Stigao je odgovor.
 */
void UpdateManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    Serial.println(F("[UpdateManager] Stigao odgovor..."));

    // TODO: Parsirati ACK/NACK po uzoru na 'UpdateManager_ProcessResponse'
    // i azurirati m_session.state
    
    // Primjer:
    // if (packet[0] == SUB_CMD_START_ACK && m_session.state == S_WAITING_FOR_START_ACK)
    // {
    //    m_session.state = S_SENDING_DATA;
    // }
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void UpdateManager::OnTimeout()
{
    Serial.println(F("[UpdateManager] Timeout paketa!"));

    if (m_session.retryCount > 0)
    {
        m_session.retryCount--;
        Serial.printf("[UpdateManager] Pokusaj ponovo... (%d/%d)\n", m_session.retryCount, MAX_UPDATE_RETRIES);
        
        // Vrati stanje na ponovno slanje
        if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK) m_session.state = UpdateState::S_STARTING;
        if (m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK) m_session.state = UpdateState::S_SENDING_DATA;
        if (m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK) m_session.state = UpdateState::S_FINISHING;
    }
    else
    {
        Serial.println(F("[UpdateManager] GRESKA: Previse neuspjesnih pokusaja. Update prekinut."));
        m_session.state = UpdateState::S_FAILED;
    }
}

void UpdateManager::SendStartRequest(UpdateSession* s)
{
    Serial.printf("[UpdateManager] Slanje START_REQUEST za klijenta %d\n", s->clientAddress);
    // TODO: Kreirati START paket
    uint8_t packet[64];
    uint16_t length = 0;
    
    m_rs485_service->SendPacket(packet, length);
    s->timeoutStart = millis();
    s->state = UpdateState::S_WAITING_FOR_START_ACK;
}

void UpdateManager::SendDataPacket(UpdateSession* s)
{
    Serial.printf("[UpdateManager] Slanje DATA paketa %d za klijenta %d\n", s->currentSequenceNum, s->clientAddress);
    
    // TODO: Procitati dio sa SPI Flasha
    // m_spi_storage->ReadChunk(...)
    
    // TODO: Kreirati DATA paket
    uint8_t packet[256];
    uint16_t length = 0;
    
    m_rs485_service->SendPacket(packet, length);
    s->timeoutStart = millis();
    s->state = UpdateState::S_WAITING_FOR_DATA_ACK;
}

void UpdateManager::SendFinishRequest(UpdateSession* s)
{
    Serial.printf("[UpdateManager] Slanje FINISH_REQUEST za klijenta %d\n", s->clientAddress);
    // TODO: Kreirati FINISH paket
    uint8_t packet[32];
    uint16_t length = 0;
    
    m_rs485_service->SendPacket(packet, length);
    s->timeoutStart = millis();
    s->state = UpdateState::S_WAITING_FOR_FINISH_ACK;
}

void UpdateManager::CleanupSession(UpdateSession* s)
{
    Serial.printf("[UpdateManager] Ciscenje sesije za klijenta %d.\n", s->clientAddress);
    m_spi_storage->EndRead(); // Zatvori fajl na flashu
    s->state = UpdateState::S_IDLE;
}
