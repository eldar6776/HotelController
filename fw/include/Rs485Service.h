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
#include "ProjectConfig.h"

// Apstraktni interfejs (ugovor) koji svaki Menadzer mora postovati
class IRs485Manager
{
public:
    virtual ~IRs485Manager() {}
    virtual void Service() = 0; // Glavna petlja Menadzera
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) = 0; // Callback za odgovor
    virtual void OnTimeout() = 0; // Callback za timeout
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

    // Funkcije koje Menadzeri pozivaju da bi dobili pristup magistrali
    bool RequestBusAccess(IRs485Manager* manager);
    bool SendPacket(uint8_t* data, uint16_t length);
    void ReleaseBusAccess(IRs485Manager* manager);

private:
    // Staticka funkcija koja sluzi kao wrapper za FreeRTOS zadatak
    static void TaskWrapper(void* pvParameters);
    // Stvarna petlja zadatka
    void Run(); 

    void Dispatch();
    void HandleReceive();
    bool ValidatePacket(uint8_t* buffer, uint16_t length);

    TaskHandle_t m_task_handle;
    HardwareSerial m_rs485_serial;

    // Pokazivaci na nase Menadzere, sortirani po prioritetu
    IRs485Manager* m_http_query_manager;
    IRs485Manager* m_update_manager;
    IRs485Manager* m_log_pull_manager;
    IRs485Manager* m_time_sync;

    IRs485Manager* m_current_bus_owner; // Koji menadzer trenutno ima magistralu
    
    enum class Rs485State
    {
        IDLE,       // Ceka na Menadzera da zatrazi magistralu (Dispecer)
        SENDING,    // Salje paket
        WAITING,    // Ceka odgovor
        RECEIVING,  // Prima odgovor
        TIMEOUT     // Obradjuje timeout
    };

    Rs485State m_state;
    unsigned long m_timeout_start;
    uint8_t m_rx_buffer[RS485_BUFFER_SIZE];
    uint16_t m_rx_count;
};

#endif // RS485_SERVICE_H
