/**
 ******************************************************************************
 * @file    EepromStorage.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija EepromStorage modula.
 ******************************************************************************
 */

#include "EepromStorage.h"
#include "DebugConfig.h"
#include "HttpResponseStrings.h" // NOVO: Uključujemo centralizovane stringove

// Konstante za EEPROM
#define EEPROM_PAGE_SIZE 256 // ISPRAVKA: Prema AT24C1024 datasheet-u, veličina stranice je 256 bajtova.
#define EEPROM_WRITE_DELAY 5 

// Globalni objekat za konfiguraciju
AppConfig g_appConfig; 

EepromStorage::EepromStorage() :
    m_log_write_index(0),
    m_log_read_index(0),
    m_log_count(0)
{
    // Konstruktor
}

// ============================================================================
// --- DODANA FUNKCIJA ZA UČITAVANJE DEFAULTNIH VRIJEDNOSTI ---
// ============================================================================
void EepromStorage::LoadDefaultConfig()
{
    LOG_DEBUG(2, "[Eeprom] UPOZORENJE: EEPROM je prazan ili neispravan. Učitavam podrazumijevane vrijednosti...\n");

    // 1. Mrežne postavke
    g_appConfig.ip_address = IPAddress(DEFAULT_IP_ADDR0, DEFAULT_IP_ADDR1, DEFAULT_IP_ADDR2, DEFAULT_IP_ADDR3);
    g_appConfig.subnet_mask = IPAddress(DEFAULT_SUBNET_ADDR0, DEFAULT_SUBNET_ADDR1, DEFAULT_SUBNET_ADDR2, DEFAULT_SUBNET_ADDR3);
    g_appConfig.gateway = IPAddress(DEFAULT_GW_ADDR0, DEFAULT_GW_ADDR1, DEFAULT_GW_ADDR2, DEFAULT_GW_ADDR3);

    // 2. RS485 Adrese
    g_appConfig.rs485_iface_addr = DEFAULT_RS485_IFACE_ADDR;
    g_appConfig.rs485_group_addr = DEFAULT_RS485_GROUP_ADDR;
    g_appConfig.rs485_bcast_addr = DEFAULT_RS485_BCAST_ADDR;

    // 3. Sistem
    g_appConfig.system_id = DEFAULT_SYSTEM_ID;
    
    // 4. mDNS Ime (kopiranje stringa)
    memset(g_appConfig.mdns_name, 0, sizeof(g_appConfig.mdns_name));
    strncpy(g_appConfig.mdns_name, DEFAULT_MDNS_NAME, sizeof(g_appConfig.mdns_name) - 1);

    // 5. NOVO: Postavi podrazumijevani protokol
    g_appConfig.main_protocol = ProtocolVersion::BJELASNICA; // Npr. Bjelašnica kao default

    // 6. NOVO: Inicijalizuj dodatne TimeSync pakete kao neaktivne
    for (int i = 0; i < 3; ++i)
    {
        g_appConfig.additional_syncs[i].enabled = false;
        g_appConfig.additional_syncs[i].broadcast_addr = 0;
        g_appConfig.additional_syncs[i].protocol = ProtocolVersion::BJELASNICA;
    }

    // 5. Snimi nove (defaultne) vrijednosti u EEPROM
    if (WriteConfig(&g_appConfig))
    {
        LOG_DEBUG(3, "[Eeprom] Podrazumijevane vrijednosti uspješno snimljene u EEPROM.\n");
    }
    else
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Snimanje podrazumijevanih vrijednosti nije uspjelo!\n");
    }
}

