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

class LogPullManager
{
public:
    /**
     * @brief Konstruktor.
     */
    LogPullManager();

    /**
     * @brief Inicijalizuje menadžera.
     * @param pRs485Service Pointer na RS485 servis.
     * @param pEepromStorage Pointer na EEPROM storage.
     */
    void Initialize(Rs485Service* pRs485Service, EepromStorage* pEepromStorage);

    /**
     * @brief Izvršava ciklus prikupljanja logova (polling).
     */
    void Run();

private:
    void ProcessResponse(uint8_t* packet, uint16_t length);
    void SendStatusRequest(uint16_t address);
    void SendDeleteLogRequest(uint16_t address);
    void SendLogRequest(uint16_t address);
    uint16_t GetNextAddress();
    
    // HILLS Protocol helpers
    bool IsHillsProtocol();
    uint8_t GetStatusCommand();
    uint8_t GetLogCommand();
    uint8_t GetDeleteCommand();
    uint32_t GetResponseTimeout();
    uint32_t GetRxTxDelay();

    Rs485Service* m_rs485_service;
    EepromStorage* m_eeprom_storage;
    
    enum class PullState
    {
        IDLE,
        SENDING_STATUS_REQUEST,
        SENDING_LOG_REQUEST,
        WAITING_FOR_RESPONSE,
        WAITING_FOR_DELETE_CONFIRMATION  // HILLS: wait for DELETE ACK
    };

    PullState m_state;
    uint16_t m_current_address_index;
    uint16_t m_current_pull_address;
    uint16_t m_address_list[MAX_ADDRESS_LIST_SIZE];
    uint16_t m_address_list_count;
    uint8_t m_retry_count;
    uint8_t m_hills_query_attempts;  // HILLS ping-pong counter
    unsigned long m_last_activity_time;
};

#endif // LOG_PULL_MANAGER_H
