#pragma once

#include <pgmspace.h>

// Sadržaj preuzet iz sysctrl.html + NOVE FUNKCIJE
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
    <title>ESP32 Hotel Controller</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: Arial, sans-serif; background: #f4f4f4; }
        .container { max-width: 900px; margin: auto; background: #fff; padding: 20px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }
        hr { border: 0; border-top: 1px solid #ddd; }
        input[type="number"], select { width: 100px; padding: 5px; }
        input[type="button"] { padding: 8px 12px; background: #007bff; color: white; border: none; cursor: pointer; border-radius: 4px; margin: 2px; }
        input[type="button"]:hover { background: #0056b3; }
        #log0 { display: block; margin-top: 15px; padding: 10px; border: 1px solid #ccc; background: #fafafa; min-height: 40px; white-space: pre-wrap; }
        
        /* NEW: File Browser Styles */
        .file-section { margin-top: 30px; padding: 15px; background: #f9f9f9; border: 1px solid #ddd; border-radius: 5px; }
        .file-list { max-height: 300px; overflow-y: auto; border: 1px solid #ccc; padding: 10px; background: white; }
        .file-item { padding: 8px; border-bottom: 1px solid #eee; display: flex; justify-content: space-between; align-items: center; }
        .file-item:hover { background: #f0f0f0; }
        .file-name { font-family: 'Courier New', monospace; flex-grow: 1; }
        .file-size { color: #666; margin-right: 10px; }
        .dir-item { cursor: pointer; color: #0056b3; font-weight: bold; }
        #currentPath { font-family: 'Courier New', monospace; color: #555; margin-bottom: 10px; }
        .delete-btn { padding: 4px 8px; background: #dc3545; color: white; border: none; cursor: pointer; border-radius: 3px; font-size: 0.9em; }
        .delete-btn:hover { background: #c82333; }
        .rename-btn { padding: 4px 8px; background: #ffc107; color: black; border: none; cursor: pointer; border-radius: 3px; font-size: 0.9em; margin-left: 5px; }
        .rename-btn:hover { background: #e0a800; }
        .refresh-btn { background: #28a745; }
        .refresh-btn:hover { background: #218838; }
        .create-folder-section { margin-top: 10px; display: flex; gap: 10px; }
    </style>
</head>
<body>
    <div class="container">
    <h2>ESP32 Hotel Controller - Web Interface</h2>
    <br>
    Prva/Odabrana adresa.......<input id="kont101" value="100" type="number" min="1" max="65000" onchange="kont102.value = this.value">
    <hr>
    Zadnja/Nova adresa..........<input id="kont102" value="100" type="number" min="1" max="65000"> &nbsp; &nbsp; &nbsp;
    <hr>
    Grupna adresa..................<input id="kont103" value="26486" type="number" min="1" max="65000"> &nbsp; &nbsp; &nbsp;
    <hr>
    Broadcast adresa..............<input id="kont104" value="39321" type="number" min="1" max="65000"> &nbsp;
    <hr>
    RS485 interface baudrate:
    <select id="kont105" name="interface_baudrate">
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
    <input id="kont106" value="Promjena adresa" type="button" onclick="send_event(106)">
    <hr>
    
    <!-- NEW: Address List Management -->
    <h3>Upravljanje Listom Adresa</h3>
    <input value="Učitaj Listu Kontrolera" type="button" onclick="load_address_list()" 
           title="Učitava CTRL_ADD.TXT sa uSD kartice i kešira u EEPROM">
    <span style="color: #666; font-size: 0.9em;">(Učitava iz CTRL_ADD.TXT na uSD kartici)</span>
    <hr>
    
    <input id="kont201" value="Podesi vrijeme" type="button" onclick="set_time()">
    <input id="kont202" value="Pregledaj log" type="button" onclick="get_log()">
    <input id="kont203" value="Obrisi log" type="button" onclick="delete_log()">
    <input id="kont204" value="Brisi log listu" type="button" onclick="delete_log_list()">
    <br>
    <br>
    <span id="log0">Status: Spreman.</span>
    <hr>
    <input id="kont301" value="Provjeri status" type="button" onclick="send_event(301)">
    Promjeni status:
    <select id="kont302" onchange="send_event(302)">
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
    Podesi period zamjene posteljine: <input id="kont303" onchange="send_event(303)" value="0" type="number" min="0" max="99">
    <hr>
    Podesi osvjetljenje displeja......... <input id="kont304" onchange="send_event(304)" value="500" type="number" min="100" max="900">
    <input id="kont305" value="Pregled slika" type="button" onclick="send_event(305)">
    <hr>
    <input id="kont408" onchange="send_event(408)" type="checkbox" value="0">
    Forsiraj digitalne izlaze...
    0<input id="kont400" onchange="send_event(408)" type="checkbox" value="0">
    1<input id="kont401" onchange="send_event(408)" type="checkbox" value="0">
    2<input id="kont402" onchange="send_event(408)" type="checkbox" value="0">
    3<input id="kont403" onchange="send_event(408)" type="checkbox" value="0">
    4<input id="kont404" onchange="send_event(408)" type="checkbox" value="0">
    5<input id="kont405" onchange="send_event(408)" type="checkbox" value="0">
    6<input id="kont406" onchange="send_event(408)" type="checkbox" value="0">
    7<input id="kont407" onchange="send_event(408)" type="checkbox" value="0">
    <hr>
    <input value="Reset glavnog kontrolera" type="button" onclick="send_event(450)">
    <input value="Reset adresiranog kontrolera" type="button" onclick="send_event(451)">
    <input value="Reset SOS" type="button" onclick="send_event(452)">
    <hr>
    <input value="Update Firmwarea" type="button" onclick="send_event(480)">
    <input value="Update Bootloadera" type="button" onclick="send_event(481)">
    <hr>
    Prva slika: <input id="kont501" onchange="kont502.value = this.value" value="1" type="number" min="1" max="21">&nbsp;
    Zadnja slika: <input id="kont502" onchange="" value="1" type="number" min="1" max="21">&nbsp; &nbsp; &nbsp;
    <input value="Update slika" type="button" onclick="send_event(503)">
    <hr>
    IP adresa............<input id="kont550" value="192" type="number" min="0" max="255"> <input id="kont551" value="168" type="number" min="0" max="255"> <input id="kont552" value="20" type="number" min="0" max="255"> <input id="kont553" value="199" type="number" min="0" max="255">
    <hr>
    Subnet Mask......<input id="kont560" value="255" type="number" min="0" max="255"> <input id="kont561" value="255" type="number" min="0" max="255"> <input id="kont562" value="255" type="number" min="0" max="255"> <input id="kont563" value="0" type="number" min="0" max="255">
    <hr>
    Default Gateway.<input id="kont570" value="192" type="number" min="0" max="255"> <input id="kont571" value="168" type="number" min="0" max="255"> <input id="kont572" value="20" type="number" min="0" max="255"> <input id="kont573" value="1" type="number" min="0" max="255">
    <br>
    <br>&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;  &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;
    <input value="Promjena IP adresa" type="button" onclick="send_event(575)">
    <hr>
    Sistem ID:..........<input id="kont580" value="43981" type="number" min="1" max="65000">&nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp; &nbsp;
    <input value="Promjena ID sistema" type="button" onclick="send_event(581)">
    <hr>
    
    <!-- NEW: File Upload Section -->
    <div class="file-section">
        <h3>Upload Fajlova na uSD Karticu</h3>
        <form id="uploadForm" enctype="multipart/form-data">
            <input type="file" id="fileInput" name="file" accept=".bin,.raw,.txt,.cfg">
            <input type="button" value="Upload" onclick="upload_file()">
        </form>
        <div id="uploadProgress" style="margin-top: 10px; color: #666;"></div>
    </div>
    <hr>
    
    <!-- NEW: File Browser Section -->
    <div class="file-section">
        <h3>File Browser (uSD Kartica)
            <input type="button" class="refresh-btn" value="↻ Refresh" onclick="load_file_list()">
        </h3>
        <div id="currentPath">/</div>
        <div id="fileList" class="file-list">
            <p style="text-align: center; color: #999;">Klikni "Refresh" za učitavanje...</p>
        </div>
        <div class="create-folder-section">
            <input type="text" id="newFolderName" placeholder="Ime novog foldera..." style="flex-grow: 1; padding: 6px;">
            <input type="button" value="Kreiraj Folder" onclick="create_folder()" style="background-color: #17a2b8;">
        </div>
    </div>
    
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
                // Parse HTML i izvuci sadržaj između <div class="response">
                var parser = new DOMParser();
                var doc = parser.parseFromString(text, 'text/html');
                var responseDiv = doc.querySelector('.response');
                var message = responseDiv ? responseDiv.textContent : text;
                show_response("Odgovor: " + message, false);
            })
            .catch(e => {
                show_response(`Greška: ${e.message}`, true);
            });
    }

    // --- NEW: Load Address List ---
    function load_address_list() {
        show_response('Učitavam listu adresa sa uSD kartice...', false);
        send_request("sysctrl.cgi?cad=load");
    }

    // --- NEW: File Upload ---
    function upload_file() {
        var fileInput = document.getElementById('fileInput');
        var file = fileInput.files[0];
        
        if (!file) {
            alert('Odaberi fajl prvo!');
            return;
        }
        
        var formData = new FormData();
        formData.append('file', file);

        var currentPath = document.getElementById('currentPath').textContent;
        if (currentPath === '/') currentPath = ''; // Ne dodajemo duplu kosu crtu za root
        var destinationPath = currentPath + '/' + file.name;
        
        document.getElementById('uploadProgress').textContent = 'Uploading ' + file.name + '...';
        
        fetch('/upload-firmware?file=' + encodeURIComponent(destinationPath), {
            method: 'POST',
            body: formData
        })
        .then(response => response.text())
        .then(text => {
            document.getElementById('uploadProgress').textContent = 'Upload OK: ' + file.name;
            document.getElementById('uploadProgress').style.color = 'green';
            // Osvježi trenutni direktorij
            setTimeout(() => load_file_list(document.getElementById('currentPath').textContent), 500); 
        })
        .catch(e => {
            document.getElementById('uploadProgress').textContent = 'Upload ERROR: ' + e.message;
            document.getElementById('uploadProgress').style.color = 'red';
        });
    }

    // --- NEW: Load File List ---
    function load_file_list(path) {
        path = path || '/'; // Default na root
        fetch('/list_files?path=' + encodeURIComponent(path))
            .then(response => response.json())
            .then(data => {
                var fileListDiv = document.getElementById('fileList');
                fileListDiv.innerHTML = '';
                document.getElementById('currentPath').textContent = data.path || '/';
                
                if (data.error) {
                    fileListDiv.innerHTML = '<p style="color: red;">' + data.error + '</p>';
                    return;
                }
                
                if (data.files.length === 0) {
                    // Ako nema fajlova, provjeri da li smo u root-u. Ako nismo, prikaži samo ".."
                    if (data.path && data.path === '/') {
                        fileListDiv.innerHTML = '<p style="text-align: center; color: #999;">Nema fajlova</p>';
                    }
                }
                
                // Dodaj ".." za navigaciju nazad, ako nismo u root-u
                // Ovo se sada izvršava čak i ako je folder prazan
                if (data.path && data.path !== '/') {
                    var parentPath = data.path.substring(0, data.path.lastIndexOf('/')) || '/';
                    var backItem = document.createElement('div');
                    backItem.className = 'file-item dir-item';
                    backItem.textContent = '..';
                    backItem.onclick = function() { load_file_list(parentPath); };
                    fileListDiv.appendChild(backItem);
                }

                // Sortiraj: prvo folderi, pa fajlovi
                data.files.sort(function(a, b) {
                    if (a.dir === b.dir) return a.name.localeCompare(b.name);
                    return a.dir ? -1 : 1;
                });
                
                data.files.forEach(function(file) {
                    var fileItem = document.createElement('div');
                    fileItem.className = 'file-item';
                    
                    var fileName = document.createElement('span');
                    fileName.className = 'file-name';
                    fileName.textContent = file.name.split('/').pop(); // Prikazi samo ime, ne cijelu putanju
                    
                    fileItem.appendChild(fileName);

                    if (file.dir) {
                        fileItem.classList.add('dir-item');
                        
                        var renameBtn = document.createElement('button');
                        renameBtn.className = 'rename-btn';
                        renameBtn.textContent = 'Preimenuj';
                        renameBtn.onclick = function(e) {
                            e.stopPropagation(); // Spriječi klik na folder
                            rename_item(file.name, true);
                        };
                        fileItem.appendChild(renameBtn);
                        fileItem.onclick = function() { load_file_list(file.name); };
                    } else {
                        var fileSize = document.createElement('span');
                        fileSize.className = 'file-size';
                        fileSize.textContent = formatBytes(file.size);
                        
                        var deleteBtn = document.createElement('button');
                        deleteBtn.className = 'delete-btn';
                        deleteBtn.textContent = '✕ Obriši';
                        deleteBtn.onclick = function() {
                            delete_file(file.path || file.name);  // Koristi punu putanju ako postoji
                        };
                        
                        fileItem.appendChild(fileSize);
                        fileItem.appendChild(deleteBtn);
                    }
                    
                    fileListDiv.appendChild(fileItem);
                });
            })
            .catch(e => {
                document.getElementById('fileList').innerHTML = '<p style="color: red;">Greška: ' + e.message + '</p>';
                document.getElementById('currentPath').textContent = path;
            });
    }

    // --- NEW: Rename Item ---
    function rename_item(oldPath, isDir) {
        var oldName = oldPath.split('/').pop();
        var newName = prompt("Unesite novo ime za '" + oldName + "':", oldName);

        if (newName === null || newName.trim() === '' || newName === oldName) {
            return; // Korisnik odustao ili nije promijenio ime
        }

        newName = newName.trim();
        // Jednostavna validacija imena
        if (/[\\/:*?"<>|]/.test(newName)) {
            alert('Ime sadrži nedozvoljene karaktere!');
            return;
        }

        var currentDir = document.getElementById('currentPath').textContent;
        if (currentDir === '/') currentDir = '';

        var newPath = currentDir + '/' + newName;

        fetch('/rename_item?old=' + encodeURIComponent(oldPath) + '&new=' + encodeURIComponent(newPath))
            .then(response => response.text())
            .then(text => {
                show_response(text, !text.includes("successfully"));
                load_file_list(document.getElementById('currentPath').textContent); // Osvježi prikaz
            })
            .catch(e => {
                alert('Greška: ' + e.message);
            });
    }

    // --- NEW: Create Folder ---
    function create_folder() {
        var folderName = document.getElementById('newFolderName').value.trim();
        if (!folderName) {
            alert('Unesite ime foldera!');
            return;
        }
        // Jednostavna validacija imena
        if (/[\\/:*?"<>|]/.test(folderName)) {
            alert('Ime foldera sadrži nedozvoljene karaktere!');
            return;
        }

        var currentPath = document.getElementById('currentPath').textContent;
        if (currentPath === '/') currentPath = '';
        var newFolderPath = currentPath + '/' + folderName;

        fetch('/create_folder?path=' + encodeURIComponent(newFolderPath))
            .then(response => response.text())
            .then(text => {
                show_response(text, false);
                load_file_list(document.getElementById('currentPath').textContent); // Osvježi prikaz
                document.getElementById('newFolderName').value = ''; // Očisti polje
            })
            .catch(e => alert('Greška: ' + e.message));
    }

    // --- NEW: Delete File ---
    function delete_file(filename) {
        if (!confirm('Obriši fajl: ' + filename + '?')) {
            return;
        }
        
        fetch('/delete_file?file=' + encodeURIComponent(filename))
            .then(response => response.text())
            .then(text => {
                show_response(text, false);
                // Osvježi trenutni direktorij
                load_file_list(document.getElementById('currentPath').textContent); 
            })
            .catch(e => {
                alert('Greška: ' + e.message);
            });
    }

    // Helper: Format bytes
    function formatBytes(bytes) {
        if (bytes === 0) return '0 B';
        var k = 1024;
        var sizes = ['B', 'KB', 'MB', 'GB'];
        var i = Math.floor(Math.log(bytes) / Math.log(k));
        return Math.round(bytes / Math.pow(k, i) * 100) / 100 + ' ' + sizes[i];
    }

    // --- Original Functions ---
    function get_log() {
        send_request("sysctrl.cgi?log=3");
    }
    function delete_log() {
        send_request("sysctrl.cgi?log=4");
    }
    function delete_log_list() {
        send_request("sysctrl.cgi?log=5");
    }
    function set_time() {
        var currentTime = new Date();
        var month = addZero(currentTime.getMonth() + 1);
        var weekday = (currentTime.getDay());
        if (weekday === 0) weekday = 7;
        var day = addZero(currentTime.getDate());
        var year = addZero(currentTime.getFullYear());
        var hours = addZero(currentTime.getHours());
        var minutes = addZero(currentTime.getMinutes());
        var sec = addZero(currentTime.getSeconds());
        var htt = "sysctrl.cgi?tdu=" + weekday + day + month + year + hours + minutes + sec;
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
            htt = "sysctrl.cgi?rst=0";
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
        else if (t == "503") {
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
        
        if (htt) {
            send_request(htt);
        }
    }
</SCRIPT>
)rawliteral";