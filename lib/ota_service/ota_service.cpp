// Copyright by Alex Koessler

// Provides OTA Service that flashes the firmware.

#include <ArduinoOTA.h>

#include "ota_service.h"
#include <esp_log.h>
#include <SPIFFS.h>
#include <libconfig.h>

static const char * TAG = "OTA";

static int nextProgressOut=0;
static const int PROGRESSSTEPS=10;


void OTAService::loop() {
    ArduinoOTA.handle();
}  


void OTAService::startService(){
    //Populate ESP Hostname also to OTA Stack
    ArduinoOTA.setHostname(Config::getInstance().getHostName().c_str());
    //mDNS is handled in a seperate service
    ArduinoOTA.setMdnsEnabled(false);

    //setup callbacks
    ArduinoOTA
        .onStart([]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
            {
                type = "firmware";
            }
            else  // U_SPIFFS
            {
                type = "filesystem";
                SPIFFS.end(); //unmounting SPIFFS FS
            }
            ESP_LOGI(TAG, "Start updating %s", type);
            nextProgressOut = 0;
        })
        .onEnd([]() {
            ESP_LOGI(TAG, "Updating done");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            unsigned int p = (progress / (total / 100));
            if(p >= nextProgressOut){
                ESP_LOGD(TAG, "Updating Progress: %u%%", (progress / (total / 100)));
                nextProgressOut += PROGRESSSTEPS;
            }
        })
        .onError([](ota_error_t error) {
            ESP_LOGE(TAG, "OTA Error %u", error);
            if (error == OTA_AUTH_ERROR) ESP_LOGD(TAG, "Auth Failed");
            else if (error == OTA_BEGIN_ERROR) ESP_LOGD(TAG, "Begin Failed");
            else if (error == OTA_CONNECT_ERROR) ESP_LOGD(TAG, "Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) ESP_LOGD(TAG, "Receive Failed");
            else if (error == OTA_END_ERROR) ESP_LOGD(TAG, "End Failed");
        });
    ArduinoOTA.begin();
}