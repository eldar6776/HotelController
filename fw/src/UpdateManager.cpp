/**
 ******************************************************************************
 * @file    UpdateManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija UpdateManager modula.
 ******************************************************************************
 */

#include "UpdateManager.h"
#include "ProjectConfig.h" 
#include <cstring>

// RS485 Kontrolni Karakteri i Komande
#define SOH 0x01
#define STX 0x02
#define EOT 0x04
#define ACK 0x06
#define NAK 0x15

// CMD-ovi za Update Protokol
#define CMD_UPDATE_START    0x14 
#define CMD_UPDATE_DATA     0x15 
#define CMD_UPDATE_FINISH   0x16 

// Makro Vrijednosti
#define VERS_INF_OFFSET 0x2000 
#define METADATA_SIZE 16

#define CMD_DWNLD_FWR_IMG   0x77U 
#define CMD_DWNLD_BLDR_IMG  0x78U 
#define CMD_RT_DWNLD_FWR    0x79U 
#define CMD_RT_DWNLD_BLDR   0x7AU
#define CMD_RT_DWNLD_LOGO   0x7BU 

// Globalna konfiguracija (extern)
extern AppConfig g_appConfig; 


UpdateManager::UpdateManager()
{
    m_session.state = UpdateState::S_IDLE;
    m_rs485_service = NULL;
    m_spi_storage = NULL;
    m_session.is_read_active = false;
}

void UpdateManager::Initialize(Rs485Service* pRs485Service, SpiFlashStorage* pSpiStorage)
{
    m_rs485_service = pRs485Service;
    m_spi_storage = pSpiStorage;
}


/**
 * @brief Inicijalizuje Flash adresu, čita i validira metadatu na ofsetu 0x2000.
 */
bool UpdateManager::PrepareSession(UpdateSession* s, uint8_t updateCmd)
{
    uint32_t size_sim = 0; 

    // 1. Odredi Base Adresu, Veličinu Slota i Update Type na osnovu CMD koda
    if (updateCmd >= CMD_IMG_RC_START && updateCmd <= CMD_IMG_RC_END)
    {
        // 14 sekvencijalnih slika (IMG1 do IMG14)
        size_sim = 128 * 1024; 
        s->type = TYPE_IMG_RC;
        
        uint8_t image_index = updateCmd - CMD_IMG_RC_START; 
        s->fw_address_slot = SLOT_ADDR_IMG_RC_START + (image_index * size_sim);
    }
    else
    {
        switch (updateCmd)
        {
            case CMD_DWNLD_FWR_IMG: s->type = TYPE_FW_RC; s->fw_address_slot = SLOT_ADDR_FW_RC; size_sim = SLOT_SIZE_128K; break;
            // KOREKCIJA GREŠKE: Zamijenjeno SLOT_ADDR_BLDR_RC sa SLOT_ADDR_BL_RC
            case CMD_DWNLD_BLDR_IMG: s->type = TYPE_BLDR_RC; s->fw_address_slot = SLOT_ADDR_BL_RC; size_sim = SLOT_ADDR_FW_RC; break; 
            case CMD_RT_DWNLD_FWR: s->type = TYPE_FW_TH; s->fw_address_slot = SLOT_ADDR_FW_TH_1M; size_sim = SLOT_SIZE_1M; break;
            case CMD_RT_DWNLD_BLDR: s->type = TYPE_BLDR_TH; s->fw_address_slot = SLOT_ADDR_BL_TH; size_sim = SLOT_ADDR_FW_RC; break;
            case CMD_RT_DWNLD_LOGO: s->type = TYPE_LOGO_RT; s->fw_address_slot = SLOT_ADDR_IMG_LOGO; size_sim = SLOT_SIZE_128K; break;
            default:
                // KOREKCIJA GREŠKE: Uklanjanje F() makroa unutar Serial.printf()
                Serial.printf("[UpdateManager] GRESKA: Nepodržana CMD: 0x%X\n", updateCmd);
                return false;
        }
    }
    
    // 2. Čitanje Metadata (Size, CRC) sa Flash adrese + OFFSET (0x2000)
    uint32_t read_address = s->fw_address_slot + VERS_INF_OFFSET;
    uint8_t metadata_buffer[METADATA_SIZE];

    if (!m_spi_storage->BeginRead(read_address)) 
    {
        Serial.println(F("[UpdateManager] GRESKA: Neuspjeh otvaranja SPI Flash za metadatu."));
        return false;
    }
    if (m_spi_storage->ReadChunk(metadata_buffer, METADATA_SIZE) != METADATA_SIZE)
    {
        Serial.println(F("[UpdateManager] GRESKA: Nije moguće pročitati 16 bajtova metadata."));
        m_spi_storage->EndRead();
        return false;
    }
    m_spi_storage->EndRead();

    // 3. Parsiranje (Big-Endian konverzija)
    s->fw_size = (uint32_t)metadata_buffer[3] | (metadata_buffer[2] << 8) | (metadata_buffer[1] << 16) | (metadata_buffer[0] << 24);
    s->fw_crc = (uint32_t)metadata_buffer[7] | (metadata_buffer[6] << 8) | (metadata_buffer[5] << 16) | (metadata_buffer[4] << 24);

    // Validacija
    if (s->fw_size == 0x00000000U || s->fw_size > size_sim || s->fw_crc == 0x00000000U)
    {
        Serial.printf("[UpdateManager] GRESKA: Nevalidna metadata. Size: %lu, CRC: 0x%lX\n", s->fw_size, s->fw_crc);
        return false;
    }
    
    // 4. Otvori Flash za stvarno čitanje podataka (od početka slota)
    if (!m_spi_storage->BeginRead(s->fw_address_slot)) 
    {
        return false;
    }
    s->is_read_active = true;

    return true;
}


