# Detaljna Analiza Protokola za Update Firmvera i Bootloadera

Ova analiza poredi "Stariji Projekt" i "Noviji Projekt" na osnovu dostavljenih izvornih fajlova. Fokus je na protokolima za ažuriranje (update) firmvera, bootloadera i slika (image), uključujući tajminge, bafere i mehanizme prenosa.

## 1. Uporedna Tablica Razlika

| Karakteristika | Stariji Projekt (`hotel_ctrl_stariji`) | Noviji Projekt (`hotel_ctrl_noviji`, `common`) |
| :--- | :--- | :--- |
| **Veličina Bafera (Hotel Ctrl)** | `RUBICON_BUFFER_SIZE`: **512 bajtova** | `HC_BSIZE`: **512 bajtova** |
| **Veličina Bafera (RS485)** | `RS485_BUFF_SIZE`: Nije eksplicitno def. u .c (koristi se kao extern/global) | `RS_BSIZE`: **256 bajtova** (definisano u `common.h`) |
| **Struktura Paketa (Header)** | `SOH` (0x01), `STX` (0x02), `EOT` (0x04) | `SOH` (0x01), `STX` (0x02), `EOT` (0x04) |
| **Timeout: Odgovor (Response)** | `RUBICON_RESPONSE_TIMEOUT`: **78 ms** | `RESP_TOUT`: **45 ms** |
| **Timeout: Byte RX** | `RUBICON_BYTE_RX_TIMEOUT`: **3 ms** | `RX_TOUT`: **3 ms** |
| **Timeout: RX to TX Delay** | `RUBICON_RX_TO_TX_DELAY`: **10 ms** | `RX2TX_DEL`: **3 ms** |
| **Timeout: FW Upload Delay** | `RUBICON_FW_UPLOAD_TIMEOUT`: **2345 ms** | `FWR_UPLD_DEL`: **2345 ms** |
| **Timeout: Bootloader Start** | `RUBICON_BOOTLOADER_START_TIME`: **3456 ms** | `BLDR_START_DEL`: **3456 ms** |
| **Timeout: App Start / Restart** | `RUBICON_RESTART_TIME`: **12345 ms** | `APP_START_DEL`: **12345 ms** |
| **Timeout: Image Copy** | `RUBICON_IMAGE_COPY_TIME`: **4567 ms** | `IMG_COPY_DEL`: **4567 ms** |
| **Timeout: File Upload** | `RUBICON_FILE_UPLOAD_TIMEOUT`: **321 ms** | `BIN_TOUT`: **321 ms** |
| **Max Broj Grešaka (Retry)** | `RUBICON_MAX_ERRORS`: **10** | `MAXREP_CNT`: **100** (definisano u `common.h`) |
| **Stanja Transfera (Enum)** | `RUBICON_PACKET_ENUMERATOR`<br>`RUBICON_PACKET_SEND`<br>`RUBICON_PACKET_PENDING` (Čeka ACK/NAK)<br>`RUBICON_PACKET_RECEIVING`<br>`RUBICON_PACKET_RECEIVED` | `PCK_ENUM`<br>`PCK_SEND`<br>`PCK_RECEIVING` (Čeka Paket/Timeout)<br>`PCK_RECEIVED`<br>*(Nema eksplicitnog PENDING stanja)* |
| **Detekcija ACK/NAK** | Trenutna u `RUBICON_PACKET_PENDING` stanju | Putem timeout-a u `PCK_RECEIVING` stanju (ako nije validan paket > 9 bajtova) |

## 2. Detaljna Analiza Protokola

### 2.1. Stariji Projekt (`hotel_ctrl_stariji_projekt.c`)

*   **Logika Slanja i Prijema**:
    *   Koristi eksplicitno stanje `RUBICON_PACKET_PENDING`.
    *   Nakon slanja paketa (`RUBICON_PACKET_SEND`), prelazi u `PENDING`.
    *   U `PENDING` stanju, ako stigne `RUBICON_ACK` (0x06) ili `RUBICON_NAK` (0x15), **odmah** prelazi u naredno stanje (`RECEIVED` ili `ENUMERATOR`), bez čekanja punog timeout-a.
    *   Ovo omogućava bržu reakciju na kratke potvrdne poruke.