void EepromStorage::Initialize(int8_t sda_pin, int8_t scl_pin)
{
    LOG_DEBUG(5, "[Eeprom] Entering Initialize()...\n");
    LOG_DEBUG(3, "[Eeprom] Inicijalizacija I2C na SDA=%d, SCL=%d\n", sda_pin, scl_pin);
    Wire.begin(sda_pin, scl_pin);
    
    // Učitaj globalnu konfiguraciju
    if (!ReadConfig(&g_appConfig))
    {
        // Greška pri čitanju I2C (npr. hardverski kvar)
        LOG_DEBUG(1, "[Eeprom] GRESKA: Nije moguce pročitati I2C EEPROM.\n");
        LoadDefaultConfig(); // Učitaj i snimi defaultne
    }
    else
    {
        // Provjeri da li je EEPROM prazan (0xFFFF) ili obrisan (0x0000)
        // Koristimo rs485_iface_addr kao "magic number" za provjeru
        if (g_appConfig.rs485_iface_addr == 0xFFFF || g_appConfig.rs485_iface_addr == 0x0000)
        {
            LoadDefaultConfig(); // Učitaj i snimi defaultne
        }
        else
        {
            LOG_DEBUG(3, "[Eeprom] Konfiguracija ucitana. RS485 Adresa: 0x%X\n", g_appConfig.rs485_iface_addr);
        }
    }

    LOG_DEBUG(3, "[Eeprom] Inicijalizacija Logera...\n");
    LoggerInit();
    LOG_DEBUG(5, "[Eeprom] Exiting Initialize().\n");
}

//=============================================================================
// API za Konfiguraciju
//=============================================================================

bool EepromStorage::ReadConfig(AppConfig* config)
{
    return ReadBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

bool EepromStorage::WriteConfig(const AppConfig* config)
{
    return WriteBytes(EEPROM_CONFIG_START_ADDR, (uint8_t*)config, sizeof(AppConfig));
}

//=============================================================================
// I2C Drajver - Implementacija Page Write logike (24C1024)
//=============================================================================

bool EepromStorage::WriteBytes(uint16_t address, const uint8_t* data, uint16_t length)
{
    LOG_DEBUG(5, "[Eeprom] Entering WriteBytes(addr=0x%04X, len=%u)...\n", address, length);
    uint16_t current_addr = address;
    uint16_t bytes_remaining = length;
    uint16_t data_offset = 0;

    while (bytes_remaining > 0)
    {
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t bytes_to_end_of_page = EEPROM_PAGE_SIZE - page_offset;
        uint16_t chunk_size = min(bytes_remaining, bytes_to_end_of_page);

        // KONAČNA ISPRAVKA: Ograniči chunk_size na maksimalnu veličinu Wire bafera (128 bajtova).
        // Iako EEPROM podržava stranice od 256B, Wire.write() ne može poslati više od 128B odjednom.
        // Ovo sprečava tiho odbacivanje podataka i rješava "roll-over" problem.
        chunk_size = min(chunk_size, (uint16_t)128);
        
        LOG_DEBUG(4, "[Eeprom] -> Pisanje chunk-a: addr=0x%04X, size=%u\n", current_addr, chunk_size);
        Wire.beginTransmission(EEPROM_I2C_ADDR);
        Wire.write((uint8_t)(current_addr >> 8));   
        Wire.write((uint8_t)(current_addr & 0xFF)); 
        
        Wire.write(data + data_offset, chunk_size);
        
        if (Wire.endTransmission() != 0)
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA: I2C endTransmission nije uspio.\n");
            return false;
        }
        
        // IMPLEMENTACIJA "ACKNOWLEDGE POLLING" PREMA DATASHEET-u
        // Nakon što je poslat STOP bit (sa Wire.endTransmission()), EEPROM započinje interni
        // ciklus pisanja. Tokom ovog ciklusa, on ne odgovara (ne šalje ACK) na svoju I2C adresu.
        // Petlja ispod konstantno "ping-uje" EEPROM slanjem njegove adrese.
        // Čim EEPROM odgovori sa ACK (endTransmission vrati 0), znamo da je spreman za sljedeću komandu.
        unsigned long ack_poll_start = millis();
        while (true)
        {
            Wire.beginTransmission(EEPROM_I2C_ADDR);
            if (Wire.endTransmission() == 0) {
                break; // Uspjeh! EEPROM je odgovorio sa ACK, spreman je.
            }
            if (millis() - ack_poll_start > 15) { // Sigurnosni timeout od 15ms
                LOG_DEBUG(1, "[Eeprom] GRESKA: ACK Polling timeout. EEPROM ne odgovara.\n");
                return false;
            }
            // Ne treba delay, petlja se vrti maksimalnom brzinom dok čeka odgovor.
        }

        current_addr += chunk_size;
        data_offset += chunk_size;
        bytes_remaining -= chunk_size;
    }
    
    LOG_DEBUG(5, "[Eeprom] Exiting WriteBytes()... OK\n");
    return true;
}

