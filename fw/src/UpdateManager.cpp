/**
 ******************************************************************************
 * @file    UpdateManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija UpdateManager modula.
 *
 * @note
 * REFAKTORISAN: Čita firmware fajlove sa uSD kartice umjesto SPI Flash-a.
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

// CMD-ovi za različite vrste update-a
#define CMD_DWNLD_FWR_IMG   0x77U 
#define CMD_DWNLD_BLDR_IMG  0x78U 
#define CMD_RT_DWNLD_FWR    0x79U 
#define CMD_RT_DWNLD_BLDR   0x7AU
#define CMD_RT_DWNLD_LOGO   0x7BU 
#define CMD_START_BLDR      0x17U // Definicija komande koja nedostaje

// Tajminzi iz common.h za replikaciju originalnog protokola
#define FWR_COPY_DEL        1567U // Pauza za RC da iskopira novi firmware
#define IMG_COPY_DEL        4567U // Pauza za RC da iskopira novu sliku

// Globalna konfiguracija (extern)
extern AppConfig g_appConfig; 

// ============================================================================
// --- NOVA STM32-KOMPATIBILNA CRC32 IMPLEMENTACIJA ---
// ============================================================================
#define CRC32_POLYNOMIAL 0x04C11DB7
#define STM32_CRC_INITIAL_VALUE 0xFFFFFFFF

/**
 * @brief Ažurira CRC obradom jedne 32-bitne riječi, imitirajući hardver.
 */
