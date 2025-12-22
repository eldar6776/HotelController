/**
 ******************************************************************************
 * @file    TimeSync.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header za TimeSync modul.
 *
 * @note
 * Salje `SET_RTC_DATE_TIME` broadcast periodično.
 * Implementira IRs485Manager interfejs.
 ******************************************************************************
 */

#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include "Rs485Service.h" // Za IRs485Manager
#include "ProjectConfig.h"
#include "EepromStorage.h" // Za g_appConfig

class TimeSync
{
public:
    /**
     * @brief Konstruktor.
     */
    TimeSync();

    /**
     * @brief Inicijalizuje TimeSync modul.
     * @param pRs485Service Pointer na RS485 servis.
     */
    void Initialize(Rs485Service* pRs485Service);

    /**
     * @brief Provjerava i šalje broadcast vremena ako je potrebno.
     */
    void Run();

    /**
     * @brief Provjerava da li je vrijeme za sinhronizaciju.
     * @return true ako je interval istekao.
     */
    bool IsTimeToSync();

    /**
     * @brief Resetuje tajmer za sinhronizaciju.
     */
    void ResetTimer();

private:
    void SendTimeBroadcast();

    Rs485Service* m_rs485_service;
    unsigned long m_last_sync_time;
};

#endif // TIME_SYNC_H