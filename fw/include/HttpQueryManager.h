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
#include "ProjectConfig.h" 

// Komande (preuzete iz common.h)
#define SOH 0x01
#define EOT 0x04
#define SET_ROOM_TEMP 0xD6
#define GET_APPL_STAT 0xA1
#define RESET_SOS_ALARM 0xD4
#define SET_DISPL_BCKLGHT 0xD7
#define RESTART_CTRL 0xC0
#define SET_SYSTEM_ID 0xD8
#define SET_RS485_CFG 0xD1
#define SET_BEDDING_REPL 0xDA
#define SET_BR2OW 0xE9 // Onewire Bridge command

// Definisana struktura za komandu
struct HttpCommand
{
    uint8_t cmd_id;
    uint16_t address;
    uint16_t owa_addr; // Onewire Address (za bridge)
    uint32_t param1;   // Opšti parametar 1 (npr. Setpoint, System ID, Brzina)
    uint32_t param2;   // Opšti parametar 2 (npr. Differential)
    uint32_t param3;   // Opšti parametar 3 (npr. Config Byte)
    uint8_t* string_ptr; // Pokazivac na string (za RS485_CFG, TXA, itd.)
};


class HttpQueryManager : public IRs485Manager
{
public:
    HttpQueryManager();
    void Initialize(Rs485Service* pRs485Service);

    /**
     * @brief Glavna BLOKIRAJUCA funkcija koju poziva HttpServer.
     */
    bool ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer);

    // Implementacija IRs485Manager interfejsa
    virtual void Service() override;
    virtual void ProcessResponse(uint8_t* packet, uint16_t length) override;
    virtual void OnTimeout() override;

private:
    Rs485Service* m_rs485_service;
    
    SemaphoreHandle_t m_query_mutex;  
    SemaphoreHandle_t m_response_semaphore; 

    HttpCommand* m_pending_cmd;
    uint8_t* m_response_buffer_ptr;
    bool m_query_result;

    uint16_t CreateRs485Packet(HttpCommand* cmd, uint8_t* buffer);
};

#endif // HTTP_QUERY_MANAGER_H