bool EepromStorage::ReadBytes(uint16_t address, uint8_t* data, uint16_t length)
{
    LOG_DEBUG(5, "[Eeprom] Entering CHUNKED ReadBytes(addr=0x%04X, len=%u)...\n", address, length);
    
    uint16_t bytes_remaining = length;
    uint16_t current_addr = address;
    uint16_t data_offset = 0;

    while (bytes_remaining > 0)
    {
        // Definišemo veličinu "komada" za čitanje, npr. 128 bajtova, što je sigurno ispod limita Wire biblioteke.
        uint16_t chunk_size = min((uint16_t)bytes_remaining, (uint16_t)128);
        LOG_DEBUG(4, "[Eeprom] -> Čitanje chunk-a: addr=0x%04X, size=%u\n", current_addr, chunk_size);

        // 1. Postavi adresu sa koje se čita
        Wire.beginTransmission(EEPROM_I2C_ADDR);
        Wire.write((uint8_t)(current_addr >> 8));
        Wire.write((uint8_t)(current_addr & 0xFF));
        
        // endTransmission(false) šalje REPEATED START, što je ključno za čitanje.
        if (Wire.endTransmission(false) != 0)
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA: I2C endTransmission (za čitanje) nije uspio.\n");
            return false;
        }

        // 2. Zatraži i pročitaj "komad" podataka
        if (Wire.requestFrom((uint8_t)EEPROM_I2C_ADDR, (size_t)chunk_size) != chunk_size)
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA: I2C requestFrom nije vratio očekivani broj bajtova za chunk.\n");
            return false;
        }

        for (uint16_t i = 0; i < chunk_size; i++)
        {
            data[data_offset + i] = Wire.read();
        }

        // Ažuriraj pokazivače za sljedeću iteraciju
        bytes_remaining -= chunk_size;
        current_addr += chunk_size;
        data_offset += chunk_size;
    }
    
    LOG_DEBUG(5, "[Eeprom] Exiting ReadBytes()... OK\n");
    return true;
}

//=============================================================================
// API za Logger (Rjesava greske u Loger funkcijama)
//=============================================================================

