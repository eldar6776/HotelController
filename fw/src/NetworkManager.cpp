/**
 ******************************************************************************
 * @file    NetworkManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija NetworkManager modula.
 ******************************************************************************
 */

#include "NetworkManager.h"
#include "VirtualGpio.h" 
#include <esp_task_wdt.h> 
#include <WiFi.h> // Za WiFiClient
#include <cstring> 
#include "ProjectConfig.h" 

// Potrebno da bi statičke metode mogle pristupiti instanci
extern NetworkManager g_networkManager; 

// Globalni objekti (extern deklaracije)
extern AppConfig g_appConfig; 
extern VirtualGpio g_virtualGpio; 
WiFiManager g_wifiManager;

// Konstante preuzete iz ProjectConfig.h
#define MAX_PING_FAILURES 10 
#define PING_INTERVAL_MS 60000 

NetworkManager::NetworkManager() : 
    m_eth_connected(false), 
    m_wifi_connected(false), 
    m_last_ping_time(0),
    m_ping_failures(0) 
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
        g_virtualGpio.SetStatusLed(LedState::LED_BLINK_SLOW);
        g_networkManager.m_eth_connected = false; // Korišćenje g_networkManager
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.printf("[NetworkManager:ETH] IP: %s\r\n", ETH.localIP().toString().c_str());
        Serial.printf("[NetworkManager:ETH] MAC: %s\r\n", ETH.macAddress().c_str());
        g_virtualGpio.SetStatusLed(LedState::LED_ON);
        g_networkManager.m_eth_connected = true; // Korišćenje g_networkManager
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
        g_virtualGpio.SetStatusLed(LedState::LED_ON);
        g_networkManager.InitializeNTP();
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println(F("[NetworkManager:WiFi] WiFi Disconnected"));
        g_networkManager.m_wifi_connected = false; // Korišćenje g_networkManager
        g_virtualGpio.SetStatusLed(LedState::LED_BLINK_SLOW);
        break;
    default:
        break;
    }
}


/**
 * @brief Inicijalizuje mrežne interfejse (ETH i WiFi) i NTP.
 */
void NetworkManager::Initialize()
{
    Serial.println(F("[NetworkManager] Inicijalizacija..."));
    
    // Podesi Event Handlere
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_START);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_CONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_DISCONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiEvent, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    
    InitializeETH();
    
    if (!m_eth_connected)
    {
        InitializeWiFi();
    }
}

/**
 * @brief Glavna petlja modula, poziva se iz main loop().
 */
void NetworkManager::Loop()
{
    // Upravlja WLAN_RST_BTN (GPIO32) za reset Wi-Fi postavki.
    if (digitalRead(WIFI_RST_BTN_PIN) == LOW)
    {
        Serial.println(F("[NetworkManager] WiFi Reset Dugme pritisnuto. Brisem WiFi konfiguraciju..."));
        g_wifiManager.resetSettings();
        delay(100);
        ESP.restart(); 
    }

    // Odrzava Ping Watchdog
    HandlePingWatchdog();
}

/**
 * @brief Inicijalizuje Ethernet. (Bazirano na pinoutu iz ProjectConfig.h i statičkoj IP)
 */
void NetworkManager::InitializeETH()
{
    Serial.println(F("[NetworkManager] Pokusaj konekcije preko Etherneta..."));

    ETH.setHostname("HC-ESP32");
    
    IPAddress localIP(g_appConfig.ip_address);
    IPAddress gateway(g_appConfig.gateway);
    IPAddress subnet(g_appConfig.subnet_mask);
    
    ETH.begin(ETH_PHY_ADDR, ETH_POWER_PIN, ETH_PHY_TYPE, ETH_CLK_MODE);
    
    ETH.config(localIP, gateway, subnet);

    delay(500);
}

/**
 * @brief Inicijalizuje WiFi (WiFiManager) kao backup.
 */
void NetworkManager::InitializeWiFi()
{
    Serial.println(F("[NetworkManager] ETH neuspjesan. Pokretanje WiFiManager-a..."));
    
    g_virtualGpio.SetStatusLed(LedState::LED_BLINK_FAST); 
    
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