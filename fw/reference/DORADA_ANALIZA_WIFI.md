# Analiza i Plan Dorade: Statički Odabir Mreže i Emergency WiFi

Ovaj dokument opisuje plan izmjena softvera za podršku statičkog odabira mrežnog interfejsa (Ethernet ili WiFi) uz mogućnost "Emergency" konfiguracije putem hardverskog pina.

## 1. Koncept Rada

Umjesto dinamičkog prebacivanja (failover), sistem koristi **statički odabir** primarnog interfejsa.

1. **Normalan Rad (Pin 39 HIGH):**
    * Uređaj čita konfiguraciju iz EEPROM-a (`use_wifi_as_primary`).
    * Ako je `false` (Default): Inicijalizuje se **samo Ethernet**. WiFi radio je isključen.
    * Ako je `true`: Inicijalizuje se **samo WiFi** (koristeći sačuvane kredencijale). Ethernet je isključen.

2. **Emergency / Config Mode (Pin 39 LOW pri paljenju):**
    * Ignoriše se trenutna postavka.
    * Pokreće se `WiFiManager` (AP Mode: "HotelController_Setup").
    * Korisnik se spaja mobitelom, unosi SSID/Pass.
    * Nakon uspješnog spajanja, sistem automatski postavlja `use_wifi_as_primary = true` u EEPROM-u i nastavlja rad na WiFi-u.

3. **Promjena Interfejsa (Web UI):**
    * Korisnik kroz Web interfejs može promijeniti "Primarni Interfejs" (Ethernet/WiFi).
    * Promjena zahtijeva restart.

## 2. Arhitektura i Izmjene po Modulima

### 2.1. `EepromStorage` - Nova Konfiguracija

* U strukturu `AppConfig` dodati:

    ```cpp
    bool use_wifi_as_primary; // false = Ethernet (default), true = WiFi
    ```

* U `MigrateConfig` postaviti default na `false` (Ethernet).

### 2.2. `NetworkManager` - Logika Inicijalizacije

* Izmijeniti `RunTask` da ne pokušava automatski oba interfejsa.
* Dodati logiku:

    ```cpp
    if (g_appConfig.use_wifi_as_primary) {
        InitializeWiFi(); // Blokirajuće spajanje na sačuvani SSID
    } else {
        InitializeETH();
    }
    ```

* Dodati metodu `StartWiFiConfigMode()`:
  * Pokreće `WiFiManager` u "On Demand" modu.
  * Ako se uspješno spoji -> Ažurira `g_appConfig.use_wifi_as_primary = true` i snima u EEPROM.

### 2.3. `main.cpp` - Boot Detekcija

* U `setup()`:
  * Konfigurisati Pin 39 kao INPUT.
  * Provjeriti stanje:
    * **LOW:** Pozvati `g_networkManager.StartWiFiConfigMode()`.
    * **HIGH:** Pozvati `g_networkManager.StartTask()` (koji će pročitati EEPROM i odlučiti).

### 2.4. `HttpServer` / `index.html` - UI

* Dodati opciju u sekciju "Mrežne Postavke":
  * Checkbox ili Select: "Primarna Konekcija: Ethernet / WiFi".
  * Slanje komande (npr. `sysctrl.cgi?set_iface=1` za WiFi, `0` za Ethernet).
  * Backend ažurira EEPROM.

## 3. Detaljan Plan Implementacije

1. **Ažuriranje `EepromStorage.h`**: Dodavanje polja u strukturu.
2. **Ažuriranje `EepromStorage.cpp`**: Migracija i default vrijednosti.
3. **Ažuriranje `NetworkManager.h/cpp`**: Implementacija `StartWiFiConfigMode` i izmjena `RunTask`.
4. **Ažuriranje `main.cpp`**: Logika za Pin 39.
5. **Ažuriranje `index.html`**: Dodavanje UI kontrola.

## 4. Napomene

* **Pin 39:** Zahtijeva eksterni pull-up otpornik jer je Input-Only pin na ESP32.
* **Sigurnost:** Emergency mod omogućava rekonfiguraciju WiFi-a fizičkim pristupom uređaju, što je standardna praksa.
