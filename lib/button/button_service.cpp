// Copyright by Alex Koessler

// Provides service for short and long press of hardware button to reset or factory-reset the blaster.

#include <Arduino.h>
#include "button_service.h"

#include <libconfig.h>
#include <blaster_config.h>

#include <esp_log.h>

static const char * TAG = "button";

static const long minReset = 500;
static const long minFactoryReset = 5000;



void ButtonService::init()
{
    #if BLASTER_ENABLE_RESETBTN == true 
        pinMode(BLASTER_PIN_RESETBTN, INPUT);
        ESP_LOGD(TAG, "Reset button initialized.");
    #endif
}

//again not nice!
extern void setLedStateReset();
extern void setLedStateFactoryReset();

void ButtonService::loop()
{
    #if BLASTER_ENABLE_RESETBTN == true
        if(digitalRead(BLASTER_PIN_RESETBTN) == LOW)
        {
            cur_state = pressed;
        }
        else
        {
            cur_state = released;
        }

        if (last_state == released && cur_state == pressed)
        {
            //button recently pressed
            pressedTime = millis();
        }
        if (cur_state == pressed)
        {
            //check how lognew already have pressed the button
            long now = millis();
            long pressDuration = now - pressedTime;

            //check dureations of pressed button. order matters here!
            if(pressDuration >= minFactoryReset){
                //long enough to factory reset --> indicate on led
                setLedStateFactoryReset();
            } else if (pressDuration >= minReset){
                //long enough to reset --> indicat on led
                setLedStateReset();
            }

        }
        if (last_state == pressed && cur_state == released)
        {
            //button recently released
            releasedTime = millis();

            //eval pressed time
            long pressDuration = releasedTime - pressedTime;

            if (pressDuration < minReset){
                // button press was very short. ignore
                ESP_LOGD(TAG, "Button press ignored. Too short.");
            }else if(pressDuration < minFactoryReset) {
                // short button press detected - persorm reset
                ESP_LOGD(TAG, "Short button press detected - Reset Blaster.");
                ESP_LOGI(TAG, "Rebooting...");
                delay(500);
                ESP.restart();
            } else {
                // longshort button press detected - persorm factory reset
                setLedStateFactoryReset();
                ESP_LOGD(TAG, "Long button press detected - Factory-reset Blaster.");
                ESP_LOGI(TAG, "Resetting Configuration Storage ...");
                //Config::getInstance().reset();
                ESP_LOGI(TAG, "Rebooting ...");
                //delay for finish logging
                delay(500);
                //restart
                ESP.restart();
            }

        }
        last_state = cur_state;
    #endif
}
