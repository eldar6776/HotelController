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
#include "TimeSync.h"
#include "HttpServer.h"  // NAKON ostalih da izbjegnemo FILE_READ konflikt
#include <cstring>

// HttpServer.h može da override-uje FILE_READ macro, pa ga eksplicitno definišemo
#ifndef FILE_READ
#define FILE_READ "r"
#endif

// Globalna konfiguracija (extern)
extern AppConfig g_appConfig; 

// ============================================================================
// --- STM32 CRC32 HARDWARE-LIKE SOFTWARE IMPLEMENTATION ---
// Replicating the exact logic from the provided stm32_crc.c file.
// ============================================================================

/**
 * @brief Updates the CRC by processing a single 32-bit word, mimicking the hardware.
 * This is a direct C++ port of the function from stm32_crc.c.
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
 * @brief Updates a CRC value with a buffer of data, simulating STM32 hardware behavior.
 * This is a direct C++ port of the function from stm32_crc.c.
 */
static uint32_t stm32_crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    while (len--) {
        // The STM32 CRC hardware processes each byte as a 32-bit word.
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
    m_http_server = NULL;
    m_session.is_read_active = false;
    
    // Inicijalizuj zastavice za sekvencu
    m_first_image_in_sequence = true;
    m_session_in_progress = false;
}

void UpdateManager::Initialize(Rs485Service* pRs485Service, SdCardManager* pSdCardManager)
{
    m_rs485_service = pRs485Service;
    m_sd_card_manager = pSdCardManager;
}

/**
 * @brief Vraća veličinu chunk-a (payload) za trenutni protokol.
 * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): 64 bajta
 *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): 128 bajtova (default)
 * @return Veličina chunk-a u bajtovima (64 ili 128)
 * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
 */
uint16_t UpdateManager::GetChunkSizeForProtocol()
{
    ProtocolVersion proto = static_cast<ProtocolVersion>(g_appConfig.protocol_version);
    
    switch (proto)
    {
        // GRUPA 1 - STARI PROTOKOL (64-bajtni chunk)
        case ProtocolVersion::HILLS:
        case ProtocolVersion::BJELASNICA:
        case ProtocolVersion::SAPLAST:
        case ProtocolVersion::SAX:
        case ProtocolVersion::BOSS:
        case ProtocolVersion::BASKUCA:
            return 64;  // STARI protokol - manji chunk
        
        // GRUPA 2 - NOVI PROTOKOL (128-bajtni chunk) - TRENUTNO STANJE PROJEKTA
        case ProtocolVersion::VUCKO:
        case ProtocolVersion::ULM:
        case ProtocolVersion::VRATA_BOSNE:
        case ProtocolVersion::DZAFIC:
        default:
            return 128; // NOVI protokol - kako projekat trenutno radi
    }
}

/**
 * @brief Vraća timeout za čekanje odgovora tokom update-a za trenutni protokol.
 * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): 78ms
 *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): 45ms (default)
 * @return Timeout u milisekundama (78 ili 45)
 * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
 */
uint32_t UpdateManager::GetUpdateTimeoutForProtocol()
{
    ProtocolVersion proto = static_cast<ProtocolVersion>(g_appConfig.protocol_version);
    
    switch (proto)
    {
        // GRUPA 1 - STARI PROTOKOL (duži timeout)
        case ProtocolVersion::HILLS:
        case ProtocolVersion::BJELASNICA:
        case ProtocolVersion::SAPLAST:
        case ProtocolVersion::SAX:
        case ProtocolVersion::BOSS:
        case ProtocolVersion::BASKUCA:
            return 78;  // STARI protokol - duži timeout
        
        // GRUPA 2 - NOVI PROTOKOL (kraći timeout) - TRENUTNO STANJE PROJEKTA
        case ProtocolVersion::VUCKO:
        case ProtocolVersion::ULM:
        case ProtocolVersion::VRATA_BOSNE:
        case ProtocolVersion::DZAFIC:
        default:
            return 45;  // NOVI protokol - kako projekat trenutno radi
    }
}

