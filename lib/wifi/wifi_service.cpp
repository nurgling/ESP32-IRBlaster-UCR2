// Copyright by Alex Koessler


// Provides WiFi Service that connects the Dock to a Wifi network and handles reconnections if needed.

#include <Arduino.h>
#include "wifi_service.h"

#include <mdns_service.h>

#include <esp_log.h>

static const char * TAG = "wifi";

const unsigned long thirtySecs = 30 * 1000UL;

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
    ESP_LOGD(TAG, "Wifi connected successfully.");
    WifiService::getInstance().updateMillis();
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
    ESP_LOGI(TAG, "IP Address: %s", WiFi.localIP().toString().c_str());
    MDNSService::getInstance().restartService();
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
    ESP_LOGW(TAG, "Disconnected from WiFi access point");
    ESP_LOGD(TAG, "WiFi lost connection. Reason: %d", info.wifi_sta_disconnected.reason);
    WifiService::getInstance().updateMillis();
}

void WifiService::loop(){
    unsigned long currentMillis = millis();
    //check if wifi is active
    if(isActive()){
        //if wifi is down, reconnect after 30secs
        if((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= thirtySecs)){
            ESP_LOGD(TAG, "Wifi reconnection attempt ...");
            WiFi.disconnect();
            WiFi.reconnect();
            previousMillis = currentMillis;
            if(WiFi.status() != WL_CONNECTED){
                ESP_LOGW(TAG, "Reconnection failed. Trying again in 30 seconds.");
            }else{
                ESP_LOGI(TAG, "Reconnection successful.");
            }
        }
    }
}


void WifiService::disconnect(){
    ESP_LOGD(TAG, "Gotten request to stop Wifi radio.");
    if(WiFi.isConnected()){
        WiFi.disconnect();
    }
    WiFi.mode(WIFI_OFF);
    ESP_LOGI(TAG, "Wifi radio is turned off.");
}


void WifiService::connect(){

   Config &config = Config::getInstance();

    if (config.getWifiSsid() != "")
    {
        ESP_LOGD(TAG, "SSID present in config.");
        WiFi.enableSTA(true);
        WiFi.mode(WIFI_STA);
        WiFi.setSleep(false);
        ESP_LOGI(TAG, "Setting Wifi Hostname: %s", config.getHostName().c_str());
        strcpy(wifihostname, config.getHostName().c_str());
        WiFi.setHostname(wifihostname);

        WiFi.onEvent(WiFiStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
        WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
        WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

        ESP_LOGI(TAG, "Connecting to Wifi");
        for(int i = 0; i < MAX_WIFI_CONNECTION_TRIES; ++i){
            ESP_LOGD(TAG, "Connection attempt %d/%d ...", i+1, MAX_WIFI_CONNECTION_TRIES);
            WiFi.begin(config.getWifiSsid().c_str(), config.getWifiPassword().c_str());
            // try to connect to wifi for 10 secs
            if (WiFi.waitForConnectResult(10000) == WL_CONNECTED)
            {
                //we are connected.
                break;
            }
        }
        if(!WiFi.isConnected())
        {
            ESP_LOGE(TAG, "Starting WiFi Failed! Falling back to Bluetooth discovery.");
            // turn off wifi! nasty aborts will happen when accessing bluetooth if you don't.
            WiFi.mode(WIFI_OFF);
            delay(500);
        }
    }
    else
    {
        ESP_LOGI(TAG, "Booting without Wifi. SSID not found in config.");
    }
}

