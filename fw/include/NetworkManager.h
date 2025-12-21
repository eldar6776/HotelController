/**
 ******************************************************************************
 * @file    NetworkManager.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za NetworkManager modul.
 *
 * @note
 * Upravlja sa ETH, WiFi, NTP i Ping Watchdog.
 ******************************************************************************
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <WiFiManager.h>
#include "ProjectConfig.h"
#include "EepromStorage.h" // Za AppConfig

// Forward deklaracija za module koje pozivamo (iz main.cpp)
class HttpServer;

class NetworkManager
{
public:
    NetworkManager();
    void Initialize();
    void StartTask(); // NOVO: Metoda za pokretanje zadatka
    void Loop();
    bool IsNetworkConnected();
    bool IsInitializationComplete(); // NOVO: Getter za provjeru

private:
    // Metode za zadatak
    static void TaskWrapper(void* pvParameters);
    void RunTask();

    void InitializeETH();
    void InitializeWiFi();
    void InitializeNTP();
    void HandlePingWatchdog();
    bool PingGoogleDns(); // Nova privatna funkcija za izvornu Ping logiku

    // ETH Event Handler (staticka metoda)
    static void WiFiEvent(WiFiEvent_t event);
    static void EthEvent(WiFiEvent_t event);

    bool m_eth_connected;
    bool m_wifi_connected;
    unsigned long m_last_ping_time;
    int m_ping_failures;
    TaskHandle_t m_task_handle; // NOVO: Handle za zadatak
    bool m_initialization_complete; // NOVO: Flag da je inicijalizacija završena
};

#endif // NETWORK_MANAGER_H