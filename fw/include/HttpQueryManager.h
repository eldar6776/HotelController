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

// Komande (preuzete iz httpd_cgi_ssi.c i hotel_ctrl.c)
#define RESTART_CTRL 0xC0 // (rst)
#define GET_APPL_STAT 0xA1 // (cst)
#define SET_APPL_STAT 0xD5 // (stg)
#define SET_ROOM_TEMP 0xD6 // (tha)
#define RESET_SOS_ALARM 0xD4 // (rud)
#define PREVIEW_DISPL_IMG 0xD9 // (ipr)
#define SET_DISPL_BCKLGHT 0xD7 // (cbr)
#define SET_SYSTEM_ID 0xD8 // (sid)
#define SET_RS485_CFG 0xD1 // (rsc)
#define SET_BEDDING_REPL 0xDA // (sbr)
#define GET_LOG_LIST 0xA3 // (log=3)
#define DEL_LOG_LIST 0xD3 // (log=4)
#define DWNLD_JRNL 0x4A // (HSset)
#define CMD_SET_DIN_CFG 0xC6 // (cdi)
#define CMD_SET_DOUT_STATE 0xD2 // (cdo)
#define SET_PERMITED_GROUP 0xE0 // (pga)
#define CMD_RT_DISP_MSG 0x48 // (txa)
#define RT_SET_DISP_STA 0x49 // (tda)
#define CMD_RT_UPD_QRC 0x52 // (qra + qrc)
#define RT_DISP_QRC 0x53 // (qra + qrd)
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
    uint16_t string_len; // Dužina string_ptr podataka
};


class HttpQueryManager
{
public:
    HttpQueryManager();
    void Initialize(Rs485Service* pRs485Service);

    /**
     * @brief Glavna funkcija koju poziva HttpServer. Sada je BLOKIRAJUĆA.
     */
    bool ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer);

private:
    Rs485Service* m_rs485_service;
    uint16_t CreateRs485Packet(HttpCommand* cmd, uint8_t* buffer);
};

#endif // HTTP_QUERY_MANAGER_H