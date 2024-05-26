// Copyright 2024 Craig Petchell
// Contributions by Alex Koessler

#include <Arduino.h>
#include "web_task.h"
#include "blaster_config.h"

#include <freertos/FreeRTOS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>

#include <esp_log.h>

#include <mdns_service.h>
#include <api_service.h>
#include <libconfig.h>

#include <moustache.h>

#include <WiFi.h>
#include <wifi_service.h>
#include <eth_service.h>
#include <fs_service.h>


#define SOCKET_DATA_SIZE 4096

char socketData[SOCKET_DATA_SIZE];
uint16_t currSocketBufferIndex = 0;

static const char *TAG = "webtask";

void notFound(AsyncWebServerRequest *request)
{
    ESP_LOGW(TAG, "404 for: %s", request->url().c_str());
    request->send(404, "text/plain", "Not found");
}

void debugWSMessage(const AwsFrameInfo *info, uint8_t *pay_data, size_t pay_len)
{
    ESP_LOGV(TAG, "Verbose debugging the received websocket message");
    ESP_LOGV(TAG, "  Frame.Final: %s", info->final ? "True" : "False");
    ESP_LOGV(TAG, "  Frame.Opcode: %u", info->opcode);
    ESP_LOGV(TAG, "  Frame.Masked: %s", info->masked ? "True" : "False");
    ESP_LOGV(TAG, "  Frame.Payload Length: %llu", info->len);
    ESP_LOGV(TAG, "  Info-Num: %u", info->num);
    ESP_LOGV(TAG, "  Info-Index: %llu", info->index);
    ESP_LOGV(TAG, "  Info-MsgOpcode: %u", info->message_opcode);
    ESP_LOGV(TAG, "  Length: %u", pay_len);
    ESP_LOGV(TAG, "  Data: %.*s", pay_len, pay_data);
}

String getHeap(){
    uint32_t freeheap = esp_get_free_heap_size();
    String heapstr = String(freeheap/1024) + " kbytes";
    return(heapstr);
}

String getReset(){
// typedef enum {
//     ESP_RST_UNKNOWN,    //!< Reset reason can not be determined
//     ESP_RST_POWERON,    //!< Reset due to power-on event
//     ESP_RST_EXT,        //!< Reset by external pin (not applicable for ESP32)
//     ESP_RST_SW,         //!< Software reset via esp_restart
//     ESP_RST_PANIC,      //!< Software reset due to exception/panic
//     ESP_RST_INT_WDT,    //!< Reset (software or hardware) due to interrupt watchdog
//     ESP_RST_TASK_WDT,   //!< Reset due to task watchdog
//     ESP_RST_WDT,        //!< Reset due to other watchdogs
//     ESP_RST_DEEPSLEEP,  //!< Reset after exiting deep sleep mode
//     ESP_RST_BROWNOUT,   //!< Brownout reset (software or hardware)
//     ESP_RST_SDIO,       //!< Reset over SDIO
// } esp_reset_reason_t;
    esp_reset_reason_t r = esp_reset_reason();
    String reason = "";
    switch (r)
    {
    case ESP_RST_UNKNOWN:
        reason = "Unknown";
        break;
    case ESP_RST_POWERON:
        reason = "PowerOn";
        break;
    case ESP_RST_EXT:
        reason = "External Reset";
        break;        
    case ESP_RST_SW:
        reason = "Software Reset";
        break;
    case ESP_RST_PANIC:
        reason = "Exception/Panic";
        break;
    case ESP_RST_INT_WDT:
        reason = "Interrupt Watchdog";
        break;
    case ESP_RST_TASK_WDT:
        reason = "Task Watchdog";
        break;
    case ESP_RST_WDT:
        reason = "Watchdog";
        break;
    case ESP_RST_BROWNOUT:
        reason = "Brownout";
        break;
    default:
        reason = "other";
        break;
    }
    return(reason);
}

