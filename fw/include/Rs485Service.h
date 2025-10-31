/**
 ******************************************************************************
 * @file    Rs485Service.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za Rs485Service modul (Dispecer).
 ******************************************************************************
 */

#ifndef RS485_SERVICE_H
#define RS485_SERVICE_H

#include <Arduino.h>
#include <freertos/task.h> 
#include "ProjectConfig.h" // Ukljucuje RS485_BUFFER_SIZE

// Apstraktni interfejs (ugovor) koji svaki Menadzer mora postovati
class IRs485Manager
{
public:
    virtual ~IRs485Manager() {}
    virtual void Service() = 0; 
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) = 0; 
    virtual void OnTimeout() = 0; 
};

enum class Rs485State
{
    IDLE,
    SENDING,
    WAITING,
    RECEIVING,
    TIMEOUT
};

class Rs485Service
{
public:
    Rs485Service();
    void Initialize(
        IRs485Manager* pHttpQueryManager,
        IRs485Manager* pUpdateManager,
        IRs485Manager* pLogPullManager,
        IRs485Manager* pTimeSync
    );
    void StartTask();

    bool RequestBusAccess(IRs485Manager* manager);
    bool SendPacket(uint8_t* data, uint16_t length);
    void ReleaseBusAccess(IRs485Manager* manager);
    
private:
    static void TaskWrapper(void* pvParameters);
    void Run(); 
    void Dispatch();
    void HandleReceive();
    bool ValidatePacket(uint8_t* buffer, uint16_t length);
    uint16_t CalculateChecksum(uint8_t* buffer, uint16_t length); // Dodano za kompletnost

    TaskHandle_t m_task_handle;
    HardwareSerial m_rs485_serial;

    IRs485Manager* m_http_query_manager;
    IRs485Manager* m_update_manager;
    IRs485Manager* m_log_pull_manager;
    IRs485Manager* m_time_sync;

    IRs485Manager* m_current_bus_owner;
    Rs485State m_state;
    unsigned long m_timeout_start; 

    // Koristi fiksnu velicinu
    uint8_t m_rx_buffer[RS485_BUFFER_SIZE]; 
    uint16_t m_rx_count;
};

#endif // RS485_SERVICE_H