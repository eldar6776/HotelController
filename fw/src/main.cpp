/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Glavni ulazni fajl za Hotel Controller ESP32
 ******************************************************************************
 */

#include <Arduino.h>
#include <FS.h>
#include <SD.h> // Ukljuceno za File/SD
// Ukljucivanje svih Headera (za globalne objekte)
#include "ProjectConfig.h"   
#include "NetworkManager.h"    
#include "EepromStorage.h"     
#include "SdCardManager.h"   // CHANGED: Ukljucen SdCardManager
#include "Rs485Service.h"      
#include "HttpServer.h"        
#include "HttpQueryManager.h"
#include "LogPullManager.h"
#include "TimeSync.h"
#include "UpdateManager.h"


// Deklaracija globalnih objekata
NetworkManager g_networkManager;
EepromStorage g_eepromStorage;
SdCardManager g_sdCardManager; // Ispravno: Koristimo SdCardManager
Rs485Service g_rs485Service;
HttpServer g_httpServer;
HttpQueryManager g_httpQueryManager;
LogPullManager g_logPullManager;
TimeSync g_timeSync;
UpdateManager g_updateManager;

// Globalna varijabla konfiguracije
extern AppConfig g_appConfig; // Inicijalizacija na 0

void setup() 
{
    // --- FAZA 1: Inicijalizacija Serijske Komunikacije ---
    Serial.begin(SERIAL_DEBUG_BAUDRATE); 
    while (!Serial && millis() < 2000) {} 
    Serial.println(F("==================================="));
    Serial.println(F("Hotel Controller ESP32 - Pokretanje"));
    Serial.println(F("==================================="));
    
    // --- FAZA 2: Inicijalizacija HW Drajvera ---
    Serial.println(F("[setup] Inicijalizacija HW Drajvera...\r\n"));

    // Rjesava gresku: OVDJE JE BILA LINIJA KOJA JE KORISTILA g_spiFlashStorage
    // Ispravno: Inicijalizacija SdCardManager-a
    g_sdCardManager.Initialize(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_FLASH_CS_PIN);

    g_eepromStorage.Initialize(I2C_SDA_PIN, I2C_SCL_PIN);

    // ISPRAVKA: Inicijalizacija se premješta u poseban zadatak
    g_networkManager.Initialize(); // Inicijalizuje samo event handlere
    g_networkManager.StartTask();  // Pokreće zadatak koji će inicijalizovati ETH/WiFi

    // NOVO: Proslijedi pokazivač na HTTP server u NetworkManager
    g_networkManager.SetHttpServer(&g_httpServer);

    // --- FAZA 3: Inicijalizacija Sub-Modula ---
    Serial.println(F("[setup] Inicijalizacija Sub-Modula...\r\n"));

    // Proslijedjujemo pokazivace
    g_logPullManager.Initialize(&g_rs485Service, &g_eepromStorage);
    g_httpQueryManager.Initialize(&g_rs485Service);
    
    // Ispravno: Proslijedjen SdCardManager
    g_updateManager.Initialize(&g_rs485Service, &g_sdCardManager); 
    g_timeSync.Initialize(&g_rs485Service);

    Serial.println(F("[setup] Priprema HTTP Servera (ne pokreće se još)..."));
    
    // Ispravno: Proslijedjen SdCardManager (cetvrti argument)
    g_httpServer.Initialize(
        &g_httpQueryManager,
        &g_updateManager,
        &g_eepromStorage,
        &g_sdCardManager 
    );

    // --- FAZA 4: Pokretanje FreeRTOS Zadataka ---
    Serial.println(F("[setup] Pokretanje Rs485Service zadatka..."));
    
    // Rs485Service inicijalizacija (prije starta taska!)
    g_rs485Service.Initialize(
        &g_httpQueryManager,
        &g_updateManager,
        &g_logPullManager,
        &g_timeSync
    );
    g_rs485Service.StartTask();

    Serial.println(F("==================================="));
    Serial.println(F("[setup] Inicijalizacija zavrsena. Sistem radi."));
    Serial.println(F("==================================="));
}

void loop() 
{
    // Glavna petlja: koristimo samo za ne-blokirajuce Loop funkcije
    g_networkManager.Loop(); // Provjerava reset dugme
    delay(1); 
}