static uint32_t crc_update_word(uint32_t crc, uint32_t word) {
    crc ^= word;
    for (int i = 0; i < 32; i++) {
        if (crc & 0x80000000) {
            crc = (crc << 1) ^ CRC32_POLYNOMIAL;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Ažurira CRC vrijednost sa baferom podataka.
 * Ova implementacija ispravno simulira ponašanje STM32 hardvera.
 */
static uint32_t stm32_crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    while (len--) {
        // STM32 CRC hardver obrađuje svaki bajt kao 32-bitnu riječ.
        crc = crc_update_word(crc, (uint32_t)*data++);
    }
    return crc;
}

UpdateManager::UpdateManager()
{
    m_session.state = UpdateState::S_IDLE;
    m_sequence.is_active = false; // NOVO
    m_rs485_service = NULL;
    m_sd_card_manager = NULL;
    m_session.is_read_active = false;
}

void UpdateManager::Initialize(Rs485Service* pRs485Service, SdCardManager* pSdCardManager)
{
    m_rs485_service = pRs485Service;
    m_sd_card_manager = pSdCardManager;
}

uint32_t UpdateManager::CalculateCRC32(File& file)
{
    uint32_t crc = STM32_CRC_INITIAL_VALUE;
    uint8_t buffer[256];
    
    file.seek(0);
    
    while (file.available())
    {
        size_t len = file.read(buffer, sizeof(buffer)); // NOLINT(bugprone-sizeof-expression)
        crc = stm32_crc32_update(crc, buffer, len);
    }
    
    file.seek(0);
    return crc; // Vraća konačnu CRC vrijednost bez dodatne XOR operacije
}

bool UpdateManager::ReadMetadataFromFile(const String& metaFilePath, uint32_t* size, uint32_t* crc)
{
    if (!m_sd_card_manager->FileExists(metaFilePath.c_str()))
    {
        return false;
    }
    
    String content = m_sd_card_manager->ReadTextFile(metaFilePath.c_str());
    if (content.length() == 0)
    {
        return false;
    }
    
    int sizeIdx = content.indexOf("size=");
    int crcIdx = content.indexOf("crc=");
    
    if (sizeIdx == -1 || crcIdx == -1)
    {
        return false;
    }
    
    String sizeStr = content.substring(sizeIdx + 5, content.indexOf(' ', sizeIdx));
    String crcStr = content.substring(crcIdx + 4);
    
    *size = sizeStr.toInt();
    *crc = strtoul(crcStr.c_str(), NULL, 16);
    
    return true;
}

bool UpdateManager::PrepareSession(UpdateSession* s, uint8_t updateCmd)
{
    if (!m_sd_card_manager->IsCardMounted())
    {
        Serial.println(F("[UpdateManager] GREŠKA: uSD kartica nije montirana!"));
        return false;
    }

    String filename = "";
    
    if (updateCmd >= CMD_IMG_RC_START && updateCmd <= CMD_IMG_RC_END)
    {
        uint8_t img_num = updateCmd - CMD_IMG_RC_START + 1;

        // KONAČNA ISPRAVKA: Koristi adresu klijenta za formiranje imena fajla
        // s->clientAddress je postavljen prije poziva ove funkcije
        if (s->clientAddress != 0) {
            filename = "/" + String(s->clientAddress) + "/" + String(s->clientAddress) + "_" + String(img_num) + ".RAW";
        } else {
            // Fallback ako adresa nije poznata (ne bi se smjelo desiti za slike)
            filename = "/IMG" + String(img_num) + ".RAW";
        }
        s->type = TYPE_IMG_RC;
        Serial.printf("[UpdateManager] Priprema za slanje slike, fajl: %s\n", filename.c_str());
    }
    else
    {
        switch (updateCmd)
        {
            case CMD_DWNLD_FWR_IMG:   filename = "/NEW.BIN";        s->type = TYPE_FW_RC; break;
            case CMD_DWNLD_BLDR_IMG:  filename = "/BOOTLOADER.BIN"; s->type = TYPE_BLDR_RC; break;
            case CMD_RT_DWNLD_FWR:    filename = "/TH_FW.BIN";      s->type = TYPE_FW_TH; break;
            case CMD_RT_DWNLD_BLDR:   filename = "/TH_BL.BIN";      s->type = TYPE_BLDR_TH; break;
            case CMD_RT_DWNLD_LOGO:   filename = "/LOGO.RAW";       s->type = TYPE_LOGO_RT; break;
            default:
                Serial.printf("[UpdateManager] GREŠKA: Nepoznata CMD: 0x%X\n", updateCmd);
                return false;
        }
    }
    
    s->filename = filename;
    
    if (!m_sd_card_manager->FileExists(filename.c_str()))
    {
        Serial.printf("[UpdateManager] GREŠKA: Fajl '%s' ne postoji!\n", filename.c_str());
        return false;
    }
    
    File file = m_sd_card_manager->OpenFile(filename.c_str(), FILE_READ);
    if (!file)
    {
        Serial.printf("[UpdateManager] GREŠKA: Ne mogu otvoriti fajl '%s'\n", filename.c_str());
        return false;
    }
    
    s->fw_size = file.size();
    
    if (s->fw_size == 0)
    {
        Serial.printf("[UpdateManager] GREŠKA: Fajl '%s' je prazan!\n", filename.c_str());
        file.close();
        return false;
    }
    
    String metaPath = filename + ".meta";
    
    if (m_sd_card_manager->FileExists(metaPath.c_str()))
    {
        if (ReadMetadataFromFile(metaPath, &s->fw_size, &s->fw_crc))
        {
            Serial.printf("[UpdateManager] Metadata učitana iz '%s'\n", metaPath.c_str());
        }
        else
        {
            Serial.println(F("[UpdateManager] .meta fajl neispravan, računam CRC32..."));
            s->fw_crc = CalculateCRC32(file);
        }
    }
    else
    {
        Serial.println(F("[UpdateManager] Računam CRC32 (može trajati nekoliko sekundi)..."));
        s->fw_crc = CalculateCRC32(file);
        Serial.printf("[UpdateManager] CRC32: 0x%08lX\n", s->fw_crc);
    }
    
    s->fw_file = file;
    s->is_read_active = true;
    
    Serial.printf("[UpdateManager] Sesija pripremljena: '%s', Veličina: %lu bytes, CRC: 0x%08lX\n", 
                  filename.c_str(), s->fw_size, s->fw_crc);
    
    return true;
}

bool UpdateManager::IsActive()
{
    return (m_session.state != S_IDLE || m_sequence.is_active);
}

void UpdateManager::StartImageUpdateSequence(uint16_t first_addr, uint16_t last_addr, uint8_t first_img, uint8_t last_img)
{
    if (m_sequence.is_active || m_session.state != S_IDLE) {
        Serial.println("[UpdateManager] UPOZORENJE: Nova sekvenca zatražena dok je stara aktivna.");
        return;
    }
    m_sequence.is_active = true;
    m_sequence.first_addr = first_addr;
    m_sequence.last_addr = last_addr;
    m_sequence.first_img = first_img;
    m_sequence.last_img = last_img;
    m_sequence.current_addr = first_addr;
    m_sequence.current_img = first_img;
    Serial.printf("[UpdateManager] Sekvenca ažuriranja slika pokrenuta: Adrese %d-%d, Slike %d-%d\n", first_addr, last_addr, first_img, last_img);
}

bool UpdateManager::StartSession(uint8_t clientAddress, uint8_t updateCmd)
{
    if (m_session.state != UpdateState::S_IDLE)
    {
        Serial.println(F("[UpdateManager] GREŠKA: Sesija već aktivna!"));
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

    Serial.printf("[UpdateManager] Sesija pokrenuta za klijenta 0x%X\n", clientAddress);
    
    m_session.state = UpdateState::S_STARTING;
    
    return true;
}

/**
 * @brief Glavna funkcija koju poziva state-mašina.
 */
void UpdateManager::Run()
{
    // NOVO: Logika za pokretanje sekvence
    if (m_sequence.is_active && m_session.state == UpdateState::S_IDLE)
    {
        // Provjeri jesmo li završili cijelu sekvencu
        if (m_sequence.current_addr > m_sequence.last_addr)
        {
            Serial.println("[UpdateManager] Sekvenca ažuriranja slika ZAVRŠENA.");
            m_sequence.is_active = false;
            return;
        }

        // Pokreni sesiju za trenutnu adresu i sliku
        uint8_t updateCmd = CMD_IMG_RC_START + m_sequence.current_img - 1;
        Serial.printf("[UpdateManager] Pokretanje dijela sekvence: Adresa %d, Slika %d\n", m_sequence.current_addr, m_sequence.current_img);
        if (!StartSession(m_sequence.current_addr, updateCmd))
        {
             // Greška pri pokretanju (npr. fajl ne postoji), preskoči na sljedeću sliku
             Serial.printf("[UpdateManager] Greška pri pokretanju sesije za %d_%d.RAW. Preskačem.\n", m_sequence.current_addr, m_sequence.current_img);
             // Odmah prelazimo na sljedeću iteraciju
             CleanupSession(false);
             OnTimeout(); // Pozivamo OnTimeout da bi se pokrenula logika za sljedeću sliku/adresu
        }
        // Ako je StartSession uspio, on će preuzeti kontrolu i pozvati SendStartRequest.
        // Mi ovdje ne radimo ništa više, čekamo da se sesija završi.
    }

    if (m_session.state == UpdateState::S_IDLE)
    {
        // Ako nemamo posla, ne radimo ništa.
        return;
    }

    // Specijalni tajmer za pauzu prije slanja START_BLDR
    if (m_session.state == UpdateState::S_PENDING_RESTART_CMD && (millis() - m_session.timeoutStart < APP_START_DEL))
    {
        return;
    }

    switch (m_session.state)
    {
    case UpdateState::S_STARTING:
        SendStartRequest();
        break;
    case UpdateState::S_SENDING_DATA:
        SendDataPacket();
        break;
    case UpdateState::S_FINISHING:
        SendFinishRequest();
        break;
    case UpdateState::S_PENDING_RESTART_CMD:
        SendRestartCommand();
        break;
    case UpdateState::S_PENDING_CLEANUP:
        CleanupSession(false);
        break;
    case UpdateState::S_COMPLETED_OK:
    case UpdateState::S_FAILED:
        // U ovim stanjima, ProcessResponse ili OnTimeout su već pozvani i postavili su stanje.
        // Prelazimo na čišćenje.
        m_session.state = UpdateState::S_PENDING_CLEANUP;
        break;
    default:
        break;
    }
}

void UpdateManager::ProcessResponse(const uint8_t* packet, uint16_t length)
{
    // Ako paket ne postoji (preskakanje fajla), odmah izađi
    if (packet == nullptr) return;

    uint8_t response_cmd = packet[6];
    uint8_t ack_nack = packet[0];

    if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_START) 
        {
            Serial.println(F("[UpdateManager] -> Primljen START ACK. Započinjem slanje podataka..."));
            m_session.state = UpdateState::S_SENDING_DATA;
            m_session.retryCount = 0;
            m_session.currentSequenceNum = 1;
        }
        else 
        {
            Serial.printf("[UpdateManager] -> Primljen START NACK ili pogrešan odgovor (ACK: 0x%02X, CMD: 0x%02X). Prekidam.\n", ack_nack, response_cmd);
            CleanupSession(true);
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_DATA) 
        {
            Serial.printf("[UpdateManager] -> Primljen DATA ACK za paket #%lu.\n", m_session.currentSequenceNum);
            m_session.retryCount = 0;
            m_session.bytesSent += m_session.read_chunk_size;

            if (m_session.bytesSent >= m_session.fw_size)
            {
                // REPLIKACIJA STAROG PROTOKOLA:
                // Ako je ovo bio firmware update, ne šaljemo FINISH, već se pripremamo za START_BLDR
                if (m_session.type == TYPE_FW_RC)
                {
                    Serial.println(F("[UpdateManager] Slanje fajla završeno. Priprema za START_BLDR..."));
                    m_session.state = S_PENDING_RESTART_CMD;
                    m_session.timeoutStart = millis(); // Pokreni pauzu od APP_START_DEL
                }
                else // Za sve ostale tipove (slike, itd.), koristi se standardni FINISH
                {
                    m_session.state = S_FINISHING;
                }
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
            
            Serial.printf("[UpdateManager] -> Primljen NACK. Klijent traži paket %lu.\n", requested_seq);
            
            uint32_t offset = (requested_seq - 1) * UPDATE_DATA_CHUNK_SIZE;
            
            if (!m_session.fw_file.seek(offset))
            {
                Serial.println(F("[UpdateManager] GREŠKA: seek() neuspješan!"));
                m_session.state = S_FAILED;
                return;
            }
            
            m_session.currentSequenceNum = requested_seq;
            m_session.bytesSent = offset;
            m_session.state = S_SENDING_DATA;
            m_session.retryCount = 0;
        }
        else
        {
            Serial.printf("[UpdateManager] -> Primljen NEOČEKIVAN ODGOVOR (ACK: 0x%02X, CMD: 0x%02X). Prekidam.\n", ack_nack, response_cmd);
            CleanupSession(true);
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
    {
        if (ack_nack == ACK && response_cmd == CMD_UPDATE_FINISH) 
        {
            Serial.println(F("[UpdateManager] -> Primljen FINISH ACK. Update USPJEŠAN!"));
            CleanupSession(false);
        }
        else
        {
            Serial.printf("[UpdateManager] -> Primljen FINISH NACK ili pogrešan odgovor (ACK: 0x%02X, CMD: 0x%02X). Update NEUSPJEŠAN.\n", ack_nack, response_cmd);
            CleanupSession(true);
        }
    }
}

void UpdateManager::OnTimeout()
{
    // Stara logika za pojedinačne sesije
    if (m_session.retryCount >= MAX_UPDATE_RETRIES)
    {
        Serial.println(F("[UpdateManager] GREŠKA: Previše neuspješnih pokušaja. Update prekinut."));
        CleanupSession(true);
    }
    else
    {
        m_session.retryCount++; // Uvećaj brojač za sljedeći pokušaj
        Serial.printf("[UpdateManager] Timeout. Pokušaj %d od %d...\n", m_session.retryCount, MAX_UPDATE_RETRIES);

        // Ponovo pošalji isti paket
        switch(m_session.state) {
            case S_WAITING_FOR_START_ACK:
                SendStartRequest();
                break;
            case S_WAITING_FOR_DATA_ACK:
                // Vraćamo fajl pointer na početak zadnjeg pročitanog chunka.
                m_session.fw_file.seek(m_session.bytesSent);
                SendDataPacket();
                break;
            case S_WAITING_FOR_FINISH_ACK:
                SendFinishRequest();
                break;
            default:
                CleanupSession(true); // Neočekivano stanje
                break;
        }
    }
}

void UpdateManager::SendStartRequest()
{
    Serial.println(F("[UpdateManager] -> Šaljem START paket..."));
    UpdateSession* s = &m_session;
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 12;

    uint8_t sub_cmd = (uint8_t)s->type;
    uint8_t tx_start_cmd = CMD_UPDATE_START; 

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    packet[6] = tx_start_cmd; 
    
    packet[7] = (s->fw_size >> 24);
    packet[8] = (s->fw_size >> 16);
    packet[9] = (s->fw_size >> 8);
    packet[10] = (s->fw_size & 0xFF);

    packet[11] = (s->fw_crc >> 24);
    packet[12] = (s->fw_crc >> 16);
    packet[13] = (s->fw_crc >> 8);
    packet[14] = (s->fw_crc & 0xFF);

    packet[15] = sub_cmd; 
    packet[16] = 0x00; 
    packet[17] = 0x00; 
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[18] = (checksum >> 8);
    packet[19] = (checksum & 0xFF);
    packet[20] = EOT;
    
    // REPLIKACIJA TAJMINGA IZ STAROG KODA
    uint32_t response_timeout = UPDATE_PACKET_TIMEOUT_MS;
    if (s->type == TYPE_FW_RC || s->type == TYPE_BLDR_RC)
    {
        // Za firmware/bootloader, koristi se duža pauza da se omogući brisanje flash-a
        response_timeout = IMG_COPY_DEL;
    }
    else if (s->type == TYPE_IMG_RC)
    {
        // Za slike, koristi se kraća pauza
        response_timeout = FWR_COPY_DEL;
    }

    if (m_rs485_service->SendPacket(packet, 21))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_START_ACK;

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        // Koristimo dinamički timeout umjesto fiksnog
        int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, response_timeout);
        if (response_len > 0) {
            ProcessResponse(response_buffer, response_len);
        } else {
            OnTimeout();
        }
    }
    else
    {
        CleanupSession(true);
    }
}

void UpdateManager::SendDataPacket()
{
    Serial.printf("[UpdateManager] -> Šaljem DATA paket #%lu...\n", m_session.currentSequenceNum);
    UpdateSession* s = &m_session;
    int16_t bytes_read = s->fw_file.read(s->read_buffer, UPDATE_DATA_CHUNK_SIZE);
    
    if (bytes_read <= 0)
    {
        Serial.println(F("[UpdateManager] Čitanje sa kartice završeno. Prelazim na FINISH."));
        s->state = S_FINISHING; 
        return;
    }
    
    s->read_chunk_size = (uint16_t)bytes_read;
    
    uint8_t packet[MAX_PACKET_LENGTH]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = s->read_chunk_size + 3;
    uint16_t total_packet_length = 9 + data_len;

    packet[0] = STX; 
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    packet[6] = CMD_UPDATE_DATA;
    packet[7] = (s->currentSequenceNum >> 8);
    packet[8] = (s->currentSequenceNum & 0xFF);
    
    memcpy(&packet[9], s->read_buffer, s->read_chunk_size);
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[total_packet_length - 3] = (checksum >> 8);
    packet[total_packet_length - 2] = (checksum & 0xFF);
    packet[total_packet_length - 1] = EOT;
    
    // REPLIKACIJA TAJMINGA IZ STAROG KODA
    uint32_t response_timeout = UPDATE_PACKET_TIMEOUT_MS;
    // Provjera da li je ovo zadnji paket
    if ((s->bytesSent + s->read_chunk_size) >= s->fw_size)
    {
        if (s->type == TYPE_FW_RC || s->type == TYPE_BLDR_RC) {
            response_timeout = IMG_COPY_DEL;
        } else if (s->type == TYPE_IMG_RC) {
            response_timeout = FWR_COPY_DEL;
        }
    }

    if (m_rs485_service->SendPacket(packet, total_packet_length))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_DATA_ACK;

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        // Koristimo dinamički timeout umjesto fiksnog
        int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, response_timeout);
        if (response_len > 0) {
            ProcessResponse(response_buffer, response_len);
        } else {
            OnTimeout();
        }
    }
    else
    {
        CleanupSession(true);
    }
}

