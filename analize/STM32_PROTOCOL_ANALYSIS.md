# Analiza RS485 Protokola - STM32 Hotel Controller (Legacy)

Ovaj dokument pruža detaljnu analizu RS485 komunikacionog protokola koji se koristi u originalnom STM32 Hotel Controller sistemu. Informacije su izvučene direktno iz izvornog koda (`hotel_ctrl.c`, `common.h`).

## 1. Struktura Paketa

Komunikacija na RS485 magistrali odvija se putem standardizovanog formata paketa. Ključna razlika je u prvom bajtu, koji identifikuje da li je paket zahtev ili odgovor.

### Paket Zahteva (Request Packet)

Paketi koje inicira Hotel Controller (zahtevi) uvek počinju sa `SOH` (Start of Header).

| Offset | Dužina (bajta) | Opis | Vrednost/Primer |
| :--- | :--- | :--- | :--- |
| 0 | 1 | **SOH** (Start of Header) | `0x01` |
| 1 | 2 | **Adresa primaoca** | Big Endian (npr. `0x0065` za adresu 101) |
| 3 | 2 | **Adresa pošiljaoca** | Big Endian |
| 5 | 1 | **Dužina podataka (N)** | Dužina polja "Podaci" |
| 6 | N | **Podaci (Payload)** | Komanda i njeni parametri |
| 6+N | 2 | **Checksum** | 16-bitna suma svih bajtova u polju "Podaci" (Big Endian) |
| 8+N | 1 | **EOT** (End of Transmission) | `0x04` |

**Primer (zahtev za status):**
`01 00 65 00 05 01 A0 00 A1 04`
- `01`: SOH
- `00 65`: Adresa primaoca (101)
- `00 05`: Adresa pošiljaoca (5, Hotel Controller)
- `01`: Dužina podataka = 1 bajt
- `A0`: Podaci (komanda `GET_SYS_STAT`)
- `00 A1`: Checksum od `A0`
- `04`: EOT

### Paket Odgovora (Response Packet)

Paketi koje šalju periferni uređaji (odgovori) počinju sa `ACK` (Acknowledge) ili `NAK` (Negative Acknowledge), ali zadržavaju istu osnovnu strukturu.

| Offset | Dužina (bajta) | Opis | Vrednost/Primer |
| :--- | :--- | :--- | :--- |
| 0 | 1 | **ACK** ili **NAK** | `0x06` (ACK) ili `0x15` (NAK) |
| 1 | 2 | **Adresa primaoca** (HC) | Big Endian |
| 3 | 2 | **Adresa pošiljaoca** (Uređaj)| Big Endian |
| 5 | 1 | **Dužina podataka (N)** | Dužina polja "Podaci" |
| 6 | N | **Podaci (Payload)** | Status ili traženi podaci |
| 6+N | 2 | **Checksum** | 16-bitna suma svih bajtova u polju "Podaci" (Big Endian) |
| 8+N | 1 | **EOT** (End of Transmission) | `0x04` |

**Primer (potvrdni odgovor od uređaja 101):**
`06 00 05 00 65 01 64 00 64 04`
- `06`: ACK
- `00 05`: Adresa primaoca (Hotel Controller)
- `00 65`: Adresa pošiljaoca (101)
- `01`: Dužina podataka = 1 bajt
- `64`: Podaci (npr. potvrda komande `DWNLD_DISP_IMG_1`)
- `00 64`: Checksum od `64`
- `04`: EOT

### Checksum Kalkulacija

Checksum je jednostavna 16-bitna suma svih bajtova u polju "Podaci".

```c
// Izvor: hotel_ctrl.c -> PCK_RECEIVING state
rs485_pkt_chksum = 0x0U;
for (j = bcnt + 0x6U; j < (rx_buff[bcnt + 0x5U] + 0x6U); j++)
{
    rs485_pkt_chksum += rx_buff[j];
}
```

## 2. Ključni Vremenski Parametri (Tajminzi)

Vremenski parametri definisani u `common.h` su kritični za razumevanje toka komunikacije.

