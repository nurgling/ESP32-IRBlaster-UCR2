// Copyright 2024 Craig Petchell
// Contributions by Alex Koessler

#include <freertos/FreeRTOS.h>
#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include <mdns_service.h>
#include <api_service.h>
#include <libconfig.h>

#include "web_task.h"
#include "blaster_config.h"

#define SOCKET_DATA_SIZE 4096

char socketData[SOCKET_DATA_SIZE];
int currSocketBufferIndex=0;

void notFound(AsyncWebServerRequest *request)
{
    Serial.printf("404 for: %s\n", request->url());
    request->send(404, "text/plain", "Not found");
}

void debugWSMessage(AwsFrameInfo *info, uint8_t *pay_data, size_t pay_len)
{
    Serial.println("Debugging received websocket message");
    Serial.printf("  Frame.Final: %s\n", info->final?"True":"False");
    Serial.printf("  Frame.Opcode: %u\n", info->opcode);
    Serial.printf("  Frame.Masked: %s\n", info->masked?"True":"False");
    Serial.printf("  Frame.Payload Length: %u\n", info->len);
    Serial.printf("  Info-Num: %u\n", info->num);
    Serial.printf("  Info-Index: %u\n", info->index);
    Serial.printf("  Info-MsgOpcode: %u\n", info->message_opcode);    
    Serial.printf("  Length: %u\n", pay_len);
    Serial.printf("  Data: %.*s\n\n", pay_len, pay_data);
    Serial.flush();
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
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        client->keepAlivePeriod(1);
        api_buildConnectionResponse(input, output);
        break;
    case WS_EVT_DISCONNECT:
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
        break;
    case WS_EVT_DATA:
    {
        const AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);

        //enable for debugging output for WS messages
        //debugWSMessage(info, data, len);

        // currently we are only expecting text messages
        if (info->opcode == WS_BINARY )
        {
            // receiving new message (first frame) reset buffer index
            Serial.printf("Websocket received an unhandled binary message from %s.\n", client->remoteIP().toString().c_str());
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
                Serial.printf("Raw JSON message too big for buffer. Not processing.\n");
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
                //if (info->final) 
                if(currSocketBufferIndex == info->len)
                {
                    Serial.printf("Raw JSON Message: %.*s\n", currSocketBufferIndex, socketData);
                    // deserialize data after last chunk
                    DeserializationError err = deserializeJson(input, socketData, currSocketBufferIndex);
                    if (err)
                    {
                        Serial.print(F("deserializeJson() failed with code "));
                        Serial.println(err.f_str());
                    }
                }
                else
                {
                    Serial.printf("Received non-final WS frame. Current buffer content: %.*s\n", currSocketBufferIndex, socketData);
                }
            }
        }
        if (!input.isNull())
        {
            api_processData(input, output);
        }
        else
        {
            if (currSocketBufferIndex == info->len)
            {
                Serial.print("WebSocket received no JSON document. ");
                Serial.printf("Raw Message of length %d received: %.*s\n", len, len, data);
            }
        }
    }
    case WS_EVT_PONG:
        Serial.print("WebSocket Event PONG\n");
        break;
    case WS_EVT_ERROR:
        Serial.printf("WebSocket client #%u error #%u: %s\n", client->id(), *(static_cast<uint16_t *>(arg)), static_cast<unsigned char *>(data));
        break;
    }
    if (!output.isNull())
    {
        // send document back
        size_t out_len = measureJson(output);
        AsyncWebSocketMessageBuffer *buf = server->makeBuffer(out_len);
        serializeJson(output, buf->get(), out_len);
        Serial.printf("Raw JSON response %.*s\n", out_len, buf->get());
        client->text(buf);

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
            Serial.println(F("Rebooting..."));
            delay(500);
            ESP.restart();
        }
    }
}

void TaskWeb(void *pvParameters)
{
    Serial.printf("TaskWeb running on core %d\n", xPortGetCoreID());

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
