// Copyright by Alex Koessler

// Provides OTA Service that flashes the firmware.


#ifndef OTA_SERVICE_H
#define OTA_SERVICE_H

#include <Arduino.h>


class OTAService
{
public:

    static OTAService& getInstance()
    {
        static OTAService instance;
        return instance;
    }

    void startService();

    // Worker loop
    void loop();

private:
    explicit OTAService() {}
    virtual ~OTAService() {}

//    boolean m_forceRestart = false;
//    String m_mdnsHostname;

};

#endif
