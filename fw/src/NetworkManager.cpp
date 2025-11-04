/**
 ******************************************************************************
 * @file    NetworkManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija NetworkManager modula.
 ******************************************************************************
 */

#include "NetworkManager.h"
#include "HttpServer.h"     // NOVO: Uključujemo punu definiciju HttpServer klase
#include <esp_task_wdt.h> 
#include <WiFi.h> // Za WiFiClient
#include <cstring> 
#include "ProjectConfig.h" 

// Potrebno da bi statičke metode mogle pristupiti instanci
extern NetworkManager g_networkManager; 
extern HttpServer g_httpServer; // NOVO: Treba nam pristup globalnom objektu

// Globalni objekti (extern deklaracije)
extern AppConfig g_appConfig; 
WiFiManager g_wifiManager;

// Konstante preuzete iz ProjectConfig.h
#define MAX_PING_FAILURES 10 
#define PING_INTERVAL_MS 60000 

NetworkManager::NetworkManager() : 
    m_eth_connected(false), 
    m_wifi_connected(false), 
    m_last_ping_time(0),
    m_ping_failures(0),
    m_task_handle(NULL),
    m_http_server(NULL)
{
    // Konstruktor
}

// --- ETH Event Handler (Staticka metoda) ---
void NetworkManager::EthEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_ETH_START:
        Serial.println(F("[NetworkManager:ETH] Ethernet Started"));
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println(F("[NetworkManager:ETH] Ethernet Link Up"));
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println(F("[NetworkManager:ETH] Ethernet Link Down"));
        g_networkManager.m_eth_connected = false; // Korišćenje g_networkManager
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.printf("[NetworkManager:ETH] IP: %s\r\n", ETH.localIP().toString().c_str());
        Serial.printf("[NetworkManager:ETH] MAC: %s\r\n", ETH.macAddress().c_str());
        g_networkManager.m_eth_connected = true; // Korišćenje g_networkManager
        
        // NOVO: Pokreni HTTP server SADA kada imamo IP adresu
        if (g_networkManager.m_http_server) {
            g_networkManager.m_http_server->Start();
        }

        g_networkManager.InitializeNTP();
        break;
    default:
        break;
    }
}

// --- WiFi Event Handler (Staticka metoda) ---
void NetworkManager::WiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[NetworkManager:WiFi] IP: %s\r\n", WiFi.localIP().toString().c_str());
        g_networkManager.m_wifi_connected = true; // Korišćenje g_networkManager

        // NOVO: Pokreni HTTP server SADA kada imamo IP adresu
        if (g_networkManager.m_http_server) {
            g_networkManager.m_http_server->Start();
        }

        g_networkManager.InitializeNTP();
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println(F("[NetworkManager:WiFi] WiFi Disconnected"));
        g_networkManager.m_wifi_connected = false; // Korišćenje g_networkManager
        break;
    default:
        break;
    }
}

void NetworkManager::SetHttpServer(HttpServer* httpServer)
{
    m_http_server = httpServer;
}


/**
 * @brief Inicijalizuje mrežne interfejse (ETH i WiFi) i NTP.
 */
void NetworkManager::Initialize()
{
    Serial.println(F("[NetworkManager] Inicijalizacija..."));
    
    // Podesi Event Handlere - OVO OSTAJE OVDJE
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_START);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_CONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_DISCONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // KONAČNA ISPRAVKA: Aktiviraj interni pull-up otpornik za reset dugme
    // da se spriječi lažna detekcija pritiska zbog "plutajućeg" pina.
    pinMode(WIFI_RST_BTN_PIN, INPUT_PULLUP);
}

/**
 * @brief Pokreće FreeRTOS zadatak za mrežnu inicijalizaciju.
 */
void NetworkManager::StartTask()
{
    Serial.println(F("[NetworkManager] Pokretanje mrežnog zadatka..."));
    xTaskCreate(
        TaskWrapper,
        "NetworkManagerTask",
        4096,
        this,
        5,
        &m_task_handle
    );
}

void NetworkManager::TaskWrapper(void* pvParameters)
{
    static_cast<NetworkManager*>(pvParameters)->RunTask();
}

void NetworkManager::RunTask()
{
    // Inicijalizacija se sada dešava unutar zadatka
    InitializeETH();

    // KONAČNA ISPRAVKA: Sačekaj do 10 sekundi da se ETH konektuje
    // prije nego što pokreneš WiFi kao rezervu.
    Serial.println(F("[NetworkManager] Čekam na Ethernet konekciju (do 10s)..."));
    for (int i = 0; i < 20; i++) {
        if (m_eth_connected) break;
        delay(500);
    }

    if (!m_eth_connected)
    {
        Serial.println(F("[NetworkManager] Ethernet nije uspio. Pokrećem WiFi..."));
        InitializeWiFi();
    }
}

/**
 * @brief Glavna petlja modula, poziva se iz main loop().
 */
