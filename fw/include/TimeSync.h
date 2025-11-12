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

class TimeSync : public IRs485Manager
{
public:
    TimeSync();
    void Initialize(Rs485Service* pRs485Service);

    // Implementacija IRs485Manager interfejsa
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;
    bool WantsBus() override;
    const char* Name() const override;
    uint32_t GetTimeoutMs() const override;

private:
    void SendTimeBroadcast();

    Rs485Service* m_rs485_service;
    unsigned long m_last_sync_time;
};

#endif // TIME_SYNC_H