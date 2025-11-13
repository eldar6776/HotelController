/**
 ******************************************************************************
 * @file    NetworkManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija NetworkManager modula.
 ******************************************************************************
 */
#include "DebugConfig.h"
#include "NetworkManager.h"
// Uključujemo sve servise koje ćemo pokretati
#include "HttpServer.h"
#include "Rs485Service.h"
#include "HttpQueryManager.h"
#include "UpdateManager.h"
#include "LogPullManager.h"
#include "TimeSync.h"
#include <esp_task_wdt.h> 
#include <WiFi.h> // Za WiFiClient
#include <cstring> 
#include "ProjectConfig.h" 

// Potrebno da bi statičke metode mogle pristupiti instanci
extern NetworkManager g_networkManager; 
// Uključujemo sve globalne objekte servisa
extern HttpServer g_httpServer;
extern Rs485Service g_rs485Service;
extern HttpQueryManager g_httpQueryManager;
extern UpdateManager g_updateManager;
extern LogPullManager g_logPullManager;
extern TimeSync g_timeSync;

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
    m_http_server(NULL),
    m_initialization_complete(false) // NOVO: Inicijalizacija na false
{
    // Konstruktor
}

// --- ETH Event Handler (Staticka metoda) ---
void NetworkManager::EthEvent(WiFiEvent_t event)
{
    LOG_DEBUG(5, "[EthEvent] Primljen event: %d\n", event);
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
        
        LOG_DEBUG(4, "[EthEvent] ETH dobio IP. Pokrećem NTP...\n");
        // ONEMOGUĆENO: Pokretanje servisa je prebačeno na kraj RunTask(). Samo NTP ostaje ovde.
        g_networkManager.InitializeNTP();
        break;
    default:
        break;
    }
}

// --- WiFi Event Handler (Staticka metoda) ---
void NetworkManager::WiFiEvent(WiFiEvent_t event)
{
    LOG_DEBUG(5, "[WiFiEvent] Primljen event: %d\n", event);
    switch (event)
    {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.printf("[NetworkManager:WiFi] IP: %s\r\n", WiFi.localIP().toString().c_str());
        g_networkManager.m_wifi_connected = true; // Korišćenje g_networkManager

        LOG_DEBUG(4, "[WiFiEvent] WiFi dobio IP. Pokrećem NTP...\n");
        // ONEMOGUĆENO: Pokretanje servisa je prebačeno na kraj RunTask(). Samo NTP ostaje ovde.
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
    LOG_DEBUG(5, "[NetworkManager] Entering Initialize()...\n");
    
    // VRAĆAMO ETHERNET EVENT HANDLERE
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_START);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_CONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_DISCONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // KONAČNA ISPRAVKA: Aktiviraj interni pull-up otpornik za reset dugme
    // da se spriječi lažna detekcija pritiska zbog "plutajućeg" pina.
    pinMode(WIFI_RST_BTN_PIN, INPUT_PULLUP);
    LOG_DEBUG(5, "[NetworkManager] Exiting Initialize().\n");
}

/**
 * @brief Pokreće FreeRTOS zadatak za mrežnu inicijalizaciju.
 */
void NetworkManager::StartTask()
{
    LOG_DEBUG(5, "[NetworkManager] Entering StartTask()...\n");
    xTaskCreate(
        TaskWrapper,
        "NetworkManagerTask",
        4096,
        this,
        5,
        &m_task_handle
    );
    LOG_DEBUG(5, "[NetworkManager] Exiting StartTask().\n");
}

void NetworkManager::TaskWrapper(void* pvParameters)
{
    static_cast<NetworkManager*>(pvParameters)->RunTask();
}

