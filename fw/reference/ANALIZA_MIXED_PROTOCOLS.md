# Analiza Izvodljivosti: Mješoviti Protokoli na Dual RS485 Bus-u

Ovaj dokument analizira mogućnost nadogradnje sistema tako da **Lijevi RS485 Bus** koristi jedan komunikacijski protokol (npr. HILLS), dok **Desni RS485 Bus** koristi drugi protokol (npr. SAX/STANDARD), umjesto trenutnog rješenja gdje je protokol globalan za cijeli sistem.

## 1. Trenutno Stanje

U trenutnoj implementaciji (`ProjectConfig.h`, `LogPullManager.cpp`, `EepromStorage.h`):
*   Postoji jedna globalna varijabla: `g_appConfig.protocol_version`.
*   Sve odluke o formatu paketa (npr. da li je komanda `0xBA` ili `0xA0`) donose se na osnovu ove jedne varijable.
*   Funkcija `IsHillsProtocol()` u `LogPullManager` samo provjerava tu globalnu varijablu.

## 2. Analiza Izvodljivosti

**ZAKLJUČAK: Implementacija JE MOGUĆA.**

S obzirom da smo već planirali razdvajanje na dvije liste adresa (`CTRL_ADD_L` i `CTRL_ADD_R`) i kontrolu dva DE pina, softver već mora znati na kojem se bus-u trenutno odvija komunikacija. Tu informaciju možemo iskoristiti za odabir protokola.

### Preduslovi i Ograničenja

1.  **Fizički Sloj (Baudrate):**
    *   **Ograničenje:** Oba protokola moraju raditi na **istoj brzini** (trenutno 115200) i istim serijskim postavkama (8N1).
    *   *Razlog:* Prebacivanje brzine `Serial2` interfejsa u "letu" (između dva paketa) je rizično, sporo i može dovesti do gubitka sinhronizacije ili podataka u bufferu.
    *   *Status:* Većina podržanih protokola (Hills, Sax, Bjelasnica...) koriste 115200, tako da ovo vjerovatno nije problem.

2.  **Struktura Paketa:**
    *   Većina protokola u sistemu dijeli istu osnovnu strukturu (SOH, ADDR, ... CHECK, EOT). Razlike su u *Payloadu* (komandni bajtovi).
    *   Ako bi jedan protokol bio ASCII (npr. Modbus ASCII), a drugi Binarni, to bi zahtijevalo izmjene u `Rs485Service::ValidatePacket`. Za sada pretpostavljamo da su svi podržani protokoli varijacije osnovnog "Hotel Controller" protokola.

## 3. Potrebne Izmjene u Kodu

### 3.1. `EepromStorage` i `ProjectConfig` (Konfiguracija)

Potrebno je proširiti `AppConfig` strukturu.
*   **Trenutno:** `uint8_t protocol_version;`
*   **Novo:**
    *   `uint8_t protocol_version_L;` (Za uređaje iz `CTRL_ADD_L`)
    *   `uint8_t protocol_version_R;` (Za uređaje iz `CTRL_ADD_R`)
*   *Migracija:* Prilikom update-a, stara vrijednost `protocol_version` se kopira u obje nove varijable kako bi se zadržala kompatibilnost.

### 3.2. `LogPullManager` (Polling)

Ovo je najjednostavniji dio za izmjenu jer `LogPullManager` tačno zna koju listu trenutno skenira.

*   Dodati logiku u `Run()`:
    ```cpp
    // Pseudo-kod
    ProtocolVersion currentProto;
    if (m_current_bus == BUS_LEFT) {
        currentProto = g_appConfig.protocol_version_L;
    } else {
        currentProto = g_appConfig.protocol_version_R;
    }
    
    // Sve helper funkcije moraju primati protokol kao argument
    uint8_t cmd = GetStatusCommand(currentProto); 
    ```
*   Izmijeniti `GetStatusCommand`, `GetLogCommand`, `IsHillsProtocol` da ne gledaju globalni config, već da primaju parametar.

### 3.3. `HttpQueryManager` (Direktne Komande)

Ovo je kritični dio. Kada stigne HTTP zahtjev (npr. `sysctrl.cgi?stg=101&val=1`), sistem ne zna da li je soba 101 lijevo ili desno, pa samim tim ne zna ni koji protokol da koristi za formatiranje paketa.

