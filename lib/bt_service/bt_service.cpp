// Copyright by Alex Koessler

// This file was created using information and code from:
// https://github.com/YIO-Remote/dock-software
// https://github.com/espressif/arduino-esp32/tree/master/libraries/BluetoothSerial

// Provides bluetooth service for initial configuration of dock without Wifi.

#include "bt_service.h"

#include <api_service.h>
#include <libconfig.h>

#include <esp_log.h>

static const char * TAG = "bt";

BluetoothService *BluetoothService::s_instance = nullptr;

BluetoothService::BluetoothService()
{
  s_instance = this;
}

void btConnectCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
  if (event == ESP_SPP_SRV_OPEN_EVT)
  {
    ESP_LOGI(TAG, "Bluetooth client connected");
  }
  if (event == ESP_SPP_CLOSE_EVT)
  {
    ESP_LOGI(TAG, "Bluetooth client disconnected");
  }
}

void BluetoothService::sendCallback(JsonDocument responseJson)
{
  String outString;
  serializeJson(responseJson, outString);
  btSerial->println(outString);
  ESP_LOGD(TAG, "Raw Json response: %s", outString.c_str());
}


void BluetoothService::init()
{
  btSerial->register_callback(btConnectCallback);

  if (btSerial->begin(Config::getInstance().getHostName()))
  {
    ESP_LOGI(TAG, "BT initialized with Hostname %s", Config::getInstance().getHostName().c_str());
  }
  else
  {
    ESP_LOGE(TAG, "BT initialization failed!");
  }
}

void BluetoothService::handle()
{
  char incomingChar = btSerial->read();
  // int charAsciiNumber = incomingChar + 0;

  if (String(incomingChar) == "{")
  {
    m_interestingData = true;
    m_receivedData = "";
  }
  else if (String(incomingChar) == "}")
  {
    m_interestingData = false;
    m_receivedData += "}";

    JsonDocument requestJson;
    JsonDocument responseJson;
    DeserializationError error = deserializeJson(requestJson, m_receivedData);

    if (error)
    {
      ESP_LOGE(TAG, "deserializeJson() failed: %s", error.c_str());
    }
    else
    {
      ESP_LOGD(TAG, "Received Json: %s", m_receivedData.c_str());

      api_processData(requestJson, responseJson);

      // if there is anyting that we need to send back
      if (!responseJson.isNull())
      {
        // send document back via callback
        sendCallback(responseJson);

        // check if reboot is required
        if (responseJson["reboot"].as<boolean>())
        {
          ESP_LOGI(TAG, "Rebooting...");
          delay(1000);
          ESP.restart();
        }
      }
    }
    m_receivedData = "";
  }
  if (m_interestingData)
  {
    m_receivedData += String(incomingChar);
  }
  vTaskDelay(10 / portTICK_PERIOD_MS);
}
