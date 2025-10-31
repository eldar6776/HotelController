# Migracija Hotel Controller-a na ESP32 (WT32-ETH01)

Ovaj projekat predstavlja potpunu softversku i hardversku migraciju postojećeg "Hotel Controller" sistema sa STM32F429 platforme na moderniju ESP32 (WT32-ETH01) platformu.

Glavni cilj je zadržati **100% kompatibilnost** sa postojećim RS485 protokolom i HTTP CGI komandama. Zbog kompleksnosti sistema i nedostatka naprednih *in-circuit* debagera (npr. Keil MDK), monolitna `HC_Service` "superloop" arhitektura je napuštena.

Umjesto nje, projekat je refaktorisan u **modularnu FreeRTOS arhitekturu** baziranu na nezavisnim "Menadžerima" (servisima).

## 1. Ključne Značajke

* **Platforma:** ESP32 (WT32-ETH01) sa Arduino frameworkom.
* **Mreža:** Primarno Ethernet (ETH), sa Wi-Fi (`WiFiManager`) kao backup opcijom.
* **Komunikacija:** RS485 (`Serial2`) dispečer zadatak koji upravlja sa više logičkih menadžera.
* **HTTP Server:** `AsyncWebServer` koji u potpunosti replicira stare CGI GET komande i `HTTP2RS485` blokirajuću logiku.
* **Skladištenje Konfiguracije:** I2C EEPROM (`24C1024`) za konfiguraciju, listu RS485 adresa i cirkularni log (`head/tail`).
* **Skladištenje Fajlova:** Eksterni SPI Flash (`W25Q512`, 64MB) za skladištenje velikih *update* fajlova (npr. 11.4MB QSPI fajl).
* **Grafika:** Podrška za DWIN serijski displej (UART) za prikaz logova i statusa.

## 2. Hardverska Arhitektura (Mapa Pinova v11)

Ovo je finalna, zaključana mapa pinova koja koristi jedan SPI Flash čip (`W25Q512`) i vraća `Status LED` u funkciju.

| Funkcija | Periferija | Pin na ploči | GPIO | Obrazloženje |
| :--- | :--- | :--- | :--- | :--- |
| **Kritični Sat**| EMAC CLK | `IO0` | `GPIO0` | **Interno zauzeto. Ne koristiti!** |
| **RS485** | `Serial2` RX | `RXD` | `GPIO5` | Zauzeto (Hardverski UART) |
| **RS485** | `Serial2` TX | `TXD` | `GPIO17` | Zauzeto (Hardverski UART) |
| **RS485** | Driver Enable | `485_EN` | `GPIO33` | Namjenski pin na ploči. |
| **I2C (EEPROM)**| I2C SDA | `IO14` | `GPIO14` | Zauzeto (Hardverski I2C) |
| **I2C (EEPROM)**| I2C SCL | `IO15` | `GPIO15` | Zauzeto (Hardverski I2C) |
| **DWIN Displej**| `Serial0` TX | `TX0` | `GPIO1` | **Trajno zauzeto** (Debug / DWIN). |
| **DWIN Displej**| `Serial0` RX | `RX0` | `GPIO3` | **Trajno zauzeto** (Debug / DWIN). |
| **Ext. Flash** | SPI SCK (Sat) | `IO12` | `GPIO12` | Namjenski SPI za Flash (W25Q512). |
| **Ext. Flash** | SPI MOSI (Izlaz)| `IO4` | `GPIO4` | Namjenski SPI za Flash. |
| **Ext. Flash** | SPI MISO (Ulaz)| `IO36` | `GPIO36` | Namjenski SPI za Flash (Input-Only pin). |
| **Ext. Flash** | SPI CS | `CFG` | `GPIO32` | Chip Select za Flash. |
| **Status LED** | On-board LED | `IO2` | `GPIO2` | **Vraćeno u funkciju** za dijagnostiku. |
| **WLAN Reset** | Dugme | `IO39` | `GPIO39` | Input-Only pin (reset WiFi postavki). |
| **Rasvjeta** | Relej | (Nema) | **VIRTUALNI PIN** | Softverska logika (za budući I2C Expander). |

## 3. Softverska Arhitektura (Pregled Modula)

Projekat je podijeljen na sljedeće logičke module, koji se nalaze u `src/` i `include/` direktorijima:

* **`main.cpp`**
    * Glavna ulazna tačka. Odgovoran samo za inicijalizaciju modula i pokretanje FreeRTOS zadataka. `loop()` sadrži samo ne-blokirajuće servisne pozive.

* **`ProjectConfig.h`**
    * Definiše hardversku Mapu Pinova (v11) i fiksnu Memorijsku Mapu za EEPROM i SPI Flash.

* **`NetworkManager.h / .cpp`**
    * Upravlja ETH i Wi-Fi konekcijom, pokreće NTP sinhronizaciju (`configTzTime`) i Ping Watchdog.

