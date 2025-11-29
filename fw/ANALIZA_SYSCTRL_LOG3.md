# DETALJNA ANALIZA: `/sysctrl.cgi?log=3` KOMANDA

## 📋 SADRŽAJ
1. [STM32 Implementacija (Original)](#stm32-implementacija)
2. [ESP32 Implementacija (Nova)](#esp32-implementacija)
3. [Razlike i Problemi](#razlike-i-problemi)
4. [Dijagram Toka](#dijagram-toka)
5. [Rješenje](#rješenje)

---

## 🔧 STM32 IMPLEMENTACIJA (Original)

### Fajlovi:
- `reference/httpd_cgi_ssi.c` - HTTP CGI handler
- `reference/hotel_ctrl.c` - Glavna logika
- `reference/common.h` - Definicije

### 1. HTTP Request Handler

**Lokacija**: `httpd_cgi_ssi.c` linija ~319

```c
/* HC log list request: log; 3, 4, 5 */   
else if (!strcmp(pcParam[0], "log") || !strcmp(pcParam[0], "RQlog"))
{
    if (!strcmp(pcValue[0], "3") || !strcmp(pcValue[0], "RDlog"))
    {
        HTTP_LogTransfer.state = HTTP_GET_LOG_LIST;
        request = GET_LOG_LIST;
        HC_ReadLogListBlock();  // ← KLJUČNI POZIV
    }
```

**Tok**:
1. HTTP server primi `/sysctrl.cgi?log=3`
2. Parsira parametar `log` sa vrednošću `3`
3. Postavlja globalni flag `HTTP_LogTransfer.state = HTTP_GET_LOG_LIST`
4. Poziva `HC_ReadLogListBlock()` koja čita EEPROM
5. Vraća se na `HTTP_ResponseHandler()` koji formira odgovor

---

### 2. Čitanje Log Bloka

**Lokacija**: `hotel_ctrl.c` linija 1335

```c
void HC_ReadLogListBlock(void)
{
    uint32_t read_cnt;
    
    switch (HC_LogMemory.Allocation)  // ← 6 tipova alokacije
    {
       case TYPE_2:
            read_cnt = HC_LogMemory.first_addr;                
            while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
            if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
            else read_cnt = I2CEE_BLOCK;
            
            if (HC_LogMemory.first_addr < I2CEE_PAGE_SIZE)
                I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, read_cnt);
            else
                I2CEE_ReadBytes16(I2CEE_PAGE_1, HC_LogMemory.first_addr, eebuff, read_cnt);
            
            Hex2Str((char*)hc_buff, eebuff, (read_cnt * 2U));  // ← KONVERZIJA U HEX
            break;
        
        case TYPE_3:
            // ... identična logika
            break;
        
        case TYPE_4:
            I2CEE_ReadBytes16(I2CEE_PAGE_0, HC_LogMemory.first_addr, eebuff, I2CEE_BLOCK);
            Hex2Str((char*)hc_buff, eebuff, (I2CEE_BLOCK * 2U));
            HTTP_LogTransfer.last_addr = EE_LOG_LIST_START_ADD + I2CEE_BLOCK;
            break;
        
        case TYPE_5:
        case TYPE_6:
            // ... identična logika
            break;
        
        case TYPE_1:
        default:
            break;
    }
    
    if (HTTP_LogTransfer.state == HTTP_GET_LOG_LIST)
        HTTP_LogTransfer.state = HTTP_LOG_READY;  // ← POSTAVLJA STATUS
}
```

**Ključne Karakteristike**:
- Čita **fiksnih 128 bajtova** (`I2CEE_BLOCK`)
- Poravnava čitanje na 128-bajtne blokove
- Konvertuje u HEX string (256 karaktera)
- Koristi **globalni buffer** `hc_buff`
- Postavlja status `HTTP_LOG_READY` nakon čitanja

---

### 3. HTTP Response Formiranje

**Lokacija**: `httpd_cgi_ssi.c` linija ~90

```c
u16_t HTTP_ResponseHandler(int iIndex, char *pcInsert, int iInsertLen)
{
    if (iIndex == 0)
    {
		if (HTTP_LogTransfer.state == HTTP_LOG_READY)  // ← PROVJERAVA STATUS
		{
            request = 0U;
            if (!strlen(hc_buff)) 
                strcpy(pcInsert, "EMPTY");  // ← Ako nema logova
            else 
                strcpy(pcInsert, hc_buff);   // ← KOPIRA HEX STRING
            
            HTTP_LogTransfer.state = HTTP_LOG_TRANSFER_IDLE;
		}
        // ... ostali slučajevi
    }
    led_clr(0);
    return iInsertLen;
}
```

**Tok**:
1. `HTTP_ResponseHandler()` se poziva nakon `HC_ReadLogListBlock()`
2. Provjerava status `HTTP_LOG_READY`
3. Ako je buffer prazan → vraća `"EMPTY"`
4. Ako ima podataka → vraća HEX string iz `hc_buff`

---

### 4. HTML Template (SSI)

**Lokacija**: `reference/log.html`

```html
<!DOCTYPE html>
<html>
<head><title>Hotel Controller Logs</title></head>
<body>
<!--#t-->  <!-- ← SSI tag koji se zamjenjuje sa HEX stringom -->
</body>
</html>
```

**Mehanizam**:
- LwIP web server pronalazi `<!--#t-->` tag
- Poziva `HTTP_ResponseHandler()` sa `iIndex=0`
- Zamjenjuje tag sa sadržajem iz `pcInsert`

---

## 🆕 ESP32 IMPLEMENTACIJA (Nova)

### Fajlovi:
- `src/HttpServer.cpp` - HTTP server
- `src/EepromStorage.cpp` - EEPROM logika

### 1. HTTP Request Handler

**Lokacija**: `HttpServer.cpp` linija 375

```cpp
// --- HC log list request: log ---
if (request->hasParam("log") || request->hasParam("RQlog"))
{
    String log_op = request->getParam(request->hasParam("log") ? "log" : "RQlog")->value();

    if (log_op == "3" || log_op.equalsIgnoreCase("RDlog"))
    {
        String hex_log_block = m_eeprom_storage->ReadLogBlockAsHexString();  // ← DIREKTAN POZIV
        if (hex_log_block != HTTP_RESPONSE_ERROR)
        {
            SendSSIResponse(request, hex_log_block);  // ← DIREKTAN ODGOVOR
        }
        else
        {
            SendSSIResponse(request, HTTP_RESPONSE_ERROR);
        }
        return;
    }
```

**Razlike**:
- ✅ Koristi **String** umjesto `char*` buffera
- ✅ **Sinhrono** čita i šalje (bez global state-a)
- ✅ **AsyncWebServer** direktno šalje odgovor

---

### 2. Čitanje Log Bloka

**Lokacija**: `EepromStorage.cpp` linija 380

```cpp
String EepromStorage::ReadLogBlockAsHexString()
{
    LOG_DEBUG(4, "[Eeprom] Čitanje bloka logova kao HEX string (V1 kompatibilnost)...\n");
    
    if (m_log_count == 0)
    {
        return HTTP_RESPONSE_EMPTY;  // ← "EMPTY"
    }

    // Čitamo fiksni blok od 128 bajtova
    uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
    uint16_t bytes_to_read = 128;  // ← FIKSNA VELIČINA

    // Osiguraj da ne čitamo preko kraja
    uint32_t end_of_log_area = EEPROM_LOG_START_ADDR + EEPROM_LOG_AREA_SIZE;
    if (read_addr + bytes_to_read > end_of_log_area) {
        bytes_to_read = end_of_log_area - read_addr;
    }

    if (bytes_to_read == 0) {
        return HTTP_RESPONSE_EMPTY;
    }

    uint8_t read_buffer[bytes_to_read];
    if (!ReadBytes(read_addr, read_buffer, bytes_to_read))
    {
        LOG_DEBUG(1, "[Eeprom] GRESKA: Čitanje bloka logova nije uspjelo.\n");
        return HTTP_RESPONSE_ERROR;
    }

    // Replikacija `Hex2Str` funkcije
    String hex_string = "";
    hex_string.reserve(bytes_to_read * 2);  // Pre-alokacija
    
    for (uint16_t i = 0; i < bytes_to_read; i++)
    {
        char hex_buf[3];
        sprintf(hex_buf, "%02X", read_buffer[i]);  // ← UPPERCASE HEX
        hex_string += hex_buf;
    }

    LOG_DEBUG(3, "[Eeprom] Vraćen HEX string dužine %d.\n", hex_string.length());
    return hex_string;
}
```

**Implementacija**:
- ✅ Čita **128 bajtova**
- ✅ Konvertuje u **uppercase HEX** (`%02X`)
- ✅ Vraća String direktno
- ⚠️ **PROBLEM**: Ne koristi `HC_LogMemory.Allocation` logiku!

---

## ⚠️ RAZLIKE I PROBLEMI

### 1. **KRITIČNA RAZLIKA: Log Memory Allocation**

#### STM32 (Original):
```c
switch (HC_LogMemory.Allocation)  // 6 tipova alokacije
{
   case TYPE_2:  // Log kreće od sredine, ide do kraja
   case TYPE_3:  // Log kreće od sredine, završava prije kraja
   case TYPE_4:  // Log pun, od početka do kraja
   case TYPE_5:  // Log kreće od početka, završava prije kraja
   case TYPE_6:  // Log wrapped-around, od sredine preko kraja do početka
   case TYPE_1:  // Prazan log
}
```

**Blok Alignment**:
```c
read_cnt = HC_LogMemory.first_addr;                
while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;                
if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt);
else read_cnt = I2CEE_BLOCK;
```

**Svrha**: Poravnava čitanje na 128-bajtnu granicu. Npr:
- Ako je `first_addr = 100`, čita do 128 (28 bajtova)
- Ako je `first_addr = 128`, čita 128 bajtova

---

#### ESP32 (Nova):
```cpp
uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
uint16_t bytes_to_read = 128;  // Fiksno 128 bajtova
```

**Problem**:
- ❌ Ignoriše circular buffer logiku
- ❌ Uvijek čita od `m_log_read_index * LOG_RECORD_SIZE`
- ❌ Ne poravnava na blokove

---

### 2. **RAZLIKA: Buffer Management**

| Aspekt | STM32 | ESP32 |
|--------|-------|-------|
| Buffer tip | `char hc_buff[HC_BSIZE]` (globalni) | `String hex_string` (lokalni) |
| Alokacija | Statička | Dinamička (heap) |
| Lifetime | Traje između poziva | Oslobađa se nakon vraćanja |
| Veličina | Fiksna | Dinamička (reserve) |

---

### 3. **RAZLIKA: State Management**

#### STM32:
```c
// GLOBALNI STATE
HTTP_LogTransfer.state = HTTP_GET_LOG_LIST;
request = GET_LOG_LIST;

HC_ReadLogListBlock();  // Postavlja state na HTTP_LOG_READY

// HTTP_ResponseHandler provjerava state
if (HTTP_LogTransfer.state == HTTP_LOG_READY)
```

#### ESP32:
```cpp
// DIREKTAN POZIV I ODGOVOR
String hex_log_block = m_eeprom_storage->ReadLogBlockAsHexString();
if (hex_log_block != HTTP_RESPONSE_ERROR)
{
    SendSSIResponse(request, hex_log_block);
}
```

**Razlika**: ESP32 je **stateless** (bolje za multi-threaded okruženje).

---

### 4. **RAZLIKA: Response Formatting**

#### STM32:
```c
if (!strlen(hc_buff)) strcpy(pcInsert, "EMPTY");
else strcpy(pcInsert, hc_buff);
```

Vraća direktno u SSI template (`<!--#t-->`).

#### ESP32:
```cpp
void SendSSIResponse(AsyncWebServerRequest *request, const String &content)
{
    String html = "<!DOCTYPE html><html><head><title>HC Response</title></head><body>";
    html += content;
    html += "</body></html>";
    request->send(200, "text/html", html);
}
```

Wrauje u HTML strukturu.

---

## 📊 DIJAGRAM TOKA

### STM32 Original:

```
┌─────────────────────────────────────┐
│   HTTP Request: /sysctrl.cgi?log=3 │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  HTTP_CGI_Handler()                 │
│  - Parsira parametar "log"="3"      │
│  - Postavlja state flag             │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  HC_ReadLogListBlock()              │
│  - Provjerava HC_LogMemory.         │
│    Allocation tip (TYPE_1..TYPE_6)  │
│  - Poravnava na 128-bajtnu granicu  │
│  - Čita I2CEE_ReadBytes16()         │
│  - Hex2Str() konverzija             │
│  - Upisuje u GLOBALNI hc_buff       │
│  - state = HTTP_LOG_READY           │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  HTTP_ResponseHandler()             │
│  - Provjerava state == HTTP_LOG_    │
│    READY                            │
│  - Kopira hc_buff → pcInsert        │
│  - state = HTTP_LOG_TRANSFER_IDLE   │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  LwIP SSI Engine                    │
│  - Zamjenjuje <!--#t--> sa pcInsert │
│  - Šalje HTTP response              │
└─────────────────────────────────────┘
```

---

### ESP32 Nova:

```
┌─────────────────────────────────────┐
│   HTTP Request: /sysctrl.cgi?log=3 │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  HandleSysCtrl()                    │
│  - Parsira parametar "log"="3"      │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  ReadLogBlockAsHexString()          │
│  - Čita direktno od m_log_read_     │
│    index * LOG_RECORD_SIZE          │
│  - Čita 128 bajtova                 │
│  - sprintf("%02X") konverzija       │
│  - Vraća String                     │
└──────────────┬──────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│  SendSSIResponse()                  │
│  - Wrapa u HTML strukturu           │
│  - request->send(200, html)         │
└─────────────────────────────────────┘
```

---

## 🔍 ŠTA NE RADI 1:1?

### 1. **Circular Buffer Logika**

**STM32** koristi složenu logiku za circular buffer sa 6 tipova alokacije:

```
TYPE_1: Prazan log
0000000000000000000000000000000000000000000000000000000000000000

TYPE_2: Počinje od sredine, ide do kraja, wrap-around
000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
                  ↑ first_addr                                     ↑ end

TYPE_3: Počinje od sredine, završava prije kraja
000000000000000000xxxxxxxxxxxxxxxxxxxxxxxxxxxx000000000000000000000000000
                  ↑ first                   ↑ last

TYPE_4: Potpuno pun
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
↑ first                                                              ↑ last

TYPE_5: Počinje od početka, završava prije kraja
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx00000000000000000000000000000000000000000
↑ first                        ↑ last

TYPE_6: Wrap-around: počinje od sredine, prelazi kraj, završava na početku
xxxxxxxxxxxx0000000000000000000000000000000000000000000000xxxxxxxxxxxxxxx
            ↑ free space          first →                 ← last
```

**ESP32** ignoriše ovo potpuno!

---

### 2. **Block Alignment**

**STM32** poravnava čitanje:
```c
// Primjer: first_addr = 100
read_cnt = HC_LogMemory.first_addr;                // 100
while (read_cnt >= I2CEE_BLOCK) read_cnt -= I2CEE_BLOCK;  // 100 - 0 = 100
if (read_cnt != 0U) read_cnt = (I2CEE_BLOCK - read_cnt); // 128 - 100 = 28
else read_cnt = I2CEE_BLOCK;
// Čita 28 bajtova (od 100 do 128)
```

**ESP32** ne poravnava:
```cpp
uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
// Uvijek čita od iste adrese
```

---

### 3. **Multi-Block Transfer**

**STM32** vraća **PRVI blok** od 128 bajtova. Za naredne blokove, klijent opet poziva `log=3`, a `HC_LogMemory.first_addr` se **pomjera naprijed**.

```c
// U DeleteOldestLog():
HC_LogMemory.first_addr += delete_cnt;  // Pomjera pokazivač
```

**ESP32** koristi `m_log_read_index` ali ga **NE POMJERA** nakon čitanja!

```cpp
// U ReadLogBlockAsHexString():
uint16_t read_addr = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
// m_log_read_index se NE MIJENJA!
```

---

## ✅ RJEŠENJE

### Faza 1: Implementiraj Log Memory Alokaciju

**U `EepromStorage.h`**:
```cpp
// Dodaj strukturu iz original koda
enum LogAllocationType {
    LOG_TYPE_1 = 1,  // Prazan
    LOG_TYPE_2 = 2,  // Wrap from middle to end
    LOG_TYPE_3 = 3,  // Middle to middle (no wrap)
    LOG_TYPE_4 = 4,  // Full
    LOG_TYPE_5 = 5,  // Start to middle
    LOG_TYPE_6 = 6   // Wrap from upper to lower
};

struct LogMemoryInfo {
    uint16_t first_addr;         // Adresa najstarijeg loga
    uint16_t last_addr;          // Adresa najnovijeg loga
    uint16_t next_addr;          // Adresa za sledeći log
    uint16_t log_cnt;            // Broj logova
    LogAllocationType allocation; // Tip alokacije
};

private:
    LogMemoryInfo m_log_memory;
    uint16_t m_http_transfer_addr;  // Trenutna adresa za HTTP transfer
```

---

### Faza 2: Dodaj Funkciju za Određivanje Tipa

**U `EepromStorage.cpp`**:
```cpp
void EepromStorage::UpdateLogMemoryAllocation()
{
    if (m_log_count == 0) {
        m_log_memory.allocation = LOG_TYPE_1;
        m_log_memory.first_addr = EEPROM_LOG_START_ADDR;
        m_log_memory.last_addr = EEPROM_LOG_START_ADDR;
        m_log_memory.next_addr = EEPROM_LOG_START_ADDR;
        return;
    }

    uint16_t first = EEPROM_LOG_START_ADDR + (m_log_read_index * LOG_RECORD_SIZE);
    uint16_t last = EEPROM_LOG_START_ADDR + (m_log_write_index * LOG_RECORD_SIZE);
    uint16_t next = last + LOG_RECORD_SIZE;
    
    if (next >= EEPROM_LOG_START_ADDR + EEPROM_LOG_AREA_SIZE) {
        next = EEPROM_LOG_START_ADDR;
    }

    m_log_memory.first_addr = first;
    m_log_memory.last_addr = last;
    m_log_memory.next_addr = next;
    m_log_memory.log_cnt = m_log_count;

    // Odredi tip alokacije
    if (m_log_count == 0) {
        m_log_memory.allocation = LOG_TYPE_1;
    }
    else if (m_log_count == MAX_LOG_ENTRIES) {
        m_log_memory.allocation = LOG_TYPE_4;  // Full
    }
    else if (first == EEPROM_LOG_START_ADDR) {
        m_log_memory.allocation = LOG_TYPE_5;  // Start to middle
    }
    else if (last < first) {
        m_log_memory.allocation = LOG_TYPE_6;  // Wrap-around
    }
    else if (last >= EEPROM_LOG_START_ADDR + EEPROM_LOG_AREA_SIZE - LOG_RECORD_SIZE) {
        m_log_memory.allocation = LOG_TYPE_2;  // Middle to end
    }
    else {
        m_log_memory.allocation = LOG_TYPE_3;  // Middle to middle
    }
}
```

---

### Faza 3: Reimplementiraj ReadLogBlockAsHexString()

```cpp
String EepromStorage::ReadLogBlockAsHexString()
{
    UpdateLogMemoryAllocation();  // Ažuriraj tip alokacije

    if (m_log_memory.log_cnt == 0) {
        return HTTP_RESPONSE_EMPTY;
    }

    uint16_t read_cnt = 0;
    uint16_t read_addr = m_http_transfer_addr;  // Koristimo transfer adresu umjesto log_read_index

    // Replikacija original logike iz HC_ReadLogListBlock()
    switch (m_log_memory.allocation) {
        case LOG_TYPE_2:
        case LOG_TYPE_3:
        case LOG_TYPE_5:
        case LOG_TYPE_6:
            // Poravnaj na 128-bajtnu granicu
            read_addr = m_http_transfer_addr;
            uint16_t temp = read_addr - EEPROM_LOG_START_ADDR;
            while (temp >= 128) temp -= 128;
            
            if (temp != 0) read_cnt = 128 - temp;
            else read_cnt = 128;
            break;

        case LOG_TYPE_4:
            read_cnt = 128;
            break;

        case LOG_TYPE_1:
        default:
            return HTTP_RESPONSE_EMPTY;
    }

    // Osiguraj da ne čitamo preko kraja
    uint32_t end_of_log_area = EEPROM_LOG_START_ADDR + EEPROM_LOG_AREA_SIZE;
    if (read_addr + read_cnt > end_of_log_area) {
        read_cnt = end_of_log_area - read_addr;
    }

    uint8_t read_buffer[read_cnt];
    if (!ReadBytes(read_addr, read_buffer, read_cnt)) {
        return HTTP_RESPONSE_ERROR;
    }

    // Hex konverzija
    String hex_string = "";
    hex_string.reserve(read_cnt * 2);
    for (uint16_t i = 0; i < read_cnt; i++) {
        char hex_buf[3];
        sprintf(hex_buf, "%02X", read_buffer[i]);
        hex_string += hex_buf;
    }

    // Pomjeri transfer adresu za sledeći poziv
    m_http_transfer_addr += read_cnt;
    if (m_http_transfer_addr >= end_of_log_area) {
        m_http_transfer_addr = EEPROM_LOG_START_ADDR;
    }

    return hex_string;
}
```

---

### Faza 4: Resetuj Transfer Adresu Prilikom Novog Transfera

**U `HttpServer.cpp`**:
```cpp
if (log_op == "3" || log_op.equalsIgnoreCase("RDlog"))
{
    m_eeprom_storage->ResetLogTransferAddress();  // ← NOVO: Resetuj prije čitanja
    String hex_log_block = m_eeprom_storage->ReadLogBlockAsHexString();
```

**U `EepromStorage.cpp`**:
```cpp
void EepromStorage::ResetLogTransferAddress()
{
    UpdateLogMemoryAllocation();
    m_http_transfer_addr = m_log_memory.first_addr;
}
```

---

## 📌 ZAKLJUČAK

### Problem:
ESP32 implementacija **ne implementira circular buffer logiku** iz STM32 original koda.

### Simptomi:
- ✅ Vraća HEX string
- ✅ Format je ispravan (uppercase HEX)
- ❌ Ne poštuje log memory alokaciju
- ❌ Ne poravnava blokove na 128 bajtova
- ❌ Ne omogućava multi-block transfer

### Rješenje:
1. Dodaj `LogMemoryInfo` strukturu
2. Implementiraj `UpdateLogMemoryAllocation()` za određivanje tipa
3. Reimplementiraj `ReadLogBlockAsHexString()` sa switch/case logikom
4. Dodaj `ResetLogTransferAddress()` za početak transfera
5. Koristi `m_http_transfer_addr` umjesto `m_log_read_index`

---

**Autor**: GitHub Copilot  
**Datum**: 2025-11-28  
**Status**: ✅ KOMPLETNA ANALIZA
