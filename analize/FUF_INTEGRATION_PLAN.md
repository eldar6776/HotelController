# Plan Integracije za `fuf` Komandu (Dopunjena Analiza)

## 1. Cilj

Cilj je modifikovati ESP32 projekat tako da HTTP komanda `fuf` (Firmware Update) funkcioniše identično kao u nasleđenom STM32F429 sistemu. To podrazumeva pokretanje procesa ažuriranja firmvera za grupu uređaja (Room Controllers) korišćenjem fajla `IMG20.RAW` sa SD kartice, a ne `NEW.BIN`.

## 2. Analiza Nasleđene (STM32) Implementacije

Na starom sistemu, komanda `fuf` je bila pametan alias koji je koristio postojeći, robustan mehanizam za ažuriranje slika.

-   **HTTP Komanda:** `http://hotel_ctrl/sysctrl.cgi?fuf=101&ful=103`
-   **Ponašanje:** Pokreće update za sve adrese u opsegu od 101 do 103.
-   **Fajl:** Sistem implicitno traži i koristi fajl `IMG20.RAW` sa root direktorijuma SD kartice.
-   **RS485 Protokol:** Interno, HC ne šalje standardnu komandu za update firmvera. Umesto toga, šalje komandu `DWNLD_DISP_IMG_20` (vrednost `0x77`), koja je deo protokola za update slika.
-   **Zaključak:** Proces je u potpunosti tretiran kao da je korisnik zatražio update slike broj 20, čime se reciklira provereni mehanizam za transfer blokova podataka sa ACK/NAK kontrolom.

## 3. Analiza Trenutne (ESP32) Implementacije

ESP32 projekat već ima delimičnu implementaciju, ali sa ključnom greškom u logici.

-   **`HttpServer.cpp`:**
    -   HTTP handler `HandleSysctrlRequest` ispravno prepoznaje `fuf` i `ful` parametre.
    -   Poziva se funkcija `StartUpdateSession` koja prosleđuje komandu `CMD_DWNLD_FWR_IMG` `UpdateManager`-u.
-   **`UpdateManager.cpp`:**
    -   U funkciji `PrepareSession`, `CMD_DWNLD_FWR_IMG` se **pogrešno mapira** na `filename = "/NEW.BIN"`.
    -   Ovo pokreće standardni proces ažuriranja firmvera, što nije u skladu sa nasleđenim sistemom.

---

## 4. Detaljna Analiza Update Procesa (ESP32 `UpdateManager`)

Postoje dva fundamentalno različita procesa unutar `UpdateManager`-a, koji se aktiviraju u zavisnosti od tipa sesije (`UpdateType`).

### 4.1. Proces Ažuriranja Slike (`TYPE_IMG_RC`)

Ovaj proces se aktivira `iuf` komandom i služi kao **ispravan model** za `fuf` komandu.

1.  **Inicijacija:** `HttpServer` poziva `StartImageUpdateSequence`, koja za svaku adresu/sliku poziva `StartSession`. `PrepareSession` određuje `type = TYPE_IMG_RC` i formira putanju do fajla (npr. `/101/101_1.RAW`).
2.  **Start Paket (Handshake):**
    -   Poziva se `SendFileStartRequest()`.
    -   Šalje se **jedan** startni paket koji sadrži sve metapodatke.
    -   **Sadržaj Payload-a (11 bajtova):**
        -   `sub_cmd` (1B): Komanda za sliku, npr. `0x64` za sliku 1.
        -   `total_packets` (2B): Ukupan broj paketa.
        -   `fw_size` (4B): Ukupna veličina fajla u bajtovima.
        -   `fw_crc` (4B): CRC32 fajla.
    -   Nakon slanja, čeka se `ACK` od klijenta **1300 ms**.
3.  **Transfer Podataka:**
    -   Nakon `ACK`-a, poziva se `SendDataPacket()` u petlji.
    -   Šalju se paketi sa `STX` (0x02) na početku, rednim brojem (2B) i podacima (128B).
    -   Nakon svakog paketa, čeka se `ACK` do `UPDATE_PACKET_TIMEOUT_MS` (45 ms).
4.  **Završetak i Tajminzi:**
    -   Transfer se smatra **završenim odmah nakon prijema `ACK`-a na poslednji paket podataka**.
    -   Ne šalje se nikakav dodatni "finish" paket.
    -   `UpdateManager` zatim pravi pauzu od `FWR_COPY_DEL` (**1567 ms**) pre nego što `ImageUpdateSequence` pokrene update sledeće slike ili pređe na sledeću adresu.

### 4.2. Proces Ažuriranja Firmvera (`TYPE_FW_RC`)

Ovaj proces se **pogrešno aktivira** `fuf` komandom u trenutnoj implementaciji.

1.  **Inicijacija:** `HttpServer` poziva `StartSession` sa `CMD_DWNLD_FWR_IMG`. `PrepareSession` određuje `type = TYPE_FW_RC` i koristi fajl `/NEW.BIN`.
2.  **Start Paket (Handshake):**
    -   Poziva se `SendFirmwareStartRequest()`.
    -   Ovo je **dvofazni handshake**:
        1.  Prvo se šalje paket sa komandom `CMD_START_BLDR` (0xBC).
        2.  Nakon `ACK`-a, šalje se drugi paket sa komandom `CMD_DWNLD_FWR_IMG` i ukupnim brojem paketa.
    -   Ovaj proces je fundamentalno drugačiji od handshake-a za slike.
