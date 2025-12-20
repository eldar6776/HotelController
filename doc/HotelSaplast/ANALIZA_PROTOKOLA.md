# Analiza Protokola i Formata Podataka (HotelSaplast)

Ovaj dokument sadrži detaljnu analizu formata podataka korištenih u projektu, s posebnim fokusom na format logova koji se šalju putem HTTP-a i format RS485 paketa za sinhronizaciju vremena. Analiza obuhvata oba dijela sistema: `HotelController` (Server) i `RoomController` (Klijent).

## 1. Format Logova (HTTP i Interna Memorija)

Logovi se u sistemu čuvaju u EEPROM memoriji u blokovima od 16 bajtova. Prilikom slanja na HTTP upit (npr. `HTTP_GET_LOG_LIST`), ovaj binarni sadržaj se konvertuje u **Hexadecimalni String** (svaki bajt predstavljen sa 2 ASCII karaktera).

### Struktura Log Zapisa (16 Bajtova)

Ovo je binarna struktura jednog log zapisa kako se formira u funkciji `CONTROLLER_WriteLogEvent` unutar `HotelController`-a.

| Bajt (Index) | Naziv Polja | Opis |
| :--- | :--- | :--- |
| **0** | `Log ID (MSB)` | Viši bajt rednog broja loga. |
| **1** | `Log ID (LSB)` | Niži bajt rednog broja loga. |
| **2** | `Log Event` | Identifikator događaja (pogledati tabelu događaja ispod). |
| **3** | `Reserved` | Rezervisano (obično 0x00). |
| **4** | `Reserved` | Rezervisano (obično 0x00). |
| **5** | `Event Detail (MSB)` | Viši bajt detalja događaja (npr. ID kartice ili greška). |
| **6** | `Event Detail (LSB)` | Niži bajt detalja događaja. |
| **7** | `Reserved` | Rezervisano (0xFF). |
| **8** | `Reserved` | Rezervisano (0xFF). |
| **9** | `Reserved` | Rezervisano (0xFF). |
| **10** | `Datum` | Dan u mjesecu (BCD format ili raw, zavisno od implementacije). |
| **11** | `Mjesec` | Mjesec (1-12). |
| **12** | `Godina` | Godina (npr. 16 za 2016). |
| **13** | `Sati` | Sati (0-23). |
| **14** | `Minute` | Minute (0-59). |
| **15** | `Sekunde` | Sekunde (0-59). |

> **Napomena za HTTP:** Prilikom slanja putem HTTP-a, ovih 16 bajtova se pretvara u string od 32 karaktera (npr. bajt `0x01` postaje string `"01"`). Ako se šalje cijeli blok memorije, on sadrži niz ovakvih 16-bajtnih zapisa.

---

## 2. Format Paketa za Slanje Vremena (RS485)

Sinhronizacija vremena se vrši slanjem paketa sa `HotelController`-a na `RoomController` jedinice. Format paketa je definisan u funkciji `RUBICON_PrepareTimeUpdatePacket`.

### Struktura RS485 Paketa (Time Sync)

Ukupna dužina paketa: **22 bajta**.

| Bajt (Index) | Vrijednost / Naziv | Opis |
| :--- | :--- | :--- |
| **0** | `SOH` (0x01) | Start of Header - Označava početak paketa. |
| **1** | `Dest. Adr (MSB)` | Adresa primaoca (RoomController) - viši bajt. |
| **2** | `Dest. Adr (LSB)` | Adresa primaoca (RoomController) - niži bajt. |
| **3** | `Src. Adr (MSB)` | Adresa pošiljaoca (HotelController) - viši bajt. |
| **4** | `Src. Adr (LSB)` | Adresa pošiljaoca (HotelController) - niži bajt. |
| **5** | `Length` (0x0D) | Dužina podataka (payload-a) koji slijede = 13 bajtova. |
| **6** | `CMD` (0xD5) | Komanda `SET_RTC_DATE_TIME`. |
| **7** | `Dan (Desetice)` | ASCII karakter (npr. '1' za 1x). `(Dan >> 4) + 48`. |
| **8** | `Dan (Jedinice)` | ASCII karakter (npr. '5' za x5). `(Dan & 0x0F) + 48`. |
| **9** | `Mjesec (Desetice)` | ASCII karakter. |
| **10** | `Mjesec (Jedinice)` | ASCII karakter. |
| **11** | `Godina (Desetice)` | ASCII karakter. |
| **12** | `Godina (Jedinice)` | ASCII karakter. |
| **13** | `Sati (Desetice)` | ASCII karakter. |
| **14** | `Sati (Jedinice)` | ASCII karakter. |
| **15** | `Minute (Desetice)` | ASCII karakter. |
| **16** | `Minute (Jedinice)` | ASCII karakter. |
| **17** | `Sekunde (Desetice)` | ASCII karakter. |
| **18** | `Sekunde (Jedinice)` | ASCII karakter. |
| **19** | `Checksum (MSB)` | Suma bajtova od indexa 6 do 18 (uključivo). |
| **20** | `Checksum (LSB)` | Niži bajt sume. |
| **21** | `EOT` (0x04) | End of Transmission - Kraj paketa. |

