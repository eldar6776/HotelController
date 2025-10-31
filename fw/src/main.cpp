/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Glavni ulazni fajl za Hotel Controller ESP32
 *
 * @note
 * Ovo je kostur aplikacije baziran na nasoj modularnoj FreeRTOS arhitekturi.
 * setup() inicijalizuje module i pokrece zadatke (Tasks).
 * loop() je skoro prazan i sluzi samo za ne-blokirajuce servisne petlje.
 ******************************************************************************
 */

#include <Arduino.h>

/*
 * 1. UKLJUCIVANJE SVIH MODULA PROJEKTA
 * Ovi fajlovi jos ne postoje, ali cemo ih kreirati jedan po jedan.
 */
#include "ProjectConfig.h"    // Mapa Pinova (v11) i Memorijska Mapa
#include "NetworkManager.h"     // Upravlja sa ETH, WiFi, NTP i Ping Watchdog
#include "EepromStorage.h"      // Upravlja I2C EEPROM-om (Logovi, Konfig, Lista Adresa)
#include "SpiFlashStorage.h"    // Upravlja SPI Flash-om (FW fajlovi)
#include "Rs485Service.h"       // Glavni dispecer zadatak za RS485
#include "HttpServer.h"         // Obradjuje HTTP CGI komande
#include "VirtualGpio.h"        // Upravlja stanjima za LED i Rasvjetu (za I2C Expander)
#include "TimeSync.h"           // Modul za slanje `SET_RTC_DATE_TIME` broadcast-a
#include "LogPullManager.h"     // Modul za `UPD_RC_STAT` i `UPD_LOG` logiku
#include "HttpQueryManager.h"   // Modul za blokirajuce `HTTP2RS485` upite
#include "UpdateManager.h"      // Modul za `UPDATE_FWR` logiku

/*
 * 2. KREIRANJE GLOBALNIH OBJEKATA ZA NASE SERVISE
 * (Pratimo nasu Nomenklaturu)
 */
NetworkManager    g_networkManager;
EepromStorage     g_eepromStorage;
SpiFlashStorage   g_spiFlashStorage;
VirtualGpio       g_virtualGpio;
TimeSync          g_timeSync;
LogPullManager    g_logPullManager;
HttpQueryManager  g_httpQueryManager;
UpdateManager     g_updateManager;
HttpServer        g_httpServer;

// Rs485Service je centralni dispecer i on ce interno kreirati svoj zadatak.
Rs485Service      g_rs485Service;


/**
 * @brief Glavna setup() funkcija, pokrece se jednom pri startu.
 */
void setup()
{
    // --- FAZA 1: Inicijalizacija Debug Porta ---
    // Koristimo Serial0 (GPIO1, GPIO3) za debug poruke u VS Code.
    // DWIN displej NE SMIJE biti spojen na TX0/RX0 tokom Faze 1.
    Serial.begin(SERIAL_DEBUG_BAUDRATE);
    delay(1000); // Pauza da se Serial Monitor stigne povezati
    Serial.println(F("==================================="));
    Serial.println(F("Pokretanje Hotel Controller ESP32..."));
    Serial.println(F("Arhitektura: v11 (DWIN + 1x W25Q512)"));
    Serial.println(F("Faza 1: Debug port aktivan na Serial0."));
    Serial.println(F("==================================="));

    // --- FAZA 2: Inicijalizacija Hardverskih Drajvera ---
    Serial.println(F("[setup] Inicijalizacija I2C (EEPROM)..."));
    // Proslijedjujemo I2C pinove iz ProjectConfig.h
    g_eepromStorage.Initialize(I2C_SDA_PIN, I2C_SCL_PIN);

    Serial.println(F("[setup] Inicijalizacija SPI (Flash)..."));
    // Proslijedjujemo SPI pinove iz ProjectConfig.h
    g_spiFlashStorage.Initialize(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_FLASH_CS_PIN);
    
    // --- FAZA 3: Inicijalizacija Logickih Modula ---
    Serial.println(F("[setup] Inicijalizacija Virtualnih GPIO..."));
    g_virtualGpio.Initialize(); // Postavlja virtuelne pinove na default

    Serial.println(F("[setup] Inicijalizacija Mreznog Menadzera..."));
    g_networkManager.Initialize(); // Pokrece ETH, WiFi, NTP...

    Serial.println(F("[setup] Inicijalizacija RS485 Menadzera..."));
    // Injektiramo zavisnosti (dependency injection) u nase Menadzere
    // Onaj koji je pozvan prvi dobija prvi prioritet kod dispecera
    g_rs485Service.Initialize(
        &g_httpQueryManager,    // P1: Blokirajuci HTTP upiti
        &g_updateManager,       // P2: Update firmvera
        &g_logPullManager,      // P3: Redovni polling logova
        &g_timeSync             // P4: Broadcast vremena
    );

    Serial.println(F("[setup] Inicijalizacija Sub-Modula..."));
    // Inicijalizujemo svaki modul i dajemo mu pokazivace na servise koji mu trebaju
    g_logPullManager.Initialize(&g_rs485Service, &g_eepromStorage);
    g_httpQueryManager.Initialize(&g_rs485Service);
    g_updateManager.Initialize(&g_rs485Service, &g_spiFlashStorage);
    g_timeSync.Initialize(&g_rs485Service);

    Serial.println(F("[setup] Inicijalizacija HTTP Servera..."));
    // HTTP Serveru trebaju Menadzeri koje ce pozivati
    g_httpServer.Initialize(&g_httpQueryManager, &g_updateManager, &g_eepromStorage);

    // --- FAZA 4: Pokretanje FreeRTOS Zadataka ---
    Serial.println(F("[setup] Pokretanje Rs485Service zadatka..."));
    g_rs485Service.StartTask(); // Ovo pokrece glavni Rs485 dispecer zadatak

    Serial.println(F("==================================="));
    Serial.println(F("[setup] Inicijalizacija zavrsena. Sistem radi."));
    Serial.println(F("==================================="));
}

/**
 * @brief Glavna Arduino petlja.
 * @note  Drzimo je sto praznijom. Koristi se samo za pozivanje
 * ne-blokirajucih funkcija koje se moraju vrtiti u glavnoj petlji.
 */
void loop()
{
    // NetworkManager.Loop() ce obradjivati Ping Watchdog
    // i provjeravati stanje mrezne konekcije.
    g_networkManager.Loop();

    // VirtualGpio.Loop() ce obradjivati blinkanje LED-a ako je potrebno
    g_virtualGpio.Loop();
    
    // Ostatak logike se desava u FreeRTOS zadacima (npr. g_rs485Service)
}
