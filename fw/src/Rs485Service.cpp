/**
 ******************************************************************************
 * @file    Rs485Service.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija Rs485Service modula (Dispecer).
 ******************************************************************************
 */

#include "Rs485Service.h"
#include "DebugConfig.h"   // Uključujemo za LOG_RS485
#include "ProjectConfig.h" 
#include "EepromStorage.h" // Za g_appConfig

// Globalna konfiguracija (treba biti ucitana u EepromStorage::Initialize)
extern AppConfig g_appConfig; 

// RS485 Kontrolni Karakteri
#define SOH 0x01
#define STX 0x02
#define ACK 0x06 // ISPRAVKA: Vraćeno - Neophodno za validaciju odgovora
#define NAK 0x15 // ISPRAVKA: Vraćeno - Neophodno za validaciju odgovora
#define EOT 0x04

// NOVO: Definicija timeout-a za watchdog
#define BUS_WATCHDOG_TIMEOUT_MS 5000

Rs485Service::Rs485Service() : m_rs485_serial(2) // Koristi UART2 (Serial2)
{
}

void Rs485Service::Initialize()
{
    Serial.println(F("[Rs485Service] Inicijalizacija..."));
    pinMode(RS485_DE_PIN, OUTPUT);
    digitalWrite(RS485_DE_PIN, LOW); // Postavi na RX (prijem)

    m_rs485_serial.begin(RS485_BAUDRATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
}

/**
 * @brief Izračunava Checksum na Data Polju paketa (počinje od indeksa 6).
 */
uint16_t Rs485Service::CalculateChecksum(uint8_t* buffer, uint16_t data_length)
{
    uint16_t checksum = 0;
    for (uint16_t i = 6; i < (6 + data_length); i++) 
    {
        checksum += buffer[i]; 
    }
    return checksum;
}

/**
 * @brief Validira SOH/STX, Adrese, Dužinu i Checksum paketa. (Replicira logiku iz hotel_ctrl.c)
 */
bool Rs485Service::ValidatePacket(uint8_t* buffer, uint16_t length)
{
    // Minimalna dužina paketa je 10 bajtova
    if (length < 10) { // Svi paketi, uključujući ACK/NAK odgovore, imaju minimalnu dužinu
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Prekratak paket: %d)\n", length);
        return false;
    }

    uint16_t data_length = buffer[5];
    uint16_t expectedLength = data_length + 9; 

    if (length != expectedLength) { // Provjera konzistentnosti dužine
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Pogrešna dužina. Očekivano: %d, Primljeno: %d)\n", expectedLength, length);
        return false;
    }
    
    // 1. Provjera početnog i krajnjeg bajta (SOH/STX/ACK/NAK i EOT)
    // Ovo je ključna ispravka koja prihvata ACK kao validan početak.
    bool is_valid_start = (buffer[0] == SOH || buffer[0] == STX || buffer[0] == ACK || buffer[0] == NAK);
    if (!is_valid_start) {
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Pogrešan početni bajt: 0x%02X)\n", buffer[0]);
        return false;
    }
    if (buffer[length - 1] != EOT) { // Svi paketi se moraju završiti sa EOT
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Nedostaje EOT)\n");
        return false;
    }

    // 2. Provjera Adrese Cilja (Target Address) - mora biti naša adresa (rsifa)
    uint16_t target_addr = (buffer[1] << 8) | buffer[2];
    uint16_t my_addr = g_appConfig.rs485_iface_addr;
    
    if (target_addr != my_addr) { // Paket mora biti adresiran na nas
        LOG_DEBUG(4, "[Rs485] ValidatePacket -> INFO (Paket za drugog: 0x%04X != 0x%04X)\n", target_addr, my_addr);
        return false;
    }

    // 3. Provjera Checksum-a
    uint16_t received_checksum = (buffer[expectedLength - 3] << 8) | buffer[expectedLength - 2];
    uint16_t calculated_checksum = CalculateChecksum(buffer, data_length);
    
    if (received_checksum != calculated_checksum) {
        LOG_DEBUG(2, "[Rs485] ValidatePacket -> FAILED (Checksum neispravan. Primljen: 0x%X, Očekivan: 0x%X)\n", received_checksum, calculated_checksum);
        return false;
    }
    
    return true;
}

