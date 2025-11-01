#pragma once

#include <pgmspace.h>

// Sadržaj preuzet direktno iz sysctrl.html
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
    <title>ESP32 Hotel Controller</title>
    <meta http-equiv="Content-Type" content="text/html; charset=windows-1252">
    <meta content="MSHTML 6.00.2800.1561" name="GENERATOR">
    <style>
        body { font-family: Arial, sans-serif; background: #f4f4f4; }
        .container { max-width: 800px; margin: auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        hr { border: 0; border-top: 1px solid #ddd; }
        input[type="number"], select { width: 100px; padding: 5px; }
        input[type="button"] { padding: 8px 12px; background: #007bff; color: white; border: none; cursor: pointer; border-radius: 4px; margin: 2px; }
        input[type="button"]:hover { background: #0056b3; }
        #log0 { display: block; margin-top: 15px; padding: 10px; border: 1px solid #ccc; background: #fafafa; min-height: 40px; }
    </style>
</head>
<body>
    <div class="container">
    <br>
    Prva/Odabrana adresa.......<input id="kont101" value="100" type="number" min="1" max="65000" onchange="kont102.value = this.value" title="Jedina odabrana adresa za sve komande ako je zadnja/nova adresa ista ili prva odabrana adresa u rasponu do zadnje adrese. Odabirom adrese rs485 interfejsa glavnog kontrolera, po defaultu = 5, komanda se odnosi na glavni kontroler">
    <hr>
    Zadnja/Nova adresa..........<input id="kont102" value="100" type="number" min="1" max="65000" title="zadnja odabrana adresa u rasponu za update komande ili nova adresa kontrolera"> &nbsp; &nbsp; &nbsp;
    <hr>
    Grupna adresa..................<input id="kont103" value="26486" type="number" min="1" max="65000" title="grupna adresa kontrolera za sinhronizovane komande grupama: pozarni put, kontrola rasvjete sprata, novi zurnal za sprat"> &nbsp; &nbsp; &nbsp;
    <hr>
    Broadcast adresa..............<input id="kont104" value="39321" type="number" min="1" max="65000" title="zajednicka adresa za cijeli sistem za sinhronizovane komande svim uredjajima: vrijeme, kontrola rasvjete balkona, pozarni put, zurnal za sistem...."> &nbsp;
    <hr>
    RS485 interface baudrate:
    <select id="kont105" name="interface_baudrate" title="brzina rs485 interfejsa kontrolera sobe mora biti ista kao i kod glavnog kontrolera za uspjesnu komunikaciju. Za promjenu brzine rs485 interfejsa glavnog kontrolera, prva i zadnja odabrana adresa trebaju biti rs485 adresa interfejsa glavnog kontrolera, po defaultu = 5 ">
        <option value="0">2400 bps</option>
        <option value="1">4800 bps</option>
        <option value="2">9600 bps</option>
        <option value="3">19200 bps</option>
        <option value="4">38400 bps</option>
        <option value="5">57600 bps</option>
        <option selected value="6">115200 bps</option>
        <option value="7">230400 bps</option>
        <option value="8">460800 bps</option>
        <option value="9">921600 bps</option>
    </select>
    <input id="kont106" value="Promjena adresa" type="button" onclick="send_event(106)" title="promjena postavki rs485 interfejsa ukljucuje i vrijednost brzine interfejsa !!!">
    <hr>
    <input id="kont201" value="Podesi vrijeme" type="button" onclick="set_time()" title="podesi tacno vrijeme i datum sistema prema lokalnom vremenu racunara">
    <input id="kont202" value="Pregledaj log" type="button" onclick="get_log()" title="pregledaj zadnji blok log liste">
    <input id="kont203" value="Obrisi log" type="button" onclick="delete_log()" title="obrisi zadnji blok log liste">
    <input id="kont204" value="Brisi log listu" type="button" onclick="delete_log_list()" title="obrisi cijelu log listu">
    <br>
    <br>
    <span id="log0">Status: Spreman.</span>
    <hr>
    <input id="kont301" value="Provjeri status" type="button" onclick="send_event(301)" title="zahtjev za status kontrolera sobe sa adresom podesenom u input boksu Prva/Odabrana adresa i pregled vracenog odgovora">
    Promjeni status:
    <select id="kont302" onchange="send_event(302)" title="promjeni status kontrolera sobe sa adresom podesenom u input boksu Prva/Odabrana adresa">
        <option value="0">U pripremi</option>
        <option value="1">Spremna</option>
        <option value="2">Zauzeta</option>
        <option value="3">Ciscenje</option>
        <option value="4">Zamjena posteljine</option>
        <option value="5">Generalno C.</option>
        <option value="6">Neupotrebljiva</option>
        <option value="7">Late CheckOut</option>
        <option value="8">Sobarica u sobi</option>
        <option value="9">Reset DND</option>
        <option value="10">Alarm pozara</option>
        <option value="11">Pozarni put</option>
    </select>
    <hr>
    Podesi period zamjene posteljine: <input id="kont303" onchange="send_event(303)" value="0" type="number" min="0" max="99" title="period ponavljanja u danima, zahtjeva za zamjenu posteljine kontrolera sobe, sa adresom podesenom u input boksu Prva/Odabrana adresa, vrijednost  0 = nula iskljucuje funkciju. Vrijednost brojaca aktivne funkcije se može resetovati na 0 = nulu tako da se status sa bilo kojeg drugog promjeni na status U PRIPREMI">
    <hr>
    Podesi osvjetljenje displeja......... <input id="kont304" onchange="send_event(304)" value="500" type="number" min="100" max="900" title="podesi jacinu pozadinskog osvjetljenja lcd displeja kontrolera sobe sa adresom podesenom u input boksu Prva/Odabrana adresa">
    <input id="kont305" value="Pregled slika" type="button" onclick="send_event(305)" title="komanda kontroleru sobe sa adresom podesenom u input boksu Prva/Odabrana adresa, za pregled svih slika na displeju, po redu, od prve do zadnje">
    <hr>
    <input id="kont408" onchange="send_event(408)" type="checkbox" value="0" title="selektuj ovo polje za forsiranje izlaza kontrolera sobe sa adresom podesenom u input boksu Prva/Odabrana adresa, a prema stanju checkbox kontrola 0 do 7. dok god je ova kontrola selektovana, digitalni izlazi na kontroleru sobe ce biti poostavljeni u stanje kontrola 0 do 7 bez bilo kakve promjene iz softvera kontrolera sobe">
    Forsiraj digitalne izlaze...
    0<input id="kont400" onchange="send_event(408)" type="checkbox" value="0" title="izlaz kontakter napajanja">
    1<input id="kont401" onchange="send_event(408)" type="checkbox" value="0" title="izlaz DND modul">
    2<input id="kont402" onchange="send_event(408)" type="checkbox" value="0" title="izlaz balkonska rasvjeta">
    3<input id="kont403" onchange="send_event(408)" type="checkbox" value="0" title="izlaz zvono">
    4<input id="kont404" onchange="send_event(408)" type="checkbox" value="0" title="izlaz kontakter klime">
    5<input id="kont405" onchange="send_event(408)" type="checkbox" value="0" title="izlaz termostat klime">
    6<input id="kont406" onchange="send_event(408)" type="checkbox" value="0" title="izlaz elektro brava">
    7<input id="kont407" onchange="send_event(408)" type="checkbox" value="0" title="izlaz buzzer">
    <hr>
    <input value="Reset glavnog kontrolera" type="button" onclick="send_event(450)" title="resetuj direktnom komandom glavni kontroler">
    <input value="Reset adresiranog kontrolera" type="button" onclick="send_event(451)" title="resetuj kontroler sobe sa adresom podesenom u input boksu Prva/Odabrana adresa">
    <input value="Reset SOS" type="button" onclick="send_event(452)" title="resetuj SOS alarm kontrolera sobe sa adresom podesenom u input boksu Prva/Odabrana adresa, ili SOS alarm na glavnom kontroleru odabirom adrese rs485 interfejsa glavnog kontrolera u input boksu Prva/Odabrana adresa, a po defaultu = 5">
    <hr>
    <input value="Update Firmwarea" type="button" onclick="send_event(480)" title="komanda za update firmwera adresiranog kontrolera. prethodno je potrebno komandom za prenos slika izvrsiti transfer slike img20.raw koja je preimenovani binarni fajl novog firmwera kontrolera sobe. odabirom adrese rs485 interfejsa glavnog kontrolera u input boksu Prva/Odabrana adresa, a po defaultu = 5, komanda se odnosi na glavni kontroler za koji je potrebno prethodno tftp klijentom uploadowati novi firmware CTRL_NEW.BIN ili kopirati na uSD karticu">
    <input value="Update Bootloadera" type="button" onclick="send_event(481)" title="komanda za update bootloadera adresiranog kontrolera. prethodno je potrebno komandom za prenos slika izvrsiti transfer slike img21.raw koja je preimenovani binarni fajl novog bootloadera kontrolera sobe.  odabirom adrese rs485 interfejsa glavnog kontrolera u input boksu Prva/Odabrana adresa, a po defaultu = 5, komanda se odnosi na glavni kontroler za koji je potrebno prethodno tftp klijentom uploadowati novi firmware CTRL_NEW.BIN ili kopirati na uSD karticu">
    <input value="Update config listom" type="button" onclick="send_event(482)" title="pokreni update koji je prethodno upisan u fajl UPDATE.CFG i uploadovan na glavni kontroler">
    <hr>
    Prva slika: <input id="kont501" onchange="kont502.value = this.value" value="1" type="number" min="1" max="21" title="odabir prve od vise slika za update ili jedine odabrane">&nbsp;
    Zadnja slika: <input id="kont502" onchange="" value="1" type="number" min="1" max="21" title="odabir poslednje slike kod više slika za update. za jednu sliku postaviti isti ili manji broj">&nbsp; &nbsp; &nbsp;
    <input value="Update slika" type="button" onclick="send_event(503)" title="pokreni update slika odabranih od prve do zadnje i na adresirane kontrolere soba od Prve/Odabrane do Zadnje/Nove adrese">
    <hr>
    IP adresa............<input id="kont550" value="192" type="number" min="0" max="255"> <input id="kont551" value="168" type="number" min="0" max="255"> <input id="kont552" value="20" type="number" min="0" max="255"> <input id="kont553" value="199" type="number" min="0" max="255">
    <hr>
    Subnet Mask......<input id="kont560" value="255" type="number" min="0" max="255"> <input id="kont561" value="255" type="number" min="0" max="255"> <input id="kont562" value="255" type="number" min="0" max="255"> <input id="kont563" value="0" type="number" min="0" max="255">
    <hr>
    Default Gateway.<input id="kont570" value="192" type="number" min="0" max="255"> <input id="kont571" value="168" type="number" min="0" max="255"> <input id="kont572" value="20" type="number" min="0" max="255"> <input id="kont573" value="1" type="number" min="0" max="255">
    <br>
    <br>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;  &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;
    <input value="Promjena IP adresa" type="button" onclick="send_event(575)" title="U slucaju pogresnih postavki i gubitka konekcije sa glavnim kontrolerom, pritisnuti i zadrzati srednji taster na glavnomm kontroleru. kada ccrvena LED treci puta zasvijetli, odmah otpustiti taster. ucitane su defaultne adrese, IP:192.168.20.199 SUBNET:255.255.255.0 GW:192.168.20.199">
    <hr>
    Sistem ID:..........<input id="kont580" value="43981" type="number" min="1" max="65000">&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;
    <input value="Promjena ID sistema" type="button" onclick="send_event(581)" title="Promjena ID broja sistema za adresirani uredjaj u polju Prva/Odabrana adresa. Ako se komanda posalje na rs485 adresu glavnog kontrolera, defaultno = 5, momentalno ce biti broadcast adresom proslijedjena svim kontrolerima, sto znaci da ni jedna rfid kartica nece biti validna do sledeceg programiranja sa novim ID brojem sistema">
    <hr>
    <input value="Tesni Zurnal  1" type="button" onclick="send_event(590)" title="testni zurnal text 1">
    &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;
    <input value="Testni Zurnal 2" type="button" onclick="send_event(591)" title="testni zurnal text 1">
    <hr>
    </div>
</body>
</html>
<SCRIPT>
    // Funkcija za prikaz odgovora servera
    function show_response(text, isError) {
        var logEl = document.getElementById("log0");
        logEl.innerHTML = text;
        logEl.style.color = isError ? "red" : "green";
    }

    // Funkcija za slanje (moderni fetch)
    function send_request(url) {
        show_response("Šaljem: " + url, false);
        fetch(url)
            .then(response => {
                return response.text().then(text => {
                    if (!response.ok) {
                        throw new Error(`HTTP greška ${response.status}: ${text}`);
                    }
                    return text;
                });
            })
            .then(text => {
                show_response("Odgovor servera: " + text, false);
            })
            .catch(e => {
                show_response(`Greška: ${e.message}`, true);
            });
    }

    // --- Funkcije preuzete iz sysctrl.html ---
    function get_log() {
        show_response('Zahtjev poslan', false);
        send_request("sysctrl.cgi?log=3");
    }
    function delete_log() {
        show_response("Zahtjev za brisanje", false);
        send_request("sysctrl.cgi?log=4");
    }
    function delete_log_list() {
        show_response("Zahtjev za brisanje", false);
        send_request("sysctrl.cgi?log=5");
    }
    function set_time() {
        var htt = "";
        var currentTime = new Date();
        var month = addZero(currentTime.getMonth() + 1);
        var weekday = (currentTime.getDay());
        if (weekday === 0) weekday = 7; // JS D=0 -> Protokol D=7
        var day = addZero(currentTime.getDate());
        var year = addZero(currentTime.getFullYear());
        var hours = addZero(currentTime.getHours());
        var minutes = addZero(currentTime.getMinutes());
        var sec = addZero(currentTime.getSeconds());
        // Format: WDDMMYYYYHHMMSS
        htt = "sysctrl.cgi?tdu=" + weekday + day + month + year + hours + minutes + sec;
        send_request(htt);
    }
    function addZero(i) {
        if (i < 10) {
            i = "0" + i;
        }
        return i;
    }
    function send_event(t) {
        var htt = "";
        if (t == "106") {
            htt = "sysctrl.cgi?rsc=" + document.getElementById("kont101").value + "&rsa=" + document.getElementById("kont102").value +
                "&rga=" + document.getElementById("kont103").value + "&rba=" + document.getElementById("kont104").value +
                "&rib=" + document.getElementById("kont105").value;
        }
        else if (t == "301") {
            htt = "sysctrl.cgi?cst=" + document.getElementById("kont101").value;
        }
        else if (t == "302") {
            htt = "sysctrl.cgi?stg=" + document.getElementById("kont101").value + "&val=" + document.getElementById("kont302").value;
        }
        else if (t == "303") {
            htt = "sysctrl.cgi?sbr=" + document.getElementById("kont101").value + "&per=" + document.getElementById("kont303").value;
        }
        else if (t == "304") {
            htt = "sysctrl.cgi?cbr=" + document.getElementById("kont101").value + "&br=" + document.getElementById("kont304").value;
        }
        else if (t == "305") {
            htt = "sysctrl.cgi?ipr=" + document.getElementById("kont101").value;
        }
        else if (t == "408") {
            var n0 = 0; var n1 = 0; var n2 = 0; var n3 = 0; var n4 = 0; var n5 = 0; var n6 = 0; var n7 = 0; var n8 = 0;

            if (document.getElementById("kont408").checked == true) {
                if (document.getElementById("kont400").checked) { n0 = 1; };
                if (document.getElementById("kont401").checked) { n1 = 1; };
                if (document.getElementById("kont402").checked) { n2 = 1; };
                if (document.getElementById("kont403").checked) { n3 = 1; };
                if (document.getElementById("kont404").checked) { n4 = 1; };
                if (document.getElementById("kont405").checked) { n5 = 1; };
                if (document.getElementById("kont406").checked) { n6 = 1; };
                if (document.getElementById("kont407").checked) { n7 = 1; };
                n8 = 1;
            }
            htt = "sysctrl.cgi?cdo=" + document.getElementById("kont101").value + "&do0=" + n0 + "&do1=" + n1 + "&do2=" + n2 + "&do3=" + n3 + "&do4=" + n4 + "&do5=" + n5 + "&do6=" + n6 + "&do7=" + n7 + "&ctrl=" + n8;
        }
        else if (t == "450") {
            htt = "sysctrl.cgi?rst=0"; // Koristi 0 za HC kao u starom kodu
        }
        else if (t == "451") {
            htt = "sysctrl.cgi?rst=" + document.getElementById("kont101").value;
        }
        else if (t == "452") {
            htt = "sysctrl.cgi?rud=" + document.getElementById("kont101").value;
        }
        else if (t == "480") {
            htt = "sysctrl.cgi?fuf=" + document.getElementById("kont101").value + "&ful=" + document.getElementById("kont102").value;
        }
        else if (t == "481") {
            htt = "sysctrl.cgi?buf=" + document.getElementById("kont101").value + "&bul=" + document.getElementById("kont102").value;
        }
        else if (t == "482") {
            htt = "sysctrl.cgi?cfg=run";
        }
        else if (t == "503") {
            // GREŠKA u originalnom JS: ifa/ila i kont101/102 su zamijenjeni.
            // Original: iuf=slika1&iul=slika2&ifa=adresa1&ila=adresa2
            // Ispravljeno prema Procitaj.txt (iuf=adresa1, ifa=slika1):
            htt = "sysctrl.cgi?iuf=" + document.getElementById("kont101").value + "&iul=" + document.getElementById("kont102").value +
                "&ifa=" + document.getElementById("kont501").value + "&ila=" + document.getElementById("kont502").value;
        }
        else if (t == "575") {
            var ipp = document.getElementById("kont550").value + "." + document.getElementById("kont551").value + "." + document.getElementById("kont552").value + "." + document.getElementById("kont553").value;
            var smm = document.getElementById("kont560").value + "." + document.getElementById("kont561").value + "." + document.getElementById("kont562").value + "." + document.getElementById("kont563").value;
            var dgg = document.getElementById("kont570").value + "." + document.getElementById("kont571").value + "." + document.getElementById("kont572").value + "." + document.getElementById("kont573").value;
            htt = "sysctrl.cgi?ipa=" + ipp + "&snm=" + smm + "&gwa=" + dgg;
        }
        else if (t == "581") {
            htt = "sysctrl.cgi?sid=" + document.getElementById("kont101").value + "&nid=" + document.getElementById("kont580").value;
        }
        else if (t == "590") {
            htt = "sysctrl.cgi?HSset=1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,1,2,3,4;";
        }
        else if (t == "591") {
            htt = "sysctrl.cgi?HSset=1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,1,2,3,4;";
        }
        
        if (htt) {
            send_request(htt);
        }
    }
</SCRIPT>
)rawliteral";