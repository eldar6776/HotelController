/**
 ******************************************************************************
 * @file    SdCardManager.cpp
 * @author  Code Review Assistant & [Vase Ime]
 * @brief   Implementacija SdCardManager modula.
 ******************************************************************************
 */

#include "SdCardManager.h"
#include "DebugConfig.h"

SdCardManager::SdCardManager() :
    m_card_mounted(false),
    m_cs_pin(-1)
{
    // Konstruktor
}

bool SdCardManager::Initialize(int8_t sck_pin, int8_t miso_pin, int8_t mosi_pin, int8_t cs_pin)
{
    LOG_DEBUG(5, "[SdCard] Entering Initialize()...\n");
    LOG_DEBUG(3, "[SdCard] Inicijalizacija SPI: SCK=%d, MISO=%d, MOSI=%d, CS=%d\n", sck_pin, miso_pin, mosi_pin, cs_pin);
    
    m_cs_pin = cs_pin;
    
    // Inicijalizuj SPI sa custom pinovima
    SPI.begin(sck_pin, miso_pin, mosi_pin, cs_pin);
    
    // Pokušaj montiranje kartice sa većim timeout-om (5 sekundi)
    LOG_DEBUG(3, "[SdCard] Pokušaj montiranja uSD kartice...\n");
    
    // Pokušaj sa standardnom frekvencijom (4MHz je safe default)
    if (!SD.begin(cs_pin, SPI, 4000000))
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Montiranje kartice neuspješno!\n");
        LOG_DEBUG(1, "[SdCard] Provjerite:\n");
        LOG_DEBUG(1, "[SdCard]   - Da li je kartica umetnuta?\n");
        LOG_DEBUG(1, "[SdCard]   - Da li je kartica formatirana (FAT32)?\n");
        LOG_DEBUG(1, "[SdCard]   - Da li su SPI pinovi ispravno povezani?\n");
        m_card_mounted = false;
        return false;
    }
    
    m_card_mounted = true;
    
    // Ispis informacija o kartici
    uint8_t cardType = SD.cardType();    
    
    switch (cardType)
    {
        case CARD_MMC:
            LOG_DEBUG(3, "[SdCard] Tip kartice: MMC\n");
            break;
        case CARD_SD:
            LOG_DEBUG(3, "[SdCard] Tip kartice: SDSC\n");
            break;
        case CARD_SDHC:
            LOG_DEBUG(3, "[SdCard] Tip kartice: SDHC\n");
            break;
        default:
            LOG_DEBUG(3, "[SdCard] Tip kartice: UNKNOWN\n");
            break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    LOG_DEBUG(3, "[SdCard] Veličina kartice: %llu MB\n", cardSize);
    
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
    LOG_DEBUG(3, "[SdCard] Iskorišteno: %llu MB / %llu MB\n", usedBytes, totalBytes);
    
    LOG_DEBUG(3, "[SdCard] Inicijalizacija uspješna!\n");
    LOG_DEBUG(5, "[SdCard] Exiting Initialize().\n");
    return true;
}

bool SdCardManager::IsCardMounted()
{
    return m_card_mounted;
}

String SdCardManager::ReadTextFile(const char* path)
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj čitanja fajla '%s' dok kartica nije montirana.\n", path);
        return String();
    }
    
    LOG_DEBUG(4, "[SdCard] Čitanje tekstualnog fajla: %s\n", path);
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        LOG_DEBUG(2, "[SdCard] UPOZORENJE: Fajl '%s' ne postoji.\n", path);
        return String();
    }
    
    String content;
    content.reserve(file.size()); // Pre-alociraj memoriju
    
    while (file.available())
    {
        content += (char)file.read();
    }
    
    file.close();
    
    LOG_DEBUG(3, "[SdCard] Pročitano %d bajtova iz '%s'\n", content.length(), path);
    return content;
}

File SdCardManager::OpenFile(const char* path, const char* mode) // Prihvata const char*
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj otvaranja fajla '%s' dok kartica nije montirana.\n", path);
        return File();
    }
    
    // Nema više greške! Sada prosleđujemo string literal koji SD.open() očekuje.
    File file = SD.open(path, mode); 
    
    if (!file)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA pri otvaranju fajla '%s' u modu '%s'.\n", path, mode);
    }
    
    return file;
}

File SdCardManager::CreateFile(const char* path)
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj kreiranja fajla '%s' dok kartica nije montirana.\n", path);
        return File();
    }
    
    // Obriši postojeći fajl ako postoji
    if (SD.exists(path))
    {
        SD.remove(path);
    }
    
    File file = SD.open(path, FILE_WRITE);
    
    if (!file)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA pri kreiranju fajla '%s'.\n", path);
    }
    else
    {
        LOG_DEBUG(3, "[SdCard] Kreiran novi fajl '%s'.\n", path);
    }
    
    return file;
}

