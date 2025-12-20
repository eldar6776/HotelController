# Analiza Protokola i Formata Podataka

## 1. Format Logova (EEPROM)

Logovi se u `RoomController` uređaju (klijent) čuvaju u EEPROM memoriji. Svaki log zapis zauzima fiksnih **16 bajtova**.

### Struktura Log Zapisa (16 bajtova)

| Pozicija | Naziv Polja | Veličina | Opis |
| :--- | :--- | :--- | :--- |
| 0 - 1 | `log_id` | 2 bajta | Jedinstveni identifikator loga (Big-Endian: High Byte, Low Byte). |
| 2 | `log_event` | 1 bajt | Tip događaja (videti tabelu "Definicije Događaja"). |
| 3 | `log_type` | 1 bajt | Tip loga (dodatna kategorizacija). |
| 4 | `log_group` | 1 bajt | Grupa loga. |
| 5 - 9 | `log_card_id` | 5 bajtova | ID RFID kartice (5 bajtova). |
| 10 | `Date` | 1 bajt | Datum (BCD format). |
| 11 | `Month` | 1 bajt | Mesec (BCD format). |
| 12 | `Year` | 1 bajt | Godina (BCD format). |
| 13 | `Hours` | 1 bajt | Sati (BCD format). |
| 14 | `Minutes` | 1 bajt | Minuti (BCD format). |
| 15 | `Seconds` | 1 bajt | Sekunde (BCD format). |

---

## 2. Definicije Događaja (`log_event`)

Tabela svih mogućih vrednosti za `log_event` polje, definisanih u `logger.h`.

### Sistemske Greške i Statusi (0xB0 - 0xC6)
| Hex Vrednost | Simbol | Opis |
| :--- | :--- | :--- |
| 0xB0 | `WATER_FLOOD_SENOR_ACTIV` | Senzor poplave aktiviran |
| 0xB1 | `WATER_FLOOD_SENOR_INACTIV` | Senzor poplave deaktiviran |
| 0xC0 | `FANCOIL_RPM_SENSOR_ERROR` | Greška RPM senzora fancoila |
| 0xC1 | `FANCOIL_NTC_SENSOR_ERROR` | Greška NTC senzora fancoila |
| 0xC2 | `FANCOIL_LO_TEMP_ERROR` | Greška niske temperature |
| 0xC3 | `FANCOIL_HI_TEMP_ERROR` | Greška visoke temperature |
| 0xC4 | `FANCOIL_FREEZING_PROTECTION` | Zaštita od smrzavanja |
| 0xC5 | `AMBIENT_NTC_SENSOR_ERROR` | Greška ambijentalnog senzora |
| 0xC6 | `THERMOSTAT_ERROR` | Greška termostata |

### Reset i Update Događaji (0xD0 - 0xDE)
| Hex Vrednost | Simbol | Opis |
| :--- | :--- | :--- |
| 0xD0 | `PIN_RESET` | Reset na pin |
| 0xD1 | `POWER_ON_RESET` | Reset uključivanjem napajanja |
| 0xD2 | `SOFTWARE_RESET` | Softverski reset |
| 0xD3 | `IWDG_RESET` | Watchdog reset (Independent) |
| 0xD4 | `WWDG_RESET` | Watchdog reset (Window) |
| 0xD5 | `LOW_POWER_RESET` | Reset usled niskog napona |
| 0xD6 | `FIRMWARE_UPDATED` | Firmware uspešno ažuriran |
| 0xD7 | `FIRMWARE_UPDATE_FAIL` | Neuspešno ažuriranje firmware-a |
| 0xD8 | `BOOTLOADER_UPDATED` | Bootloader ažuriran |
| 0xD9 | `BOOTLOADER_UPDATE_FAIL` | Neuspešno ažuriranje bootloadera |
| 0xDA | `IMAGE_UPDATED` | Slike ažurirane |
| 0xDB | `IMAGE_UPDATE_FAIL` | Neuspešno ažuriranje slika |
| 0xDC | `DISPLAY_FAIL` | Greška displeja |
| 0xDD | `DRIVER_OR_FUNCTION_FAIL` | Greška drajvera ili funkcije |
| 0xDE | `ONEWIRE_BUS_EXCESSIVE_ERROR` | Prekomerna greška na OneWire magistrali |

