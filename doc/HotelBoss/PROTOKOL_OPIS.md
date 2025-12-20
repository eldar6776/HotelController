# Opis Protokola

## 1. Format Logova

Log zapisi se čuvaju u EEPROM memoriji. Veličina jednog log zapisa je 16 bajtova (`RUBICON_LOG_SIZE`).

| Pozicija (Bajt) | Opis | Detalji |
| :--- | :--- | :--- |
| 0 | Log ID (MSB) | Viši bajt rednog broja loga |
| 1 | Log ID (LSB) | Niži bajt rednog broja loga |
| 2 | Kod Događaja | Identifikator tipa događaja (npr. `RUBICON_LOG_GUEST_CARD_VALID`, `RUBICON_LOG_ENTRY_DOOR_OPENED`, itd.) |
| 3 | Adresa Kontrolera (MSB) | Viši bajt RS485 adrese kontrolera (upisuje se u `RUBICON_WriteLogToList`) |
| 4 | Adresa Kontrolera (LSB) | Niži bajt RS485 adrese kontrolera (upisuje se u `RUBICON_WriteLogToList`) |
| 5 | Detalji Događaja | Dodatne informacije o događaju |
| 6 | Rezervisano | 0x00 |
| 7 | Rezervisano | 0x00 |
| 8 | Rezervisano | 0x00 |
| 9 | Rezervisano | 0x00 |
| 10 | Datum | Dan u mesecu (BCD format) |
| 11 | Mesec | Mesec (BCD format) |
| 12 | Godina | Godina (BCD format) |
| 13 | Sat | Sati (BCD format) |
| 14 | Minut | Minuti (BCD format) |
| 15 | Sekunda | Sekunde (BCD format) |

**Napomena:** Podaci o vremenu (bajtovi 10-15) se preuzimaju direktno iz RTC-a u BCD formatu prilikom kreiranja događaja u funkciji `CONTROLLER_WriteLogEvent`.

---

## 2. Format Paketa za Slanje Vremena (RS485)

Ovaj paket se koristi za sinhronizaciju vremena na uređajima (`RUBICON_PrepareTimeUpdatePacket`). Komanda je `RUBICON_SET_RTC_DATE_TIME` (0xD5).

Ukupna dužina paketa je 22 bajta.

| Pozicija (Bajt) | Naziv | Vrednost / Opis |
| :--- | :--- | :--- |
| 0 | SOH | `0x01` (Početak paketa) |
| 1 | Destination Address MSB | Viši bajt adrese primaoca |
| 2 | Destination Address LSB | Niži bajt adrese primaoca |
| 3 | Source Address MSB | Viši bajt adrese pošiljaoca (interfejs adresa) |
| 4 | Source Address LSB | Niži bajt adrese pošiljaoca (interfejs adresa) |
| 5 | Dužina Podataka | `0x0D` (13 bajtova podataka koji slede, od bajta 6 do 18) |
| 6 | Komanda | `0xD5` (`RUBICON_SET_RTC_DATE_TIME`) |
| 7 | Dan (Desetice) | ASCII karakter (npr. '0', '1', '2', '3') |
| 8 | Dan (Jedinice) | ASCII karakter (0-9) |
| 9 | Mesec (Desetice) | ASCII karakter ('0', '1') |
| 10 | Mesec (Jedinice) | ASCII karakter (0-9) |
| 11 | Godina (Desetice) | ASCII karakter (0-9) |
| 12 | Godina (Jedinice) | ASCII karakter (0-9) |
| 13 | Sat (Desetice) | ASCII karakter (0-2) |
| 14 | Sat (Jedinice) | ASCII karakter (0-9) |
| 15 | Minut (Desetice) | ASCII karakter (0-5) |
| 16 | Minut (Jedinice) | ASCII karakter (0-9) |
| 17 | Sekunda (Desetice) | ASCII karakter (0-5) |
| 18 | Sekunda (Jedinice) | ASCII karakter (0-9) |
| 19 | CRC MSB | Viši bajt CRC sume (suma bajtova od pozicije 6 do 18) |
| 20 | CRC LSB | Niži bajt CRC sume |
| 21 | EOT | `0x04` (Kraj transmisije) |

**Format Vremena:** Vreme se šalje kao niz ASCII karaktera koji predstavljaju BCD vrednosti (npr. ako je dan 21., šalje se karakter '2' pa '1'). Vrednosti se dobijaju konverzijom BCD vrednosti iz RTC-a: `(BCD >> 4) + 48` za desetice i `(BCD & 0x0F) + 48` za jedinice.
