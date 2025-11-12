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
#include "DebugConfig.h"
#include "ProjectConfig.h"
#include "NetworkManager.h"    // VRAĆAMO NETWORK MANAGER
#include "EepromStorage.h"
#include "SdCardManager.h"
#include "Rs485Service.h"
#include "HttpServer.h"
#include "HttpQueryManager.h"
#include "LogPullManager.h"
#include "TimeSync.h"
#include "UpdateManager.h"
#include <esp_task_wdt.h> // Uključujemo za Watchdog


// Deklaracija globalnih objekata
NetworkManager g_networkManager; // VRAĆAMO NETWORK MANAGER
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

// --- WATCHDOG KONFIGURACIJA ---
#define WDT_TIMEOUT 10 // 10 sekundi

void setup() 
{
    
    // --- FAZA 1: Inicijalizacija Serijske Komunikacije ---
    Serial.begin(SERIAL_DEBUG_BAUDRATE); 
    while (!Serial && millis() < 2000) {} 
    Serial.println(F("==================================="));
    Serial.println(F("Hotel Controller ESP32 - Pokretanje"));
    Serial.println(F("==================================="));
    
    // --- FAZA 1.5: Inicijalizacija Watchdog-a ---
    Serial.println(F("[setup] Inicijalizacija Task Watchdog-a..."));
    esp_task_wdt_init(WDT_TIMEOUT, true); // true = panic (restart) on timeout
    esp_task_wdt_add(NULL); // Dodaj 'loopTask' (Arduino zadatak) na praćenje
    Serial.println(F("[setup] Watchdog aktivan."));


    LOG_DEBUG(5, "[main] Entering setup()...\n");

    // --- FAZA 2: Inicijalizacija HW Drajvera ---
    Serial.println(F("[setup] Inicijalizacija HW Drajvera...\r\n"));

    // Rjesava gresku: OVDJE JE BILA LINIJA KOJA JE KORISTILA g_spiFlashStorage
    // Ispravno: Inicijalizacija SdCardManager-a
    g_sdCardManager.Initialize(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_FLASH_CS_PIN);

    g_eepromStorage.Initialize(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Inicijalizujemo samo event handlere za WiFi
    g_networkManager.Initialize();

    // --- FAZA 3: Inicijalizacija Sub-Modula ---
    LOG_DEBUG(3, "[setup] Inicijalizacija Sub-Modula...\r\n");

    // Vraćamo inicijalizaciju svih zavisnih modula
    g_logPullManager.Initialize(&g_rs485Service, &g_eepromStorage);
    g_httpQueryManager.Initialize(&g_rs485Service);
    g_updateManager.Initialize(&g_rs485Service, &g_sdCardManager);
    g_timeSync.Initialize(&g_rs485Service);

    g_networkManager.SetHttpServer(&g_httpServer);

    LOG_DEBUG(3, "[setup] Priprema HTTP Servera (ne pokreće se još)...\n");
    g_httpServer.Initialize(
        &g_httpQueryManager,
        &g_updateManager,
        &g_eepromStorage,
        &g_sdCardManager
    );

    // Pokrećemo mrežni zadatak
    g_networkManager.StartTask();

    Serial.println(F("==================================="));
    Serial.println(F("[setup] Inicijalizacija zavrsena. Sistem radi."));
    Serial.println(F("==================================="));
    LOG_DEBUG(5, "[main] Exiting setup().\n");
}

void loop() 
{
    // --- WATCHDOG RESET ---
    esp_task_wdt_reset(); // Resetuj watchdog tajmer u svakom ciklusu

    // Glavna petlja: koristimo samo za ne-blokirajuce Loop funkcije
    // static bool servicesStarted = false; // VIŠE NIJE POTREBNO

    // =================================================================================
    // DEBUG LINIJA: Svake 2 sekunde ispiši status ključnih flagova
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 2000) {
        lastDebugPrint = millis();
        // Uklonjen 'servicesStarted' iz ispisa
        LOG_DEBUG(4, "[loop] DEBUG: IsInitComplete=%d, IsNetConnected=%d\n",
            g_networkManager.IsInitializationComplete(), g_networkManager.IsNetworkConnected());
    }
    // =================================================================================

    // ONEMOGUĆENO: Logika je prebačena u NetworkManager::RunTask()
    // kako bi se osiguralo da se servisi pokrenu samo jednom i nakon što
    // je WiFiManager sigurno završen.

    g_networkManager.Loop(); // Vraćamo poziv za Loop()
    delay(1); 
    // LOG_DEBUG(5, "[main] Exiting loop().\n"); // Previše bučno, isključeno
}