/**
 ******************************************************************************
 * @file    LogPullManager.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header za LogPullManager modul.
 *
 * @note
 * Upravlja redovnim pollingom (UPD_RC_STAT) i skupljanjem logova (UPD_LOG).
 * Implementira IRs485Manager interfejs.
 ******************************************************************************
 */

#ifndef LOG_PULL_MANAGER_H
#define LOG_PULL_MANAGER_H

#include "Rs485Service.h"
#include "EepromStorage.h"

class LogPullManager : public IRs485Manager
{
public:
    LogPullManager();
    void Initialize(Rs485Service* pRs485Service, EepromStorage* pEepromStorage);

    // Implementacija IRs485Manager interfejsa
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;

private:
    void SendStatusRequest(uint16_t address);
    void SendLogRequest(uint16_t address);
    uint16_t GetNextAddress();

    Rs485Service* m_rs485_service;
    EepromStorage* m_eeprom_storage;
    
    enum class PullState
    {
        IDLE,
        WAITING_FOR_STATUS_ACK,
        WAITING_FOR_LOG_ACK
    };

    PullState m_state;
    uint16_t m_current_address_index;
    uint16_t m_current_pull_address;
    uint16_t m_address_list[MAX_ADDRESS_LIST_SIZE];
    uint16_t m_address_list_count;
};

#endif // LOG_PULL_MANAGER_H
