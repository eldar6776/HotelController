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
#include <esp_task_wdt.h> // Za watchdog reset tokom dugih operacija

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
// --- MIGRACIJA KONFIGURACIJE ---
// ============================================================================
/**
 * SVRHA VERZIONIRANJA:
 * --------------------
 * Verzija omogućava dodavanje NOVIH POLJA u strukturu bez gubitka korisničkih
 * podataka. Kada korisnik ima staru verziju firmwarea na uređaju, a uploaduje
 * novu verziju sa dodatnim poljima, migracija:
 *   - ČUVA sve postojeće podatke (IP, RS485, username, password...)
 *   - INICIJALIZUJE nova polja na default vrijednosti
 * 
 * KAKO RADI:
 * -----------
 * 1. Firmware učita strukturu iz EEPROM-a (može biti stara verzija, manja)
 * 2. Detektuje da je oldVersion < currentVersion
 * 3. Pozove MigrateConfig() koja:
 *    - NE GLEDA KONKRETNE BROJEVE verzija (npr. "if oldVersion < 3")
 *    - PROVJERAVA DA LI JE POLJE PRAZNO (0x00, 0xFF, prazan string...)
 *    - Ako JESTE prazan -> postavi default
 *    - Ako NIJE prazan -> OSTAVI (korisnik je konfigurirao)
 * 
 * PRIMJER UPOTREBE:
 * -----------------
 * Trenutna verzija: V2 (ima: ip, username, password)
 * 
 * SCENARIO 1: Dodajemo V3 sa novim poljem "language"
 * ---------------------------------------------------
 * 1. U EepromStorage.h dodaj u strukturu:
 *    char language[8];  // NOVO POLJE!
 * 
 * 2. U ProjectConfig.h promeni:
 *    #define EEPROM_CONFIG_VERSION 3
 * 
 * 3. U ovoj funkciji (MigrateConfig) dodaj:
 *    if (strlen(g_appConfig.language) == 0)
 *    {
 *        strncpy(g_appConfig.language, "en", sizeof(g_appConfig.language) - 1);
 *    }
 * 
 * REZULTAT:
 *   - Stari uređaj V2: [IP=192.168.1.10][user=admin][pass=mypass][language=0x00]
 *   - Nakon migracije: [IP=192.168.1.10][user=admin][pass=mypass][language="en"]
 *   - IP/user/pass OSTAJU (korisnik ih postavio)
 *   - language DOBIJA default "en" (novo polje)
 * 
 * SCENARIO 2: Dodajemo V4 sa brojčanim poljem "timeout"
 * ------------------------------------------------------
 * 1. U strukturu dodaj: uint16_t timeout;
 * 2. Promeni verziju na 4
 * 3. U MigrateConfig dodaj:
 *    if (g_appConfig.timeout == 0 || g_appConfig.timeout == 0xFFFF)
 *    {
 *        g_appConfig.timeout = 30;
 *    }
 * 
 * NAPOMENA ZA NUMERIČKA POLJA:
 * -----------------------------
 * Prazna memorija iz EEPROM-a može biti 0x00 ili 0xFF (zavisi od čipa).
 * Proveri OBA slučaja:
 *   if (polje == 0 || polje == 0xFFFF) { ... }
 * 
 * ZAŠTO NE KORISTIMO "if (oldVersion < 3)"?
 * -----------------------------------------
 * Problem: Ako preskočite verzije (npr. V1 -> V5), hardcoded provjere ne rade:
 *   if (oldVersion < 2) { init_field_v2(); }  // Radi za V1->V2
 *   if (oldVersion < 3) { init_field_v3(); }  // NE RADI za V1->V5 jer 1 < 3!
 * 
 * Rješenje: Provjera praznog polja UVIJEK radi:
 *   if (field == empty) { init_field(); }  // Radi za V1->V2, V1->V5, V3->V5...
 */
