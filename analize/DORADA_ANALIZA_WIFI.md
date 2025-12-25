# Analiza i Plan Dorade: WiFi AP Configuration Mode

Ovaj dokument opisuje plan izmjena softvera za podršku "Configuration Mode-a" putem WiFi Access Point-a, aktiviranog hardverskim pinom prilikom boota, na osnovu zahtjeva iz `dorada2.txt`.

## 1. Zahtjevi

1.  **Boot Detekcija:**
    *   Prilikom startupa (boot), provjeriti stanje digitalnog ulaza (PIN 36 ili 39).
    *   Ako je pin na GND (LOW), pokrenuti **WiFi AP (Access Point) Mode**.
    *   U suprotnom (HIGH/Floating), nastaviti sa normalnim radom (Ethernet/WiFi STA).
    *   *Napomena:* Pinovi 36 i 39 na ESP32 su **Input Only** (nemaju interni pull-up/pull-down). Potreban je eksterni pull-up otpornik na PCB-u.

2.  **WiFi AP Funkcionalnost:**
    *   Kada je u AP modu, uređaj treba kreirati WiFi mrežu (npr. "HotelController_Config").
    *   IP adresa AP-a treba biti fiksna (npr. 192.168.4.1).

3.  **Web Interfejs:**
    *   Web server (`HttpServer`) mora biti dostupan preko WiFi AP-a.
    *   Mora prikazivati **identičan interfejs** kao i preko Etherneta (index.html).
    *   Korisnik mora moći mijenjati konfiguraciju (IP adresa, Gateway, RS485 postavke, itd.).
    *   Nakon snimanja konfiguracije, uređaj se restartuje.

## 2. Arhitektura i Izmjene po Modulima

### 2.1. `ProjectConfig.h` - Pin Definicija

*   Definisati pin za ulaz u konfiguracijski mod.
    ```cpp
    #define CONFIG_MODE_PIN     39  // (P0 Pin XX - Input Only)
    // Napomena: Zahtijeva eksterni pull-up otpornik!
    ```

### 2.2. `NetworkManager` - AP Mode Podrška

*   Dodati metodu `StartAPMode()`.
    *   Ova metoda će konfigurisati `WiFi.softAP(...)`.
    *   Parametri: SSID ("HotelController_Config"), Password (opciono, ili prazno).
*   Dodati flag `m_is_ap_mode`.
*   Izmijeniti `RunTask` (ili `Initialize`):
    *   Logika za detekciju moda rada treba biti proslijeđena iz `main.cpp` ili detektovana unutar `Initialize`.
    *   Ako je AP mod aktivan, **preskočiti** inicijalizaciju Etherneta i WiFi STA klijenta. Samo podići AP.

### 2.3. `main.cpp` - Boot Sekvenca

*   U `setup()` funkciji:
    *   Konfigurisati `CONFIG_MODE_PIN` kao `INPUT`.
    *   Pročitati stanje pina: `bool configMode = (digitalRead(CONFIG_MODE_PIN) == LOW);`
    *   Inicijalizirati `g_networkManager` sa informacijom o modu rada.
        *   Opcija A: Dodati metodu `SetConfigMode(bool)` u `NetworkManager` prije poziva `StartTask`.
        *   Opcija B: Proslijediti parametar u `Initialize`.

### 2.4. `HttpServer` - Dostupnost

*   `HttpServer` već koristi `ESPAsyncWebServer`. On se automatski veže na sve dostupne mrežne interfejse (ETH, WiFi STA, WiFi AP).
*   **Nema potrebe za izmjenama u `HttpServer` kodu.** On će automatski raditi i na 192.168.4.1.

## 3. Detaljan Plan Implementacije

1.  **Ažuriranje `ProjectConfig.h`**:
    *   Dodati `#define CONFIG_MODE_PIN 39`.

2.  **Ažuriranje `NetworkManager.h` i `.cpp`**:
    *   Dodati `StartAPMode()`.
    *   Dodati metodu `EnableConfigMode()` koju će `main.cpp` pozvati.
    *   U `RunTask()` dodati grananje:
        ```cpp
        if (m_config_mode) {
             StartAPMode();
        } else {
             // Postojeća logika (ETH -> WiFi STA)
             InitializeETH();
             ...
        }
        ```

3.  **Ažuriranje `main.cpp`**:
    *   Učitati stanje pina na početku `setup()`.
    *   Logirati detektovano stanje.
    *   Pozvati `g_networkManager.EnableConfigMode()` ako je pin LOW.

## 4. Napomene i Rizici

*   **Input Only Pinovi:** Pinovi 36 i 39 (SENSOR_VP/VN) nemaju interne pull-up otpornike. Ako na PCB-u nema pull-up otpornika, pin će "plutati" i može lažno aktivirati konfiguracijski mod.
    *   *Rješenje:* Korisnik mora osigurati pull-up na PCB-u. Ako to nije moguće, moramo koristiti drugi pin koji podržava `INPUT_PULLUP` (npr. neki od neiskorištenih GPIO, ako ih ima). Prema `ProjectConfig.h`, većina pinova je zauzeta.
    *   Alternativa: Ako nema pull-up-a, softver ne može pouzdano detektovati HIGH stanje.
*   **Sigurnost:** AP je otvoren. Svako se može spojiti. S obzirom da je ovo "maintenance mode" koji se fizički aktivira (kratkospojnik/taster pri boot-u), ovo je prihvatljiv rizik. Možemo dodati WPA2 password ako je potrebno (npr. "admin123").

