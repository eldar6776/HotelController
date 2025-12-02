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
        // NOVO: Ispis sadržaja paketa radi dijagnostike
        char raw_packet_str[length * 3 + 1];
        raw_packet_str[0] = '\0';
        for (int i = 0; i < length; i++) {
            sprintf(raw_packet_str + strlen(raw_packet_str), "%02X ", buffer[i]);
        }
        LOG_DEBUG(2, "  -> SADRŽAJ: [ %s]\n", raw_packet_str);
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
    // ISPRAVKA: Vraćena ispravna logika sa dva tajmera
    unsigned long start_time = millis();
    unsigned long last_byte_time = millis();
    uint16_t bytes_received = 0;

    while (millis() - start_time < timeout_ms)
    {
        if (m_rs485_serial.available())
        {
            buffer[bytes_received++] = m_rs485_serial.read();
            last_byte_time = millis(); // Resetuj tajmer nakon svakog primljenog bajta

            if (bytes_received >= buffer_size)
            {
                LOG_DEBUG(1, "[Rs485] GRESKA: Prijemni bafer je pun (overflow). Sadržaj bafera:\n");
                // Ispis sadržaja bafera radi dijagnostike
                char raw_packet_str[bytes_received * 3 + 1] = {0};
                for (int i = 0; i < bytes_received; i++) {
                    sprintf(raw_packet_str + strlen(raw_packet_str), "%02X ", buffer[i]);
                }
                LOG_DEBUG(1, "  -> RAW: [ %s]\n", raw_packet_str);
                return -1; // Greška - bafer je pun
            }
        }

        // Provjera za kraj paketa (EOT)
        if (bytes_received > 0 && buffer[bytes_received - 1] == EOT)
        {
            if (ValidatePacket(buffer, bytes_received))
            {
                return bytes_received; // Uspjeh
            }
            return -1; // Greška u validaciji
        }

        // Provjera za inter-byte timeout
        if (bytes_received > 0 && (millis() - last_byte_time > RS485_TIMEOUT_MS))
        {
            break; // Prekini ako je pauza između bajtova preduga
        }
    }

    // Ako smo izašli iz petlje zbog timeout-a, a imamo neke podatke
    if (bytes_received > 0) {
        // NOVO: Detaljan ispis nekompletnog paketa
        char raw_packet_str[bytes_received * 3 + 1] = {0};
        for (int i = 0; i < bytes_received; i++) {
            sprintf(raw_packet_str + strlen(raw_packet_str), "%02X ", buffer[i]);
        }
        LOG_DEBUG(2, "[Rs485] TIMEOUT! Primljen nekompletan/oštećen paket (%d B): [ %s]\n", bytes_received, raw_packet_str);
    }

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

/**
 * @brief NOVO: Vraća pokazivač na HardwareSerial objekat za direktno čitanje.
 */
HardwareSerial* Rs485Service::GetSerial()
{
    return &m_rs485_serial;
}