/**
 * @brief Pokreće novu update sesiju sa specifičnim tipom (CMD).
 */
bool UpdateManager::StartSession(uint8_t clientAddress, uint8_t updateCmd)
{
    if (m_session.state != UpdateState::S_IDLE)
    {
        return false;
    }

    m_session.clientAddress = clientAddress;
    m_session.bytesSent = 0;
    m_session.currentSequenceNum = 0;
    m_session.retryCount = 0;

    if (!PrepareSession(&m_session, updateCmd))
    {
        return false;
    }

    Serial.printf("[UpdateManager] Sesija CMD 0x%X pokrenuta za 0x%X. Velicina: %lu\n", 
        updateCmd, clientAddress, m_session.fw_size);
    
    m_session.state = UpdateState::S_STARTING;
    
    return true;
}


/**
 * @brief Mašina Stanja (State Machine) za Update Manager.
 */
void UpdateManager::Service()
{
    if (m_session.state == UpdateState::S_IDLE)
    {
        m_rs485_service->ReleaseBusAccess(this);
        return;
    }

    if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK ||
        m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK ||
        m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
    {
        return;
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
        m_session.state = UpdateState::S_PENDING_CLEANUP;
        break;
    default:
        break;
    }
}

/**
 * @brief Callback - Stigao je odgovor od klijenta.
 */
void UpdateManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    uint8_t response_cmd = packet[6];
    uint8_t ack_nack = packet[0];

    // Provjera ACK/NACK statusa
    if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_START) 
        {
            m_session.state = UpdateState::S_SENDING_DATA;
            m_session.retryCount = 0;
            m_session.currentSequenceNum = 1;
        }
        else 
        {
            Serial.println(F("[UpdateManager] START NACK/GRESKA."));
            m_session.state = S_FAILED;
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_DATA) 
        {
            m_session.retryCount = 0;
            m_session.bytesSent += m_session.read_chunk_size;

            if (m_session.bytesSent >= m_session.fw_size)
            {
                m_session.state = S_FINISHING;
            }
            else
            {
                m_session.currentSequenceNum++;
                m_session.state = S_SENDING_DATA;
            }
        }
        else if (ack_nack == NAK && response_cmd == CMD_UPDATE_DATA) 
        {
            uint32_t requested_seq = (packet[8] << 8) | packet[9]; 
            
            Serial.printf("[UpdateManager] NACK. Klijent trazi paket %lu.\n", requested_seq);
            
            uint32_t offset = (requested_seq - 1) * UPDATE_DATA_CHUNK_SIZE;
            
            m_spi_storage->EndRead();
            m_spi_storage->BeginRead(m_session.fw_address_slot + offset);
            
            m_session.currentSequenceNum = requested_seq;
            m_session.bytesSent = offset;
            m_session.state = S_SENDING_DATA;
            m_session.retryCount = 0;
        }
        else
        {
            Serial.println(F("[UpdateManager] NEOČEKIVAN OGD. Prekid update-a."));
            m_session.state = S_FAILED;
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_FINISH) 
        {
            Serial.println(F("[UpdateManager] FINISH ACK. Update USPJESAN!"));
            m_session.state = S_COMPLETED_OK;
        }
        else
        {
            Serial.println(F("[UpdateManager] FINISH NACK/GRESKA. Update NEUSPJESAN."));
            m_session.state = S_FAILED;
        }
    }
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void UpdateManager::OnTimeout()
{
    if (m_session.retryCount < MAX_UPDATE_RETRIES)
    {
        m_session.retryCount++;
        
        // Vrati stanje na ponovno slanje
        if (m_session.state == S_WAITING_FOR_START_ACK) m_session.state = S_STARTING;
        if (m_session.state == S_WAITING_FOR_DATA_ACK) m_session.state = S_SENDING_DATA;
        if (m_session.state == S_WAITING_FOR_FINISH_ACK) m_session.state = S_FINISHING;
    }
    else
    {
        Serial.println(F("[UpdateManager] GRESKA: Previse neuspjesnih pokusaja. Update prekinut."));
        m_session.state = S_FAILED;
    }
}

