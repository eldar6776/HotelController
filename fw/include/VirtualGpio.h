/**
 ******************************************************************************
 * @file    VirtualGpio.h
 * @author  Gemini & [Vase Ime]
 * @brief   Header fajl za VirtualGpio modul.
 *
 * @note
 * Upravlja stanjima za LED i Rasvjetu koji ce biti na I2C Expanderu.
 * Takodjer upravlja fizickim STATUS_LED_PIN-om.
 ******************************************************************************
 */

#ifndef VIRTUAL_GPIO_H
#define VIRTUAL_GPIO_H

#include <Arduino.h>
#include "ProjectConfig.h"

enum class LedState
{
    LED_OFF,
    LED_ON,
    LED_BLINK_SLOW,
    LED_BLINK_FAST
};

class VirtualGpio
{
public:
    VirtualGpio();
    void Initialize();
    void Loop(); // Za blinkanje LED-a

    // API za fizicku LED
    void SetStatusLed(LedState state);

    // API za virtuelne pinove (kasnije pozivaju I2C)
    void SetLightState(bool state);
    bool GetLightState();

private:
    LedState m_status_led_state;
    bool m_virtual_light_state;
    unsigned long m_last_blink_time;
};

#endif // VIRTUAL_GPIO_H