void EepromStorage::LoggerInit()
{
    LOG_DEBUG(3, "[Eeprom] Započeto skeniranje EEPROM-a za logove...\n");

    uint8_t status_byte;
    uint16_t first_valid_index = 0xFFFF;
    uint16_t last_valid_index = 0xFFFF;
    uint16_t valid_count = 0;
    bool first_valid_found = false;

    // 1. Pronađi prvi i zadnji validan log i prebroj ih
    for (uint16_t i = 0; i < MAX_LOG_ENTRIES; ++i)
    {
        uint16_t addr = EEPROM_LOG_START_ADDR + (i * LOG_RECORD_SIZE);
        if (ReadBytes(addr, &status_byte, 1) && status_byte == STATUS_BYTE_VALID)
        {
            if (!first_valid_found)
            {
                first_valid_index = i;
                first_valid_found = true;
            }
            last_valid_index = i;
            valid_count++;
        }
    }

    // 2. Analiziraj rezultate i postavi pokazivače
    if (valid_count == 0)
    {
        // Slučaj 1: Logger je potpuno prazan
        LOG_DEBUG(3, "[Eeprom] Skeniranje završeno. Logger je prazan.\n");
        m_log_write_index = 0; // head
        m_log_read_index = 0;  // tail
        m_log_count = 0;
    }
    else if (valid_count == MAX_LOG_ENTRIES)
    {
        // Slučaj 2: Logger je potpuno pun.
        // Proizvoljno postavljamo da je najstariji na indeksu 0, a sljedeći za upis će ga prepisati.
        LOG_DEBUG(3, "[Eeprom] Skeniranje završeno. Logger je pun.\n");
        m_log_read_index = 0; // tail
        m_log_write_index = 0; // head
        m_log_count = MAX_LOG_ENTRIES;
    }
    else
    {
        // Slučaj 3: Djelimično popunjen logger. Moramo provjeriti da li je "obmotan".
        bool is_wrapped = (first_valid_index > last_valid_index) || 
                          ( (last_valid_index == MAX_LOG_ENTRIES - 1) && (first_valid_index > 0) );

        // Tražimo prvu "rupu" (prazan slot) nakon zadnjeg validnog loga.
        uint16_t next_free_index = (last_valid_index + 1) % MAX_LOG_ENTRIES;
        uint16_t check_addr = EEPROM_LOG_START_ADDR + (next_free_index * LOG_RECORD_SIZE);
        ReadBytes(check_addr, &status_byte, 1);

        if (status_byte != STATUS_BYTE_VALID && !is_wrapped)
        {
            // Normalan, neobmotan slučaj. Logovi su u kontinuitetu.
            m_log_read_index = first_valid_index; // tail
            m_log_write_index = next_free_index;  // head
        }
        else
        {
            // Obmotan slučaj. "Rupa" se nalazi između last_valid_index i first_valid_index.
            // Najstariji log (tail) je onaj koji slijedi nakon rupe.
            m_log_write_index = (last_valid_index + 1) % MAX_LOG_ENTRIES; // head je nakon zadnjeg
            m_log_read_index = m_log_write_index; // tail je na istom mjestu kao head u punom baferu, ali ovdje je to prvi validni nakon rupe
        }
        m_log_count = valid_count;
        LOG_DEBUG(3, "[Eeprom] Skeniranje završeno. Pronađeno %u logova.\n", m_log_count);
        LOG_DEBUG(4, "[Eeprom] -> Read Index (tail): %u\n", m_log_read_index);
        LOG_DEBUG(4, "[Eeprom] -> Write Index (head): %u\n", m_log_write_index);
    }
}

LoggerStatus EepromStorage::WriteLog(const LogEntry* entry)
{
    // Adresa na koju upisujemo novi log (head), uzimajući u obzir veličinu zapisa
    uint16_t write_addr = EEPROM_LOG_START_ADDR + (m_log_write_index * LOG_RECORD_SIZE);

    // ISPRAVKA: Upisujemo statusni bajt i podatke odvojeno da izbjegnemo overflow.
    // Prvo upisujemo statusni bajt na početak rekorda.
    uint8_t status_byte = STATUS_BYTE_VALID;
    if (!WriteBytes(write_addr, &status_byte, 1))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje status bajta za log nije uspjelo.\n");
        return LoggerStatus::LOGGER_ERROR;
    }

    // Zatim upisujemo sam LogEntry (16 bajtova) odmah nakon statusnog bajta.
    if (!WriteBytes(write_addr + 1, (const uint8_t*)entry, LOG_ENTRY_SIZE))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje podataka loga nije uspjelo.\n");
        return LoggerStatus::LOGGER_ERROR;
    }

    // Pomjeramo head (indeks za pisanje) na sljedeću poziciju
    m_log_write_index = (m_log_write_index + 1) % MAX_LOG_ENTRIES;

    // Ako je bafer pun, tail (indeks čitanja) također mora pratiti head.
    if (m_log_count >= MAX_LOG_ENTRIES)
    {
        m_log_read_index = (m_log_read_index + 1) % MAX_LOG_ENTRIES;
        LOG_DEBUG(4, "[Eeprom] Bafer je pun, prepisan je najstariji log.\n");
    }
    else
    {
        // Ako bafer nije pun, samo povećavamo brojač
        m_log_count++;
    }

    LOG_DEBUG(3, "[Eeprom] Log uspješno zapisan. Ukupno logova: %u. Head: %u, Tail: %u\n", m_log_count, m_log_write_index, m_log_read_index);
    return LoggerStatus::LOGGER_OK;
}

