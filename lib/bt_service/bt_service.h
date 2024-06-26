// Copyright by Alex Koessler

// This file was created using information and code from:
// https://github.com/YIO-Remote/dock-software

// Provides bluetooth service for initial configuration of dock without Wifi.

#ifndef BT_SERVICE_H
#define BT_SERVICE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "BluetoothSerial.h"


class BluetoothService
{
public:
    explicit BluetoothService();
    virtual ~BluetoothService() {}

    static BluetoothService *getInstance() { return s_instance; }

    void init();
    void handle();

private:
    static BluetoothService *s_instance;

    BluetoothSerial *btSerial = new BluetoothSerial();

    void sendCallback(JsonDocument responseJson);

    String m_receivedData = "";
    bool m_interestingData = false;
};

#endif