void NetworkManager::Loop()
{
    // Ova funkcija se više ne koristi za inicijalizaciju,
    // ali je ostavljamo za buduće ne-blokirajuće operacije ako zatrebaju.
    // Ping watchdog se sada može pozvati iz RunTask() petlje.

    // Ostavljamo provjeru za reset dugme ovdje, jer loop() se i dalje poziva.
   //f (digitalRead(WIFI_RST_BTN_PIN) == LOW)
   //
   //   Serial.println(F("[NetworkManager] WiFi Reset Dugme pritisnuto. Brisem WiFi konfiguraciju..."));
   //   g_wifiManager.resetSettings();
   //   delay(100);
   //   ESP.restart(); 
   //
}

/**
 * @brief Inicijalizuje Ethernet. (Bazirano na pinoutu iz ProjectConfig.h i statičkoj IP)
 */
void NetworkManager::InitializeETH()
{
    Serial.println(F("[NetworkManager] Pokusaj konekcije preko Etherneta..."));

    IPAddress localIP(g_appConfig.ip_address);
    IPAddress gateway(g_appConfig.gateway);
    IPAddress subnet(g_appConfig.subnet_mask);
    
    // =================================================================================================
    // KONAČNA ISPRAVKA: Ručni reset PHY čipa PRIJE poziva ETH.begin().
    // Ovo osigurava da je čip u poznatom stanju i rješava "power up timeout".
    pinMode(ETH_RESET_PIN, OUTPUT);
    digitalWrite(ETH_RESET_PIN, LOW);
    delay(200);
    digitalWrite(ETH_RESET_PIN, HIGH);
    // =================================================================================================

    Serial.println(F("[NetworkManager] Pozivam ETH.begin()..."));
    // =================================================================================================
    // KONAČNA ISPRAVKA 2: Vraćamo ETH_POWER_PIN na -1. Iako se ne koristi, funkcija zahtijeva
    // argument na toj poziciji. Ovo osigurava ispravan redoslijed svih ostalih argumenata.
    bool eth_success = ETH.begin(ETH_PHY_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_TYPE, ETH_CLK_MODE);

    if (eth_success)
    {
        Serial.println(F("[NetworkManager] ETH.begin() USPJEŠNO IZVRŠEN. Konfigurišem IP..."));
        ETH.config(localIP, gateway, subnet); // Konfiguracija statičke IP adrese

        // ISPRAVKA: setHostname se poziva NAKON uspješnog ETH.begin()
        Serial.println(F("[NetworkManager] Provjeravam mdns_name..."));
        char safe_hostname[32];
        strncpy(safe_hostname, g_appConfig.mdns_name, sizeof(safe_hostname) - 1);
        safe_hostname[sizeof(safe_hostname) - 1] = '\0';
        
        if (strlen(safe_hostname) > 0) {
            ETH.setHostname(safe_hostname);
            Serial.printf("[NetworkManager] Hostname postavljen na: %s\n", safe_hostname);
        } else {
            Serial.println(F("[NetworkManager] Hostname nije postavljen (prazan string). Preskačem ETH.setHostname()."));
        }
    }
    else
    {
        Serial.println(F("[NetworkManager] !!! GREŠKA: ETH.begin() vratio 'false'. ETH inicijalizacija neuspješna. Provjerite hardver i pinove."));
    }
}

/**
 * @brief Inicijalizuje WiFi (WiFiManager) kao backup.
 */
void NetworkManager::InitializeWiFi()
{
    Serial.println(F("[NetworkManager] ETH neuspjesan. Pokretanje WiFiManager-a..."));
    
    g_wifiManager.autoConnect("HotelControllerSetup");
    
    if (WiFi.isConnected())
    {
        m_wifi_connected = true;
    }
}

/**
 * @brief Inicijalizuje NTP sinhronizaciju vremena.
 */
void NetworkManager::InitializeNTP()
{
    Serial.println(F("[NetworkManager] Konfigurisanje NTP-a..."));
    
    configTzTime(TIMEZONE_STRING, NTP_SERVER_1, NTP_SERVER_2);
}

/**
 * @brief Izvorni mehanizam za Ping (pokušaj TCP konekcije na Google DNS: 8.8.8.8:53).
 */
bool NetworkManager::PingGoogleDns()
{
    WiFiClient client;
    return client.connect("8.8.8.8", 53);
}

/**
 * @brief Pokreće Ping Watchdog za provjeru konekcije i restart po potrebi.
 */
void NetworkManager::HandlePingWatchdog()
{
    if (!IsNetworkConnected())
    {
        m_last_ping_time = millis();
        return;
    }

    if (millis() - m_last_ping_time >= PING_INTERVAL_MS)
    {
        m_last_ping_time = millis();

        if (PingGoogleDns())
        {
            m_ping_failures = 0; 
            Serial.println("[PingWdg] Google Response OK...");
        }
        else
        {
            m_ping_failures++; 
            Serial.printf("[PingWdg] Ping failed (%d/%d)\n", m_ping_failures, MAX_PING_FAILURES);
            
            if (m_ping_failures >= MAX_PING_FAILURES)
            {
                Serial.println("[PingWdg] Max failures reached. Restarting...");
                ESP.restart();
            }
        }
    }
}

/**
 * @brief Vraca true ako je bilo koji mrezni interfejs konektovan.
 */
bool NetworkManager::IsNetworkConnected()
{
    return m_eth_connected || m_wifi_connected;
}