String getUptime()
{
    uint32_t uptime_seconds = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;

    int16_t weeks=0;
    int16_t days=0;
    int16_t hours=0;
    int16_t mins=0;
    int16_t secs=0;

    mins = uptime_seconds / 60; //calc overall mins
    secs = uptime_seconds - mins * 60; //calc remaining secs not covered in mins
    hours = mins / 60; //calc overall hours
    mins = mins - hours * 60; //calc remaining mins not covered in hours
    days = hours / 24; //calc overall days
    hours = hours - days * 24; //calc remaining hours not covered in days
    weeks = days / 7; //calc overall weeks
    days = days - weeks * 7; //calc remaining days not covered in weeks

    String uptimeString = "";
    if (weeks > 0) {
        uptimeString += weeks;
        if(weeks == 1){
            uptimeString += "week ";
        } else {
            uptimeString += "weeks ";
        }
    }
    if (days > 0) {
        uptimeString += days;
        if(days == 1){
            uptimeString += "day ";
        } else {
            uptimeString += "days ";
        }
    } else {
        if(weeks > 0) {
            uptimeString += "0 days ";
        }
    }
    char timeString [9]; // "00:00:00" - max length=9 including \0
    sprintf(timeString, "%02d:%02d:%02d", hours, mins, secs);
    uptimeString += timeString;

    return uptimeString;
}

String getRSSI(){
   WifiService &wifiSrv = WifiService::getInstance();
   if(wifiSrv.isActive()){
       //return "blub";
       return wifiSrv.getSignalStrengthString();
   } else {
       return "Wifi not connected";
   }
}

String getIndex()
{
    String filecontent = SPIFFSService::getInstance().readFile("/index.html");

    if(filecontent.length() == 0){
        return("Error reading files from SPIFFS filesystem. Please make sure you have built and uploaded the filesystem to the dock correctly.<br/>Check https://github.com/itcorner/ESP32-IRBlaster-UCR2 for support.");
    }
    
    moustache_variable_t substitutions[] = {
      {"friendlyname", Config::getInstance().getFriendlyName()},
      {"hostname", Config::getInstance().getHostName()},
      {"version", Config::getInstance().getFWVersion()},
      {"serial", Config::getInstance().getSerial()},
      {"model", Config::getInstance().getDeviceModel()},
      {"revision", Config::getInstance().getHWRevision()},
      {"heap", getHeap()},
      {"uptime", getUptime()},
      {"resetreason", getReset()},
      {"ssid", Config::getInstance().getWifiSsid()},
      {"rssi", getRSSI()},
      {"ipv4", WiFi.localIP().toString()},
      {"gatewayv4", WiFi.gatewayIP().toString()},
      {"dnsv4", WiFi.dnsIP().toString()},
      {"eth_display", BLASTER_ENABLE_ETH?"block":"none"},
      {"eth_mac", EthService::getInstance().getMAC()},
      {"eth_speed", EthService::getInstance().getConnectionSpeed()},
      {"eth_ipv4", EthService::getInstance().getIP().toString()},
      {"eth_gatewayv4", EthService::getInstance().getGateway().toString()},
      {"eth_dnsv4", EthService::getInstance().getDNS().toString()},
    };
    auto result = moustache_render(filecontent, substitutions);
    return result;
}


void onWSEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type,
               void *arg,
               uint8_t *data,
               size_t len)
{
    JsonDocument input;
    JsonDocument output;
    switch (type)
    {
    case WS_EVT_CONNECT:
        ESP_LOGI(TAG, "WebSocket client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        client->keepAlivePeriod(1);
        api_buildConnectionResponse(input, output);
        break;
    case WS_EVT_DISCONNECT:
        ESP_LOGI(TAG, "WebSocket client #%u disconnected", client->id());
        break;
    case WS_EVT_DATA:
    {
        const AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);

        // enable for debugging output for WS messages
        debugWSMessage(info, data, len);

        // currently we are only expecting text messages
        if (info->opcode == WS_BINARY)
        {
            // receiving new message (first frame) reset buffer index
            ESP_LOGW(TAG, "Websocket received an unhandled binary message from %s.", client->remoteIP().toString().c_str());
        }
        if ((info->opcode == WS_TEXT) && (info->index == 0))
        {
            // receiving a new message (or first frame of a fragmented message). reset buffer index.
            currSocketBufferIndex = 0;
        }
        if ((info->opcode == WS_TEXT) || (info->opcode == WS_CONTINUATION))
        {
            // handle data
            // HINT: len holds the current frame payload lenght. info->len holds the overall payload length of all fragments
            if (info->len > SOCKET_DATA_SIZE)
            {
                // TODO: check about feasible max data size we expect.
                // Any IR code longer than MAX_IR_TEXT_CODE_LENGTH (2048) will not be queyed anyways
                ESP_LOGE(TAG, "Raw JSON message too big for buffer. Not processing.");
                // TODO: what will be returned in this case? Currently this will lead to a timeout.
            }
            else
            {
                for (int i = 0; i < len; i++)
                {
                    // copy data of each chunk into buffer
                    socketData[currSocketBufferIndex] = data[i];
                    currSocketBufferIndex++;
                }

                // check if this is the end of the message
                // unfortunately we cannot trust final flag in header and need to check the overall length
                // if (info->final)
                if (currSocketBufferIndex == info->len)
                {
                    ESP_LOGD(TAG, "Raw JSON Message: %.*s", currSocketBufferIndex, socketData);
                    // deserialize data after last chunk
                    DeserializationError err = deserializeJson(input, socketData, currSocketBufferIndex);
                    if (err)
                    {
                        ESP_LOGE(TAG, "deserializeJson() failed with code %s", err.f_str());
                    }
                }
                else
                {
                    ESP_LOGI(TAG, "Received non-final WS frame. Size of current buffer content: %u/%llu bytes.", currSocketBufferIndex, info->len);
                }
            }
        }
        if (!input.isNull())
        {
            api_processData(input, output, client);
        }
        else
        {
            if (currSocketBufferIndex == info->len)
            {
                ESP_LOGE(TAG, "WebSocket received no JSON document. ");
                ESP_LOGD(TAG, "Raw Message of length %d received: %.*s", len, len, data);
            }
        }
    }
    case WS_EVT_PONG:
        ESP_LOGD(TAG, "WebSocket Event PONG");
        break;
    case WS_EVT_ERROR:
        ESP_LOGE(TAG, "WebSocket client #%u error #%u: %s", client->id(), *(static_cast<uint16_t *>(arg)), static_cast<unsigned char *>(data));
        break;
    }
    if (!output.isNull())
    {
        // send document back

        api_sendJsonReply(output, client);

        // check if we have to close the ws connection (failed auth)
        String responseMsg = output["msg"].as<String>();
        int responseCode = output["code"].as<int>();
        if ((responseMsg == "authentication") && (responseCode == 401))
        {
            // client ->close();
        }

        // check if we have to reboot
        if (output["reboot"].as<boolean>())
        {
            ESP_LOGI(TAG, "Rebooting...");
            delay(500);
            ESP.restart();
        }
    }
}

void TaskWeb(void *pvParameters)
{
    ESP_LOGD(TAG, "TaskWeb running on core %d", xPortGetCoreID());

    AsyncWebServer server(Config::getInstance().API_port);
    AsyncWebSocket ws("/");

    AsyncWebServer httpserver(80);

    httpserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->redirect("/index.html");
    });
    httpserver.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
        String response = getIndex();
        request->send(200, "text/html", response);
    });
    httpserver.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/favicon.ico");
    });
    httpserver.on("/favicon-16x16.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/favicon-16x16.png");
    });
    httpserver.on("/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/favicon-32x32.png");
    });
    httpserver.on("/apple-touch-icon.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/apple-touch-icon.png");
    });
    httpserver.on("/koeblaster-192.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/koeblaster-192.png");
    });
    httpserver.on("/koeblaster-512.png", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/koeblaster-512.png");
    });
    httpserver.on("/site.webmanifest", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(SPIFFS, "/site.webmanifest", "application/manifest+json");
    });

    // start websocket server.
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    server.onNotFound(notFound);
    httpserver.onNotFound(notFound);

    server.begin();
    httpserver.begin();

    MDNSService::getInstance().startService();

    for (;;)
    {
        MDNSService::getInstance().loop();

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
