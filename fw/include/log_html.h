/**
 ******************************************************************************
 * @file    log_html.h
 * @author  Code Review Assistant & [Vase Ime]
 * @brief   SSI Template za HTTP odgovore (V1 kompatibilnost).
 *
 * @note
 * FORMAT ODGOVORA: $MESSAGE$ (sa dollar znakovima)
 * Kompatibilno sa starim sistemom.
 ******************************************************************************
 */

#pragma once

#include <pgmspace.h>

// HTML template sa SSI tagom
const char LOG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=utf-8">
    <title>Hotel Controller Response</title>
    <style>
        body { 
            font-family: 'Courier New', monospace; 
            background: #1a1a1a; 
            color: #00ff00; 
            padding: 20px;
            margin: 0;
        }
        .response { 
            background: #000; 
            border: 2px solid #00ff00; 
            padding: 15px; 
            border-radius: 5px;
            white-space: pre-wrap;
            word-wrap: break-word;
            font-size: 1.2em;
            font-weight: bold;
        }
        .timestamp {
            color: #888;
            font-size: 0.8em;
            margin-bottom: 10px;
        }
    </style>
</head>
<body>
    <div class="timestamp">Response Time: <span id="time"></span></div>
    <div class="response">$<!--#t-->$</div>
    <script>
        document.getElementById('time').textContent = new Date().toLocaleString('sr-RS');
    </script>
</body>
</html>
)rawliteral";