LoggerStatus EepromStorage::GetOldestLog(LogEntry* entry)
{
    LOG_DEBUG(5, "[Eeprom] Entering GetOldestLog()...\n");
    if (m_log_count == 0)
    {
        return LoggerStatus::LOGGER_EMPTY;
    }
    
    // Citamo 1 bajt (status) + LOG_ENTRY_SIZE bajtova podataka
    uint8_t read_buffer[LOG_RECORD_SIZE];

    uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE); 
    
    if (!ReadBytes(read_addr, read_buffer, LOG_RECORD_SIZE))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Čitanje loga sa adrese 0x%04X nije uspjelo.\n", read_addr);
        return LoggerStatus::LOGGER_ERROR;
    }

    // Status Byte mora biti VALID
    if (read_buffer[0] != STATUS_BYTE_VALID)
    {
        LOG_DEBUG(2, "[Eeprom] UPOZORENJE: Status bajt za log na adresi 0x%04X nije validan (0x%02X).\n", read_addr, read_buffer[0]);
        return LoggerStatus::LOGGER_ERROR;
    }

    // Kopiraj log (podaci pocinju od read_buffer[1])
    // ISPRAVKA: Ograničavamo kopiranje na veličinu LogEntry strukture, ali ne više od
    // dostupnih podataka u baferu (LOG_RECORD_SIZE - 1).
    size_t entry_copy_size = min(sizeof(LogEntry), (size_t)(LOG_RECORD_SIZE - 1));
    memcpy((uint8_t*)entry, &read_buffer[1], entry_copy_size);
    if (sizeof(LogEntry) > entry_copy_size)
    {
        // Zero remaining bytes in structure if any
        memset(((uint8_t*)entry) + entry_copy_size, 0, sizeof(LogEntry) - entry_copy_size);
    }

    LOG_DEBUG(4, "[Eeprom] Uspješno pročitan najstariji log sa adrese 0x%04X.\n", read_addr);
    return LoggerStatus::LOGGER_OK;
}

// ============================================================================
// --- NOVA FUNKCIJA ZA KOMPATIBILNOST SA STARIM SISTEMOM ---
// ============================================================================
String EepromStorage::ReadLogBlockAsHexString()
{
    LOG_DEBUG(3, "[Eeprom] Čitanje bloka logova kao HEX string (V2 - Kompatibilno)...\n");
    LOG_DEBUG(3, "[Eeprom] -> Trenutni log count: %u, read_index: %u\n", m_log_count, m_log_read_index);

    if (m_log_count == 0)
    {
        LOG_DEBUG(3, "[Eeprom] Nema logova, vraćam 'EMPTY'.\n");
        return HTTP_RESPONSE_EMPTY;
    }

    // ========================================================================
    // --- ISPRAVKA: Replikacija logike starog sistema (fiksni blok od 256B) ---
    // Uvijek se čita blok od 256 bajtova. Ako nema dovoljno logova,
    // ostatak bafera se popunjava nulama (zero-fill).
    // ========================================================================
    const uint16_t BLOCK_SIZE = 256; // Fiksna veličina bloka kao na starom sistemu
    uint8_t data_buffer[BLOCK_SIZE];

    // 2. Izračunaj koliko logova treba pročitati
    uint16_t logs_in_block = BLOCK_SIZE / LOG_RECORD_SIZE; // 256 / 16 = 16
    uint16_t logs_to_read = min((uint16_t)m_log_count, logs_in_block);
    uint16_t total_bytes_to_read = logs_to_read * LOG_RECORD_SIZE;

    LOG_DEBUG(3, "[Eeprom] -> Čitam %u logova (%u bajtova).\n", logs_to_read, total_bytes_to_read);

    // 3. Pročitaj validne logove u bafer
    for (uint16_t i = 0; i < logs_to_read; ++i)
    {
        // Izračunaj indeks i adresu trenutnog loga u kružnom baferu
        uint16_t current_log_index = (m_log_read_index + i) % MAX_LOG_ENTRIES;
        uint16_t read_addr = EEPROM_LOG_START_ADDR + (current_log_index * LOG_RECORD_SIZE);
        
        // Adresa u odredišnom baferu
        uint8_t* dest_buffer = data_buffer + (i * LOG_ENTRY_SIZE); // Pišemo samo 16B podataka

        // Pročitaj kompletan zapis (17B), ali preskoči statusni bajt i kopiraj samo podatke (16B)
        if (!ReadBytes(read_addr + 1, dest_buffer, LOG_ENTRY_SIZE))
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA: Čitanje loga na indeksu %u nije uspjelo.\n", current_log_index);
            return HTTP_RESPONSE_ERROR;
        }
    }

    // 4. Popuni ostatak bafera nulama (zero-fill)
    if (total_bytes_to_read < BLOCK_SIZE)
    {
        memset(data_buffer + total_bytes_to_read, 0, BLOCK_SIZE - total_bytes_to_read);
        LOG_DEBUG(4, "[Eeprom] -> Popunjeno %u bajtova nulama.\n", BLOCK_SIZE - total_bytes_to_read);
    }

    // 4. Replikacija `Hex2Str` funkcije
    String hex_string = "";
    hex_string.reserve(BLOCK_SIZE * 2); // Uvijek alociraj za 512 karaktera
    for (uint16_t i = 0; i < BLOCK_SIZE; i++)
    {
        char hex_buf[3];
        sprintf(hex_buf, "%02X", data_buffer[i]);
        hex_string += hex_buf;
    }

    LOG_DEBUG(3, "[Eeprom] Vraćen HEX string dužine 512.\n");
    return hex_string;
}