| Konstanta | Vrednost (ms) | Opis |
| :--- | :--- | :--- |
| `RESP_TOUT` | 45 | Maksimalno vreme čekanja na odgovor od uređaja. |
| `BIN_TOUT` | 321 | Timeout za transfer binarnih paketa (npr. One-Wire bridge). |
| `RX_TOUT` | 3 | Timeout za prijem sledećeg bajta unutar istog paketa. |
| `RX2TX_DEL` | 3 | Pauza između prijema paketa i slanja sledećeg. |
| `MAXREP_CNT` | 30 | Maksimalan broj pokušaja ponovnog slanja paketa na istu adresu. |
| `FWR_UPLD_DEL` | 2345 | Stari FW update: pauza za backup firmvera na klijentu. |
| `BLDR_START_DEL`| 3456 | Pauza pre slanja prvog paketa bootloaderu. |
| `FWR_COPY_DEL` | 1567 | Vreme koje se ostavlja klijentu da iskopira novi firmver pre nastavka komunikacije. |
| `IMG_COPY_DEL` | 4567 | Vreme koje se ostavlja klijentu da iskopira novu sliku i pripremi se za sledeću. |
| `APP_START_DEL`| 12345 | Vreme koje se ostavlja klijentu da pokrene novu aplikaciju nakon update-a. |

---

## 3. Analiza Komunikacionih Procesa

### 3.1. Firmware / Bootloader Update

Ovo je najkompleksniji proces, vođen stanjima definisanim u `HC_FwrUpdPck` strukturi.

**Koraci:**

1.  **Inicijacija (HTTP zahtev):**
    - Korisnik šalje HTTP komandu (`fwu`, `cud`, `buf`).
    - Hotel Controller (HC) postavlja `request = UPDATE_FWR` ili `UPDATE_BLDR`.
    - `HC_CheckNewFirmwareFile()` otvara `NEW.BIN` sa SD kartice i proverava `FwInfoTypeDef` (verzija, CRC32, veličina). Izračunava ukupan broj paketa: `pck_total`.
    - Stanje se postavlja na `FW_UPDATE_INIT`.

2.  **Slanje Startnog Paketa (Handshake):**
    - HC šalje prvi paket ka ciljnom uređaju.
    - **Komanda:** `UPDATE_FWR` (`0xC1`) ili `UPDATE_BLDR` (`0xC2`).
    - **Podaci (Payload):**
        - `FwInfoTypeDef` struktura koja sadrži metapodatke o firmveru koji se šalje.
        - **Veličina:** 20 bajtova (`size`, `crc32`, `version`, `wr_addr`, `ld_addr`).
    - HC čeka `ACK`.

3.  **Transfer Podataka (Slanje Paketa):**
    - Nakon prijema `ACK`-a, HC prelazi u stanje `FW_UPDATE_RUN`.
    - Započinje petlja slanja paketa sa podacima.
    - **Komanda:** Ista kao u startnom paketu (`0xC1` ili `0xC2`).
    - **Struktura Podataka (Payload):**
        | Offset | Dužina (bajta) | Opis |
        | :--- | :--- | :--- |
        | 0 | 2 | Ukupan broj paketa (`pck_total`) |
        | 2 | 2 | Redni broj trenutnog paketa (`pck_send`) |
        | 4 | 128 | Deo firmvera (`HC_PCK_BSIZE`) |
    - Klijent odgovara sa `ACK` na svaki uspešno primljeni paket.
    - Ako HC primi `NAK` ili ne primi odgovor (`RESP_TOUT`), ponovo šalje isti paket dok brojač pokušaja (`trial`) ne dostigne `MAXREP_CNT`.

4.  **Završetak Transfera:**
    - Nakon što je poslat i potvrđen poslednji paket (`pck_send == pck_total`), HC prelazi u stanje `FW_UPDATE_END`.
    - HC šalje finalnu komandu klijentu: `APP_EXE` (`0xBB`) da bi klijent pokrenuo novu aplikaciju, ili `START_BLDR` (`0xBC`).
    - Pre slanja ove komande, HC čeka `APP_START_DEL` (12.3 sekunde) ili `BLDR_START_DEL` (3.4 sekunde) da bi dao klijentu dovoljno vremena da procesira preuzeti fajl.

---

### 3.2. Update Slike (Image)

Proces je veoma sličan firmware update-u, ali koristi drugačije komande i strukture.

**Koraci:**