/**
 * @brief Kreira i šalje START_REQUEST paket.
 */
void UpdateManager::SendStartRequest(UpdateSession* s)
{
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 12;

    uint8_t sub_cmd = (uint8_t)s->type;
    uint8_t tx_start_cmd = CMD_UPDATE_START; 

    // 1. Popuni zaglavlje
    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    // 2. Data Polje
    packet[6] = tx_start_cmd; 
    
    // Size (32-bit)
    packet[7] = (s->fw_size >> 24);
    packet[8] = (s->fw_size >> 16);
    packet[9] = (s->fw_size >> 8);
    packet[10] = (s->fw_size & 0xFF);

    // CRC (32-bit)
    packet[11] = (s->fw_crc >> 24);
    packet[12] = (s->fw_crc >> 16);
    packet[13] = (s->fw_crc >> 8);
    packet[14] = (s->fw_crc & 0xFF);

    packet[15] = sub_cmd; 
    packet[16] = 0x00; 
    packet[17] = 0x00; 
    
    // 3. Checksum i EOT
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[18] = (checksum >> 8);
    packet[19] = (checksum & 0xFF);
    packet[20] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 21))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_START_ACK;
    }
    else
    {
        s->state = S_IDLE;
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Kreira i šalje DATA paket.
 */
void UpdateManager::SendDataPacket(UpdateSession* s)
{
    int16_t bytes_read = m_spi_storage->ReadChunk(s->read_buffer, UPDATE_DATA_CHUNK_SIZE);
    if (bytes_read <= 0)
    {
        Serial.println(F("[UpdateManager] Citanje sa SPI Flasha zavrseno."));
        s->state = S_FINISHING; 
        return;
    }
    s->read_chunk_size = (uint16_t)bytes_read;
    
    uint8_t packet[MAX_PACKET_LENGTH]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = s->read_chunk_size + 3; // Payload + CMD(1) + SeqNum(2)
    uint16_t total_packet_length = 9 + data_len;

    packet[0] = STX; 
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    // Data Polje: CMD(1) + SeqNum(2) + Payload(N)
    packet[6] = CMD_UPDATE_DATA;
    packet[7] = (s->currentSequenceNum >> 8);
    packet[8] = (s->currentSequenceNum & 0xFF);
    
    // Payload
    memcpy(&packet[9], s->read_buffer, s->read_chunk_size);
    
    // Checksum i EOT
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[total_packet_length - 3] = (checksum >> 8);
    packet[total_packet_length - 2] = (checksum & 0xFF);
    packet[total_packet_length - 1] = EOT;
    
    if (m_rs485_service->SendPacket(packet, total_packet_length))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_DATA_ACK;
    }
    else
    {
        s->state = S_IDLE; 
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Kreira i šalje FINISH_REQUEST paket.
 */
void UpdateManager::SendFinishRequest(UpdateSession* s)
{
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    
    // Data Polje: CMD(1)
    packet[6] = CMD_UPDATE_FINISH;
    
    // Checksum i EOT
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_FINISH_ACK;
    }
    else
    {
        s->state = S_IDLE; 
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Čišćenje sesije i resursa.
 */
void UpdateManager::CleanupSession(UpdateSession* s)
{
    if (s->is_read_active)
    {
        m_spi_storage->EndRead(); 
        s->is_read_active = false;
    }
    s->state = S_IDLE;
}