### Operativni Događaji (0xE0 - 0xFE)
| Hex Vrednost | Simbol | Opis |
| :--- | :--- | :--- |
| 0xE0 | `NO_EVENT` | Nema događaja |
| 0xE1 | `GUEST_CARD_VALID` | Validna kartica gosta |
| 0xE2 | `GUEST_CARD_INVALID` | Nevalidna kartica gosta |
| 0xE3 | `HANDMAID_CARD_VALID` | Validna kartica sobarice |
| 0xE4 | `ENTRY_DOOR_CLOSED` | Ulazna vrata zatvorena |
| 0xE5 | `PRESET_CARD` | Preset kartica |
| 0xE6 | `HANDMAID_SERVICE_END` | Kraj usluge sobarice |
| 0xE7 | `MANAGER_CARD` | Menadžerska kartica |
| 0xE8 | `SERVICE_CARD` | Servisna kartica |
| 0xE9 | `ENTRY_DOOR_OPENED` | Ulazna vrata otvorena |
| 0xEA | `MINIBAR_USED` | Minibar korišćen |
| 0xEB | `BALCON_DOOR_OPENED` | Balkonska vrata otvorena |
| 0xEC | `BALCON_DOOR_CLOSED` | Balkonska vrata zatvorena |
| 0xED | `CARD_STACKER_ON` | Odlaganje kartice aktivno (ušteda energije on) |
| 0xEE | `CARD_STACKER_OFF` | Odlaganje kartice neaktivno (ušteda energije off) |
| 0xEF | `DO_NOT_DISTURB_SWITCH_ON` | "Ne uznemiravaj" uključeno |
| 0xF0 | `DO_NOT_DISTURB_SWITCH_OFF` | "Ne uznemiravaj" isključeno |
| 0xF1 | `HANDMAID_SWITCH_ON` | Poziv sobarice uključen |
| 0xF2 | `HANDMAID_SWITCH_OFF` | Poziv sobarice isključen |
| 0xF3 | `SOS_ALARM_TRIGGER` | SOS alarm aktiviran |
| 0xF4 | `SOS_ALARM_RESET` | SOS alarm resetovan |
| 0xF5 | `FIRE_ALARM_TRIGGER` | Požarni alarm aktiviran |
| 0xF6 | `FIRE_ALARM_RESET` | Požarni alarm resetovan |
| 0xF7 | `UNKNOWN_CARD` | Nepoznata kartica |
| 0xF8 | `CARD_EXPIRED` | Kartica istekla |
| 0xF9 | `WRONG_ROOM` | Pogrešna soba |
| 0xFA | `WRONG_SYSTEM_ID` | Pogrešan ID sistema |
| 0xFB | `CONTROLLER_RESET` | Reset kontrolera |
| 0xFC | `ENTRY_DOOR_NOT_CLOSED` | Ulazna vrata nisu zatvorena |
| 0xFD | `DOOR_BELL_ACTIVE` | Zvono aktivno |
| 0xFE | `DOOR_LOCK_USER_OPEN` | Korisničko otvaranje brave |

---

## 3. RS485 Paket za Sinhronizaciju Vremena

Ovaj paket šalje `HotelController` (server) ka `RoomController` uređajima radi podešavanja RTC-a (Real Time Clock).

*   **Ukupna dužina paketa:** 22 bajta
*   **Podaci (Payload):** 13 bajtova (Komanda + ASCII vreme)

### Struktura Paketa

| Pozicija | Polje | Veličina | Vrednost / Opis |
| :--- | :--- | :--- | :--- |
| 0 | `SOH` | 1 bajt | `0x01` (Start of Header) |
| 1 | `Dest_Addr_Hi` | 1 bajt | Viši bajt adrese primaoca |
| 2 | `Dest_Addr_Lo` | 1 bajt | Niži bajt adrese primaoca |
| 3 | `Src_Addr_Hi` | 1 bajt | Viši bajt adrese pošiljaoca |
| 4 | `Src_Addr_Lo` | 1 bajt | Niži bajt adrese pošiljaoca |
| 5 | `Length` | 1 bajt | `0x0D` (13 decimalno) - Dužina podataka koji slede |
| **6** | **`CMD`** | **1 bajt** | **`0xD5`** (`SET_RTC_DATE_TIME`) |
| **7** | **`Day_Tens`** | **1 bajt** | **ASCII '0'-'3'** (Desetice dana) |
| **8** | **`Day_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice dana) |
| **9** | **`Month_Tens`** | **1 bajt** | **ASCII '0'-'1'** (Desetice meseca) |
| **10** | **`Month_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice meseca) |
| **11** | **`Year_Tens`** | **1 bajt** | **ASCII '0'-'9'** (Desetice godine) |
| **12** | **`Year_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice godine) |
| **13** | **`Hour_Tens`** | **1 bajt** | **ASCII '0'-'2'** (Desetice sati) |
| **14** | **`Hour_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice sati) |
| **15** | **`Min_Tens`** | **1 bajt** | **ASCII '0'-'5'** (Desetice minuta) |
| **16** | **`Min_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice minuta) |
| **17** | **`Sec_Tens`** | **1 bajt** | **ASCII '0'-'5'** (Desetice sekundi) |
| **18** | **`Sec_Units`** | **1 bajt** | **ASCII '0'-'9'** (Jedinice sekundi) |
| 19 | `CRC_Hi` | 1 bajt | Viši bajt checksum-a (suma bajtova od 6 do 18) |
| 20 | `CRC_Lo` | 1 bajt | Niži bajt checksum-a |
| 21 | `EOT` | 1 bajt | `0x04` (End of Transmission) |

**Napomena za Payload:** Vreme se šalje kao ASCII string, ne kao sirovi BCD ili binarni podaci (osim komandnog bajta). Na primer, godina "23" se šalje kao bajt `0x32` ('2') pa bajt `0x33` ('3').