/**
 * @brief Provjerava da li trenutni protokol koristi single-byte ACK/NAK odgovore.
 * @details STARI protokol (HILLS/SAX/BJELASNICA/SAPLAST/BOSS/BASKUCA): true (1 bajt ACK/NAK)
 *          NOVI protokol (VUCKO/ULM/VRATA_BOSNE/DZAFIC): false (puni ACK paket sa headerom)
 * @return true ako protokol koristi single-byte ACK/NAK, false inače
 * @note OVO SE ODNOSI SAMO NA UPDATE FAJLOVA, NE NA TRANSFER LOGOVA
 */
bool UpdateManager::UseSingleByteAckForProtocol()
{
    ProtocolVersion proto = static_cast<ProtocolVersion>(g_appConfig.protocol_version);
    
    switch (proto)
    {
        // GRUPA 1 - STARI PROTOKOL (single-byte ACK/NAK)
        case ProtocolVersion::HILLS:
        case ProtocolVersion::BJELASNICA:
        case ProtocolVersion::SAPLAST:
        case ProtocolVersion::SAX:
        case ProtocolVersion::BOSS:
        case ProtocolVersion::BASKUCA:
            return true;   // STARI protokol - ACK/NAK kao 1 bajt
        
        // GRUPA 2 - NOVI PROTOKOL (puni ACK paket) - TRENUTNO STANJE PROJEKTA
        case ProtocolVersion::VUCKO:
        case ProtocolVersion::ULM:
        case ProtocolVersion::VRATA_BOSNE:
        case ProtocolVersion::DZAFIC:
        default:
            return false;  // NOVI protokol - kako projekat trenutno radi
    }
}

