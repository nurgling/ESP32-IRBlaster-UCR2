// Copyright by Alex Koessler

// Provides WiFi Service that connects the Dock to a Wifi network and handles reconnections if needed.

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include <libconfig.h>

#define MAX_WIFI_CONNECTION_TRIES 3

class WifiService
{
public:
    static WifiService &getInstance()
    {
        static WifiService instance;
        return instance;
    }

    void connect();
    void disconnect();
    void loop();
    boolean isActive();
    boolean isConnected();
    void updateMillis() {previousMillis = millis();}
    String getSSID() {return(WiFi.isConnected() ? Config::getInstance().getWifiSsid() : "");}
    int getSignalStrength();
    String getSignalStrengthString();

private:
    explicit WifiService() {}
    virtual ~WifiService() {}
    char wifihostname[50] = {0};

    boolean reconnect = false;
    unsigned long previousMillis = 0;
};

#endif