3.  **Transfer Podataka:**
    -   Identičan kao kod slika (koristi se `SendDataPacket()`).
4.  **Završetak i Tajminzi:**
    -   Ovo je najkompleksniji deo i drastično se razlikuje od update-a slika.
    -   Nakon `ACK`-a na poslednji paket podataka, čeka se duga pauza od `IMG_COPY_DEL` (**4567 ms**).
    -   Zatim se ponovo šalje komanda `CMD_START_BLDR` (0xBC) putem `SendRestartCommand()`.
    -   Nakon toga sledi najduža pauza od `APP_START_DEL` (**12345 ms**).
    -   Na kraju, šalje se komanda `CMD_APP_EXE` (0xBB) da bi klijent pokrenuo novu aplikaciju. Na ovu komandu se ne čeka odgovor.

### 4.3. Ključne Razlike i Problem sa `fuf` Komandom

| Aspekt | Ažuriranje Slike (Ispravan model za `fuf`) | Ažuriranje Firmvera (Trenutno ponašanje `fuf`) |
| :--- | :--- | :--- |
| **Fajl** | Dinamički, npr. `/101/101_1.RAW` | Fiksni, `/NEW.BIN` |
| **Start Paket** | **Jedan paket** sa svim metapodacima (veličina, CRC32). | **Dvofazni:** `START_BLDR` -> `ACK` -> `DWNLD_FWR_IMG`. |
| **Završetak** | Završava se nakon `ACK`-a na poslednji data paket. | Kompleksan: `ACK` -> pauza (4.5s) -> `START_BLDR` -> pauza (12.3s) -> `APP_EXE`. |
| **Ukupno vreme**| Značajno brže zbog jednostavnijeg završetka. | Značajno sporije zbog dugih pauza na kraju. |

**Zaključak analize:** Trenutna implementacija `fuf` komande u ESP32 kodu je netačna jer aktivira spori i kompleksni proces ažuriranja firmvera, umesto brzog i jednostavnog procesa ažuriranja slike koji je nasleđeni sistem koristio (sa `IMG20.RAW`).

---

## 5. Predlog Plana Integracije

Da bi se postigla kompatibilnost, potrebno je izmeniti logiku u `HttpServer.cpp` tako da `fuf` komandu tretira kao poseban slučaj sekvence za ažuriranje slika.

**Fajl za modifikaciju:** `fw/src/HttpServer.cpp`

### Koraci za Implementaciju:

1.  **Pronaći `fuf` handler:**
    U funkciji `HttpServer::HandleSysctrlRequest`, pronaći postojeći blok koda:
    ```cpp
    // --- RC update firmware: fuf, ful ---
    if (request->hasParam("fuf") && request->hasParam("ful"))
    {
        // ... trenutna pogrešna logika ...
    }
    ```

2.  **Zameniti logiku:**
    Kompletan sadržaj ovog `if` bloka treba zameniti logikom koja poziva sekvencer za slike.

3.  **Implementirati novu logiku:**
    Nova logika treba da uradi sledeće:
    -   Proveriti da li je SD kartica dostupna (`m_sd_card_manager->IsCardMounted()`).
    -   Parsirati vrednosti `fuf` i `ful` parametara da bi se dobile početna i krajnja adresa.
    -   Pozvati `UpdateManager` da pokrene sekvencu ažuriranja, ali sa **fiksiranim brojevima slika na 20**.
        ```cpp
        // Primer nove logike
        uint16_t first_addr = request->getParam("fuf")->value().toInt();
        uint16_t last_addr = request->getParam("ful")->value().toInt();
        m_update_manager->StartImageUpdateSequence(first_addr, last_addr, 20, 20);
        SendSSIResponse(request, "OK (RC Firmware update sequence started)");
        ```
    -   Odgovoriti klijentu odmah, jer `StartImageUpdateSequence` radi u pozadini.

4.  **Neophodna modifikacija u `UpdateManager.cpp`:**
    -   U funkciji `UpdateManager::PrepareSession`, potrebno je dodati poseban slučaj za sliku broj 20.
    -   **Logika:** Ako je `updateCmd` za sliku broj 20, `filename` treba da bude `/IMG20.RAW` (iz root-a), a ne da se gradi dinamička putanja `/<addr>/<addr>_20.RAW`.
        ```cpp
        // Unutar PrepareSession, u delu za obradu slika
        uint8_t img_num = updateCmd - CMD_IMG_RC_START + 1;

        if (img_num == 20) {
            filename = "/IMG20.RAW"; // Specijalan slučaj za fuf
        } else {
            filename = "/" + String(s->clientAddress) + "/" + String(s->clientAddress) + "_" + String(img_num) + ".RAW";
        }
        ```

Ovim pristupom se `fuf` komanda ispravno preusmerava na brži i adekvatniji protokol za slike, čime se postiže puna kompatibilnost sa nasleđenim sistemom.
