/**
 ******************************************************************************
 * @file    main.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Glavni ulazni fajl za Hotel Controller ESP32
 ******************************************************************************
 */

#include <Arduino.h>

// Ukljucivanje svih Headera (za globalne objekte)
#include "ProjectConfig.h"   
#include "NetworkManager.h"    
#include "EepromStorage.h"     
#include "SpiFlashStorage.h"   
#include "Rs485Service.h"      
#include "HttpServer.h"        
#include "VirtualGpio.h"       
#include "HttpQueryManager.h"
#include "LogPullManager.h"
#include "TimeSync.h"
#include "UpdateManager.h"

// Deklaracija globalnih objekata
NetworkManager g_networkManager;
EepromStorage g_eepromStorage;
SpiFlashStorage g_spiFlashStorage;
Rs485Service g_rs485Service;
HttpServer g_httpServer;
VirtualGpio g_virtualGpio;
HttpQueryManager g_httpQueryManager;
LogPullManager g_logPullManager;
TimeSync g_timeSync;
UpdateManager g_updateManager;

void setup() 
{
    // --- FAZA 1: Inicijalizacija Serijske Komunikacije ---
    Serial.begin(SERIAL_DEBUG_BAUDRATE); // Rjesava gresku: SERIAL_DEBUG_BAUDRATE
    while (!Serial && millis() < 2000) {} 
    Serial.println(F("==================================="));
    Serial.println(F("Hotel Controller ESP32 - Pokretanje"));
    Serial.println(F("==================================="));
    
    // --- FAZA 2: Inicijalizacija HW Drajvera ---
    Serial.println(F("[setup] Inicijalizacija HW Drajvera...\r\n"));

    g_virtualGpio.Initialize(); 
    
    // Rjesava greske pinova: SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN
    g_spiFlashStorage.Initialize(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_FLASH_CS_PIN); 

    g_eepromStorage.Initialize(I2C_SDA_PIN, I2C_SCL_PIN);

    g_networkManager.Initialize(); // ETH/WiFi/NTP

    // --- FAZA 3: Inicijalizacija Sub-Modula ---
    Serial.println(F("[setup] Inicijalizacija Sub-Modula...\r\n"));

    // Proslijedjujemo pokazivace
    g_logPullManager.Initialize(&g_rs485Service, &g_eepromStorage);
    g_httpQueryManager.Initialize(&g_rs485Service);
    g_updateManager.Initialize(&g_rs485Service, &g_spiFlashStorage);
    g_timeSync.Initialize(&g_rs485Service);

    Serial.println(F("[setup] Inicijalizacija HTTP Servera..."));
    g_httpServer.Initialize(&g_httpQueryManager, &g_updateManager, &g_eepromStorage);

    // --- FAZA 4: Pokretanje FreeRTOS Zadataka ---
    Serial.println(F("[setup] Pokretanje Rs485Service zadatka..."));
    g_rs485Service.StartTask();

    Serial.println(F("==================================="));
    Serial.println(F("[setup] Inicijalizacija zavrsena. Sistem radi."));
    Serial.println(F("==================================="));
}

void loop() 
{
    // Glavna petlja: koristimo samo za ne-blokirajuce Loop funkcije
    g_virtualGpio.Loop();
    g_networkManager.Loop();
    // Ostali menadzeri su u FreeRTOS zadacima
    delay(1); 
}