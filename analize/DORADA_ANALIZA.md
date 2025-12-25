# Analiza i Plan Dorade: Podrška za Dvostruki RS485 Bus

Ovaj dokument opisuje plan izmjena softvera za podršku dva RS485 bus-a (Lijevi i Desni) sa zasebnim listama adresa i kontrolom DE pinova, na osnovu zahtjeva iz `dorada.txt`.

## 1. Zahtjevi i Ograničenja

1.  **Dvije Liste Adresa:**
    *   `CTRL_ADD_L.TXT`: Lista kontrolera na lijevom bus-u.
    *   `CTRL_ADD_R.TXT`: Lista kontrolera na desnom bus-u.
    *   **Fallback:** Ako ove liste ne postoje, koristi se stara `CTRL_ADD.TXT` (single bus mode).

2.  **Hardver (Dual RS485):**
    *   Postoje dva RS485 transcivera.
    *   UART RX/TX linije su dijeljene (spojene na isti UART porta ESP32).
    *   Svaki transciver ima svoj DE (Driver Enable) pin.
    *   **Logika DE pina:**
        *   Aktivan Bus: DE se koristi normalno (HIGH za TX, LOW za RX).
        *   Neaktivan Bus: DE mora biti **HIGH** sve vrijeme. (Ovo onemogućava prijemnik tog transcivera kako ne bi ometao dijeljenu RX liniju, pod pretpostavkom da je /RE vezan na DE).

3.  **Logika Skeniranja (`LogPullManager`):**
    *   Skeniraj cijelu Lijevu listu.
    *   Prebaci se na Desni bus.
    *   Skeniraj cijelu Desnu listu.
    *   Ponavljaj.

4.  **Logika Upita (`HttpQueryManager`):**
    *   Automatski detektuj na kojem je bus-u adresirani uređaj.
    *   Prijedlog za nepoznate adrese: Pokušaj prvo na Lijevom, pa na Desnom bus-u.

## 2. Arhitektura i Izmjene po Modulima

### 2.1. `ProjectConfig.h` - Konfiguracija

*   Definisati putanje za nove fajlove:
    ```cpp
    #define PATH_CTRL_ADD_L     "/CTRL_ADD_L.TXT"
    #define PATH_CTRL_ADD_R     "/CTRL_ADD_R.TXT"
    // PATH_CTRL_ADD_LIST ostaje "/CTRL_ADD.TXT"
    ```
*   `MAX_ADDRESS_LIST_SIZE` (trenutno 500) će se logički podijeliti ili redefinisati kao kapacitet *po listi*. Radi očuvanja EEPROM mapa, zadržat ćemo ukupnu veličinu prostora (1000 bajtova) ali ga podijeliti na dva dijela (2 x 250 adresa).

### 2.2. `Rs485Service` - Kontrola Bus-a

*   Dodati metodu `SelectBus(uint8_t busId)`.
    *   `busId = 0` (Lijevi/Glavni): DE1 aktivan, DE2 HIGH (Disabled).
    *   `busId = 1` (Desni): DE1 HIGH (Disabled), DE2 aktivan.
*   Dodati privatnu varijablu `m_active_bus`.
*   Ažurirati `SendPacket` i `Initialize`:
    *   `SendPacket` mora koristiti odgovarajući DE pin na osnovu `m_active_bus`.
    *   Prilikom `Initialize`, postaviti default (Bus 0).

### 2.3. `EepromStorage` - Perzistencija Listi

*   Postojeći prostor za adrese (`EEPROM_ADDRESS_LIST_SIZE` = 1000 bajtova) podijeliti na:
    *   **Lista L:** Prvih 500 bajtova (250 adresa).
    *   **Lista R:** Drugih 500 bajtova (250 adresa).
*   Dodati funkciju `LoadAddressListFromSD(SdCardManager* sd, const char* filename, uint8_t listId)`.
    *   Ova funkcija implementira parsiranje CSV fajla (preuzeto iz `HttpServer`) i upisuje u odgovarajući dio EEPROM-a.