---

## 3. Definicije Događaja (Events)

Ispod je lista svih mogućih događaja definisanih u sistemu (iz `hotel_room_controller.h` i `logger.h`).

### Sistemski Događaji i Reset
*   **0xD0** - PIN RESET
*   **0xD1** - POWER ON RESET
*   **0xD2** - SOFTWARE RESET
*   **0xD3** - IWDG RESET (Watchdog)
*   **0xD4** - WWDG RESET (Window Watchdog)
*   **0xD5** - LOW POWER RESET
*   **0xD6** - FIRMWARE UPDATE / UPDATED
*   **0xD7** - FIRMWARE UPDATE FAIL
*   **0xD8** - BOOTLOADER UPDATED
*   **0xD9** - BOOTLOADER UPDATE FAIL
*   **0xDA** - IMAGE UPDATED
*   **0xDB** - IMAGE UPDATE FAIL
*   **0xDC** - DISPLAY FAIL
*   **0xDD** - DRIVER OR FUNCTION FAIL

### Događaji Logovanja (Sobe i Kontrola Pristupa)
*   **0xE0** - NO EVENT
*   **0xE1** - GUEST CARD VALID (Gost kartica ispravna)
*   **0xE2** - GUEST CARD INVALID (Gost kartica neispravna)
*   **0xE3** - HANDMAID CARD VALID (Sobarica kartica ispravna)
*   **0xE4** - ENTRY DOOR CLOSED (Ulazna vrata zatvorena)
*   **0xE5** - PRESET CARD
*   **0xE6** - HANDMAID SERVICE END (Kraj pospremanja)
*   **0xE7** - MANAGER CARD (Menadžer kartica)
*   **0xE8** - SERVICE CARD (Servisna kartica)
*   **0xE9** - ENTRY DOOR OPENED (Ulazna vrata otvorena)
*   **0xEA** - MINIBAR USED (Minibar korišten)
*   **0xEB** - BALCON DOOR OPENED (Balkonska vrata otvorena)
*   **0xEC** - BALCON DOOR CLOSED (Balkonska vrata zatvorena)
*   **0xED** - CARD STACKER ON (Kartica u odlagachu)
*   **0xEE** - CARD STACKER OFF (Kartica izvađena)
*   **0xEF** - DO NOT DISTURB SWITCH ON (Ne uznemiravaj - Uključeno)
*   **0xF0** - DO NOT DISTURB SWITCH OFF (Ne uznemiravaj - Isključeno)
*   **0xF1** - HANDMAID SWITCH ON (Poziv sobarice - Uključeno)
*   **0xF2** - HANDMAID SWITCH OFF (Poziv sobarice - Isključeno)
*   **0xF3** - SOS ALARM TRIGGER (SOS Alarm aktiviran)
*   **0xF4** - SOS ALARM RESET (SOS Alarm resetovan)
*   **0xF5** - FIRE ALARM TRIGGER (Požarni alarm aktiviran)
*   **0xF6** - FIRE ALARM RESET (Požarni alarm resetovan)
*   **0xF7** - UNKNOWN CARD (Nepoznata kartica)
*   **0xF8** - CARD EXPIRED (Kartica istekla)
*   **0xF9** - WRONG ROOM (Pogrešna soba)
*   **0xFA** - WRONG SYSTEM ID (Pogrešan ID sistema)
*   **0xFB** - CONTROLLER RESET
*   **0xFC** - ENTRY DOOR NOT CLOSED (Vrata nisu zatvorena)
*   **0xFD** - DOOR BELL ACTIVE (Zvono aktivno)
*   **0xFE** - DOOR LOCK USER OPEN (Korisničko otvaranje brave)

### Greške Senzora i Termostata (Rubicon Specifično)
*   **0xC0** - FANCOIL RPM SENSOR ERROR
*   **0xC1** - FANCOIL NTC SENSOR ERROR
*   **0xC2** - FANCOIL LO TEMP ERROR
*   **0xC3** - FANCOIL HI TEMP ERROR
*   **0xC4** - FANCOIL FREEZING PROTECTION
*   **0xC5** - THERMOSTAT NTC SENSOR ERROR
*   **0xC6** - THERMOSTAT ERROR
*   **0xCA** - RUBICON RS485 BUS ERROR