LoggerStatus EepromStorage::DeleteLogBlock()
{
    if (m_log_count == 0)
    {
        LOG_DEBUG(3, "[Eeprom] Nema logova za brisanje.\n");
        return LoggerStatus::LOGGER_EMPTY;
    }

    // ISPRAVKA: Uklonjena konstanta. Dinamičko izračunavanje broja logova za brisanje,
    // identično logici za čitanje (ReadLogBlockAsHexString).
    const uint16_t max_bytes_to_process = 256; // Ekvivalent starom I2CEE_BLOCK
    uint16_t logs_in_block = max_bytes_to_process / LOG_RECORD_SIZE; // 256 / 16 = 16 logova
    uint16_t logs_to_delete = min((uint16_t)m_log_count, (uint16_t)(max_bytes_to_process / LOG_ENTRY_SIZE));

    LOG_DEBUG(3, "[Eeprom] Brisanje bloka od %u logova...\n", logs_to_delete);
    
    uint8_t zero_buffer[LOG_RECORD_SIZE];
    memset(zero_buffer, 0, LOG_RECORD_SIZE);

    for (uint16_t i = 0; i < logs_to_delete; i++)
    {
        uint16_t current_log_index = (m_log_read_index + i) % MAX_LOG_ENTRIES;
        uint16_t delete_addr = EEPROM_LOG_START_ADDR + (current_log_index * LOG_RECORD_SIZE);
        if (!WriteBytes(delete_addr, zero_buffer, LOG_RECORD_SIZE))
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA: Brisanje loga na indeksu %u nije uspjelo.\n", current_log_index);
            return LoggerStatus::LOGGER_ERROR;
        }
    }

    m_log_read_index = (m_log_read_index + logs_to_delete) % MAX_LOG_ENTRIES;
    m_log_count -= logs_to_delete;

    LOG_DEBUG(3, "[Eeprom] Blok od %u logova obrisan. Preostalo logova: %u\n", logs_to_delete, m_log_count);
    return LoggerStatus::LOGGER_OK;
}