*   Dodati funkcije za čitanje:
    *   `ReadAddressListL(...)`
    *   `ReadAddressListR(...)`

### 2.4. `main.cpp` - Inicijalizacija

*   U `setup()` funkciji dodati logiku za učitavanje lista:
    1.  Provjeri postojanje `CTRL_ADD_L.TXT` i `CTRL_ADD_R.TXT`.
    2.  Ako postoje: Učitaj L u EEPROM (Slot 0) i R u EEPROM (Slot 1).
    3.  Ako NE postoje: Provjeri `CTRL_ADD.TXT`.
        *   Ako postoji: Učitaj u EEPROM (Slot 0 - Lijevo). Očisti EEPROM Slot 1 (Desno).
        *   Ovo osigurava "Single Bus" kompatibilnost (uređaj radi samo na Bus 0).

### 2.5. `LogPullManager` - Logika Skeniranja

*   Proširiti klasu da drži dvije liste u RAM-u: `m_address_list_L` i `m_address_list_R`.
*   Dodati state varijablu `m_current_bus_index` (0 ili 1).
*   Izmijeniti `Run()` i `GetNextAddress()`:
    *   Kada se završi ciklus prolaska kroz jednu listu, prebaci `m_current_bus_index`, pozovi `m_rs485_service->SelectBus(...)` i nastavi sa drugom listom.
    *   Ako je lista prazna, odmah pređi na sljedeću.
*   Dodati javnu metodu `GetBusForAddress(uint16_t addr)` koja vraća ID bus-a (0 ili 1) ako adresa postoji u jednoj od lista, ili -1 ako ne postoji.

### 2.6. `HttpQueryManager` - Logika Upita

*   Izmijeniti `ExecuteBlockingQuery`:
    1.  Provjeri na kojem je bus-u adresa pozivom `LogPullManager::GetBusForAddress(addr)`.
    2.  Ako je poznato (0 ili 1):
        *   `m_rs485_service->SelectBus(busId)`
        *   Pošalji upit.
    3.  Ako NIJE poznato (-1):
        *   **Strategija:** Pokušaj Bus 0.
        *   `m_rs485_service->SelectBus(0)`
        *   Pošalji i čekaj odgovor (50ms).
        *   Ako timeout:
            *   `m_rs485_service->SelectBus(1)`
            *   Pošalji i čekaj odgovor.
    4.  Nakon upita, vratiti bus u stanje koje očekuje `LogPullManager` (ili neka `LogPullManager` sam resetuje bus prije svog ciklusa - sigurnije).

## 3. Detaljan Plan Implementacije

1.  **Refaktorisanje `ProjectConfig.h`**: Dodavanje definicija.
2.  **Nadogradnja `EepromStorage`**: Implementacija `LoadAddressListFromSD` i podjela memorijskog prostora.
3.  **Nadogradnja `Rs485Service`**: Implementacija `SelectBus` i dual-DE logike.
4.  **Nadogradnja `LogPullManager`**: Učitavanje dvije liste i "ping-pong" logika skeniranja.
5.  **Povezivanje u `main.cpp`**: Pozivanje loadera pri startup-u.
6.  **Nadogradnja `HttpQueryManager`**: Pametno rutiranje upita.
7.  **Čišćenje `HttpServer.cpp`**: Zamjena inline parsiranja pozivom nove funkcije u `EepromStorage` (za `cad=load` komandu).

## 4. Napomene

*   **EEPROM Kompatibilnost:** Zadržavanjem `EEPROM_ADDRESS_LIST_SIZE` na 1000 bajtova i `EEPROM_LOG_START_ADDR` na istom mjestu, ne narušavamo postojeće logove. Stari uređaji koji pređu na novi FW će imati "smeće" u listama dok se ne učita novi fajl, ali `LoadAddressListFromSD` će to pregaziti pri prvom boot-u (ili `LoadDefault`).
*   **DE Pin Logika:** Ključno je osigurati da se `SelectBus` poziva prije bilo kakve komunikacije i da neaktivni bus drži DE visoko (disable RX).