void NetworkManager::RunTask()
{
    LOG_DEBUG(5, "[NetworkManager] Entering RunTask()...\n");
    
    // Vraćamo punu logiku: prvo Ethernet, pa WiFi kao fallback
    InitializeETH();

    LOG_DEBUG(4, "[NetworkManager] Čekam na Ethernet konekciju (do 10s)...\n");
    for (int i = 0; i < 20; i++) {
        if (m_eth_connected) break;
        delay(500);
    }

    if (!m_eth_connected)
    {
        LOG_DEBUG(3, "[NetworkManager] Ethernet nije uspio. Pokrećem WiFi...\n");
        InitializeWiFi();
    }
    
    m_initialization_complete = true;
    LOG_DEBUG(3, "[NetworkManager] Mrežna inicijalizacija završena.\n");

    // ========================================================================
    // KONAČNA ISPRAVKA: Pokretanje svih servisa NAKON što je mrežna
    // inicijalizacija (ETH ili WiFi) potpuno završena.
    // Ovo je jedino sigurno mesto za pokretanje.
    // ========================================================================
    LOG_DEBUG(3, "[NetworkManager] Pokretanje sistemskih servisa...\n");

    // 1. RS485 se već inicijalizuje u setup() funkciji.
    // Ovdje ne radimo ništa po tom pitanju.

    // 2. Pokreni HTTP Server ako smo povezani na mrežu
    if (IsNetworkConnected()) {
        LOG_DEBUG(5, "[NetworkManager] -> Pokretanje HttpServer...\n");
        g_httpServer.Start();
    }
    LOG_DEBUG(3, "[NetworkManager] Svi servisi pokrenuti. Mrežni zadatak ulazi u idle mod.\n");

    // ========================================================================
    // KONAČNA ISPRAVKA: ZADATAK NE SME DA SE ZAVRŠI!
    // Ako se zadatak koji je inicijalizovao ETH završi, njegov stek se
    // oslobađa, što dovodi do pada sistema jer ETH drajver na Core 1
    // gubi resurse. Zato uvodimo beskonačnu petlju koja drži zadatak "živim".
    // ========================================================================
    while (true)
    {
        // Ovaj zadatak je završio svoj posao. Sada može da miruje i ne troši CPU.
        vTaskDelay(pdMS_TO_TICKS(10000)); // Spavaj 10 sekundi
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

    // ONEMOGUĆENO: Logika za reset dugme je privremeno isključena.
    // Problem je bio što je pin "plutao" i izazivao lažne resete.
    // Dugme će biti implementirano na drugi način.
}

/**
 * @brief Inicijalizuje Ethernet. (Bazirano na pinoutu iz ProjectConfig.h i statičkoj IP)
 */
void NetworkManager::InitializeETH()
{
    LOG_DEBUG(5, "[NetworkManager] Entering InitializeETH()...\n");

    IPAddress localIP(g_appConfig.ip_address);
    IPAddress gateway(g_appConfig.gateway);
    IPAddress subnet(g_appConfig.subnet_mask);
    
    // =================================================================================================
    // KLJUČNA ISPRAVKA: Ručni reset PHY čipa PRIJE poziva ETH.begin().
    // Ovo osigurava da je čip u poznatom stanju i rješava probleme sa inicijalizacijom
    // koji su uzrokovali pad sistema.
    if (ETH_RESET_PIN >= 0) 
    {
        LOG_DEBUG(4, "[NetworkManager] Izvršavam ručni reset PHY čipa...\n");
        pinMode(ETH_RESET_PIN, OUTPUT);
        digitalWrite(ETH_RESET_PIN, LOW);
        delay(200);
        digitalWrite(ETH_RESET_PIN, HIGH);
        delay(200); // Dajemo čipu vremena da se stabilizuje
    }
    // =================================================================================================

    LOG_DEBUG(4, "[NetworkManager] Pozivam ETH.begin()...\n");
    // =================================================================================================
    // KONAČNA ISPRAVKA 2: Vraćamo ETH_POWER_PIN na -1. Iako se ne koristi, funkcija zahtijeva
    // argument na toj poziciji. Ovo osigurava ispravan redoslijed svih ostalih argumenata.
    bool eth_success = ETH.begin(ETH_PHY_ADDR, ETH_POWER_PIN, ETH_MDC_PIN, ETH_MDIO_PIN, ETH_PHY_TYPE, ETH_CLK_MODE);
    if (eth_success)
    {
        Serial.println(F("[NetworkManager] ETH.begin() USPJEŠNO IZVRŠEN. Konfigurišem IP..."));
        LOG_DEBUG(5, "[NetworkManager] -> Pozivam ETH.config()...\n");
        ETH.config(localIP, gateway, subnet); // Konfiguracija statičke IP adrese

        // ISPRAVKA: setHostname se poziva NAKON uspješnog ETH.begin()
        LOG_DEBUG(5, "[NetworkManager] -> Provjeravam mdns_name...\n");
        char safe_hostname[32];
        strncpy(safe_hostname, g_appConfig.mdns_name, sizeof(safe_hostname) - 1);
        safe_hostname[sizeof(safe_hostname) - 1] = '\0';
        
        if (strlen(safe_hostname) > 0) {
            ETH.setHostname(safe_hostname);
            LOG_DEBUG(3, "[NetworkManager] Hostname postavljen na: %s\n", safe_hostname);
        } else {
            LOG_DEBUG(2, "[NetworkManager] Hostname nije postavljen (prazan string). Preskačem ETH.setHostname().\n");
        }
    }
    else
    {
        LOG_DEBUG(1, "[NetworkManager] !!! GREŠKA: ETH.begin() vratio 'false'. ETH inicijalizacija neuspješna. Provjerite hardver i pinove.\n");
    }
    LOG_DEBUG(5, "[NetworkManager] Exiting InitializeETH().\n");
}

/**
 * @brief Inicijalizuje WiFi (WiFiManager) kao backup.
 */
void NetworkManager::InitializeWiFi()
{
    LOG_DEBUG(5, "[NetworkManager] Entering InitializeWiFi()...\n");
    LOG_DEBUG(3, "[NetworkManager] Pokretanje WiFiManager-a...\n");
    
    g_wifiManager.autoConnect("HotelControllerSetup");
    
    if (WiFi.isConnected())
    {
        m_wifi_connected = true;
    }
    LOG_DEBUG(5, "[NetworkManager] Exiting InitializeWiFi().\n");
}

/**
 * @brief Inicijalizuje NTP sinhronizaciju vremena.
 */
void NetworkManager::InitializeNTP()
{
    LOG_DEBUG(5, "[NetworkManager] Entering InitializeNTP()...\n");
    LOG_DEBUG(3, "[NetworkManager] Konfigurisanje NTP-a...\n");
    
    configTzTime(TIMEZONE_STRING, NTP_SERVER_1, NTP_SERVER_2);
    LOG_DEBUG(5, "[NetworkManager] Exiting InitializeNTP().\n");
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

/**
 * @brief Vraca true ako je inicijalizacioni zadatak završen.
 */
bool NetworkManager::IsInitializationComplete()
{
    return m_initialization_complete;
}