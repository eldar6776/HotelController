# Plan Integracije za `fuf` Komandu

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
-   **Logika za update slika (`iuf`):**
    -   `HttpServer.cpp` za `iuf` komandu ispravno poziva `m_update_manager->StartImageUpdateSequence()`.
    -   Ova funkcija u `UpdateManager`-u zatim iterira kroz adrese i slike, i za svaku kombinaciju poziva `StartSession` sa odgovarajućom komandom za sliku (npr. `CMD_IMG_RC_START + img_num - 1`).
    -   Ovaj mehanizam je ispravan model koji treba slediti.

## 4. Predlog Plana Integracije (Bez Izmene Koda)

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
        ```cpp
        uint16_t first_addr = request->getParam("fuf")->value().toInt();
        uint16_t last_addr = request->getParam("ful")->value().toInt();
        ```
    -   Izvršiti osnovnu validaciju adresa (npr. `first_addr > 0`, `last_addr >= first_addr`).
    -   Pozvati `UpdateManager` da pokrene sekvencu ažuriranja, ali sa **fiksiranim brojevima slika na 20**.
        ```cpp
        m_update_manager->StartImageUpdateSequence(first_addr, last_addr, 20, 20);
        ```
    -   Odgovoriti klijentu odmah sa `HTTP_RESPONSE_OK` ili sličnom porukom, jer `StartImageUpdateSequence` radi u pozadini.

### Tok Podataka (Data Flow) nakon izmene:

1.  HTTP GET zahtev sa `fuf=101&ful=103` stiže na server.
2.  `HttpServer.cpp` ulazi u modifikovani `if` blok.
3.  Parsiraju se adrese `101` i `103`.
4.  Poziva se `m_update_manager->StartImageUpdateSequence(101, 103, 20, 20)`.
5.  `UpdateManager` u svojoj `Run()` petlji detektuje aktivnu sekvencu.
6.  Za adresu `101`, pokreće sesiju za sliku `20`.
7.  `PrepareSession` unutar `UpdateManager`-a treba da ispravno formira putanju do fajla. **Potrebna provera:** Logika u `PrepareSession` trenutno formira putanje kao `/<addr>/<addr>_<img_num>.RAW`. Za `fuf` komandu, potrebno je osigurati da se traži fajl `/IMG20.RAW` iz root direktorijuma. Ovo može zahtevati manju modifikaciju unutar `PrepareSession` da, ako je `img_num == 20`, koristi fiksnu putanju.
8.  `UpdateManager` šalje RS485 pakete sa komandom `DWNLD_DISP_IMG_20` (0x77).
9.  Nakon završetka za adresu 101, sekvencer prelazi na 102, pa na 103, ponavljajući proces.

### Zaključak

Ovaj pristup je robustan jer:
-   U potpunosti replicira logiku nasleđenog sistema.
-   Koristi postojeći, ispravan mehanizam za ažuriranje slika (`StartImageUpdateSequence`).
-   Zahteva minimalne, logički izolovane izmene unutar `HttpServer.cpp`.
-   Izbegava se "prljanje" `UpdateManager`-a sa specijalnim slučajevima, jer se `fuf` efektivno tretira kao `iuf` sa fiksiranim parametrima.
