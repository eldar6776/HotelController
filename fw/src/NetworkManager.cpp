/**
 ******************************************************************************
 * @file    NetworkManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija NetworkManager modula.
 ******************************************************************************
 */

#include "NetworkManager.h"
#include <esp_task_wdt.h> // Za Ping Watchdog

// Globalni objekti (ako su potrebni za callback-ove)
WiFiManager g_wifiManager;

NetworkManager::NetworkManager() : 
    m_eth_connected(false), 
    m_wifi_connected(false), 
    m_last_ping_time(0)
{
    // Konstruktor
}

/**
 * @brief Inicijalizuje mrežne interfejse (ETH i WiFi) i NTP.
 */
void NetworkManager::Initialize()
{
    Serial.println(F("[NetworkManager] Inicijalizacija..."));
    
    // Ovdje cemo dodati logiku za ETH.begin()
    InitializeETH();
    
    // Ako ETH ne uspije, pokreni WiFi
    if (!m_eth_connected)
    {
        InitializeWiFi();
    }

    // Ako je bilo sta konektovano, pokreni NTP
    if (m_eth_connected || m_wifi_connected)
    {
        InitializeNTP();
    }
}

/**
 * @brief Glavna petlja modula, poziva se iz main loop().
 */
void NetworkManager::Loop()
{
    // Provjerava WiFi reset dugme (WLAN_RST_BTN_PIN)
    // ...

    // Odrzava Ping Watchdog
    HandlePingWatchdog();
}

/**
 * @brief Inicijalizuje Ethernet.
 */
void NetworkManager::InitializeETH()
{
    Serial.println(F("[NetworkManager] Pokusaj konekcije preko Etherneta..."));
    // TODO: Implementirati ETH.begin() sa statickom IP adresom
    // ucitati adresu iz EepromStorage
    // m_eth_connected = true;
}

/**
 * @brief Inicijalizuje WiFi (WiFiManager).
 */
void NetworkManager::InitializeWiFi()
{
    Serial.println(F("[NetworkManager] ETH neuspjesan. Pokretanje WiFiManager-a..."));
    g_wifiManager.autoConnect("HotelControllerSetup");
    // TODO: Provjeriti status konekcije
    // m_wifi_connected = true;
}

/**
 * @brief Inicijalizuje NTP sinhronizaciju vremena.
 */
void NetworkManager::InitializeNTP()
{
    Serial.println(F("[NetworkManager] Konfigurisanje NTP-a..."));
    // Koristimo ugradjenu funkciju
    configTzTime(TIMEZONE_STRING, NTP_SERVER_1, NTP_SERVER_2);
}

/**
 * @brief Provjerava konekciju i restartuje ako je potrebno.
 */
void NetworkManager::HandlePingWatchdog()
{
    // TODO: Implementirati logiku za Ping Watchdog
    if (millis() - m_last_ping_time > PING_INTERVAL_MS)
    {
        m_last_ping_time = millis();
        // ... logika za ping ...
    }
}

/**
 * @brief Vraca true ako je bilo koji mrezni interfejs konektovan.
 */
bool NetworkManager::IsNetworkConnected()
{
    return m_eth_connected || m_wifi_connected;
}
