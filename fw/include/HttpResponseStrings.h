/**
 ******************************************************************************
 * @file    HttpResponseStrings.h
 * @author  Gemini Code Assist
 * @brief   Centralizovana tabela stringova za HTTP odgovore.
 *
 * @note
 * Svi standardni odgovori za `sysctrl.cgi` su definisani ovdje
 * kako bi se osigurala 100% kompatibilnost sa starim sistemom i
 * olakšalo održavanje.
 ******************************************************************************
 */

#ifndef HTTP_RESPONSE_STRINGS_H
#define HTTP_RESPONSE_STRINGS_H

// Standardni SSI odgovori za V1 kompatibilnost
const char HTTP_RESPONSE_OK[] = "OK";
const char HTTP_RESPONSE_ERROR[] = "ERROR";
const char HTTP_RESPONSE_BUSY[] = "BUSY";
const char HTTP_RESPONSE_TIMEOUT[] = "TIMEOUT";
const char HTTP_RESPONSE_DELETED[] = "DELETED";
const char HTTP_RESPONSE_EMPTY[] = "EMPTY";

#endif // HTTP_RESPONSE_STRINGS_H