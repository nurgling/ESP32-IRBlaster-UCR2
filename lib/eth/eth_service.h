// Copyright by Alex Koessler

// Provides ETH Service that connects the Dock to a wired ethernet network.

#ifndef ETH_SERVICE_H
#define ETH_SERVICE_H

#include <Arduino.h>

#include <libconfig.h>



class EthService
{
public:
    static EthService &getInstance()
    {
        static EthService instance;
        return instance;
    }

    void connect();
    void loop();
    boolean isActive();
    void updateMillis() {previousMillis = millis();}

private:
    explicit EthService() {}
    virtual ~EthService() {}
    char hostname[50] = {0};

    

    boolean reconnect = false;
    unsigned long previousMillis = 0;
};

#endif