void UpdateManager::SendFinishRequest()
{
    UpdateSession* s = &m_session;
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    
    packet[6] = CMD_UPDATE_FINISH;
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_FINISH_ACK;

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        int response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, UPDATE_PACKET_TIMEOUT_MS);
        if (response_len > 0) {
            ProcessResponse(response_buffer, response_len);
        } else {
            OnTimeout();
        }
    }
    else
    {
        CleanupSession(true);
    }
}

/**
 * @brief Šalje START_BLDR komandu nakon UPDATE_FWR. Replicira stari protokol.
 */
void UpdateManager::SendRestartCommand()
{
    UpdateSession* s = &m_session;
    Serial.println(F("[UpdateManager] Slanje START_BLDR komande..."));
    
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    
    packet[6] = CMD_START_BLDR; // Komanda za restart u bootloader
    
    uint16_t checksum = CMD_START_BLDR;

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    // Ovdje ne očekujemo odgovor, ali Rs485Service će čekati timeout.
    // Nakon timeout-a, sesija će se završiti.
    if (m_rs485_service->SendPacket(packet, 10))
    {
        // Ne čekamo odgovor, smatramo uspješnim
        CleanupSession(false);
    }
    else
    {
        CleanupSession(true);
    }
}

void UpdateManager::CleanupSession(bool failed /*= false*/)
{
    if (m_session.is_read_active && m_session.fw_file)
    {
        m_session.fw_file.close();
        m_session.is_read_active = false;
    }
    m_session.state = S_IDLE;

    // NOVO: Logika za napredovanje sekvence nakon završetka jedne sesije
    if (m_sequence.is_active) {
        if (failed) {
            Serial.printf("[UpdateManager] Sesija NEUSPJEŠNA za Adresu %d, Slika %d. Prelazim na sljedeću.\n", m_sequence.current_addr, m_sequence.current_img);
        } else {
            Serial.printf("[UpdateManager] Sesija USPJEŠNA za Adresu %d, Slika %d. Prelazim na sljedeću.\n", m_sequence.current_addr, m_sequence.current_img);
        }
        
        m_sequence.current_img++;
        if (m_sequence.current_img > m_sequence.last_img) {
            m_sequence.current_img = m_sequence.first_img;
            m_sequence.current_addr++;
        }
    }
}