bool SdCardManager::CreateFolder(const char* path)
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj kreiranja direktorijuma '%s' dok kartica nije montirana.\n", path);
        return false;
    }

    if (SD.exists(path))
    {
        LOG_DEBUG(2, "[SdCard] UPOZORENJE: Pokušaj kreiranja direktorijuma '%s' koji već postoji.\n", path);
        return false; 
    }

    if (SD.mkdir(path))
    {
        LOG_DEBUG(3, "[SdCard] Direktorijum '%s' uspješno kreiran.\n", path);
        return true;
    }
    else
    {
        LOG_DEBUG(1, "[SdCard] GRESKA pri kreiranju direktorijuma '%s'.\n", path);
        return false;
    }
}

bool SdCardManager::Rename(const char* oldPath, const char* newPath)
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj preimenovanja '%s' u '%s' dok kartica nije montirana.\n", oldPath, newPath);
        return false;
    }

    if (!SD.exists(oldPath))
    {
        LOG_DEBUG(2, "[SdCard] UPOZORENJE: Pokušaj preimenovanja nepostojećeg fajla/foldera '%s'.\n", oldPath);
        return false;
    }

    if (SD.exists(newPath))
    {
        LOG_DEBUG(2, "[SdCard] UPOZORENJE: Odredišna putanja '%s' već postoji.\n", newPath);
        return false;
    }

    if (SD.rename(oldPath, newPath))
    {
        LOG_DEBUG(3, "[SdCard] Uspješno preimenovan '%s' u '%s'.\n", oldPath, newPath);
        return true;
    }
    else
    {
        LOG_DEBUG(1, "[SdCard] GRESKA pri preimenovanju '%s'.\n", oldPath, newPath);
        return false;
    }
}


bool SdCardManager::FileExists(const char* path)
{
    if (!m_card_mounted)
    {
        return false;
    }
    
    return SD.exists(path);
}

size_t SdCardManager::GetFileSize(const char* path)
{
    if (!m_card_mounted)
    {
        return 0;
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        return 0;
    }
    
    size_t size = file.size();
    file.close();
    
    return size;
}

String SdCardManager::ListFiles(const char* path)
{
    if (!m_card_mounted)
    {
        return "{\"error\":\"Card not mounted\"}";
    }
    
    File root = SD.open(path);
    if (!root)
    {
        return "{\"error\":\"Failed to open directory\"}";
    }
    
    if (!root.isDirectory())
    {
        root.close();
        return "{\"error\":\"Not a directory\"}";
    }
    
    String output = "{\"path\":\"" + String(path) + "\",\"files\":[";
    bool first = true;
    
    File file = root.openNextFile();
    while (file)
    {
        // Preskačemo ispisivanje samog sebe u listi (relevantno za root)
        if (strcmp(file.name(), path) == 0) { file = root.openNextFile(); continue; }
        if (!first) 
        {
            output += ",";
        }
        first = false;
        
        output += "{\"name\":\"";
        output += String(file.name());
        output += "\",\"size\":";
        output += String(file.size());
        output += ",\"dir\":";
        output += file.isDirectory() ? "true" : "false";
        output += "}";
        
        file.close();
        file = root.openNextFile();
    }
    
    output += "]}";
    root.close();
    
    return output;
}

bool SdCardManager::DeleteFile(const char* path)
{
    if (!m_card_mounted)
    {
        LOG_DEBUG(1, "[SdCard] GRESKA: Pokušaj brisanja fajla '%s' dok kartica nije montirana.\n", path);
        return false;
    }
    
    if (!SD.exists(path))
    {
        LOG_DEBUG(2, "[SdCard] UPOZORENJE: Pokušaj brisanja fajla '%s' koji ne postoji.\n", path);
        return false;
    }
    
    if (SD.remove(path))
    {
        LOG_DEBUG(3, "[SdCard] Fajl '%s' uspješno obrisan.\n", path);
        return true;
    }
    else
    {
        LOG_DEBUG(1, "[SdCard] GRESKA pri brisanju fajla '%s'.\n", path);
        return false;
    }
}

void SdCardManager::ListDirectory(File dir, String& output, int numTabs)
{
    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry)
        {
            break;
        }
        
        for (uint8_t i = 0; i < numTabs; i++)
        {
            output += "  ";
        }
        
        output += entry.name();
        
        if (entry.isDirectory())
        {
            output += "/\n";
            ListDirectory(entry, output, numTabs + 1);
        }
        else
        {
            output += " (";
            output += String(entry.size());
            output += " bytes)\n";
        }
        
        entry.close();
    }
}