*   **Rješenje:** Lookup Adrese.
    1.  Prije kreiranja paketa, pozvati `LogPullManager::GetBusForAddress(101)`.
    2.  Ako vrati `BUS_LEFT` -> Koristi `protocol_version_L`.
    3.  Ako vrati `BUS_RIGHT` -> Koristi `protocol_version_R`.
    4.  Ako vrati `UNKNOWN` (adresa nije u listama):
        *   *Fallback Strategija:* Pokušaj slanje na **Oba Bus-a**? Ili koristi "Default" protokol definisan u postavkama?
        *   *Preporuka:* Koristiti protokol definisan za **Lijevi** bus kao default za nepoznate adrese, pa ako ne uspije, probati Desni (ako su protokoli isti). Ako su različiti, ovo je problematično. Najbolje je zahtijevati da su sve adrese u listama.

### 3.4. Web Interfejs (`index.html` / `HttpServer`)

*   Dodati UI elemente za odabir dva protokola umjesto jednog.
*   Endpoint `/sysctrl.cgi?proto=X` treba zamijeniti sa `?protoL=X&protoR=Y`.

## 4. UI/UX Strategija (Web Interfejs)

Kako bi se pojednostavila implementacija na backend-u (firmware) i zadržala intuitivnost za korisnika, primijenit ćemo **"UI Convenience"** pristup.

### 4.1. Koncept

Firmware će interno **UVIJEK** koristiti dvije varijable: `protocol_L` i `protocol_R`. Neće postojati varijabla `main_protocol` u EEPROM-u niti logika "use_single_protocol" u C++ kodu. Sva logika "jednog protokola" se prebacuje na Frontend (JavaScript).

### 4.2. Ponašanje Web Interfejsa

Na stranici `index.html` (ili `sysctrl.html`) kontrole će se ponašati ovako:

1.  **Elementi:**
    *   `[Checkbox]` **"Koristi različite protokole (L/R)"**
    *   `[Dropdown]` **"Glavni Protokol"** (ID: `main_proto`)
    *   `[Dropdown]` **"Lijevi Bus Protokol"** (ID: `proto_l`)
    *   `[Dropdown]` **"Desni Bus Protokol"** (ID: `proto_r`)

2.  **Logika (JavaScript):**
    *   **Inicijalizacija (Load):**
        *   Ako su vrijednosti `proto_L` i `proto_R` primljene od uređaja **ISTE**:
            *   Checkbox -> **UNCHECKED**.
            *   `main_proto` -> Postavljen na tu vrijednost i **ENABLED**.
            *   `proto_l`, `proto_r` -> Postavljeni na istu vrijednost, ali **DISABLED** (sivi).
        *   Ako su vrijednosti **RAZLIČITE**:
            *   Checkbox -> **CHECKED**.
            *   `main_proto` -> **DISABLED**.
            *   `proto_l`, `proto_r` -> Postavljeni na svoje vrijednosti i **ENABLED**.

    *   **Interakcija (Korisnik mijenja):**
        *   Kada je Checkbox **ISKLJUČEN**:
            *   Promjena na `main_proto` automatski mijenja `proto_l` i `proto_r` u pozadini.
            *   Korisnik vidi samo promjenu "Glavnog".
        *   Kada se Checkbox **UKLJUČI**:
            *   `main_proto` postaje siv.
            *   `proto_l` i `proto_r` postaju aktivni.

    *   **Slanje (Save):**
        *   Web uvijek šalje dvije vrijednosti: `protocol_L` i `protocol_R`.
        *   Firmware ih samo snimi. Ne zanima ga da li su iste ili različite.

### 4.3. Prednosti

*   **Pojednostavljen Firmware:** Kod mikrokontrolera je čist i uniforman. Uvijek radi sa `protocol_L` i `protocol_R`.
*   **Jednostavnost za Korisnika:** Korisnici koji ne trebaju miješane protokole (većina) vide samo jednu kontrolu.
*   **Bez redundantnih podataka:** Ne čuvamo nepotreban `bool use_single` flag u EEPROM-u.

## 5. Zaključak i Preporuka

Implementacija je tehnički potpuno izvodljiva i logičan je korak nakon razdvajanja na dva bus-a.

**Preporučeni redoslijed:**
1.  Prvo implementirati **Dual Bus (Physical Layer)** podršku (dvije liste, DE kontrola) sa jedinstvenim protokolom.
2.  Testirati stabilnost prebacivanja bus-ova.
3.  Tek onda uvesti **Mixed Protocol** podršku refaktorisanjem `AppConfig` i menadžera, jer to dodaje sloj kompleksnosti u logiku parsiranja komandi.

Ako se odmah krene na Mixed Protocol, rizikujemo "težak" debugging gdje nećemo znati da li je problem u tajmingu prebacivanja bus-a (hardver) ili u formatiranju paketa (softver).