void EepromStorage::MigrateConfig(uint16_t oldVersion)
{
    LOG_DEBUG(2, "[Eeprom] Migracija: V%d -> V%d. Proveravam i inicijalizujem prazna polja...\n", oldVersion, EEPROM_CONFIG_VERSION);
    
    // g_appConfig je već učitan iz EEPROM-a sa SVIM postojećim podacima
    
    // ========================================================================
    // PROVJERA I INICIJALIZACIJA POLJA (dodato u V2)
    // ========================================================================
    
    // Proveri DA LI JE web_username prazan (0x00 ili prazan string)
    if (strlen(g_appConfig.web_username) == 0)
    {
        strncpy(g_appConfig.web_username, "admin", sizeof(g_appConfig.web_username) - 1);
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan prazan web_username -> 'admin'\n");
    }
    
    // Proveri DA LI JE web_password prazan
    if (strlen(g_appConfig.web_password) == 0)
    {
        strncpy(g_appConfig.web_password, "admin", sizeof(g_appConfig.web_password) - 1);
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan prazan web_password -> 'admin'\n");
    }
    
    // Proveri DA LI JE logger_enable inicijalizovan (provjeravamo da nije random vrijednost)
    // Ako je true ili false, ostavi kao što jeste; ako je inicijalizovano, ne mijenjaj
    // Ali ako struktura dolazi iz stare verzije, postavi default na true (omogući logger)
    // Budući da je bool, ne možemo provjeriti "prazninu", ali možemo provjeriti da li je verzija stara
    if (oldVersion < EEPROM_CONFIG_VERSION)
    {
        // Nova instalacija ili stara verzija - omogući logger po defaultu
        g_appConfig.logger_enable = true;
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan logger_enable -> true (omogućen)\n");
    }
    
    // Proveri DA LI JE time_sync_interval_min inicijalizovan
    if (g_appConfig.time_sync_interval_min == 0 || g_appConfig.time_sync_interval_min > 250)
    {
        // Postavi default na 1 minutu (60 sekundi)
        g_appConfig.time_sync_interval_min = 1;
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan time_sync_interval_min -> 1 minuta\n");
    }
    
    // ========================================================================
    // NOVO: Dual Bus Mode (dodato u V3)
    // ========================================================================
    
    // Ako je stara verzija (< 3), postavi enable_dual_bus_mode na false (single bus mode)
    // Ovo osigurava backward compatibility - stari uređaji nastavljaju sa single bus mode-om
    if (oldVersion < 3)
    {
        g_appConfig.enable_dual_bus_mode = false;
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan enable_dual_bus_mode -> false (single bus - kompatibilnost)\n");
    }
    
    // ========================================================================
    // NOVO: Mixed Protocol Support (dodato u istoj verziji kao Dual Bus)
    // ========================================================================
    
    // Migracija: Kopiraj staru protocol_version u protocol_version_L i protocol_version_R
    // ako su nove varijable nedefinirane (0 ili 0xFF)
    if (g_appConfig.protocol_version_L == 0 || g_appConfig.protocol_version_L == 0xFF)
    {
        g_appConfig.protocol_version_L = g_appConfig.protocol_version;
        LOG_DEBUG(2, "[Eeprom] Migrirano protocol_version_L <- %d (iz starog protocol_version)\n", g_appConfig.protocol_version);
    }
    
    if (g_appConfig.protocol_version_R == 0 || g_appConfig.protocol_version_R == 0xFF)
    {
        g_appConfig.protocol_version_R = g_appConfig.protocol_version;
        LOG_DEBUG(2, "[Eeprom] Migrirano protocol_version_R <- %d (iz starog protocol_version)\n", g_appConfig.protocol_version);
    }
    
    // ========================================================================
    // NOVO: WiFi Configuration (dodato u V4)
    // ========================================================================
    
    // Postavi default za use_wifi_as_primary (default: false = Ethernet primarni)
    // Ova provera radi jer je bool tipa, ali koristimo verziju kao indikator
    if (oldVersion < EEPROM_CONFIG_VERSION)
    {
        g_appConfig.use_wifi_as_primary = false;
        LOG_DEBUG(2, "[Eeprom] Inicijalizovan use_wifi_as_primary -> false (Ethernet primarni - default)\n");
    }
    
    // ========================================================================
    // TEMPLATE ZA BUDUĆA POLJA - kopiraj i prilagodi:
    // ========================================================================
    
    // --- STRING POLJE ---
    // if (strlen(g_appConfig.NOVO_STRING_POLJE) == 0)
    // {
    //     strncpy(g_appConfig.NOVO_STRING_POLJE, "default_value", sizeof(g_appConfig.NOVO_STRING_POLJE) - 1);
    //     LOG_DEBUG(2, "[Eeprom] Inicijalizovano NOVO_STRING_POLJE -> 'default_value'\n");
    // }
    
    // --- NUMERIČKO POLJE (uint8_t, uint16_t, uint32_t) ---
    // if (g_appConfig.NOVO_NUMBER_POLJE == 0 || g_appConfig.NOVO_NUMBER_POLJE == 0xFFFF)
    // {
    //     g_appConfig.NOVO_NUMBER_POLJE = DEFAULT_VALUE;
    //     LOG_DEBUG(2, "[Eeprom] Inicijalizovano NOVO_NUMBER_POLJE -> %d\n", DEFAULT_VALUE);
    // }
    
    // --- IP ADRESA (uint32_t) ---
    // if (g_appConfig.NOVA_IP == 0 || g_appConfig.NOVA_IP == 0xFFFFFFFF)
    // {
    //     g_appConfig.NOVA_IP = ((uint32_t)192 << 24) | ((uint32_t)168 << 16) | ((uint32_t)1 << 8) | 1;
    //     LOG_DEBUG(2, "[Eeprom] Inicijalizovana NOVA_IP -> 192.168.1.1\n");
    // }
    
    // ========================================================================
    
    // Ažuriraj verziju na trenutnu
    g_appConfig.config_version = EEPROM_CONFIG_VERSION;
    
    // Snimi ažuriranu konfiguraciju nazad u EEPROM
    if (WriteConfig(&g_appConfig))
    {
        LOG_DEBUG(2, "[Eeprom] Migracija uspješna. Verzija: V%d -> V%d\n", oldVersion, EEPROM_CONFIG_VERSION);
    }
    else
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Snimanje migrirane konfiguracije nije uspjelo!\n");
    }
}

