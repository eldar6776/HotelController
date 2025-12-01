/**
 ******************************************************************************
 * @file    TimeSync.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija TimeSync modula.
 ******************************************************************************
 */

#include "TimeSync.h"
#include "DebugConfig.h"
#include "ProjectConfig.h"
#include <time.h> // Za time() i tm strukturu

// Globalna konfiguracija (extern)
extern AppConfig g_appConfig; 

// Helper funkcija za konverziju uint8_t u BCD format (kao u izvornom main.cpp/hotel_ctrl.c)
static uint8_t toBCD(uint8_t val)
{
    // Koristimo BCD konverziju iz originalnog STM32 koda
    return (uint8_t)(((val / 10) << 4) | (val % 10)); 
}


TimeSync::TimeSync() :
    m_rs485_service(NULL),
    m_last_sync_time(0)
{
    // Konstruktor
}

void TimeSync::Initialize(Rs485Service* pRs485Service)
{
    m_rs485_service = pRs485Service;
}

/**
 * @brief Glavna funkcija koju poziva state-mašina.
 */
void TimeSync::Run()
{
    if (IsTimeToSync())
    {
        SendTimeBroadcast();
    }
}



/**
 * @brief Salje SET_RTC_DATE_TIME broadcast paket. (Replicira HC_CreateTimeUpdatePacket)
 */
void TimeSync::SendTimeBroadcast()
{
    m_last_sync_time = millis(); // Resetuj tajmer tek kada stvarno šaljemo
    LOG_DEBUG(3, "[TimeSync] Slanje broadcast vremena...\n");
    
    uint8_t packet[RTC_PACKET_LENGTH];
    uint16_t rsbra = g_appConfig.rs485_bcast_addr; // Broadcast adresa iz konfiguracije
    uint16_t rsifa = g_appConfig.rs485_iface_addr; 
    uint16_t rs485_pkt_chksum = 0;
    
    time_t rawTime;
    time(&rawTime);
    struct tm* updt = localtime(&rawTime);

    // KONAČNA ISPRAVKA: Ne šalji broadcast ako vreme nije sinhronizovano.
    // Proveravamo da li je godina validna (veća od npr. 2023).
    // tm_year je broj godina od 1900.
    if (updt->tm_year < 123) { // 1900 + 123 = 2023
        LOG_DEBUG(3, "[TimeSync] Vreme još nije sinhronizovano. Preskačem slanje broadcast-a.\n");
        // Resetujemo tajmer da ne bismo stalno pokušavali u kratkom intervalu
        m_last_sync_time = millis();
        return;
    }

    // 1. Zaglavlje (Target je Broadcast Adresa)
    packet[0] = SOH;
    packet[1] = (rsbra >> 8); 
    packet[2] = (rsbra & 0xFFU);
    packet[3] = (rsifa >> 8);
    packet[4] = (rsifa & 0xFFU);
    packet[5] = 0x0d; // Data Length = 13 bajtova (CMD + 12 ASCII karaktera za vrijeme)
    
    // 2. Data Payload (13 Bajtova) - Replikacija formata sa starog sistema (BCD -> ASCII)
    packet[6] = SET_RTC_DATE_TIME;      // CMD
    
    uint8_t bcd_val;
    
    bcd_val = toBCD(updt->tm_mday);
    packet[7] = (bcd_val >> 4) + '0';
    packet[8] = (bcd_val & 0x0F) + '0';
    
    bcd_val = toBCD(updt->tm_mon + 1);
    packet[9] = (bcd_val >> 4) + '0';
    packet[10] = (bcd_val & 0x0F) + '0';

    bcd_val = toBCD(updt->tm_year % 100);
    packet[11] = (bcd_val >> 4) + '0';
    packet[12] = (bcd_val & 0x0F) + '0';
    
    bcd_val = toBCD(updt->tm_hour);
    packet[13] = (bcd_val >> 4) + '0';
    packet[14] = (bcd_val & 0x0F) + '0';

    bcd_val = toBCD(updt->tm_min);
    packet[15] = (bcd_val >> 4) + '0';
    packet[16] = (bcd_val & 0x0F) + '0';

    bcd_val = toBCD(updt->tm_sec);
    packet[17] = (bcd_val >> 4) + '0';
    packet[18] = (bcd_val & 0x0F) + '0';

    // Stari, komentarisani kod ostaje netaknut
    //packet[6] = SET_RTC_DATE_TIME;      // CMD
    //packet[7] = toBCD(weekday);         // Weekday
    //packet[8] = toBCD(updt->tm_mday);   // Day (Date)
    //packet[9] = toBCD(updt->tm_mon + 1);// Month (0-11, pa +1)
    //packet[10] = toBCD(updt->tm_year % 100); // Year (od 2000)
    //packet[11] = toBCD(updt->tm_hour);  // Hours
    //packet[12] = toBCD(updt->tm_min);   // Minutes
    //packet[13] = toBCD(updt->tm_sec);   // Seconds
    //for (uint32_t i = 6; i < 14; i++) // Indeksi 6 do 13
    //{
    //    rs485_pkt_chksum += packet[i];
    //}

    // 3. Checksum (na 13 bajtova data polja)
    for (uint32_t i = 6; i < 19; i++) // Indeksi 6 do 18
    {
        rs485_pkt_chksum += packet[i];
    }
    packet[19] = (rs485_pkt_chksum >> 8);
    packet[20] = (rs485_pkt_chksum & 0xFFU);
    packet[21] = EOT;

    // DODATA DIJAGNOSTIKA: Ispis kompletnog paketa

    char packet_str[RTC_PACKET_LENGTH * 3 + 1];
    packet_str[0] = '\0';
    for (int i = 0; i < 22; i++) { // Ukupna dužina je sada 22
        sprintf(packet_str + strlen(packet_str), "%02X ", packet[i]);
    }
    LOG_DEBUG(3, "[TimeSync] -> RAW Paket (22 B): [ %s]\n", packet_str);

    // Slanje paketa (Broadcast ne očekuje odgovor, ali Rs485Service čeka timeout)
    m_rs485_service->SendPacket(packet, 22);
}

/**
 * @brief Provjerava da li je vrijeme za slanje broadcast-a.
 * @return true ako je vrijeme za slanje broadcast-a, u suprotnom false.
 */
bool TimeSync::IsTimeToSync()
{
    return (millis() - m_last_sync_time > TIME_BROADCAST_INTERVAL_MS);
}

void TimeSync::ResetTimer()
{
    m_last_sync_time = millis();
    LOG_DEBUG(4, "[TimeSync] Tajmer resetovan nakon update-a.\n");
}