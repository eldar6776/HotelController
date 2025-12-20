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

TimeSync::TimeSync() : m_rs485_service(NULL),
                       m_last_sync_time(0)
{
    // Konstruktor
}

void TimeSync::Initialize(Rs485Service *pRs485Service)
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
    m_last_sync_time = millis();
    LOG_DEBUG(3, "[TimeSync] Slanje broadcast vremena...\n");

    uint16_t rsbra = g_appConfig.rs485_bcast_addr;
    uint16_t rsifa = g_appConfig.rs485_iface_addr;
    uint16_t rs485_pkt_chksum = 0;

    time_t rawTime;
    time(&rawTime);
    struct tm *updt = localtime(&rawTime);

    if (updt->tm_year < 123)
    {
        LOG_DEBUG(3, "[TimeSync] Vreme još nije sinhronizovano. Preskačem slanje broadcast-a.\n");
        m_last_sync_time = millis();
        return;
    }

    ProtocolVersion proto = static_cast<ProtocolVersion>(g_appConfig.protocol_version);

    switch (proto)
    {
    case ProtocolVersion::HILLS:
    case ProtocolVersion::BJELASNICA:
    case ProtocolVersion::SAPLAST:
    case ProtocolVersion::SAX:
    case ProtocolVersion::BOSS:
    case ProtocolVersion::BASKUCA:
    {
        // RUBICON protokol - 22 bajta
        uint8_t packet[22];
        uint8_t bcd_val;

        packet[0] = SOH;
        packet[1] = (rsbra >> 8);
        packet[2] = (rsbra & 0xFFU);
        packet[3] = (rsifa >> 8);
        packet[4] = (rsifa & 0xFFU);
        packet[5] = 0x0d;

        if (proto == ProtocolVersion::HILLS)
            packet[6] = 0xb5; // SET_RTC_DATE_TIME_HILLS
        else                  // BJELASNICA , SAPLAST...
            packet[6] = SET_RTC_DATE_TIME;

        bcd_val = toBCD(updt->tm_mday);
        packet[7] = (bcd_val >> 4) + 48;
        packet[8] = (bcd_val & 0x0F) + 48;

        bcd_val = toBCD(updt->tm_mon + 1);
        packet[9] = (bcd_val >> 4) + 48;
        packet[10] = (bcd_val & 0x0F) + 48;

        bcd_val = toBCD(updt->tm_year % 100);
        packet[11] = (bcd_val >> 4) + 48;
        packet[12] = (bcd_val & 0x0F) + 48;

        bcd_val = toBCD(updt->tm_hour);
        packet[13] = (bcd_val >> 4) + 48;
        packet[14] = (bcd_val & 0x0F) + 48;

        bcd_val = toBCD(updt->tm_min);
        packet[15] = (bcd_val >> 4) + 48;
        packet[16] = (bcd_val & 0x0F) + 48;

        bcd_val = toBCD(updt->tm_sec);
        packet[17] = (bcd_val >> 4) + 48;
        packet[18] = (bcd_val & 0x0F) + 48;

        rs485_pkt_chksum = 0;
        for (uint8_t i = 6; i < 19; i++)
        {
            rs485_pkt_chksum += packet[i];
        }

        packet[19] = (rs485_pkt_chksum >> 8);
        packet[20] = (rs485_pkt_chksum & 0xFFU);
        packet[21] = EOT;

        LOG_DEBUG(3, "[TimeSync] RUBICON protokol (22B)\n");
        m_rs485_service->SendPacket(packet, 22);
        break;
    }
    
    case ProtocolVersion::VUCKO:
    case ProtocolVersion::ULM:
    case ProtocolVersion::VRATA_BOSNE:
    case ProtocolVersion::DZAFIC:
    
    default:
    {
        // HC protokol - 17 bajtova
        uint8_t packet[17];
        uint8_t weekday = (updt->tm_wday == 0) ? 7 : updt->tm_wday;

        packet[0] = SOH;
        packet[1] = (rsbra >> 8);
        packet[2] = (rsbra & 0xFFU);
        packet[3] = (rsifa >> 8);
        packet[4] = (rsifa & 0xFFU);
        packet[5] = 0x08;
        packet[6] = SET_RTC_DATE_TIME;
        packet[7] = toBCD(weekday);
        packet[8] = toBCD(updt->tm_mday);
        packet[9] = toBCD(updt->tm_mon + 1);
        packet[10] = toBCD(updt->tm_year % 100);
        packet[11] = toBCD(updt->tm_hour);
        packet[12] = toBCD(updt->tm_min);
        packet[13] = toBCD(updt->tm_sec);

        rs485_pkt_chksum = 0;
        for (uint8_t i = 6; i < 14; i++)
        {
            rs485_pkt_chksum += packet[i];
        }

        packet[14] = (rs485_pkt_chksum >> 8);
        packet[15] = (rs485_pkt_chksum & 0xFFU);
        packet[16] = EOT;

        LOG_DEBUG(3, "[TimeSync] HC protokol (17B)\n");
        m_rs485_service->SendPacket(packet, 17);
        break;
    }
    }

    // --- Slanje dodatnih TimeSync paketa ---
    LOG_DEBUG(3, "[TimeSync] Provjera dodatnih paketa:\n");
    for (int i = 0; i < 3; i++)
    {
        LOG_DEBUG(3, "[TimeSync]   [%d] enabled=%d, protocol=%d, address=%d\n",
                  i,
                  g_appConfig.additional_sync[i].enabled,
                  g_appConfig.additional_sync[i].protocol_version,
                  g_appConfig.additional_sync[i].broadcast_addr);

        // STROGA PROVJERA: Samo enabled==1 je validno, sve ostalo je disabled
        if (g_appConfig.additional_sync[i].enabled != 1)
        {
            LOG_DEBUG(3, "[TimeSync]   [%d] PRESKOČEN (enabled!= 1)\n", i);
            continue;
        }

        LOG_DEBUG(3, "[TimeSync]   [%d] ŠALJE SE paket\n", i);
        ProtocolVersion add_proto = static_cast<ProtocolVersion>(g_appConfig.additional_sync[i].protocol_version);
        uint16_t broadcast_addr = g_appConfig.additional_sync[i].broadcast_addr;

        switch (add_proto)
        {
        case ProtocolVersion::HILLS:
        case ProtocolVersion::BJELASNICA:
        case ProtocolVersion::SAPLAST:
        case ProtocolVersion::SAX:
        case ProtocolVersion::BOSS:
        case ProtocolVersion::BASKUCA:
        {
            // RUBICON protokol - 22 bajta
            uint8_t packet[22];
            uint8_t bcd_val;

            packet[0] = SOH;
            packet[1] = (broadcast_addr >> 8);
            packet[2] = (broadcast_addr & 0xFFU);
            packet[3] = 0x00;
            packet[4] = 0x00;
            packet[5] = 0x0d;
            if (add_proto == ProtocolVersion::HILLS)
                packet[6] = 0xb5; // SET_RTC_DATE_TIME_HILLS
            else                  // BJELASNICA , SAPLAST...
                packet[6] = SET_RTC_DATE_TIME;

            bcd_val = toBCD(updt->tm_mday);
            packet[7] = (bcd_val >> 4) + 48;
            packet[8] = (bcd_val & 0x0F) + 48;

            bcd_val = toBCD(updt->tm_mon + 1);
            packet[9] = (bcd_val >> 4) + 48;
            packet[10] = (bcd_val & 0x0F) + 48;

            bcd_val = toBCD(updt->tm_year % 100);
            packet[11] = (bcd_val >> 4) + 48;
            packet[12] = (bcd_val & 0x0F) + 48;

            bcd_val = toBCD(updt->tm_hour);
            packet[13] = (bcd_val >> 4) + 48;
            packet[14] = (bcd_val & 0x0F) + 48;

            bcd_val = toBCD(updt->tm_min);
            packet[15] = (bcd_val >> 4) + 48;
            packet[16] = (bcd_val & 0x0F) + 48;

            bcd_val = toBCD(updt->tm_sec);
            packet[17] = (bcd_val >> 4) + 48;
            packet[18] = (bcd_val & 0x0F) + 48;

            rs485_pkt_chksum = 0;
            for (uint8_t j = 6; j < 19; j++)
            {
                rs485_pkt_chksum += packet[j];
            }

            packet[19] = (rs485_pkt_chksum >> 8);
            packet[20] = (rs485_pkt_chksum & 0xFFU);
            packet[21] = EOT;

            LOG_DEBUG(3, "[TimeSync] Dodatni RUBICON paket [%d] (22B) na adresu %d\n", i, broadcast_addr);
            m_rs485_service->SendPacket(packet, 22);
            break;
        }

        case ProtocolVersion::VUCKO:
        case ProtocolVersion::ULM:
        case ProtocolVersion::VRATA_BOSNE:
        case ProtocolVersion::DZAFIC:
        default:
        {
            // HC protokol - 17 bajtova
            uint8_t packet[17];
            uint8_t weekday = (updt->tm_wday == 0) ? 7 : updt->tm_wday;

            packet[0] = SOH;
            packet[1] = (broadcast_addr >> 8);
            packet[2] = (broadcast_addr & 0xFFU);
            packet[3] = 0x00;
            packet[4] = 0x00;
            packet[5] = 0x08;
            packet[6] = SET_RTC_DATE_TIME;
            packet[7] = toBCD(weekday);
            packet[8] = toBCD(updt->tm_mday);
            packet[9] = toBCD(updt->tm_mon + 1);
            packet[10] = toBCD(updt->tm_year % 100);
            packet[11] = toBCD(updt->tm_hour);
            packet[12] = toBCD(updt->tm_min);
            packet[13] = toBCD(updt->tm_sec);

            rs485_pkt_chksum = 0;
            for (uint8_t j = 6; j < 14; j++)
            {
                rs485_pkt_chksum += packet[j];
            }

            packet[14] = (rs485_pkt_chksum >> 8);
            packet[15] = (rs485_pkt_chksum & 0xFFU);
            packet[16] = EOT;

            LOG_DEBUG(3, "[TimeSync] Dodatni HC paket [%d] (17B) na adresu %d\n", i, broadcast_addr);
            m_rs485_service->SendPacket(packet, 17);
            break;
        }
        }
    }
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