uint32_t UpdateManager::CalculateCRC32(File& file)
{
    // Use the STM32-specific initial value.
    uint32_t crc = 0xFFFFFFFF; 
    uint8_t buffer[256];
    
    file.seek(0);
    
    while (file.available())
    {
        size_t len = file.read(buffer, sizeof(buffer));
        // Use the replicated STM32 CRC update function.
        crc = stm32_crc32_update(crc, buffer, len);
    }
    
    file.seek(0);
    return crc;
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

bool UpdateManager::IsSequenceActive()
{
    return m_sequence.is_active;
}

void UpdateManager::StopSequence()
{
    m_sequence.is_active = false;
    Serial.println(F("[UpdateManager] Sekvenca zaustavljena."));
    
    // POKRENI SERVER nakon što je cijela sekvenca završena!
    if (m_http_server) {
        m_http_server->Start();
    }
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

    // ZAUSTAVI SERVER!
    if (m_http_server) m_http_server->Stop();

    m_session.clientAddress = clientAddress;
    m_session.bytesSent = 0;
    m_session.currentSequenceNum = 0;
    m_session.retryCount = 0;

    if (!PrepareSession(&m_session, updateCmd))
    {
        if (m_http_server) m_http_server->Start();
        return false;
    }

    // =================================================================================
    // KRITIČNO: Aktiviraj single-byte mod ako je STARI protokol
    // Ovo omogućava Rs485Service da prihvati single-byte ACK/NAK
    // =================================================================================
    if (UseSingleByteAckForProtocol())
    {
        m_rs485_service->EnableSingleByteMode();
        Serial.println(F("[UpdateManager] STARI protokol detektovan - single-byte mod aktiviran"));
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
    if (m_sequence.is_active && m_session.state == UpdateState::S_IDLE && !m_session_in_progress)
    {
        // Napreduj na sljedeću sliku (osim prvog puta)
        if (!m_first_image_in_sequence) {
            Serial.printf("[UpdateManager] Pauza %dms prije sljedeće slike...\n", IMG_COPY_DEL);
            vTaskDelay(pdMS_TO_TICKS(IMG_COPY_DEL));
            
            m_sequence.current_img++;
            if (m_sequence.current_img > m_sequence.last_img) {
                m_sequence.current_img = m_sequence.first_img;
                m_sequence.current_addr++;
            }
        }
        m_first_image_in_sequence = false;
        
        if (m_sequence.current_addr > m_sequence.last_addr)
        {
            Serial.println("[UpdateManager] KRAJ SEKVENCIJE: Sve adrese su obrađene.");
            StopSequence();
            m_first_image_in_sequence = true;
            m_session_in_progress = false;
            return;
        }
        
        uint8_t updateCmd = CMD_IMG_RC_START + m_sequence.current_img - 1;
        Serial.printf("[UpdateManager] Pokretanje dijela sekvence: Adresa %d, Slika %d\n", m_sequence.current_addr, m_sequence.current_img);
        if (!StartSession(m_sequence.current_addr, updateCmd))
        {
             Serial.printf("[UpdateManager] Greška pri pokretanju sesije za %d_%d.RAW.\n", m_sequence.current_addr, m_sequence.current_img);
             CleanupSession(true);
             m_first_image_in_sequence = true;
             m_session_in_progress = false;
        }
        else
        {
            m_session_in_progress = true; // Sesija pokrenuta
        }
    }

    if (!IsActive()) {
        return;
    }

    // =================================================================================
    // --- POTPUNO BLOKIRAJUĆA PETLJA ZA UPDATE ---
    // Kada je sesija aktivna, ova petlja preuzima potpunu kontrolu.
    // =================================================================================
    while (m_session.state != S_IDLE)
    {
        if (m_session.retryCount >= MAX_UPDATE_RETRIES) {
            Serial.println(F("[UpdateManager] GREŠKA: Previše neuspješnih pokušaja. Update prekinut."));
            CleanupSession(true);
            break;
        }

        uint8_t response_buffer[MAX_PACKET_LENGTH];
        int response_len = 0;
        uint32_t response_timeout = 0;

        // --- FAZA 1: SLANJE ---
        switch (m_session.state)
        {
            case S_STARTING:
                if (m_session.type == TYPE_FW_RC || m_session.type == TYPE_BLDR_RC) {
                    SendFirmwareStartRequest();
                } else {
                    SendFileStartRequest();
                }
                response_timeout = 1300; // Pauza 1300ms (1171ms stvarno čekanje + 129ms rezerve)
                break;

            case S_SENDING_DATA:
                SendDataPacket();
                // Provjera da li je ovo zadnji paket
                if ((m_session.bytesSent + m_session.read_chunk_size) >= m_session.fw_size) {
                    response_timeout = (m_session.type == TYPE_FW_RC || m_session.type == TYPE_BLDR_RC) ? IMG_COPY_DEL : FWR_COPY_DEL;
                } else {
                    // PROMJENA: Koristi dinamički timeout za update umjesto hardkodirane konstante
                    response_timeout = GetUpdateTimeoutForProtocol(); // 78ms (stari) ili 45ms (novi)
                }
                break;

            case S_FINISHING:
                SendFinishRequest();
                response_timeout = 500; // Dajemo klijentu vremena da potvrdi
                break;

            case S_SENDING_RESTART_CMD:
                SendRestartCommand();
                m_session.state = S_PENDING_APP_START;
                // Nema čekanja na odgovor, prelazimo na pauzu
                continue;

            case S_PENDING_APP_START:
                Serial.printf("[UpdateManager] Pauza od %dms prije slanja APP_EXE...\n", APP_START_DEL);
                vTaskDelay(pdMS_TO_TICKS(APP_START_DEL));
                SendAppExeCommand();
                // Ne čekamo odgovor, završavamo sesiju
                CleanupSession(false);
                continue;

            default:
                // S_IDLE uopšte ne bi trebalo da se desi ovdje jer while provjerava state
                // Ako se ipak desi, izađi iz petlje
                if (m_session.state == S_IDLE) {
                    break;
                }
                // Ostala neočekivana stanja
                CleanupSession(true);
                break;
        }

        // --- FAZA 2: ČEKANJE ODGOVORA ---
        if (m_session.state == S_WAITING_FOR_START_ACK ||
            m_session.state == S_WAITING_FOR_DATA_ACK ||
            m_session.state == S_WAITING_FOR_FINISH_ACK)
        {
            response_len = m_rs485_service->ReceivePacket(response_buffer, MAX_PACKET_LENGTH, response_timeout);

            // =================================================================================
            // --- NOVO: Bezuslovni ispis primljenog RAW paketa, po uzoru na Rs485Service ---
            // =================================================================================
            if (response_len > 0) {
                char raw_packet_str[response_len * 3 + 1] = {0};
                for (int i = 0; i < response_len; i++) {
                    sprintf(raw_packet_str + strlen(raw_packet_str), "%02X ", response_buffer[i]);
                }
                Serial.printf("[UpdateManager] -> RAW Odgovor primljen u %lu ms (%d B): [ %s]\n", millis(), response_len, raw_packet_str);
            }
            if (response_len > 0) {
                // Imamo odgovor, obradi ga
                ProcessResponse(response_buffer, response_len);
            } else {
                // Timeout, OnTimeout će se pobrinuti za logiku ponovnog pokušaja
                OnTimeout();
            }
        }

        // Mala pauza da se spriječi 100% zauzeće CPU-a unutar ove petlje
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    // Kraj blokirajuće petlje
}

void UpdateManager::ProcessResponse(const uint8_t* packet, uint16_t length)
{
    // =================================================================================
    // --- BEZUSLOVNA DIJAGNOSTIKA PRIMLJENOG PAKETA ---
    // Ispisuje se uvijek, prije bilo kakve logike, da se tačno vidi šta je stiglo.
    // =================================================================================
    Serial.println(F("--- UpdateManager: Analiza Primljenog Odgovora ---"));
    char packet_str[length * 3 + 1];
    packet_str[0] = '\0';
    for (int i = 0; i < length; i++) {
        sprintf(packet_str + strlen(packet_str), "%02X ", packet[i]);
    }
    Serial.printf("  -> RAW Paket (%d B): [ %s]\n", length, packet_str);
    
    // =================================================================================
    // --- KRITIČNA PROVJERA: STARI PROTOKOL (SAX/HILLS/itd) KORISTI SINGLE-BYTE ACK/NAK ---
    // Ako je protokol postavljen na STARI i stigao je 1 bajt, to je VALIDNI odgovor!
    // =================================================================================
    if (UseSingleByteAckForProtocol() && length == 1)
    {
        uint8_t single_byte = packet[0];
        Serial.printf("  -> STARI PROTOKOL: Single-byte odgovor: 0x%02X ", single_byte);
        if (single_byte == ACK) Serial.println("(ACK)");
        else if (single_byte == NAK) Serial.println("(NAK)");
        else Serial.println("(NEPOZNAT)");
        Serial.println(F("----------------------------------------------------"));
        
        // Obradi single-byte ACK/NAK prema trenutnom stanju
        if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK)
        {
            if (single_byte == ACK) {
                Serial.println(F("[UpdateManager] -> Primljen START ACK (1B). Započinjem slanje podataka..."));
                m_session.state = UpdateState::S_SENDING_DATA;
                m_session.retryCount = 0;
                m_session.currentSequenceNum = 1;
            } else {
                Serial.println(F("[UpdateManager] -> Primljen START NAK (1B). Pokrećem ponovni pokušaj..."));
                OnTimeout();
            }
            return;
        }
        else if (m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK)
        {
            if (single_byte == ACK) {
                Serial.printf("[UpdateManager] -> Primljen DATA ACK (1B) za paket #%lu.\n", m_session.currentSequenceNum);
                m_session.retryCount = 0;
                m_session.bytesSent += m_session.read_chunk_size;
                
                if (m_session.bytesSent >= m_session.fw_size) {
                    if (m_session.type == TYPE_FW_RC) {
                        Serial.println(F("[UpdateManager] Slanje FW završeno. Prelazim na START_BLDR."));
                        m_session.state = S_SENDING_RESTART_CMD;
                    } else {
                        Serial.println(F("[UpdateManager] -> Slanje slike ZAVRŠENO."));
                        CleanupSession(false);
                    }
                } else {
                    m_session.currentSequenceNum++;
                    m_session.state = S_SENDING_DATA;
                }
            } else if (single_byte == NAK) {
                m_session.retryCount++;
                Serial.printf("[UpdateManager] -> Primljen NAK (1B). Pokušaj %d/%d...\n", m_session.retryCount, MAX_UPDATE_RETRIES);
                m_session.state = S_SENDING_DATA;
                m_session.fw_file.seek(m_session.bytesSent);
            } else {
                Serial.println(F("[UpdateManager] -> Nepoznat 1B odgovor. Timeout..."));
                OnTimeout();
            }
            return;
        }
        else if (m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
        {
            if (single_byte == ACK) {
                Serial.println(F("[UpdateManager] -> Primljen FINISH ACK (1B). Update završen!"));
                CleanupSession(false);
            } else {
                Serial.println(F("[UpdateManager] -> Primljen FINISH NAK (1B). Pokušaj ponovo..."));
                OnTimeout();
            }
            return;
        }
        
        return; // Izlaz za single-byte protokol
    }
    
    // =================================================================================
    // --- NOVI PROTOKOL: PUNI PAKET SA HEADEROM (10+ bajtova) - TRENUTNO STANJE ---
    // Ovaj kod radi 100% identično kao prije za VUCKO/ULM/VRATA_BOSNE/DZAFIC
    // =================================================================================
    if (length >= 10) { // Minimalna dužina za parsiranje punog paketa
        Serial.printf("  -> Status (ACK/NAK): 0x%02X\n", packet[0]);
        Serial.printf("  -> Ciljna Adresa:   0x%02X%02X\n", packet[1], packet[2]);
        Serial.printf("  -> Izvorna Adresa:  0x%02X%02X\n", packet[3], packet[4]);
        Serial.printf("  -> Dužina Podataka: %d\n", packet[5]);
        Serial.printf("  -> Komanda:         0x%02X\n", packet[6]);
    }
    Serial.println(F("----------------------------------------------------"));

    uint8_t response_cmd = packet[6];
    uint8_t ack_nack = packet[0];

    if (m_session.state == UpdateState::S_WAITING_FOR_START_ACK)
    {
        if (ack_nack == ACK)
        {
            Serial.println(F("[UpdateManager] -> Primljen START ACK. Započinjem slanje podataka..."));
            m_session.state = UpdateState::S_SENDING_DATA;
            m_session.retryCount = 0;
            m_session.currentSequenceNum = 1;
        }
        else
        {
            // ISPRAVKA: Ako dobijemo NAK, ne prekidamo odmah.
            // Tretiramo ga kao timeout da bismo pokrenuli mehanizam za ponovno slanje.
            // Ovo replicira ponašanje starog sistema gdje se `trial` brojač povećava.
            Serial.printf("[UpdateManager] -> Primljen START NACK (0x%02X) ili pogrešan odgovor (očekivano 0x%02X, primljeno 0x%02X). Pokrećem ponovni pokušaj...\n", ack_nack, m_last_sent_sub_cmd, response_cmd);
            OnTimeout();
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_DATA_ACK)
    {
        if (ack_nack == ACK) 
        {
            Serial.printf("[UpdateManager] -> Primljen DATA ACK za paket #%lu.\n", m_session.currentSequenceNum);
            m_session.retryCount = 0;
            m_session.bytesSent += m_session.read_chunk_size;

            if (m_session.bytesSent >= m_session.fw_size)
            {
                // REPLIKACIJA STAROG PROTOKOLA:
                // - Za firmware update: šalje se START_BLDR komanda nakon zadnjeg DATA paketa
                // - Za image update: zadnji DATA paket je FINALNI - nema dodatnog FINISH paketa!
                if (m_session.type == TYPE_FW_RC)
                {   
                    Serial.println(F("[UpdateManager] Slanje FW fajla završeno. Prelazim na slanje START_BLDR komande."));
                    m_session.state = S_SENDING_RESTART_CMD;
                }
                else // Za slike - zadnji DATA ACK je kraj, ne šalje se FINISH
                {
                    Serial.println(F("[UpdateManager] -> Slanje slike ZAVRŠENO. Zadnji DATA ACK primljen."));
                    CleanupSession(false);
                }
            }
            else
            {
                m_session.currentSequenceNum++;
                m_session.state = S_SENDING_DATA;
            }
        }
        else if (ack_nack == NAK) 
        {
            // ISPRAVKA: NAK se tretira kao ponovni pokušaj, ali ne resetuje sekvencu.
            // Povećavamo brojač pokušaja i ostajemo u istom stanju slanja.
            m_session.retryCount++;
            Serial.printf("[UpdateManager] -> Primljen NACK. Pokušaj %d od %d...\n", m_session.retryCount, MAX_UPDATE_RETRIES);
            m_session.state = S_SENDING_DATA; // Vrati stanje da se ponovo pošalje
            // Vrati fajl na prethodnu poziciju da se isti paket ponovo pročita i pošalje
            m_session.fw_file.seek(m_session.bytesSent);
            /* Stara logika za preskakanje na traženi paket - odbačeno radi jednostavnosti i robusnosti
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
            */
        }
        else
        {
            Serial.printf("[UpdateManager] -> Primljen NEOČEKIVAN ODGOVOR (ACK: 0x%02X, CMD: 0x%02X). Prekidam.\n", ack_nack, response_cmd);
            CleanupSession(true);
        }
    }
    else if (m_session.state == UpdateState::S_WAITING_FOR_FINISH_ACK)
    
    {
        if (ack_nack == ACK) 
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
    m_session.retryCount++;
    Serial.printf("[UpdateManager] Timeout. Pokušaj %d od %d...\n", m_session.retryCount, MAX_UPDATE_RETRIES);

    // =================================================================================
    // KRITIČNO: RX2TX delay MORA postojati prije ponovnog slanja paketa!
    // Referentni kod: rx_tmr = Get_SysTick(); rx_tout = RX2TX_DEL; HC_State = PCK_ENUM;
    // Bez ovog delay-a, kontroler ne stiže da se prebaci u RX mod i NAK se gubi.
    // =================================================================================
    vTaskDelay(pdMS_TO_TICKS(RX2TX_DEL_MS));

    // Vrati stanje na prethodno da bi se ponovo poslao isti paket
    switch(m_session.state) {
        case S_WAITING_FOR_START_ACK:
            m_session.state = S_STARTING;
            break;
        case S_WAITING_FOR_DATA_ACK:
            // Vrati fajl na prethodnu poziciju da se isti paket ponovo pročita i pošalje
            m_session.fw_file.seek(m_session.bytesSent);
            m_session.state = S_SENDING_DATA;
            break;
        case S_WAITING_FOR_FINISH_ACK:
            m_session.state = S_FINISHING;
            break;
        default: break;
    }
}

/**
 * @brief Šalje START paket za Firmware/Bootloader.
 * Replicira logiku iz HC_CreateFirmwareUpdateRequest.
 */
void UpdateManager::SendFirmwareStartRequest()
{
    UpdateSession* s = &m_session;
    uint8_t packet[16];
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 0;
    uint16_t total_packet_len = 0;

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);

    // Logika iz HC_CreateFirmwareUpdateRequest: Prvo se šalje START_BLDR, pa tek onda DWNLD_FWR
    if (s->currentSequenceNum == 0) // Prvi korak, šalje se START_BLDR
    {
        Serial.println(F("[UpdateManager] -> Šaljem START_BLDR paket..."));
        data_len = 1;
        packet[6] = CMD_START_BLDR;
    }
    else // Drugi korak (nakon ACK-a na START_BLDR), šalje se DWNLD_FWR sa brojem paketa
    {
        Serial.println(F("[UpdateManager] -> Šaljem DWNLD_FWR paket..."));
        data_len = 3;
        packet[6] = CMD_DWNLD_FWR_IMG; // U starom kodu je ovo bio DWNLD_FWR
        
        // PROMJENA: Koristi dinamički chunk size za update umjesto hardkodirane konstante
        uint16_t chunk_size = GetChunkSizeForProtocol(); // 64 (stari) ili 128 (novi)
        uint16_t total_packets = (s->fw_size + chunk_size - 1) / chunk_size;
        packet[7] = (total_packets >> 8) & 0xFF;
        packet[8] = total_packets & 0xFF;
    }

    packet[5] = data_len;
    total_packet_len = data_len + 9;

    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[total_packet_len - 3] = (checksum >> 8);
    packet[total_packet_len - 2] = (checksum & 0xFF);
    packet[total_packet_len - 1] = EOT;

    if (m_rs485_service->SendPacket(packet, total_packet_len)) {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_START_ACK;
    } else {
        CleanupSession(true);
    }
}

/**
 * @brief Šalje START paket za Slike/Logo (File Update).
 * Replicira logiku iz HC_CreateFileUpdateRequest.
 */
void UpdateManager::SendFileStartRequest()
{
    UpdateSession* s = &m_session;
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 11; // Fiksna dužina za file update start

    // Određivanje tačne komande na osnovu tipa
    uint8_t sub_cmd = 0;
    Serial.printf("[UpdateManager] -> Šaljem START paket za fajl (Tip: %d)...\n", s->type);
    if (s->type == TYPE_IMG_RC) {
        // Izvuci broj slike iz imena fajla, npr. "101_1.RAW" -> 1
        int last_ = s->filename.lastIndexOf('_');
        int last_dot = s->filename.lastIndexOf('.');
        if (last_ != -1 && last_dot != -1) {
            String img_num_str = s->filename.substring(last_ + 1, last_dot);
            sub_cmd = CMD_IMG_RC_START + img_num_str.toInt() - 1;
        }
    } else {
        // Fallback za ostale tipove
        switch(s->type) {
            case TYPE_FW_TH:   sub_cmd = CMD_RT_DWNLD_FWR; break;
            case TYPE_BLDR_TH: sub_cmd = CMD_RT_DWNLD_BLDR; break;
            case TYPE_LOGO_RT: sub_cmd = CMD_RT_DWNLD_LOGO; break;
            default:
                Serial.printf("[UpdateManager] GREŠKA: Nepodržan tip fajla za SendFileStartRequest: %d\n", s->type);
                CleanupSession(true);
                return; // Vraća void
        }
    }

    if (sub_cmd == 0) {
        Serial.println(F("[UpdateManager] GREŠKA: Nije moguće odrediti sub_cmd za sliku."));
        CleanupSession(true);
        return; // Vraća void
    }
    m_last_sent_sub_cmd = sub_cmd; // Sačuvaj poslanu komandu za provjeru odgovora
    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8); // NOLINT
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    packet[6] = sub_cmd;

    // PROMJENA: Koristi dinamički chunk size za update umjesto hardkodirane konstante
    uint16_t chunk_size = GetChunkSizeForProtocol(); // 64 (stari) ili 128 (novi)
    uint16_t total_packets = (s->fw_size + chunk_size - 1) / chunk_size;
    packet[7] = (total_packets >> 8) & 0xFF;
    packet[8] = total_packets & 0xFF;

    packet[9] = (s->fw_size >> 24);
    packet[10] = (s->fw_size >> 16);
    packet[11] = (s->fw_size >> 8);
    packet[12] = (s->fw_size & 0xFF);

    packet[13] = (s->fw_crc >> 24);
    packet[14] = (s->fw_crc >> 16);
    packet[15] = (s->fw_crc >> 8);
    packet[16] = (s->fw_crc & 0xFF);
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[17] = (checksum >> 8);
    packet[18] = (checksum & 0xFF);
    packet[19] = EOT;
    
    // --- CILJANA DIJAGNOSTIKA ZA START PAKET ---
    // Ispisuje se uvijek, bez obzira na DEBUG_LEVEL, kako bismo uhvatili grešku.
    Serial.println(F("--- START Paket za Sliku (Dijagnostika) ---"));
    Serial.printf("  -> Ciljna Adresa:  0x%02X (%d)\n", s->clientAddress, s->clientAddress);
    Serial.printf("  -> Izvorna Adresa:  0x%04X\n", rsifa);
    Serial.printf("  -> Komanda (SubCMD): 0x%02X\n", sub_cmd);
    Serial.printf("  -> Ukupno Paketa:   %u\n", total_packets);
    Serial.printf("  -> Veličina Fajla:  %lu\n", s->fw_size);
    Serial.printf("  -> CRC32 Fajla:     0x%08lX\n", s->fw_crc);
    char packet_str[61] = {0}; // 20 bajtova * 3 znaka (npr. "XX ")
    for (int i = 0; i < 20; i++) {
        sprintf(packet_str + strlen(packet_str), "%02X ", packet[i]);
    }
    Serial.printf("  -> RAW Paket (20B): [ %s]\n", packet_str);
    Serial.println(F("---------------------------------------------"));
    // --- KRAJ DIJAGNOSTIKE ---
    // ISPRAVKA: Dužina paketa je 20, a ne 21
    if (m_rs485_service->SendPacket(packet, 20)) {
        Serial.printf("  -> Paket poslan u %lu ms.\n", millis()); // DIJAGNOSTIKA VREMENA
        s->state = S_WAITING_FOR_START_ACK;
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
    // PROMJENA: Koristi dinamički chunk size za update umjesto hardkodirane konstante
    uint16_t chunk_size = GetChunkSizeForProtocol(); // 64 (stari) ili 128 (novi)
    int16_t bytes_read = s->fw_file.read(s->read_buffer, chunk_size);
    
    if (bytes_read <= 0)
    {
        Serial.println(F("[UpdateManager] Čitanje sa kartice završeno. Prelazim na FINISH."));
        s->state = S_FINISHING; 
        return;
    }
    
    s->read_chunk_size = (uint16_t)bytes_read;
    
    uint8_t packet[MAX_PACKET_LENGTH]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = s->read_chunk_size + 2; // Payload je [SeqNum_H, SeqNum_L, ...data]
    uint16_t total_packet_length = 9 + data_len;

    packet[0] = STX; 
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len; 
    
    packet[6] = (s->currentSequenceNum >> 8);
    packet[7] = (s->currentSequenceNum & 0xFF);
    
    memcpy(&packet[8], s->read_buffer, s->read_chunk_size);
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[total_packet_length - 3] = (checksum >> 8);
    packet[total_packet_length - 2] = (checksum & 0xFF);
    packet[total_packet_length - 1] = EOT;
    
    if (m_rs485_service->SendPacket(packet, total_packet_length)) {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_DATA_ACK;
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
    
    packet[6] = m_last_sent_sub_cmd;
    
    uint16_t checksum = 0;
    for (uint32_t i = 6; i < (6 + data_len); i++) checksum += packet[i];

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10)) {
        s->timeoutStart = millis();
        s->state = S_WAITING_FOR_FINISH_ACK;
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
    
    if (m_rs485_service->SendPacket(packet, 10)) {
        s->state = S_PENDING_APP_START; // Ne čekamo ACK, samo prelazimo u stanje pauze
    }
    else
    {
        CleanupSession(true);
    }
}

/**
 * @brief Šalje APP_EXE komandu nakon pauze.
 */
void UpdateManager::SendAppExeCommand()
{
    UpdateSession* s = &m_session;
    Serial.println(F("[UpdateManager] Pauza završena. Slanje APP_EXE komande..."));
    
    uint8_t packet[32]; 
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t data_len = 1;

    packet[0] = SOH;
    packet[1] = (s->clientAddress >> 8);
    packet[2] = (s->clientAddress & 0xFF);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFF);
    packet[5] = data_len;
    
    packet[6] = CMD_APP_EXE;
    
    uint16_t checksum = CMD_APP_EXE;

    packet[7] = (checksum >> 8);
    packet[8] = (checksum & 0xFF);
    packet[9] = EOT;
    
    if (m_rs485_service->SendPacket(packet, 10))
    {
        CleanupSession(false); // Završavamo sesiju, ne čekamo odgovor
    }
    else
    {
        CleanupSession(true);
    }
}

void UpdateManager::CleanupSession(bool failed /*= false*/)
{
    // =================================================================================
    // KRITIČNO: Deaktiviraj single-byte mod nakon završetka transfera
    // Ovo vraća Rs485Service u normalno stanje za LogPullManager i ostale funkcije
    // =================================================================================
    m_rs485_service->DisableSingleByteMode();
    
    // POKRENI SERVER - ALI SAMO AKO NEMA AKTIVNE SEKVENCE!
    // Ako je sekvenca aktivna, server ostaje zaustavljen do kraja cijele sekvence
    if (m_http_server && !m_sequence.is_active) {
        m_http_server->Start();
    }
    
    if (m_session.is_read_active && m_session.fw_file)
    {
        m_session.fw_file.close();
        m_session.is_read_active = false;
    }
    
    // Reset TimeSync tajmer
    extern TimeSync g_timeSync;
    g_timeSync.ResetTimer();
    
    // Samo loguj status
    if (m_sequence.is_active) {
        if (failed) {
            Serial.printf("[UpdateManager] Sesija NEUSPJEŠNA za Adresu %d, Slika %d.\n", m_sequence.current_addr, m_sequence.current_img);
            StopSequence(); // Prekini sekvencu I pokreni server
            m_first_image_in_sequence = true;
        } else {
            Serial.printf("[UpdateManager] Sesija USPJEŠNA za Adresu %d, Slika %d.\n", m_sequence.current_addr, m_sequence.current_img);
        }
    } else {
        if (failed) {
            Serial.println("[UpdateManager] Sesija (pojedinačna) NEUSPJEŠNA.");
        } else {
            Serial.println("[UpdateManager] Sesija (pojedinačna) USPJEŠNA.");
        }
    }
    
    m_session.state = S_IDLE;
    m_session_in_progress = false; // KLJUČNO: Resetuj zastavicu da omogućiš sljedeću sliku
}