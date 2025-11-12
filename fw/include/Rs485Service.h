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
#include "ProjectConfig.h" 
#include "EepromStorage.h" // Za pristup AppConfig (rsifa)

// Apstraktni interfejs (ugovor) koji svaki Menadzer mora postovati
class IRs485Manager
{
public:
    virtual ~IRs485Manager() {}
    virtual void Service() = 0; 
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) = 0; 
    virtual void OnTimeout() = 0; 
    // ISPRAVKA: Sve klase moraju implementirati ove metode
    virtual bool WantsBus() = 0;
    virtual const char* Name() const = 0;

    /**
     * @brief Vraća specifični timeout u milisekundama koji ovaj menadžer zahtijeva za svoju trenutnu operaciju.
     * @return Timeout u ms.
     */
    virtual uint32_t GetTimeoutMs() const = 0;
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

    // Javne metode koje koriste Menadžeri:
    bool RequestBusAccess(IRs485Manager* manager);
    bool SendPacket(uint8_t* data, uint16_t length);
    void ReleaseBusAccess(IRs485Manager* manager);
    void ResetBus(); // NOVO: Agresivni reset steka

    IRs485Manager* GetCurrentBusOwner(); // NOVO: Javni getter
    
private:
    static void TaskWrapper(void* pvParameters);
    void Run(); 
    void Dispatch();
    void HandleReceive();
    
    // Potpuno implementirane funkcije:
    bool ValidatePacket(uint8_t* buffer, uint16_t length);
    uint16_t CalculateChecksum(uint8_t* buffer, uint16_t data_length); 

    TaskHandle_t m_task_handle;
    HardwareSerial m_rs485_serial;

    IRs485Manager* m_http_query_manager;
    IRs485Manager* m_update_manager;
    IRs485Manager* m_log_pull_manager;
    IRs485Manager* m_time_sync;

    IRs485Manager* m_current_bus_owner;
    Rs485State m_state;
    unsigned long m_timeout_start; 
    // NOVO: Bus Watchdog
    unsigned long m_bus_watchdog_start;
    // NOVO: Inter-Bajt Tajmer
    unsigned long m_last_rx_time;
    const unsigned long INTER_BYTE_TIMEOUT = 5; // 5 ms

    // Koristi fiksnu velicinu
    uint8_t m_rx_buffer[RS485_BUFFER_SIZE]; 
    uint16_t m_rx_count;
};

#endif // RS485_SERVICE_H