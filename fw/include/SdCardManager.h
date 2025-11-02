/**
 ******************************************************************************
 * @file    SdCardManager.h
 * @author  Code Review Assistant & [Vase Ime]
 * @brief   Header fajl za SdCardManager modul.
 *
 * @note
 * Upravlja uSD karticom preko SPI interfejsa (FAT32 FS).
 * Zamenjuje SpiFlashStorage modul prema Planu Rada.
 ******************************************************************************
 */

#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "ProjectConfig.h"

class SdCardManager
{
public:
    SdCardManager();
    
    /**
     * @brief Inicijalizuje SPI i montira uSD karticu.
     * @param sck_pin  SPI Clock pin
     * @param miso_pin SPI MISO pin
     * @param mosi_pin SPI MOSI pin
     * @param cs_pin   Chip Select pin
     * @return true ako je montiranje uspješno
     */
    bool Initialize(int8_t sck_pin, int8_t miso_pin, int8_t mosi_pin, int8_t cs_pin);

    /**
     * @brief Provjera da li je kartica montirana i dostupna.
     * @return true ako je kartica montirana
     */
    bool IsCardMounted();

    /**
     * @brief Čita cijeli tekstualni fajl kao String.
     * @param path Putanja do fajla (npr. "/CTRL_ADD.TXT")
     * @return Sadržaj fajla ili prazan string ako fajl ne postoji
     */
    String ReadTextFile(const char* path);

    /**
     * @brief Otvara fajl za čitanje ili pisanje.
     * @param path Putanja do fajla
     * @param mode Režim: FILE_READ ili FILE_WRITE
     * @return File objekat (provjeri sa if(file) da li je validan)
     */
    File OpenFile(const char* path, const char* mode = "r");

    /**
     * @brief Vraća listu fajlova u direktorijumu (JSON format).
     * @param path Putanja do direktorijuma (npr. "/")
     * @return JSON string sa listom fajlova i njihovim veličinama
     */
    String ListFiles(const char* path);

    /**
     * @brief Briše fajl sa uSD kartice.
     * @param path Putanja do fajla
     * @return true ako je brisanje uspješno
     */
    bool DeleteFile(const char* path);

    /**
     * @brief Kreira novi fajl za pisanje (briše postojeći ako postoji).
     * @param path Putanja do fajla
     * @return File objekat za pisanje
     */
    File CreateFile(const char* path);

    /**
     * @brief Provjerava da li fajl postoji.
     * @param path Putanja do fajla
     * @return true ako fajl postoji
     */
    bool FileExists(const char* path);

    /**
     * @brief Vraća veličinu fajla u bajtovima.
     * @param path Putanja do fajla
     * @return Veličina fajla ili 0 ako fajl ne postoji
     */
    size_t GetFileSize(const char* path);

private:
    bool m_card_mounted;
    int8_t m_cs_pin;

    /**
     * @brief Pomoćna funkcija za rekurzivno listanje direktorijuma.
     */
    void ListDirectory(File dir, String& output, int numTabs = 0);
};

#endif // SD_CARD_MANAGER_H