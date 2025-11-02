/**
 ******************************************************************************
 * @file    SdCardManager.cpp
 * @author  Code Review Assistant & [Vase Ime]
 * @brief   Implementacija SdCardManager modula.
 ******************************************************************************
 */

#include "SdCardManager.h"

SdCardManager::SdCardManager() :
    m_card_mounted(false),
    m_cs_pin(-1)
{
    // Konstruktor
}

bool SdCardManager::Initialize(int8_t sck_pin, int8_t miso_pin, int8_t mosi_pin, int8_t cs_pin)
{
    Serial.println(F("[SdCardManager] Inicijalizacija..."));
    
    m_cs_pin = cs_pin;
    
    // Inicijalizuj SPI sa custom pinovima
    SPI.begin(sck_pin, miso_pin, mosi_pin, cs_pin);
    
    // Pokušaj montiranje kartice sa većim timeout-om (5 sekundi)
    Serial.println(F("[SdCardManager] Pokušaj montiranja uSD kartice..."));
    
    // Pokušaj sa standardnom frekvencijom (4MHz je safe default)
    if (!SD.begin(cs_pin, SPI, 4000000))
    {
        Serial.println(F("[SdCardManager] GREŠKA: Montiranje kartice neuspješno!"));
        Serial.println(F("[SdCardManager] Provjerite:"));
        Serial.println(F("  - Da li je kartica umetnuta?"));
        Serial.println(F("  - Da li je kartica formatirana (FAT32)?"));
        Serial.println(F("  - Da li su SPI pinovi ispravno povezani?"));
        m_card_mounted = false;
        return false;
    }
    
    m_card_mounted = true;
    
    // Ispis informacija o kartici
    uint8_t cardType = SD.cardType();
    Serial.print(F("[SdCardManager] Tip kartice: "));
    
    switch (cardType)
    {
        case CARD_MMC:
            Serial.println(F("MMC"));
            break;
        case CARD_SD:
            Serial.println(F("SDSC"));
            break;
        case CARD_SDHC:
            Serial.println(F("SDHC"));
            break;
        default:
            Serial.println(F("UNKNOWN"));
            break;
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SdCardManager] Veličina kartice: %llu MB\n", cardSize);
    
    uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);
    uint64_t totalBytes = SD.totalBytes() / (1024 * 1024);
    Serial.printf("[SdCardManager] Iskorišteno: %llu MB / %llu MB\n", usedBytes, totalBytes);
    
    Serial.println(F("[SdCardManager] Inicijalizacija uspješna!"));
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
        Serial.println(F("[SdCardManager] Kartica nije montirana!"));
        return String();
    }
    
    File file = SD.open(path, FILE_READ);
    if (!file)
    {
        Serial.printf("[SdCardManager] Fajl '%s' ne postoji.\n", path);
        return String();
    }
    
    String content;
    content.reserve(file.size()); // Pre-alociraj memoriju
    
    while (file.available())
    {
        content += (char)file.read();
    }
    
    file.close();
    
    Serial.printf("[SdCardManager] Pročitano %d bajtova iz '%s'\n", content.length(), path);
    return content;
}

File SdCardManager::OpenFile(const char* path, const char* mode) // Prihvata const char*
{
    if (!m_card_mounted)
    {
        Serial.println(F("[SdCardManager] Kartica nije montirana!"));
        return File();
    }
    
    // Nema više greške! Sada prosleđujemo string literal koji SD.open() očekuje.
    File file = SD.open(path, mode); 
    
    if (!file)
    {
        Serial.printf("[SdCardManager] Greška pri otvaranju fajla '%s'\n", path);
    }
    
    return file;
}

File SdCardManager::CreateFile(const char* path)
{
    if (!m_card_mounted)
    {
        Serial.println(F("[SdCardManager] Kartica nije montirana!"));
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
        Serial.printf("[SdCardManager] Greška pri kreiranju fajla '%s'\n", path);
    }
    else
    {
        Serial.printf("[SdCardManager] Kreiran novi fajl '%s'\n", path);
    }
    
    return file;
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
    
    String output = "{\"files\":[";
    bool first = true;
    
    File file = root.openNextFile();
    while (file)
    {
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
        Serial.println(F("[SdCardManager] Kartica nije montirana!"));
        return false;
    }
    
    if (!SD.exists(path))
    {
        Serial.printf("[SdCardManager] Fajl '%s' ne postoji.\n", path);
        return false;
    }
    
    if (SD.remove(path))
    {
        Serial.printf("[SdCardManager] Fajl '%s' uspješno obrisan.\n", path);
        return true;
    }
    else
    {
        Serial.printf("[SdCardManager] Greška pri brisanju fajla '%s'\n", path);
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