*   **Update Sekvenca**:
    *   Stanja ažuriranja firmvera: `FW_UPDATE_IDLE`, `FW_UPDATE_BOOTLOADER`, `FW_UPDATE_RUN`, `FW_UPDATE_FINISHED`.
    *   Koristi `RUBICON_StartFwUpdateTimer` za upravljanje dugim čekanjima (npr. brisanje flash-a, restart).
*   **Baferi**:
    *   Koristi `rubicon_ctrl_buffer` (512B) za opšte operacije i `rx_buffer`/`tx_buffer` (veličina definisana externim `DATA_BUF_SIZE`) za RS485.

### 2.2. Noviji Projekt (`hotel_ctrl_noviji_projekt.c` & `rs485_noviji_projekt.c`)

*   **Logika Slanja i Prijema**:
    *   Nema `PENDING` stanja. Iz `PCK_SEND` prelazi direktno u `PCK_RECEIVING`.
    *   U `PCK_RECEIVING` se primarno čeka validan paket (dužina > 9 bajtova, ispravan CRC).
    *   **ACK Detekcija**: Provjera za `ACK` (ili `NAK`) se vrši tek kada **istekne RX timeout** (`rx_tout`) ako nije detektovan puni paket.
        *   Kod: `else if(((Get_SysTick() - rx_tmr) >= rx_tout) ... if(rx_buff[0] == ACK) ...`
        *   Ovo implicira da sistem čeka `RX2TX_DEL` (3ms) ili `RESP_TOUT` (45ms) prije nego što procesira `ACK`, što može unijeti kašnjenje u odnosu na stariji projekat.
*   **RS485 Driver (`rs485_noviji_projekt.c`)**:
    *   Koristi Interrupt Callback (`HAL_UART_RxCpltCallback`) sa internim *state machine*-om (`switch(++receive_pcnt)`).
    *   **Kritično**: Driver očekuje da paket počinje sa `SOH` ili `STX` (Case 1).
    *   Ako uređaj pošalje samo `ACK` (0x06), driver ga **neće** prepoznati kao početak paketa u `receive_pcnt=1`. Bajt će biti ignorisan ili pogrešno upisan u bafer tek na `receive_pcnt=2`, što potencijalno komplikuje detekciju `ACK`-a u `hotel_ctrl` sloju (koji očekuje `rx_buff[0] == ACK`).
*   **Update Sekvenca**:
    *   Stanja su preimenovana ali logički slična: `FW_UPDATE_BLDR`, `FW_UPDATE_RUN`, `FW_UPDATE_END`.
    *   Dodata podrška za više tipova uređaja kroz `common.h` (`HOTEL_CONTROLLER`, `ROOM_CONTROLLER`, `ROOM_THERMOSTAT`).

## 3. Zaključak o Kompatibilnosti

*   **Tajming**: Noviji projekat koristi kraće timeoute za odgovor (45ms vs 78ms) i manipulaciju RX/TX (3ms vs 10ms). Ovo zahtijeva da uređaji na bus-u reaguju brže.
*   **Protokol**: Iako je struktura paketa ista (SOH/STX...CRC...EOT), mehanizam prijema kratkih potvrda (`ACK`) je fundamentalno drugačiji. Stariji projekat ih obrađuje trenutno, dok noviji čeka timeout i zavisi od ponašanja RS485 drajvera koji favorizuje duge pakete sa header-om.
*   **Baferi**: Veličine bafera za fajl transfer su usklađene (512B na aplikativnom nivou), ali RS485 prijemni bafer je manji u novom projektu (256B) u odnosu na `DATA_BUF_SIZE` (pretpostavljeno veći ili isti u starom, ali `HC_BSIZE` je 512). Ako stari projekat šalje pakete veće od 256B, novi projekat ih neće moći primiti u `rs485_buff`.

*Analiza je bazirana isključivo na sadržaju fajlova iz direktorijuma `reference`.*