int Rs485Service::ReceivePacket(uint8_t* buffer, uint16_t buffer_size, uint32_t timeout_ms)
{
    unsigned long start_time = millis();
    uint16_t rx_count = 0;
    uint16_t expected_length = 0; // Očekivana ukupna dužina paketa

    while (millis() - start_time < timeout_ms)
    {
        if (m_rs485_serial.available())
        {
            uint8_t incoming_byte = m_rs485_serial.read();
            
            // Osiguraj da ne pređemo veličinu bafera
            if (rx_count >= buffer_size)
            {
                return -1;
            }

            buffer[rx_count++] = incoming_byte;
            // --- DODATA DIJAGNOSTIKA ---
            LOG_DEBUG(5, "[Rs485] Primljen bajt: 0x%02X (ukupno: %d)\n", incoming_byte, rx_count);
            // --- KRAJ DIJAGNOSTIKE ---

            // KORAK 1: Čekamo minimalno 6 bajtova da bismo pročitali dužinu
            if (rx_count == 6)
            {
                uint8_t data_length = buffer[5];
                expected_length = data_length + 9; // payload + header + checksum + EOT

                // Provjera da li je očekivana dužina uopšte validna
                if (expected_length < 10 || expected_length > buffer_size)
                {
                    LOG_DEBUG(2, "[Rs485] Primljena nevalidna dužina paketa: %d. Resetujem prijem.\n", data_length);
                    rx_count = 0; // Resetuj i čekaj novi paket
                    expected_length = 0;
                    continue;
                }
            }
            
            // KORAK 2: Ako znamo očekivanu dužinu, čekamo da stignu svi bajtovi
            if (expected_length > 0 && rx_count == expected_length)
            {
                // --- DODATA DIJAGNOSTIKA ---
                LOG_DEBUG(4, "[Rs485] Primljen kompletan paket (dužina: %d). Pokrećem validaciju...\n", rx_count);
                // --- KRAJ DIJAGNOSTIKE ---
                // KORAK 3: Tek sada, kada imamo cijeli paket, validiramo ga
                if (ValidatePacket(buffer, rx_count))
                {
                    // --- DIJAGNOSTIKA: ISPIS PRIMLJENOG ODGOVORA ---
                    char response_packet_str[rx_count * 3 + 1];
                    response_packet_str[0] = '\0';
                    for (int i = 0; i < rx_count; i++) {
                        sprintf(response_packet_str + strlen(response_packet_str), "%02X ", buffer[i]);
                    }
                    Serial.printf("[Rs485] -> RAW Odgovor primljen u %lu ms (%d B): [ %s]\n", millis(), rx_count, response_packet_str);
                    // --- KRAJ DIJAGNOSTIKE ---

                    return rx_count; // Uspjeh! Vrati primljeni paket.
                }
                else
                {
                    // Paket je neispravan. Resetuj sve i čekaj novi od početka.
                    rx_count = 0;
                    expected_length = 0;
                }
            }
        }
    }

    // --- CILJANA DIJAGNOSTIKA ZA TIMEOUT ---
    // Ako je timeout istekao, a primili smo neke bajtove, ispiši ih.
    // Ovo nam pomaže da vidimo da li je stigao oštećen ili nekompletan odgovor.
    if (rx_count > 0) {
        char incomplete_packet_str[rx_count * 3 + 1];
        incomplete_packet_str[0] = '\0';
        for (int i = 0; i < rx_count; i++) {
            sprintf(incomplete_packet_str + strlen(incomplete_packet_str), "%02X ", buffer[i]);
        }
        Serial.printf("[Rs485] TIMEOUT! Primljen nekompletan paket (%d B): [ %s]\n", rx_count, incomplete_packet_str);
    }
    // --- KRAJ DIJAGNOSTIKE ---

    return 0; // Vraća 0 za timeout
}

bool Rs485Service::SendPacket(const uint8_t* data, uint16_t length)
{
    // Isprazni prijemni bafer prije slanja
    while(m_rs485_serial.available()) m_rs485_serial.read();

    LOG_DEBUG(4, "[Rs485] Slanje paketa -> Dužina: %d, Sadržaj: %02X %02X %02X %02X %02X %02X %02X...\n", length, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    digitalWrite(RS485_DE_PIN, HIGH); 
    delayMicroseconds(50); 

    m_rs485_serial.write(data, length);
    m_rs485_serial.flush(); 
    
    delayMicroseconds(50);
    digitalWrite(RS485_DE_PIN, LOW); 

    return true;
}