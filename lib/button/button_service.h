// Copyright by Alex Koessler

// Provides service for short and long press of hardware button to reset or factory-reset the blaster.

#ifndef BUTTON_SERVICE_H
#define BuTTON_SERVICE_H

#include <Arduino.h>

enum ButtonState{
    pressed,
    released,
};

class ButtonService
{
public:

    static ButtonService& getInstance()
    {
        static ButtonService instance;
        return instance;
    }

    void init();

    // Worker loop
    void loop();

private:
    explicit ButtonService() {}
    virtual ~ButtonService() {}

    bool pininit_done=false;
    ButtonState cur_state = released;
    ButtonState last_state = released;
    

    unsigned long pressedTime  = 0;
    unsigned long releasedTime = 0;


};


#endif