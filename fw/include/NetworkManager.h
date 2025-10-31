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

class NetworkManager
{
public:
    NetworkManager();
    void Initialize();
    void Loop();
    bool IsNetworkConnected();

private:
    void InitializeETH();
    void InitializeWiFi();
    void InitializeNTP();
    void HandlePingWatchdog();

    bool m_eth_connected;
    bool m_wifi_connected;
    unsigned long m_last_ping_time;
};

#endif // NETWORK_MANAGER_H
