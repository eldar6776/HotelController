/**
 ******************************************************************************
 * @file    VirtualGpio.cpp
 * @author  Gemini & [Vase Ime]
 * @brief   Implementacija VirtualGpio modula.
 ******************************************************************************
 */

#include "VirtualGpio.h"

VirtualGpio::VirtualGpio() :
    m_status_led_state(LedState::LED_OFF),
    m_virtual_light_state(false),
    m_last_blink_time(0)
{
    // Konstruktor
}

void VirtualGpio::Initialize()
{
    Serial.println(F("[VirtualGpio] Inicijalizacija..."));
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW); // Ugasen po defaultu
}

void VirtualGpio::Loop()
{
    // Ovdje implementiramo logiku za blinkanje fizicke LED
    unsigned long now = millis();
    bool led_on = false;
    
    switch (m_status_led_state)
    {
    case LedState::LED_ON:
        led_on = true;
        break;
    case LedState::LED_BLINK_SLOW:
        if (now - m_last_blink_time > 1000)
        {
            m_last_blink_time = now;
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        }
        return; // Preskoci finalni digitalWrite
    case LedState::LED_BLINK_FAST:
        if (now - m_last_blink_time > 200)
        {
            m_last_blink_time = now;
            digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
        }
        return; // Preskoci finalni digitalWrite
    case LedState::LED_OFF:
    default:
        led_on = false;
        break;
    }
    digitalWrite(STATUS_LED_PIN, led_on ? HIGH : LOW);
}

void VirtualGpio::SetStatusLed(LedState state)
{
    m_status_led_state = state;
}

void VirtualGpio::SetLightState(bool state)
{
    if (m_virtual_light_state == state) return;
    
    m_virtual_light_state = state;
    Serial.printf("[VirtualGpio] Postavljam VIRTUAL_LIGHT_PIN na: %s\n", state ? "ON" : "OFF");
    
    // TODO: U buducnosti, ovdje pozivamo I2C Expander
    // g_i2cExpander->WritePin(VIRTUAL_LIGHT_PIN, state);
}

bool VirtualGpio::GetLightState()
{
    return m_virtual_light_state;
}
