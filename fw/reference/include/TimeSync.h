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
    TimeSync();
    void Initialize(Rs485Service* pRs485Service);
    void Run();
    bool IsTimeToSync();
    void ResetTimer();

private:
    void SendTimeBroadcast();

    Rs485Service* m_rs485_service;
    unsigned long m_last_sync_time;
};

#endif // TIME_SYNC_H