1.  **Inicijacija (HTTP zahtev):**
    - Korisnik šalje komandu (`iuf`).
    - HC postavlja `request = DWNLD_DISP_IMG` (`0x63`).
    - Lista adresa i slika se popunjava u `imgupd_add_list` i `filupd_list`.
    - `HC_CheckNewImageFile()` otvara odgovarajući fajl (`IMGxx.RAW`) sa SD kartice.
    - Stanje se postavlja na `DWNLD_DISP_IMG_xx`.

2.  **Slanje Paketa sa Podacima:**
    - Za razliku od FW update-a, ne postoji eksplicitan handshake sa metapodacima. Proces odmah kreće sa slanjem podataka.
    - **Komanda:** Zavisi od broja slike, npr. `DWNLD_DISP_IMG_1` (`0x64`), `DWNLD_DISP_IMG_2` (`0x65`), itd.
    - **Struktura Podataka (Payload):**
        | Offset | Dužina (bajta) | Opis |
        | :--- | :--- | :--- |
        | 0 | 2 | Ukupan broj paketa (`pck_total`) |
        | 2 | 2 | Redni broj trenutnog paketa (`pck_send`) |
        | 4 | 128 | Deo slike |
    - Tok ACK/NAK kontrole i ponovnog slanja je identičan kao kod FW update-a.

3.  **Završetak Transfera:**
    - Nakon potvrde poslednjeg paketa, HC čeka `IMG_COPY_DEL` (4.5 sekunde).
    - Ako postoji još slika za slanje na istu adresu, proces se ponavlja za sledeću sliku.
    - Ako ima još adresa, proces se ponavlja za sledeću adresu.

---

### 3.3. Pooling Statusa Uređaja

Ovo je podrazumevana operacija koju HC izvršava u petlji kada nema drugih prioritetnijih zadataka.

**Koraci:**

1.  **Odabir Adrese:**
    - `HC_GetNextAddr()` uzima sledeću adresu iz `addr_list` (učitana u RAM sa SPI Flasha).
2.  **Slanje Zahteva:**
    - `HC_CreateRoomCtrlStatReq()` kreira paket.
    - **Komanda:** `GET_SYS_STAT` (`0xA0`).
    - **Podaci (Payload):** Samo jedan bajt - komanda.
3.  **Prijem Odgovora:**
    - Klijent odgovara paketom koji sadrži njegov status.
    - **Komanda u odgovoru:** `GET_SYS_STAT` (`0xA0`).
    - **Podaci (Payload) u odgovoru:** String koji sadrži statusne informacije, opisan u `Procitaj !!!.txt`.
    - HC parsira ovaj string i reaguje ako je potrebno (npr. ako uređaj signalizira da ima logove za slanje ili zahteva update).

---

### 3.4. Transfer Logova

Ovaj proces se pokreće kada status uređaja ukaže na postojanje novih logova.

**Koraci:**

1.  **Inicijacija:**
    - HC detektuje iz statusnog odgovora da uređaj ima logove.
    - Postavlja stanje `HC_LogTransfer.state = TRANSFER_QUERY_LIST`.
2.  **Slanje Zahteva za Log:**
    - `HC_CreateLogRequest()` kreira paket.
    - **Komanda:** `GET_LOG_LIST` (`0xA3`).
    - **Podaci (Payload):** Samo jedan bajt - komanda.
3.  **Prijem Loga:**
    - Uređaj odgovara `ACK` paketom koji sadrži log.
    - **Komanda u odgovoru:** `GET_LOG_LIST` (`0xA3`).
    - **Struktura Podataka (Payload) u odgovoru:**
        | Offset | Dužina (bajta) | Opis |
        | :--- | :--- | :--- |
        | 0 | 16 | Log zapis (`LOG_DSIZE`) |
    - HC prima paket, parsira log zapis i upisuje ga u sopstveni EEPROM (`HC_WriteLog`).
4.  **Brisanje Loga na Klijentu (Opciono):**
    - Nakon uspešnog preuzimanja, HC može poslati komandu `DEL_LOG_LIST` (`0xD3`) da obriše log na klijentskom uređaju i oslobodi prostor.
    - Klijent odgovara `ACK` paketom kao potvrdu brisanja.
