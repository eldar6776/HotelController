# Analiza: Protokol po Uređaju (Bit-Packed Address)

Ovaj dokument analizira optimizirano rješenje za definisanje protokola po uređaju koristeći gornja 2 bita adrese za kodiranje verzije protokola.

## 1. Koncept Optimizacije

Umjesto proširenja strukture u EEPROM-u (što bi zahtijevalo pomjeranje memorijske mape i brisanje logova), koristimo postojeći `uint16_t` (16-bit) format za adrese.

*   **Adresni prostor:** 16 bita omogućava 65536 adresa.
*   **Realna potreba:** Naši sistemi rijetko prelaze 1000 uređaja, a maksimalna adresa u RS485 mreži je obično manja od 65535.
*   **Rješenje:** Rezervišemo **gornja 2 bita** (Bit 15 i Bit 14) za identifikaciju protokola.
    *   Preostalih 14 bita omogućava adrese do 16383 (0x3FFF), što je više nego dovoljno za sve postojeće instalacije.

### 1.1. Kodiranje Protokola (2 Bita)

| Binarno (Bits 15-14) | Protokol ID | Prefiks u Fajlu | Opis |
| :--- | :--- | :--- | :--- |
| `00` | 0 | `H` (ili bez) | **HILLS** (Default) |
| `01` | 1 | `S` | **SAX** / Standard |
| `10` | 2 | `V` | **VUČKO** |
| `11` | 3 | - | *Reserved / Future Use* |

## 2. Implementacija

### 2.1. Novi Fajl Liste: `CTRL_ADD_P.TXT`

Uvodimo poseban fajl za ovaj način rada kako bismo zadržali kompatibilnost.
*   **Format:** CSV lista sa prefiksima.
*   **Primjer sadržaja:**
    ```text
    101,      # HILLS (00) -> 0x0065 (101)
    S102,     # SAX (01)   -> 0x4066 (16384 + 102)
    V103;     # VUCKO (10) -> 0x8067 (32768 + 103)
    ```

### 2.2. Helper Funkcije (`AddressCodec`)

Potrebno je implementirati statičke helper funkcije za kodiranje i dekodiranje.

```cpp
// Definicije maski
#define ADDR_MASK_PROTOCOL  0xC000 // 1100 0000 0000 0000
#define ADDR_MASK_ADDRESS   0x3FFF // 0011 1111 1111 1111

class AddressCodec {
public:
    static uint16_t Encode(uint16_t rawAddress, ProtocolVersion proto) {
        uint16_t protoBits = 0;
        switch (proto) {
            case ProtocolVersion::SAX:   protoBits = 0x4000; break; // 01
            case ProtocolVersion::VUCKO: protoBits = 0x8000; break; // 10
            default:                     protoBits = 0x0000; break; // 00 (HILLS)
        }
        // Sigurnosna provjera: rawAddress mora biti < 16384
        return (rawAddress & ADDR_MASK_ADDRESS) | protoBits;
    }

    static uint16_t DecodeAddress(uint16_t encodedAddress) {
        return encodedAddress & ADDR_MASK_ADDRESS;
    }

    static ProtocolVersion DecodeProtocol(uint16_t encodedAddress) {
        uint16_t bits = (encodedAddress & ADDR_MASK_PROTOCOL) >> 14;
        switch (bits) {
            case 1: return ProtocolVersion::SAX;
            case 2: return ProtocolVersion::VUCKO;
            default: return ProtocolVersion::HILLS;
        }
    }
};
```

### 2.3. Izmjene u `HttpServer` i `EepromStorage`

1.  **Web Interfejs:**
    *   Dodati checkbox **"Koristi Protokol po Uređaju"**.
    *   Ovaj checkbox treba biti omogućen (enabled) **SAMO AKO** postoji fajl `CTRL_ADD_P.TXT` na SD kartici (provjera pri učitavanju stranice).
    *   Kada se klikne "Load List" (`cad=load`), ako je ovaj mod aktivan, čita se `CTRL_ADD_P.TXT`.

2.  **Parser (`ParseAddressListFromCSV`):**
    *   Mora detektovati prefikse `S` i `V`.
    *   Kada nađe prefiks, koristi `AddressCodec::Encode` da spakuje protokol u `uint16_t` koji se snima u EEPROM.
    *   Rezultat je standardni niz `uint16_t`, tako da struktura EEPROM-a ostaje **ISTI**. Nema brisanja logova!

### 2.4. Izmjene u `LogPullManager`

*   Prilikom iteracije kroz `m_address_list`:
    *   `uint16_t rawAddr = m_address_list[i];`
    *   `uint16_t realAddr = AddressCodec::DecodeAddress(rawAddr);`
    *   `ProtocolVersion proto = AddressCodec::DecodeProtocol(rawAddr);`
    *   Poziva funkcije za slanje paketa koristeći `realAddr` i `proto`.

## 3. Zaključak

Ovo rješenje je izuzetno elegantno i efikasno:
*   **Nema promjene memorijske mape:** Logovi su sigurni.
*   **Minimalne izmjene koda:** Samo logika pakovanja/otpakivanja.
*   **Backward Compatibility:** Stari fajlovi `CTRL_ADD.TXT` nemaju postavljene gornje bite, što se dekodira kao `00` (HILLS), što je i trenutni default.

**Ograničenja:**
*   Maksimalna adresa uređaja je 16383 (što je u praksi dovoljno).
*   Podržava samo 4 varijante protokola (trenutno imamo 3: Hills, Sax, Vučko).