// Implementacija WriteAddressList
bool EepromStorage::WriteAddressList(const uint16_t* listBuffer, uint16_t count)
{
    // Vraćamo na originalnu, efikasnu metodu pisanja
    uint16_t addresses_to_write = min(count, (uint16_t)MAX_ADDRESS_LIST_SIZE);
    uint16_t bytes_to_write = addresses_to_write * sizeof(uint16_t);

    LOG_DEBUG(3, "[Eeprom] Započinjem upis %u validnih adresa...\n", addresses_to_write);
    if (!WriteBytes(EEPROM_ADDRESS_LIST_START_ADDR, (const uint8_t*)listBuffer, bytes_to_write))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje liste adresa neuspješno.\n");
        return false;
    }
    
    // Eksplicitno nuliramo ostatak memorije za listu.
    uint16_t remaining_bytes = EEPROM_ADDRESS_LIST_SIZE - bytes_to_write;
    if (remaining_bytes > 0)
    {
        LOG_DEBUG(3, "[Eeprom] Čistim ostatak (%u B) memorije za listu adresa...\n", remaining_bytes);
        uint8_t zero_buffer[16] = {0}; // Bafer sa nulama
        
        // Upisujemo nule u ostatak prostora, u komadima
        for (uint16_t offset = 0; offset < remaining_bytes; offset += sizeof(zero_buffer)) {
            uint16_t clear_address = EEPROM_ADDRESS_LIST_START_ADDR + bytes_to_write + offset;
            uint16_t chunk_to_clear = min((uint16_t)sizeof(zero_buffer), (uint16_t)(remaining_bytes - offset));
            if (!WriteBytes(clear_address, zero_buffer, chunk_to_clear)) {
                 LOG_DEBUG(1, "[Eeprom] GRESKA: Čišćenje ostatka liste adresa neuspješno.\n");
                 return false;
            }
        }
    }

    LOG_DEBUG(3, "[Eeprom] Upis i čišćenje liste adresa uspješno završeno.\n");
    return true;
}

bool EepromStorage::ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    uint16_t bytes_to_read = min((uint16_t)EEPROM_ADDRESS_LIST_SIZE, (uint16_t)(maxCount * sizeof(uint16_t)));
    
    if (ReadBytes(EEPROM_ADDRESS_LIST_START_ADDR, (uint8_t*)listBuffer, bytes_to_read))
    {
        // ISPRAVKA: Nakon čitanja bloka, prebroj validne (ne-nula) adrese.
        uint16_t valid_count = 0;
        for (uint16_t i = 0; i < (bytes_to_read / sizeof(uint16_t)); i++)
        {
            if (listBuffer[i] == 0)
            {
                // Stani kod prve nule, to je kraj liste.
                break;
            }
            valid_count++;
        }
        *actualCount = valid_count;

        LOG_DEBUG(3, "[Eeprom] Uspješno pročitan blok od %u B. Pronađeno %u validnih adresa.\n", bytes_to_read, *actualCount);
        return true;
    }
    *actualCount = 0;
    return false;
}

// ============================================================================
// --- ISPRAVKA: DODATA FUNKCIJA KOJA NEDOSTAJE ---
// ============================================================================
LoggerStatus EepromStorage::ClearAllLogs()
{
    LOG_DEBUG(3, "[Eeprom] Brisanje svih logova (punjenje nulama)...\n");

    // Pripremi buffer sa 0x00 (prazan log slot)
    uint8_t empty_buffer[EEPROM_PAGE_SIZE];
    memset(empty_buffer, 0, EEPROM_PAGE_SIZE);

    uint32_t bytes_to_clear = EEPROM_LOG_AREA_SIZE;
    uint16_t current_address = EEPROM_LOG_START_ADDR;

    while (bytes_to_clear > 0)
    {
        // Odredi koliko pisati u ovom ciklusu
        uint16_t chunk_size = min((uint32_t)bytes_to_clear, (uint32_t)sizeof(empty_buffer));
        
        if (!WriteBytes(current_address, empty_buffer, chunk_size))
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA pri brisanju logova na adresi 0x%04X.\n", current_address);
            return LoggerStatus::LOGGER_ERROR;
        }
        
        bytes_to_clear -= chunk_size;
        current_address += chunk_size;
    }

    // Resetuj head/tail pokazivače
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;

    LOG_DEBUG(3, "[Eeprom] Svi logovi obrisani.\n");
    return LoggerStatus::LOGGER_OK;
}