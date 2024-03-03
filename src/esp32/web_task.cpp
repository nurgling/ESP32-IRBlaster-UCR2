// Copyright 2024 Craig Petchell
// Contributions by Alex Koessler

#include <Arduino.h>
#include "web_task.h"
#include "blaster_config.h"

#include <freertos/FreeRTOS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <esp_log.h>

#include <mdns_service.h>
#include <api_service.h>
#include <libconfig.h>


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

    // start websocket server.
    ws.onEvent(onWSEvent);
    server.addHandler(&ws);
    server.onNotFound(notFound);

    server.begin();

    MDNSService::getInstance().startService();

    for (;;)
    {
        MDNSService::getInstance().loop();

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
