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
    m_single_byte_mode = false; // Default: normalni mod (za LogPullManager i ostale)
    m_active_bus = 0; // Default: Lijevi bus aktivan
}

void Rs485Service::Initialize()
{
    Serial.println(F("[Rs485Service] Inicijalizacija..."));
    
    // Setup oba DE pina
    pinMode(RS485_DE_PIN1, OUTPUT);
    pinMode(RS485_DE_PIN2, OUTPUT);
    
    // Default stanje: Lijevi bus aktivan, Desni bus disabled
    digitalWrite(RS485_DE_PIN1, LOW);  // DE1 = LOW (RX mod, spreman za TX)
    digitalWrite(RS485_DE_PIN2, HIGH); // DE2 = HIGH (disabled, ne ometa bus)
    
    m_rs485_serial.begin(RS485_BAUDRATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
    
    Serial.printf("[Rs485Service] Dual DE inicijalizovan: DE1=%d, DE2=%d, Aktivan bus=%d\n", 
                  RS485_DE_PIN1, RS485_DE_PIN2, m_active_bus);
}

/**
 * @brief Aktivira single-byte ACK/NAK mod za STARI protokol.
 * @note Koristi UpdateManager za transfer fajlova sa STARIM protokolom.
 */
void Rs485Service::EnableSingleByteMode()
{
    m_single_byte_mode = true;
    Serial.println(F("[Rs485Service] Single-byte mod AKTIVIRAN (STARI protokol)"));
}

/**
 * @brief Deaktivira single-byte mod i vraća normalan rad.
 * @note Poziva UpdateManager nakon završetka ili greške transfera.
 */
void Rs485Service::DisableSingleByteMode()
{
    m_single_byte_mode = false;
    Serial.println(F("[Rs485Service] Single-byte mod DEAKTIVIRAN (normalni rad)"));
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
            
            // =============================================================================
            // WORKAROUND: Ignoriši null bajtove na početku (HIGH-Z artifact od voltage divider-a)
            // Uzrok: DE=LOW drži RO pin u HIGH-Z, voltage divider vuče liniju prema GND
            // TODO HARDVER: Dodati 10kΩ pull-up otpornik između GPIO34 i 3.3V
            // =============================================================================
            if (rx_count == 0 && incoming_byte == 0x00)
            {
                continue; // Preskoči null bajtove dok ne stigne pravi paket
            }
            
            // Osiguraj da ne pređemo veličinu bafera
            if (rx_count >= buffer_size)
            {
                return -1;
            }

            buffer[rx_count++] = incoming_byte;

            // =============================================================================
            // KRITIČNA PROVJERA: SINGLE-BYTE MODE ZA STARI PROTOKOL (UpdateManager)
            // Ako je m_single_byte_mode aktivan I primi se ACK ili NAK kao prvi bajt,
            // to JE kompletan odgovor - vrati ga odmah bez čekanja!
            // =============================================================================
            if (m_single_byte_mode && rx_count == 1 && (buffer[0] == ACK || buffer[0] == NAK))
            {
                // Single-byte ACK/NAK za STARI protokol - ovo JE kompletan validni odgovor
                Serial.printf("[Rs485Service] -> Single-byte primljen: 0x%02X (%s)\n", 
                              buffer[0], buffer[0] == ACK ? "ACK" : "NAK");
                return 1; // Vrati 1 bajt kao uspješan prijem
            }

            // KORAK 1: Čekamo minimalno 6 bajtova da bismo pročitali dužinu
            // KONAČNA ISPRAVKA: Resetujemo start_time sa svakim primljenim bajtom.
            // Ovo osigurava da timeout mjeri vrijeme *između* bajtova, a ne od početka poziva.
            start_time = millis();
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
                // KORAK 3: Imamo kompletan paket. Ispiši ga prije validacije.
                // Ovo je ispunjenje zahtjeva da se vidi sve što stigne.
                if (rx_count > 0) {
                    char response_packet_str[rx_count * 3 + 1];
                    response_packet_str[0] = '\0';
                    for (int i = 0; i < rx_count; i++) {
                        sprintf(response_packet_str + strlen(response_packet_str), "%02X ", buffer[i]);
                    }
                    Serial.printf("[Rs485] -> RAW Prijem (kompletan paket) u %lu ms (%d B): [ %s]\n", millis(), rx_count, response_packet_str);
                }

                // KORAK 4: Tek sada validiraj paket.
                if (ValidatePacket(buffer, rx_count))
                {
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

    // KONAČNA ISPRAVKA: Ako je timeout istekao, a primili smo neke bajtove, ispiši ih.
    // Ovo je ključno za dijagnostiku nekompletnih/oštećenih paketa.
    if (rx_count > 0) {
        char incomplete_packet_str[rx_count * 3 + 1];
        incomplete_packet_str[0] = '\0';
        for (int i = 0; i < rx_count; i++) {
            sprintf(incomplete_packet_str + strlen(incomplete_packet_str), "%02X ", buffer[i]);
        }
        Serial.printf("[Rs485] TIMEOUT! Primljen nekompletan/oštećen paket (%d B): [ %s]\n", rx_count, incomplete_packet_str);
    }

    return 0; // Vraća 0 za timeout
}

bool Rs485Service::SendPacket(const uint8_t* data, uint16_t length)
{
    // Isprazni prijemni bafer prije slanja
    while(m_rs485_serial.available()) m_rs485_serial.read();

    LOG_DEBUG(4, "[Rs485] Slanje paketa -> Dužina: %d, Sadržaj: %02X %02X %02X %02X %02X %02X %02X...\n", length, data[0], data[1], data[2], data[3], data[4], data[5], data[6]);

    // Koristi aktivan DE pin prema trenutno odabranom busu
    uint8_t active_de_pin = (m_active_bus == 0) ? RS485_DE_PIN1 : RS485_DE_PIN2;
    
    digitalWrite(active_de_pin, HIGH); 
    delayMicroseconds(50); 

    m_rs485_serial.write(data, length);
    m_rs485_serial.flush(); 
    
    delayMicroseconds(50);
    digitalWrite(active_de_pin, LOW); 

    return true;
}

/**
 * @brief Prebacuje aktivni bus (0=Lijevi, 1=Desni).
 * @param busId ID busa (0 ili 1).
 * @note Neaktivan bus ima DE=HIGH (disabled, ne ometa dijeljenu RX liniju).
 */
void Rs485Service::SelectBus(uint8_t busId)
{
    if (busId > 1) {
        Serial.printf("[Rs485Service] ERROR: Nevažeći busId=%d. Dozvoljeno: 0 ili 1.\n", busId);
        return;
    }
    
    if (busId == m_active_bus) {
        return; // Već je odabran, nema potrebe mjenjati
    }
    
    m_active_bus = busId;
    
    if (busId == 0) {
        // Aktiviraj Lijevi bus, onemogući Desni
        digitalWrite(RS485_DE_PIN1, LOW);  // DE1 = LOW (RX mod, spreman za TX)
        digitalWrite(RS485_DE_PIN2, HIGH); // DE2 = HIGH (disabled)
    } else {
        // Aktiviraj Desni bus, onemogući Lijevi
        digitalWrite(RS485_DE_PIN1, HIGH); // DE1 = HIGH (disabled)
        digitalWrite(RS485_DE_PIN2, LOW);  // DE2 = LOW (RX mod, spreman za TX)
    }
    
    LOG_DEBUG(3, "[Rs485Service] Bus prebačen na: %s (ID=%d)\n", 
              busId == 0 ? "LIJEVI" : "DESNI", busId);
}

/**
 * @brief Vraća ID trenutno aktivnog busa.
 * @return 0 (Lijevi) ili 1 (Desni).
 */
// Implementacija je inline u header fajlu