// ============================================================================
// --- DODANA FUNKCIJA ZA UČITAVANJE DEFAULTNIH VRIJEDNOSTI ---
// ============================================================================
void EepromStorage::LoadDefaultConfig()
{
    LOG_DEBUG(2, "[Eeprom] UPOZORENJE: EEPROM je prazan ili neispravan. Učitavam podrazumijevane vrijednosti...\n");

    // 0. KRITIČNO: Postavi Magic Number i Config Version
    g_appConfig.magic_number = EEPROM_MAGIC_NUMBER;
    g_appConfig.config_version = EEPROM_CONFIG_VERSION;

    // 1. Mrežne postavke (uint32_t u BIG-ENDIAN formatu: MSB << 24 | ... | LSB)
    // Za IP 192.168.0.199 = 0xC0A800C7
    g_appConfig.ip_address = ((uint32_t)DEFAULT_IP_ADDR0 << 24) | 
                             ((uint32_t)DEFAULT_IP_ADDR1 << 16) | 
                             ((uint32_t)DEFAULT_IP_ADDR2 << 8) | 
                             ((uint32_t)DEFAULT_IP_ADDR3);
    
    g_appConfig.subnet_mask = ((uint32_t)DEFAULT_SUBNET_ADDR0 << 24) | 
                              ((uint32_t)DEFAULT_SUBNET_ADDR1 << 16) | 
                              ((uint32_t)DEFAULT_SUBNET_ADDR2 << 8) | 
                              ((uint32_t)DEFAULT_SUBNET_ADDR3);
    
    g_appConfig.gateway = ((uint32_t)DEFAULT_GW_ADDR0 << 24) | 
                          ((uint32_t)DEFAULT_GW_ADDR1 << 16) | 
                          ((uint32_t)DEFAULT_GW_ADDR2 << 8) | 
                          ((uint32_t)DEFAULT_GW_ADDR3);

    // 2. RS485 Adrese
    g_appConfig.rs485_iface_addr = DEFAULT_RS485_IFACE_ADDR;
    g_appConfig.rs485_group_addr = DEFAULT_RS485_GROUP_ADDR;
    g_appConfig.rs485_bcast_addr = DEFAULT_RS485_BCAST_ADDR;

    // 3. Sistem
    g_appConfig.system_id = DEFAULT_SYSTEM_ID;
    
    // 4. Verzija protokola (default: HILLS)
    g_appConfig.protocol_version = static_cast<uint8_t>(ProtocolVersion::HILLS);
    
    // 5. Dodatni TimeSync paketi (default: svi onemogućeni)
    for (int i = 0; i < 3; i++)
    {
        g_appConfig.additional_sync[i].enabled = 0;
        g_appConfig.additional_sync[i].protocol_version = 0;
        g_appConfig.additional_sync[i].broadcast_addr = 0;
    }
    
    // 6. mDNS Ime (kopiranje stringa)
    memset(g_appConfig.mdns_name, 0, sizeof(g_appConfig.mdns_name));
    strncpy(g_appConfig.mdns_name, DEFAULT_MDNS_NAME, sizeof(g_appConfig.mdns_name) - 1);
    
    // 7. NEW: Web Auth Default (admin/admin)
    memset(g_appConfig.web_username, 0, sizeof(g_appConfig.web_username));
    strncpy(g_appConfig.web_username, "admin", sizeof(g_appConfig.web_username) - 1);
    memset(g_appConfig.web_password, 0, sizeof(g_appConfig.web_password));
    strncpy(g_appConfig.web_password, "admin", sizeof(g_appConfig.web_password) - 1);
    
    // 8. NEW: Logger Control (default: omogućen)
    g_appConfig.logger_enable = true;
    
    // 9. NEW: TimeSync Interval (default: 1 minuta)
    g_appConfig.time_sync_interval_min = 1;
    
    // 10. NEW: Dual Bus Mode (default: onemogućen - single bus mode)
    g_appConfig.enable_dual_bus_mode = false;
    
    // 11. NEW: Mixed Protocol Support (default: oba HILLS)
    g_appConfig.protocol_version_L = static_cast<uint8_t>(ProtocolVersion::HILLS);
    g_appConfig.protocol_version_R = static_cast<uint8_t>(ProtocolVersion::HILLS);
    
    // 12. NEW: WiFi Configuration (default: Ethernet primarni)
    g_appConfig.use_wifi_as_primary = false;

    // 13. Snimi nove (defaultne) vrijednosti u EEPROM
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
        // SAFETY: Osiguraj null-terminaciju stringova prije bilo kakve obrade
        g_appConfig.mdns_name[sizeof(g_appConfig.mdns_name) - 1] = 0;
        g_appConfig.web_username[sizeof(g_appConfig.web_username) - 1] = 0;
        g_appConfig.web_password[sizeof(g_appConfig.web_password) - 1] = 0;

        // Provjeri validnost konfiguracije putem Magic Number-a i Verzije
        bool magic_ok = (g_appConfig.magic_number == EEPROM_MAGIC_NUMBER);
        bool version_ok = (g_appConfig.config_version == EEPROM_CONFIG_VERSION);
        bool username_ok = (strlen(g_appConfig.web_username) > 0);

        // --- DIJAGNOSTIKA: Ispis prvih 16 bajtova da vidimo šta je stvarno unutra ---
        uint8_t* raw_ptr = (uint8_t*)&g_appConfig;
        Serial.print("[Eeprom] RAW HEX DUMP (Prvih 16 bajtova): ");
        for(int i=0; i<16; i++) Serial.printf("%02X ", raw_ptr[i]);
        Serial.println();
        // ---------------------------------------------------------------------------

        // --- DIJAGNOSTIKA PROBLEMA ---
        LOG_DEBUG(1, "[Eeprom] DIAG: Pročitano Magic=0x%08X (Očekivano: 0x%08X)\n", g_appConfig.magic_number, EEPROM_MAGIC_NUMBER);
        LOG_DEBUG(1, "[Eeprom] DIAG: Pročitano Ver=%d (Očekivano: %d)\n", g_appConfig.config_version, EEPROM_CONFIG_VERSION);
        LOG_DEBUG(1, "[Eeprom] DIAG: Pročitano User='%s'\n", g_appConfig.web_username);

        // 1. Provjera za migraciju: Ako je Magic OK, a verzija je MANJA od trenutne -> MIGRIRAJ
        if (magic_ok && g_appConfig.config_version < EEPROM_CONFIG_VERSION)
        {
            LOG_DEBUG(2, "[Eeprom] Detektovana stara verzija (V%d). Pokrećem migraciju...\n", g_appConfig.config_version);
            MigrateConfig(g_appConfig.config_version);
        }
        // 2. Ako nije validna V2 i nije V1 -> RESETUJ NA DEFAULT
        else if (!magic_ok || !version_ok || !username_ok)
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA KORUPCIJE: Magic=%s, Ver=%s, User=%s. Resetujem na default...\n",
                magic_ok ? "OK" : "FAIL", version_ok ? "OK" : "FAIL", username_ok ? "OK" : "FAIL");
                
            LoadDefaultConfig(); // Učitaj i snimi defaultne
        }
        else
        {
            LOG_DEBUG(3, "[Eeprom] Konfiguracija validna (Magic: 0x%08X, Ver: %d).\n", g_appConfig.magic_number, g_appConfig.config_version);
            LOG_DEBUG(3, "[Eeprom] Konfiguracija ucitana:\n");
            LOG_DEBUG(3, "[Eeprom]   === RS485 Konfiguracija ===\n");
            LOG_DEBUG(3, "[Eeprom]   RS485 Iface: 0x%04X\n", g_appConfig.rs485_iface_addr);
            LOG_DEBUG(3, "[Eeprom]   RS485 Group: 0x%04X\n", g_appConfig.rs485_group_addr);
            LOG_DEBUG(3, "[Eeprom]   RS485 Bcast: 0x%04X\n", g_appConfig.rs485_bcast_addr);
            LOG_DEBUG(3, "[Eeprom]   Glavni Protokol: %d\n", g_appConfig.protocol_version);
            
            LOG_DEBUG(3, "[Eeprom]   === Mrežna Konfiguracija ===\n");
            LOG_DEBUG(3, "[Eeprom]   IP Adresa: %d.%d.%d.%d\n",
                (g_appConfig.ip_address >> 24) & 0xFF,
                (g_appConfig.ip_address >> 16) & 0xFF,
                (g_appConfig.ip_address >> 8) & 0xFF,
                g_appConfig.ip_address & 0xFF);
            LOG_DEBUG(3, "[Eeprom]   Subnet Mask: %d.%d.%d.%d\n",
                (g_appConfig.subnet_mask >> 24) & 0xFF,
                (g_appConfig.subnet_mask >> 16) & 0xFF,
                (g_appConfig.subnet_mask >> 8) & 0xFF,
                g_appConfig.subnet_mask & 0xFF);
            LOG_DEBUG(3, "[Eeprom]   Gateway: %d.%d.%d.%d\n",
                (g_appConfig.gateway >> 24) & 0xFF,
                (g_appConfig.gateway >> 16) & 0xFF,
                (g_appConfig.gateway >> 8) & 0xFF,
                g_appConfig.gateway & 0xFF);
            LOG_DEBUG(3, "[Eeprom]   mDNS Ime: %s\n", g_appConfig.mdns_name);
            LOG_DEBUG(3, "[Eeprom]   System ID: %d\n", g_appConfig.system_id);
            
            LOG_DEBUG(3, "[Eeprom]   === Dodatni TimeSync Paketi ===\n");
            for (int i = 0; i < 3; i++)
            {
                LOG_DEBUG(3, "[Eeprom]     [%d] enabled=%d, protocol=%d, address=%d\n",
                    i,
                    g_appConfig.additional_sync[i].enabled,
                    g_appConfig.additional_sync[i].protocol_version,
                    g_appConfig.additional_sync[i].broadcast_addr);
            }
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

        // ISPRAVKA: Wire buffer je 128 bajtova. 2 bajta adrese + 64 bajta podataka = 66 bajtova (SIGURNO).
        chunk_size = min(chunk_size, (uint16_t)64);
        
        LOG_DEBUG(4, "[Eeprom] -> Pisanje chunk-a: addr=0x%04X, size=%u\n", current_addr, chunk_size);
        Wire.beginTransmission(EEPROM_I2C_ADDR);
        Wire.write((uint8_t)(current_addr >> 8));   
        Wire.write((uint8_t)(current_addr & 0xFF)); 
        
        // Provjera da li je Wire biblioteka prihvatila sve bajtove
        size_t written = Wire.write(data + data_offset, chunk_size);
        if (written != chunk_size) {
             LOG_DEBUG(1, "[Eeprom] GRESKA: Wire bafer pun? Traženo %u, upisano %u\n", chunk_size, written);
             return false; // Kritična greška - ne možemo nastaviti
        }
        
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
        // Smanjujemo i čitanje na 64 radi konzistentnosti i sigurnosti I2C bafera
        uint16_t chunk_size = min((uint16_t)bytes_remaining, (uint16_t)64);
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

    uint16_t first_valid_index = 0xFFFF;
    uint16_t last_valid_index = 0xFFFF;
    uint16_t valid_count = 0;
    bool first_valid_found = false;

    // 1. Pronađi prvi uzastopni blok validnih logova
    // ISPRAVKA: Ne koristimo status bajt, čitamo log_id iz LogEntry
    for (uint16_t i = 0; i < MAX_LOG_ENTRIES; ++i)
    {
        // Reset watchdog svakih 100 iteracija (duga operacija)
        if (i % 100 == 0)
        {
            esp_task_wdt_reset();
        }

        uint16_t addr = EEPROM_LOG_START_ADDR + (i * LOG_ENTRY_SIZE);
        LogEntry temp_entry;
        if (ReadBytes(addr, (uint8_t*)&temp_entry, sizeof(LogEntry)))
        {
            // Provjeri da li je log_id != 0 i != 0xFFFF (validni log)
            if (temp_entry.log_id != 0 && temp_entry.log_id != 0xFFFF)
            {
                if (!first_valid_found)
                {
                    first_valid_index = i;
                    first_valid_found = true;
                    last_valid_index = i;
                    valid_count = 1;
                }
                else if (i == last_valid_index + 1)
                {
                    // Uzastopan log - broji ga
                    last_valid_index = i;
                    valid_count++;
                }
                else
                {
                    // Rupa u logovima - prestani brojati (ignorisi rasute stare logove)
                    LOG_DEBUG(2, "[Eeprom] Detektovana rupa na poziciji %u, zaustavljam brojanje.\n", i);
                    break;
                }
            }
            else if (first_valid_found)
            {
                // Prazan slot nakon što smo počeli brojati - kraj uzastopnog bloka
                break;
            }
        }
    }

    // Finalni watchdog reset nakon skeniranja
    esp_task_wdt_reset();

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
        LOG_DEBUG(3, "[Eeprom] Skeniranje završeno. Logger je pun.\n");
        m_log_read_index = 0; // tail
        m_log_write_index = 0; // head
        m_log_count = MAX_LOG_ENTRIES;
    }
    else
    {
        // Slučaj 3: Djelimično popunjen logger.
        bool is_wrapped = (first_valid_index > last_valid_index) || 
                          ( (last_valid_index == MAX_LOG_ENTRIES - 1) && (first_valid_index > 0) );

        uint16_t next_free_index = (last_valid_index + 1) % MAX_LOG_ENTRIES;
        uint16_t check_addr = EEPROM_LOG_START_ADDR + (next_free_index * LOG_ENTRY_SIZE);
        LogEntry check_entry;
        ReadBytes(check_addr, (uint8_t*)&check_entry, sizeof(LogEntry));

        if ((check_entry.log_id == 0 || check_entry.log_id == 0xFFFF) && !is_wrapped)
        {
            // Normalan, neobmotan slučaj: Logovi su uzastopni od first do last.
            m_log_read_index = first_valid_index; // tail (najstariji)
            m_log_write_index = next_free_index;  // head (sljedeća slobodna pozicija)
        }
        else
        {
            // Obmotan slučaj: Buffer je pun ili blizu toga, logovi su ciklični.
            // U kružnom bufferu, najstariji log je NAKON write pozicije.
            m_log_write_index = (last_valid_index + 1) % MAX_LOG_ENTRIES;
            m_log_read_index = (m_log_write_index + (MAX_LOG_ENTRIES - valid_count)) % MAX_LOG_ENTRIES;
        }
        m_log_count = valid_count;
        LOG_DEBUG(3, "[Eeprom] Skeniranje završeno. Pronađeno %u logova.\n", m_log_count);
        LOG_DEBUG(3, "[Eeprom] -> first_valid_index: %u, last_valid_index: %u\n", first_valid_index, last_valid_index);
        LOG_DEBUG(3, "[Eeprom] -> is_wrapped: %s, next_free valid: %s\n", 
            is_wrapped ? "DA" : "NE",
            ((check_entry.log_id == 0 || check_entry.log_id == 0xFFFF) ? "DA" : "NE"));
        LOG_DEBUG(3, "[Eeprom] -> Read Index (tail): %u\n", m_log_read_index);
        LOG_DEBUG(3, "[Eeprom] -> Write Index (head): %u\n", m_log_write_index);
    }
}