* **`EepromStorage.h / .cpp`**
    * Upravlja I2C (`Wire`) komunikacijom sa `24C1024` EEPROM-om.
    * Implementira `head/tail` loger.
    * Čita/piše Konfiguraciju i Listu Adresa.

* **`SpiFlashStorage.h / .cpp`**
    * Upravlja SPI komunikacijom sa `W25Q512` (64MB) Flash čipom.
    * Pruža API za `Erase`, `WriteChunk` i `ReadChunk` za velike FW fajlove.

* **`Rs485Service.h / .cpp`**
    * **Srce aplikacije.** Ovo je centralni FreeRTOS zadatak (dispečer) koji serijalizuje pristup RS485 magistrali.
    * Validira SOH/CRC/EOT pakete i prosljeđuje ih menadžerima.

* **`HttpServer.h / .cpp`**
    * Pokreće `AsyncWebServer` na oba interfejsa (ETH/WiFi).
    * Replicira sve `sysctrl.cgi` GET komande.
    * Implementira `POST` endpoint za upload fajlova na `SpiFlashStorage`.
    * Poziva druge menadžere za izvršavanje komandi.

* **`HttpQueryManager.h / .cpp`**
    * Replicira blokirajuću `HTTP2RS485` logiku.
    * Koristi FreeRTOS semafore da blokira `HttpServer` zadatak dok čeka odgovor od `Rs485Service`.

* **`LogPullManager.h / .cpp`**
    * Implementira standardni "pull" mehanizam (logika `UPD_RC_STAT` i `UPD_LOG` iz `HC_Service`).
    * Periodično poziva `HC_GetNextAddr()` i šalje upite uređajima.

* **`UpdateManager.h / .cpp`**
    * Implementira `state machine` za ažuriranje firmvera.
    * Čita fajlove sa `SpiFlashStorage` i šalje ih paket po paket preko `Rs485Service`.

* **`TimeSync.h / .cpp`**
    * Niskoprioritetni modul koji periodično šalje `SET_RTC_DATE_TIME` broadcast na RS485 magistralu.

* **`VirtualGpio.h / .cpp`**
    * Upravlja stanjem "virtuelnih" pinova (Rasvjeta, `STATUS_LED_PIN`).
    * Ovaj modul će u budućnosti biti proširen da upravlja I2C GPIO Expanderom.

## 4. Strategija Skladištenja Podataka

Umjesto `uSD` kartice i `FAT` fajl sistema, ovaj projekat koristi kombinaciju dva skladišta sa **fiksnom memorijskom mapom**:

1.  **I2C EEPROM (24C1024 - 128KB):**
    * **Blok 1 (Konfiguracija):** IP adrese, RS485 adrese, System ID...
    * **Blok 2 (Lista Adresa):** Lista od ~400 adresa za `LogPullManager`.
    * **Blok 3 (Logovi):** Cirkularni bafer (`head/tail`) za sve dolazne logove sa magistrale.
2.  **SPI Flash (W25Q512 - 64MB):**
    * Koristi se isključivo za skladištenje velikih binarnih fajlova (FW, BL, Slike, QSPI) koji se šalju na RS485 bus.
    * Svaki fajl ima definisan fiksni "slot" (adresu) unutar `ProjectConfig.h`.

## 5. Razvojni Proces (Faze)

1.  **FAZA 1 (Razvoj Jezgra):**
    * Uređaj je spojen USB kablom. `Serial0` (`GPIO1/3`) se koristi za `Serial.println()` u VS Code. DWIN Displej je **isključen**.
    * Razvijaju se i testiraju svi moduli (`EepromStorage`, `Rs485Service`, `HttpServer`...).
2.  **FAZA 2 (Integracija Grafike):**
    * Kada je Faza 1 stabilna, USB kabl se isključuje.
    * DWIN Displej se fizički spaja na `TX0`/`RX0` pinove.
    * `Serial.println()` poruke sada automatski idu na DWIN.
    * Dodaje se kod za `Serial.read()` za primanje komandi *sa* DWIN-a.

## 6. Nomenklatura Koda

Projekat koristi striktnu nomenklaturu za svu kodu:

| Kategorija | Stil (Format) | Primjer |
| :--- | :--- | :--- |
| **Fajlovi (Moduli)** | `PascalCase` | `Rs485Service.h` |
| **Tipovi (Klase, Strukture)** | `PascalCase` | `class EepromStorage` |
| **Funkcije / Metode** | `PascalCase` | `void Initialize()` |
| **Globalne Varijable** | `g_snake_case` | `g_rs485_task_handle` |
| **Članske Varijable (Klase)**| `m_snake_case` | `uint16_t m_write_index;` |
| **Lokalne Varijable** | `camelCase` | `int localCounter` |
| **Konstante / Makroi** | `ALL_CAPS_SNAKE_CASE` | `RS485_DE_PIN` |
