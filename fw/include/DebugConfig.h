/**
 ******************************************************************************
 * @file    DebugConfig.h
 * @author  Gemini
 * @brief   Centralizovana konfiguracija za debagovanje.
 ******************************************************************************
 */

#ifndef DEBUG_CONFIG_H
#define DEBUG_CONFIG_H

// Nivoi debagovanja:
// 0: Bez poruka
// 1: Samo greške (ERROR)
// 2: Greške i upozorenja (WARN)
// 3: Greške, upozorenja i važne informacije (INFO)
// 4: Detaljne poruke (VERBOSE)
// 5: Ulaz/Izlaz iz funkcija (TRACE)
#define DEBUG_LEVEL 3 // Opšti nivo debagovanja

#define LOG_DEBUG(level, format, ...) do { if (DEBUG_LEVEL >= level) Serial.printf(format, ##__VA_ARGS__); } while (0)

#endif // DEBUG_CONFIG_H