LoggerStatus EepromStorage::WriteLog(const LogEntry* entry)
{
    // Adresa na koju upisujemo novi log (head)
    uint16_t write_addr = EEPROM_LOG_START_ADDR + (m_log_write_index * LOG_ENTRY_SIZE);

    // Upisujemo 16 bajtova LogEntry strukture
    if (!WriteBytes(write_addr, (const uint8_t*)entry, LOG_ENTRY_SIZE))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje loga na adresu 0x%04X nije uspjelo.\n", write_addr);
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

    LOG_DEBUG(3, "[Eeprom] -> Čitam %u logova (%u bajtova) počevši od indeksa %u.\n", logs_to_read, total_bytes_to_read, m_log_read_index);

    // 3. Pročitaj validne logove u bafer
    for (uint16_t i = 0; i < logs_to_read; ++i)
    {
        // Izračunaj indeks i adresu trenutnog loga u kružnom baferu
        uint16_t current_log_index = (m_log_read_index + i) % MAX_LOG_ENTRIES;
        uint16_t read_addr = EEPROM_LOG_START_ADDR + (current_log_index * LOG_ENTRY_SIZE);
        
        LOG_DEBUG(4, "[Eeprom]   Log %u: index=%u, addr=0x%04X\n", i, current_log_index, read_addr);
        
        // Adresa u odredišnom baferu
        uint8_t* dest_buffer = data_buffer + (i * LOG_RECORD_SIZE);

        if (!ReadBytes(read_addr, dest_buffer, LOG_RECORD_SIZE))
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

    LOG_DEBUG(3, "[Eeprom] Vraćen HEX string dužine %d.\n", hex_string.length());
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
    uint16_t logs_to_delete = min((uint16_t)m_log_count, logs_in_block);

    LOG_DEBUG(3, "[Eeprom] Brisanje bloka od %u logova...\n", logs_to_delete);
    
    uint8_t zero_buffer[LOG_ENTRY_SIZE];
    memset(zero_buffer, 0, LOG_ENTRY_SIZE);

    for (uint16_t i = 0; i < logs_to_delete; i++)
    {
        uint16_t current_log_index = (m_log_read_index + i) % MAX_LOG_ENTRIES;
        uint16_t delete_addr = EEPROM_LOG_START_ADDR + (current_log_index * LOG_ENTRY_SIZE);
        if (!WriteBytes(delete_addr, zero_buffer, LOG_ENTRY_SIZE))
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

// Implementacija WriteAddressList (legacy - piše u offset 0)
bool EepromStorage::WriteAddressList(const uint16_t* listBuffer, uint16_t count)
{
    // Legacy metoda - piše u offset 0 (kompatibilnost sa single bus mode)
    return WriteAddressListL(listBuffer, count);
}

// Implementacija WriteAddressListL (Lijevi bus - offset 0)
bool EepromStorage::WriteAddressListL(const uint16_t* listBuffer, uint16_t count)
{
    uint16_t addresses_to_write = min(count, (uint16_t)MAX_ADDRESS_LIST_SIZE_PER_BUS);
    uint16_t bytes_to_write = addresses_to_write * sizeof(uint16_t);

    LOG_DEBUG(3, "[Eeprom] Upisujem LIJEVI bus: %u adresa (offset 0)\n", addresses_to_write);
    if (!WriteBytes(EEPROM_ADDRESS_LIST_START_ADDR, (const uint8_t*)listBuffer, bytes_to_write))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje LIJEVOG busa neuspješno.\n");
        return false;
    }
    
    // Nuliramo ostatak prostora za Lijevi bus (500 bajta)
    uint16_t remaining_bytes = 500 - bytes_to_write;
    if (remaining_bytes > 0)
    {
        uint8_t zero_buffer[16] = {0};
        for (uint16_t offset = 0; offset < remaining_bytes; offset += sizeof(zero_buffer)) {
            uint16_t clear_address = EEPROM_ADDRESS_LIST_START_ADDR + bytes_to_write + offset;
            uint16_t chunk_to_clear = min((uint16_t)sizeof(zero_buffer), (uint16_t)(remaining_bytes - offset));
            if (!WriteBytes(clear_address, zero_buffer, chunk_to_clear)) {
                 LOG_DEBUG(1, "[Eeprom] GRESKA: Čišćenje LIJEVOG busa neuspješno.\n");
                 return false;
            }
        }
    }

    LOG_DEBUG(3, "[Eeprom] LIJEVI bus uspješno upisan.\n");
    return true;
}

// Implementacija WriteAddressListR (Desni bus - offset 500)
bool EepromStorage::WriteAddressListR(const uint16_t* listBuffer, uint16_t count)
{
    uint16_t addresses_to_write = min(count, (uint16_t)MAX_ADDRESS_LIST_SIZE_PER_BUS);
    uint16_t bytes_to_write = addresses_to_write * sizeof(uint16_t);
    uint16_t offset_r = EEPROM_ADDRESS_LIST_START_ADDR + 500; // Offset za Desni bus

    LOG_DEBUG(3, "[Eeprom] Upisujem DESNI bus: %u adresa (offset 500)\n", addresses_to_write);
    if (!WriteBytes(offset_r, (const uint8_t*)listBuffer, bytes_to_write))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Pisanje DESNOG busa neuspješno.\n");
        return false;
    }
    
    // Nuliramo ostatak prostora za Desni bus (500 bajta)
    uint16_t remaining_bytes = 500 - bytes_to_write;
    if (remaining_bytes > 0)
    {
        uint8_t zero_buffer[16] = {0};
        for (uint16_t offset = 0; offset < remaining_bytes; offset += sizeof(zero_buffer)) {
            uint16_t clear_address = offset_r + bytes_to_write + offset;
            uint16_t chunk_to_clear = min((uint16_t)sizeof(zero_buffer), (uint16_t)(remaining_bytes - offset));
            if (!WriteBytes(clear_address, zero_buffer, chunk_to_clear)) {
                 LOG_DEBUG(1, "[Eeprom] GRESKA: Čišćenje DESNOG busa neuspješno.\n");
                 return false;
            }
        }
    }

    LOG_DEBUG(3, "[Eeprom] DESNI bus uspješno upisan.\n");
    return true;
}

bool EepromStorage::ReadAddressList(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    // Legacy metoda - čita sa offset 0 (kompatibilnost)
    return ReadAddressListL(listBuffer, maxCount, actualCount);
}

// Implementacija ReadAddressListL (Lijevi bus - offset 0)
bool EepromStorage::ReadAddressListL(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    uint16_t max_read = min(maxCount, (uint16_t)MAX_ADDRESS_LIST_SIZE_PER_BUS);
    uint16_t bytes_to_read = max_read * sizeof(uint16_t);
    
    if (ReadBytes(EEPROM_ADDRESS_LIST_START_ADDR, (uint8_t*)listBuffer, bytes_to_read))
    {
        uint16_t valid_count = 0;
        for (uint16_t i = 0; i < max_read; i++)
        {
            if (listBuffer[i] == 0) break;
            valid_count++;
        }
        *actualCount = valid_count;
        LOG_DEBUG(3, "[Eeprom] LIJEVI bus: %u validnih adresa.\n", *actualCount);
        return true;
    }
    *actualCount = 0;
    return false;
}

// Implementacija ReadAddressListR (Desni bus - offset 500)
bool EepromStorage::ReadAddressListR(uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    uint16_t max_read = min(maxCount, (uint16_t)MAX_ADDRESS_LIST_SIZE_PER_BUS);
    uint16_t bytes_to_read = max_read * sizeof(uint16_t);
    uint16_t offset_r = EEPROM_ADDRESS_LIST_START_ADDR + 500;
    
    if (ReadBytes(offset_r, (uint8_t*)listBuffer, bytes_to_read))
    {
        uint16_t valid_count = 0;
        for (uint16_t i = 0; i < max_read; i++)
        {
            if (listBuffer[i] == 0) break;
            valid_count++;
        }
        *actualCount = valid_count;
        LOG_DEBUG(3, "[Eeprom] DESNI bus: %u validnih adresa.\n", *actualCount);
        return true;
    }
    *actualCount = 0;
    return false;
}

/**
 * @brief Parsira CSV string i ekstraktuje adrese.
 * @param csvContent String sa CSV sadržajem (format: "101,102,103" ili "101\n102\n103").
 * @param listBuffer Buffer za smještanje parsiranih adresa.
 * @param maxCount Maksimalan broj adresa.
 * @param actualCount Pointer gdje će se upisati broj parsiranih adresa.
 * @return true ako je parsiranje uspješno, false inače.
 */
bool EepromStorage::ParseAddressListFromCSV(const String& csvContent, uint16_t* listBuffer, uint16_t maxCount, uint16_t* actualCount)
{
    memset(listBuffer, 0, maxCount * sizeof(uint16_t));
    uint16_t count = 0;
    
    String content = csvContent;
    
    // Ukloni sve nakon ';' karaktera (kraj liste marker)
    int end_char_pos = content.indexOf(';');
    if (end_char_pos != -1) {
        content = content.substring(0, end_char_pos);
    }
    
    // Zamijeni nove redove zarezima za unificiran parsing
    content.replace("\n", ",");
    content.replace("\r", "");
    
    int start = 0;
    int end = content.indexOf(',');
    content += ','; // Dodaj zarez na kraj da bi petlja obradila i zadnji element
    
    while (end != -1 && count < maxCount)
    {
        String addrStr = content.substring(start, end);
        addrStr.trim();
        
        // Ignoriši prazne linije i komentare (počinju sa '#')
        if (addrStr.length() > 0 && addrStr.charAt(0) != '#')
        {
            uint16_t addr = addrStr.toInt();
            if (addr > 0)
            {
                listBuffer[count++] = addr;
            }
        }
        start = end + 1;
        end = content.indexOf(',', start);
    }
    
    *actualCount = count;
    LOG_DEBUG(3, "[Eeprom] CSV parsirano: %u adresa.\n", count);
    return (count > 0);
}

// ============================================================================
// --- ISPRAVKA: DODATA FUNKCIJA KOJA NEDOSTAJE ---
// ============================================================================
LoggerStatus EepromStorage::ClearAllLogs()
{
    LOG_DEBUG(3, "[Eeprom] Brisanje svih logova (punjenje nulama)...\n");

    // ISPRAVKA: Ograniči brisanje na dostupan prostor u EEPROM-u
    // 24C1024 = 128KB = 131,072 bajta
    // Dostupan prostor za logove = 128KB - Config(256B) - AddressList(1000B) = 129,816 bajta
    const uint32_t EEPROM_TOTAL_SIZE = 131072; // 128KB
    const uint32_t AVAILABLE_LOG_SPACE = EEPROM_TOTAL_SIZE - EEPROM_LOG_START_ADDR;
    
    // Briši manji od: konfigurisani prostor ILI dostupni prostor
    uint32_t bytes_to_clear = min((uint32_t)EEPROM_LOG_AREA_SIZE, AVAILABLE_LOG_SPACE);
    
    LOG_DEBUG(3, "[Eeprom] -> Log start: 0x%04X, Bytes to clear: %u\n", EEPROM_LOG_START_ADDR, bytes_to_clear);

    // Pripremi buffer sa 0x00 (prazan log slot)
    uint8_t empty_buffer[EEPROM_PAGE_SIZE];
    memset(empty_buffer, 0, EEPROM_PAGE_SIZE);

    uint32_t current_address = EEPROM_LOG_START_ADDR; // ISPRAVKA: uint32_t umjesto uint16_t!
    uint16_t chunks_written = 0;

    while (bytes_to_clear > 0)
    {
        // Reset watchdog svakih 10 chunk-ova (duga operacija)
        if (chunks_written % 10 == 0)
        {
            esp_task_wdt_reset();
        }

        // Odredi koliko pisati u ovom ciklusu
        uint16_t chunk_size = min((uint32_t)bytes_to_clear, (uint32_t)sizeof(empty_buffer));
        
        // ISPRAVKA: Ograniči adresu na 16-bit opseg (24C1024 ima 17-bit adresiranje preko device address)
        uint16_t write_addr = (uint16_t)(current_address & 0xFFFF);
        
        if (!WriteBytes(write_addr, empty_buffer, chunk_size))
        {
            LOG_DEBUG(1, "[Eeprom] GRESKA pri brisanju logova na adresi 0x%04X.\n", write_addr);
            return LoggerStatus::LOGGER_ERROR;
        }
        
        bytes_to_clear -= chunk_size;
        current_address += chunk_size;
        chunks_written++;
    }

    // Finalni watchdog reset
    esp_task_wdt_reset();

    // Resetuj head/tail pokazivače
    m_log_write_index = 0;
    m_log_read_index = 0;
    m_log_count = 0;

    LOG_DEBUG(3, "[Eeprom] Svi logovi obrisani.\n");
    return LoggerStatus::LOGGER_OK;
}