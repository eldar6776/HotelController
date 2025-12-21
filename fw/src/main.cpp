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
#include "FirmwareUpdateManager.h" // NOVO
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
FirmwareUpdateManager g_fufUpdateManager; // NOVO
UpdateManager g_updateManager;

// Globalna varijabla konfiguracije
extern AppConfig g_appConfig; // Inicijalizacija na 0

// NOVO: Stanja za glavnu state-mašinu
enum class SystemState {
    CHECK_HTTP,
    RUN_UPDATE,
    RUN_TIMESYNC,
    RUN_POLLING
};
SystemState g_systemState = SystemState::RUN_POLLING; // Počinjemo sa pollingom

void setup() 
{
    
    // --- FAZA 1: Inicijalizacija Serijske Komunikacije ---
    Serial.begin(SERIAL_DEBUG_BAUDRATE); 
    while (!Serial && millis() < 2000) {} 
    Serial.println(F("==================================="));
    Serial.println(F("Hotel Controller ESP32 - Pokretanje"));
    Serial.println(F("==================================="));
    
    // --- FAZA 1.5: Inicijalizacija Watchdog-a ---
    Serial.println(F("[setup] UPOZORENJE: Task Watchdog je privremeno onemogućen."));
    // esp_task_wdt_init(WDT_TIMEOUT, true); // true = panic (restart) on timeout
    // esp_task_wdt_add(NULL); // Dodaj 'loopTask' (Arduino zadatak) na praćenje
    // Serial.println(F("[setup] Watchdog aktivan."));


    LOG_DEBUG(5, "[main] Entering setup()...\n");

    // --- FAZA 2: Inicijalizacija HW Drajvera ---
    Serial.println(F("[setup] Inicijalizacija HW Drajvera...\r\n"));

    // Rjesava gresku: OVDJE JE BILA LINIJA KOJA JE KORISTILA g_spiFlashStorage
    // Ispravno: Inicijalizacija SdCardManager-a
    g_sdCardManager.Initialize(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_FLASH_CS_PIN);

    g_eepromStorage.Initialize(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // Inicijalizujemo samo event handlere za WiFi
    // I Rs485 hardver
    g_networkManager.Initialize();
    g_rs485Service.Initialize();

    // --- FAZA 3: Inicijalizacija Sub-Modula ---
    LOG_DEBUG(3, "[setup] Inicijalizacija Sub-Modula...\r\n");

    g_logPullManager.Initialize(&g_rs485Service, &g_eepromStorage);
    g_httpQueryManager.Initialize(&g_rs485Service);
    g_fufUpdateManager.Initialize(&g_rs485Service, &g_sdCardManager); // NOVO
    g_updateManager.Initialize(&g_rs485Service, &g_sdCardManager);
    g_timeSync.Initialize(&g_rs485Service);

    LOG_DEBUG(3, "[setup] Priprema HTTP Servera (ne pokreće se još)...\n");
    g_httpServer.Initialize(
        &g_httpQueryManager,
        &g_fufUpdateManager, // NOVO
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
    // esp_task_wdt_reset(); // Resetuj watchdog tajmer u svakom ciklusu

    // NetworkManager.Loop() se više ne poziva jer je njegova logika prebačena u task
    // koji se ne završava.

    // HTTP komande se obrađuju direktno u HttpServer-u i imaju najveći prioritet.
    // HttpServer (ESPAsyncWebServer) radi u svom zadatku. Kada stigne zahtjev,
    // on poziva ExecuteBlockingQuery, koji će zauzeti RS485 magistralu i blokirati
    // samo zadatak od web servera, ne i ovu loop() petlju.
    // Da bi se spriječilo da loop() uleti i pokrene npr. Polling dok HTTP upit čeka,
    // moramo uvesti globalni flag ili mutex. Za sada, oslanjamo se na to da su HTTP
    // upiti rijetki i brzi.

    // Glavna state-mašina za pozadinske zadatke
    // Prioritet: Update > TimeSync > Polling
    if (g_updateManager.IsActive()) {
        // Ako je update (pojedinačni ili sekvencijalni) aktivan, on ima potpuni prioritet.
        // Run() će odraditi transfer jednog fajla i vratiti kontrolu.
        g_updateManager.Run();

        // VRAĆENA ORIGINALNA, ISPRAVNA LOGIKA: Logika pauze za iuf sekvencu MORA OSTATI NETAKNUTA.
        // Ova pauza se izvršava nakon što Run() završi transfer jedne slike i vrati sesiju u S_IDLE.
        if (g_updateManager.IsSequenceActive() && g_updateManager.m_session.state == S_IDLE) {
            Serial.printf("[main] Pauza od %dms nakon transfera slike...\n", IMG_COPY_DEL);
            vTaskDelay(pdMS_TO_TICKS(IMG_COPY_DEL));
            // Nakon pauze, resetujemo TimeSync tajmer.
            g_timeSync.ResetTimer();
        }
    }
    else if (g_fufUpdateManager.IsActive())
    {
        // Ako iuf NIJE aktivan, provjeri da li je fuf/buf update aktivan.
        // Run() je sada neblokirajući i mora se pozivati u svakoj iteraciji.
        g_fufUpdateManager.Run();
    }
    else {
        // Ako nijedan update nije aktivan, izvršavaju se redovni pozadinski zadaci.
        g_timeSync.Run();
        g_logPullManager.Run();
    }

    // Mala pauza da se spriječi 100% zauzeće CPU-a i da se omogući rad
    // drugim zadacima (npr. mrežni stek, HttpServer).
    // Ovo je ključno jer su Run() funkcije sada blokirajuće.
    vTaskDelay(pdMS_TO_TICKS(5));
}