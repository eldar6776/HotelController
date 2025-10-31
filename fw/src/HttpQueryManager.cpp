/**
 ******************************************************************************
 * @file    HttpQueryManager.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija HttpQueryManager modula.
 ******************************************************************************
 */

#include "HttpQueryManager.h"

HttpQueryManager::HttpQueryManager() :
    m_rs485_service(NULL),
    m_pending_cmd(NULL),
    m_response_buffer_ptr(NULL),
    m_query_result(false)
{
    // Konstruktor
}

void HttpQueryManager::Initialize(Rs485Service* pRs485Service)
{
    m_rs485_service = pRs485Service;
    
    // Kreiraj mutex za siguran pristup komandama
    m_query_mutex = xSemaphoreCreateMutex();
    
    // Kreiraj semafor koji blokira ExecuteBlockingQuery
    m_response_semaphore = xSemaphoreCreateBinary();
}

/**
 * @brief BLOKIRAJUCA funkcija. Ceka dok RS485 ne zavrsi.
 */
bool HttpQueryManager::ExecuteBlockingQuery(HttpCommand* cmd, uint8_t* responseBuffer)
{
    if (xSemaphoreTake(m_query_mutex, pdMS_TO_TICKS(100)) == pdFALSE)
    {
        Serial.println(F("[HttpQueryManager] Mutex zauzet, HTTP upit odbijen."));
        return false; // Vec je jedan upit u toku
    }

    Serial.println(F("[HttpQueryManager] Primljen blokirajuci upit od HTTP-a..."));

    // Postavi podatke koje ce 'Service' pokupiti
    m_pending_cmd = cmd;
    m_response_buffer_ptr = responseBuffer;
    m_query_result = false; // Resetuj rezultat

    // Obavijesti Rs485Service da imamo prioritetni zadatak
    // (Ovo cemo implementirati preko 'RequestBusAccess')
    
    // Blokiraj ovaj (HttpServer) zadatak dok ne stigne odgovor ili timeout
    // Koristimo dugacak timeout (npr. 10 sekundi)
    if (xSemaphoreTake(m_response_semaphore, pdMS_TO_TICKS(10000)) == pdFALSE)
    {
        // Istekao je nas HTTP timeout (10 sekundi)
        Serial.println(F("[HttpQueryManager] HTTP Blokirajuci upit istekao (10s Timeout)!"));
        m_query_result = false;
    }

    // Ocisti podatke
    m_pending_cmd = NULL;
    m_response_buffer_ptr = NULL;

    // Vrati mutex
    xSemaphoreGive(m_query_mutex);

    return m_query_result;
}


/**
 * @brief Poziva se od strane Rs485Service dispecera kada je nas red.
 */
void HttpQueryManager::Service()
{
    if (m_pending_cmd != NULL)
    {
        // Imamo komandu, posalji je
        Serial.println(F("[HttpQueryManager] Saljem HTTP komandu na RS485..."));

        // TODO: Kreirati paket na osnovu m_pending_cmd
        uint8_t packet[32]; // Placeholder
        uint16_t length = 0;
        
        m_rs485_service->SendPacket(packet, length);
        // Sada cekamo odgovor (ProcessResponse) ili timeout (OnTimeout)
    }
    else
    {
        // Nemamo nista, oslobodi magistralu
        m_rs485_service->ReleaseBusAccess(this);
    }
}

/**
 * @brief Callback - Stigao je odgovor.
 */
void HttpQueryManager::ProcessResponse(uint8_t* packet, uint16_t length)
{
    Serial.println(F("[HttpQueryManager] Stigao odgovor za HTTP upit."));
    if (m_response_buffer_ptr != NULL)
    {
        // Kopiraj odgovor u bafer od HttpServer-a
        memcpy(m_response_buffer_ptr, packet, length);
    }
    m_query_result = true;
    
    // Odblokiraj ExecuteBlockingQuery()
    xSemaphoreGive(m_response_semaphore);
    // Rs485Service ce sam osloboditi magistralu
}

/**
 * @brief Callback - Uredjaj nije odgovorio.
 */
void HttpQueryManager::OnTimeout()
{
    Serial.println(F("[HttpQueryManager] RS485 Timeout za HTTP upit."));
    m_query_result = false;
    
    // Odblokiraj ExecuteBlockingQuery() (koji ce vratiti 'false')
    xSemaphoreGive(m_response_semaphore);
}
