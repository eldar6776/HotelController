# Analiza Protokola za Transfer Fajlova: Noviji vs. Stariji Projekt

Ovaj dokument sadrži tehničku analizu i razlike u protokolu za prenos fajlova preko RS485 interfejsa, baziranu na izvornom kodu fajlova sa sufiksima `_noviji_projekt` i `_stariji_projekt`.

## 1. Osnovne Razlike u Odgovoru (Receiver Side)

Najznačajnija promjena je način na koji uređaj koji prima fajl potvrđuje paket.

| Karakteristika | Stariji Projekt (`rs485_stariji_projekt.c`) | Noviji Projekt (`rs485_noviji_projekt.c`) |
| :--- | :--- | :--- |
| **Tip Odgovora** | **Single Byte Handshake** | **Full Packet Handshake** |
| **Struktura Odgovora** | Šalje se isključivo 1 bajt: `ACK (0x06)` ili `NAK (0x15)`. | Šalje se kompletan RS485 paket sa zaglavljem, payload-om i CRC-om. |
| **Sadržaj Odgovora** | Samo potvrda prijema. | Potvrda prijema + **Broj sljedećeg očekivanog paketa**. |
| **Funkcija Slanja** | `Serial_PutByte(ACK)` (direktan UART prenos 1 bajta). | `RS485_Response(resp, resps)` (formiranje cijelog okvira). |


## 2. Baferi i Payload (Veličina podataka)


| Karakteristika | Stariji Projekt | Noviji Projekt |
| :--- | :--- | :--- |
| **Payload paketa (STX)** | **64 bajta** (`RUBICON_PACKET_BUFFER_SIZE`) | **128 bajtova** (`HC_PCK_BSIZE`) |
| **Glavni RS485 bafer** | 256 bajtova (`BUFF_SIZE`) / 512 (`RUBICON_BUFFER_SIZE`) | **1048 bajtova** (`RS485_BSIZE`) |
| **Max Trial Count** | 10 pokušaja (`RUBICON_MAX_ERRORS`) | **100 pokušaja** (`MAXREP_CNT`) |

## 3. Tajminzi i Pauze


| Parametar | Stariji Projekt | Noviji Projekt |
| :--- | :--- | :--- |
| **RX to TX Delay** | **10 ms** (`RUBICON_RX_TO_TX_DELAY`) | **3 ms** (`RX2TX_DEL`) |
| **Response Timeout** | **78 ms** (`RUBICON_RESPONSE_TIMEOUT`) | **45 ms** (`RESP_TOUT`) |
| **Byte RX Timeout** | 3 ms (`RUBICON_BYTE_RX_TIMEOUT`) | 3 ms (`RX_TOUT`) |
| **Mute Delay** | Nije eksplicitno definisan u istom kontekstu. | 10 ms (`MUTE_DEL`) |

## 4. Logika Provjere Paketa (Handshake)

### Stariji Projekt:
U fajlu `hotel_ctrl_stariji_projekt.c`, stanje `RUBICON_PACKET_PENDING` samo provjerava prvi bajt:
```c
else if(rx_buffer[0] == RUBICON_ACK) {
    eRubiconTransferState = RUBICON_PACKET_RECEIVED;
}
```

### Noviji Projekt:
U fajlu `hotel_ctrl_noviji_projekt.c`, pošiljalac u stanju `PCK_RECEIVED` analizira povratni broj paketa iz payloada odgovora:
```c
case UPD_FILE: {
    if(rx_buff[0] == NAK) {
        // Izračunava koji paket je receiver tražio
        uint32_t pktreq = ((HC_FilUpdPck.pck_send & 0xFFFFFF00) | rx_buff[6]);
        // Ako je pošiljalac iza, preskače na traženi paket
        if(HC_FilUpdPck.pck_send < pktreq) {
            HC_FilUpdPck.pck_send++; 
        }
    }
}
```
