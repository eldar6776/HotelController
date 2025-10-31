/**
 ******************************************************************************
 * @file    HttpQueryManager.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header za HttpQueryManager modul.
 *
 * @note
 * Replicira blokirajucu logiku `HTTP2RS485`.
 * Implementira IRs485Manager interfejs.
 ******************************************************************************
 */

#ifndef HTTP_QUERY_MANAGER_H
#define HTTP_QUERY_MANAGER_H

#include <Arduino.h>
#include <freertos/semphr.h> // Za Semafore (blokiranje)
#include "Rs485Service.h"

// TODO: Definisati strukturu za komandu
struct HttpCommand
{
    uint8_t cmd_id;
    uint16_t address;
    // ...
};


class HttpQueryManager : public IRs485Manager
{
public:
    HttpQueryManager();
    void Initialize(Rs485Service* pRs485Service);

    /**
     * @brief Glavna BLOKIRAJUCA funkcija koju poziva HttpServer.
     * @return true ako je stigao odgovor, false ako je timeout.
     */
    bool ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer);

    // Implementacija IRs485Manager interfejsa (poziva ih Rs485Service)
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;

private:
    Rs485Service* m_rs485_service;
    
    SemaphoreHandle_t m_query_mutex;  // Stiti pristup m_pending_cmd
    SemaphoreHandle_t m_response_semaphore; // Binarni semafor za blokiranje

    HttpCommand* m_pending_cmd;
    uint8_t* m_response_buffer_ptr;
    bool m_query_result;
};

#endif // HTTP_QUERY_MANAGER_H
