// Copyright by Alex Koessler

// Provides API decoding service for communication with dock via bluetooth and wifi.

#include <Arduino.h>
#include "api_service.h"

#include <ir_service.h>

#include <eth_service.h>
#include <wifi_service.h>

#include <mdns_service.h>
#include <libconfig.h>

static const char *TAG = "apiservice";

void api_fillTypeIDCommand(JsonDocument &input, JsonDocument &output)
{
    output["type"] = input["type"];
    if (input.containsKey("id"))
    {
        output["req_id"] = input["id"];
    }
    if (input.containsKey("command"))
    {
        output["msg"] = input["command"];
    }
}

void api_fillDefaultResponseFields(JsonDocument &input, JsonDocument &output, int code, boolean reboot)
{
    api_fillTypeIDCommand(input, output);
    output["code"] = code;
    output["reboot"] = reboot;
}

void api_buildConnectionResponse(JsonDocument &input, JsonDocument &output)
{
    output["type"] = "auth_required";
    output["model"] = Config::getInstance().getDeviceModel();
    output["revision"] = Config::getInstance().getHWRevision();
    output["version"] = Config::getInstance().getFWVersion();
}

void api_buildSysinfoResponse(JsonDocument &input, JsonDocument &output)
{
    output["name"] = Config::getInstance().getFriendlyName();
    output["hostname"] = Config::getInstance().getHostName();
    output["model"] = Config::getInstance().getDeviceModel();
    output["revision"] = Config::getInstance().getHWRevision();
    output["version"] = Config::getInstance().getFWVersion();
    output["serial"] = Config::getInstance().getSerial();
    output["ir_learning"] = Config::getInstance().getIRLearning();
    output["ethernet"] = EthService::getInstance().isActive();
    output["wifi"] = WifiService::getInstance().isActive();
    if(WifiService::getInstance().isActive()){
        output["ssid"] = WifiService::getInstance().getSSID();
    }
    output["led_brightness"] = Config::getInstance().getLedBrightness();
    output["eth_led_brightness"] = Config::getInstance().getEthBrightness();
}

void processSetConfig(JsonDocument &input, JsonDocument &output)
{
    if (input.containsKey("token"))
    {
        // not sure about the effects of setting a token
        String token = input["token"].as<String>();
        if (strlen(token.c_str()) >= 4 && strlen(token.c_str()) <= 40)
        {
            Config::getInstance().setToken(token);
        }
        else
        {
            // in case of token error we do not want to continue
            output["code"] = 400;
            return;
        }
    }

    if (input.containsKey("ssid") && input.containsKey("wifi_password"))
    {
        ESP_LOGI(TAG, "Received new WIFI config");

        String ssid = input["ssid"].as<String>();
        String pass = input["wifi_password"].as<String>();

        Config::getInstance().setWifiSsid(ssid);
        Config::getInstance().setWifiPassword(pass);
        ESP_LOGD(TAG, "Saved new WIFI config. SSID: %s PASS: %s", ssid.c_str(), pass.c_str());
        output["reboot"] = true;
    }

    if (input.containsKey("friendly_name"))
    {
        String friendlyname = input["friendly_name"].as<String>();
        if (friendlyname != Config::getInstance().getFriendlyName())
        {
            // friendlyname has changed. update conf
            ESP_LOGD(TAG, "Updating FriendlyName to %s", friendlyname.c_str());
            Config::getInstance().setFriendlyName(friendlyname);
            // inform other subsystems about the change (e.g., mdns, ...)
            MDNSService::getInstance().restartService();
        }
    }
}

void api_replyWithError(JsonDocument &request, JsonDocument &response, int errorCode, String errorMsg)
{
    api_fillDefaultResponseFields(request, response, errorCode);

    if (errorMsg != "")
    {
        response["error"] = errorMsg;
    }
}

void processPingMessage(JsonDocument &request, JsonDocument &response)
{
    ESP_LOGD(TAG, "Received Ping message type");

    response["type"] = request["type"];
    response["msg"] = "pong";
}

void processAuthMessage(JsonDocument &request, JsonDocument &response)
{
    ESP_LOGD(TAG, "Received auth message type");

    response["type"] = request["type"];
    response["msg"] = "authentication";

    // check if the auth token matches our expected one
    String token = request["token"].as<String>();
    if (token == Config::getInstance().getToken())
    {
        // auth successful
        ESP_LOGD(TAG, "Authentification successful");
        response["code"] = 200;
    }
    else
    {
        // auth failed - problem when trying to bind a previously configured dock with non-standard password!
        ESP_LOGW(TAG, "Authentification failed");
        // response["code"] = 401;
        response["code"] = 200;
    }
}

void api_sendJsonReply(JsonDocument &content, AsyncWebSocketClient *wsClient)
{
    // TODO: check if wsClient is connected and ready to send
    if (wsClient != NULL)
    {
        size_t out_len = measureJson(content);
        AsyncWebSocketMessageBuffer *buf = wsClient->server()->makeBuffer(out_len);
        serializeJson(content, buf->get(), out_len);
        ESP_LOGD(TAG, "Raw JSON response %.*s\n", out_len, buf->get());
        wsClient->text(buf);
    }
}

void api_buildIRCodeEvent(JsonDocument &event, String irCode)
{
    event["type"] = "event";
    event["msg"] = "ir_receive";
    event["ir_code"] = irCode;
}

void processIROnMessage(JsonDocument &request, JsonDocument &response, AsyncWebSocketClient *wsClient)
{
    ESP_LOGD(TAG, "Received learn IR on message");

    // check if we are capable of learning IR codes
    if (!Config::getInstance().getIRLearning())
    {
        api_replyWithError(request, response, 503, "IR learning not supported by dock.");
        ESP_LOGW(TAG, "IR learning not supported by dock.");
        return;
    }
    if (wsClient == NULL)
    {
        api_replyWithError(request, response, 503, "IR learning only supported via Websocket connection.");
        ESP_LOGW(TAG, "IR learning only supported via Websocket connection.");
        return;
    }
    api_fillDefaultResponseFields(request, response);
    learnIRStart(request, response, wsClient);
}

void processIROffMessage(JsonDocument &request, JsonDocument &response)
{
    ESP_LOGD(TAG, "Received learn IR off message");

    // check if we are capable of learning IR codes
    if (!Config::getInstance().getIRLearning())
    {
        api_replyWithError(request, response, 503, "IR learning not supported by dock.");
        ESP_LOGW(TAG, "IR learning not supported by dock.");
        return;
    }
    api_fillDefaultResponseFields(request, response);
    learnIRStop(request, response);
}


void processSetBrightness(JsonDocument &request, JsonDocument &response)
{
    if (request.containsKey("status_led"))
    {
        int brightness = request["status_led"].as<int>();
        Config::getInstance().setLedBrightness(brightness);
    }
    if (request.containsKey("eth_led"))
    {
        int brightness = request["eth_led"].as<int>();
        Config::getInstance().setEthBrightness(brightness);
    }
}


// TODO: implement a nicer solution later than crossreferencing a function
extern void setLedStateIdentify();
extern void setLedStateLearn();
extern void setLedStateNormal();



void processDockMessage(JsonDocument &request, JsonDocument &response, AsyncWebSocketClient *wsClient)
{
    String command;

    if (request.containsKey("msg"))
    {
        command = request["msg"].as<String>();
        if (command == "ping")
        {
            // we got a ping message
            processPingMessage(request, response);
            return;
        }
    }

    if (!request.containsKey("command"))
    {
        ESP_LOGE(TAG, "Missing command field in dock message");
        api_replyWithError(request, response, 400, "Missing command field");
        return;
    }
    command = request["command"].as<String>();
    ESP_LOGD(TAG, "Received dock message with command %s", command.c_str());

    if (command == "get_sysinfo")
    {
        api_fillDefaultResponseFields(request, response);
        api_buildSysinfoResponse(request, response);
    }
    else if (command == "identify")
    {
        // blink some leds
        api_fillDefaultResponseFields(request, response);
        setLedStateIdentify();
    }
    else if (command == "set_config")
    {
        api_fillDefaultResponseFields(request, response);
        processSetConfig(request, response);
    }
    else if (command == "ir_send")
    {
        api_fillDefaultResponseFields(request, response);
        queueIR(request, response);
    }
    else if (command == "ir_stop")
    {
        api_fillDefaultResponseFields(request, response);
        stopIR(request, response);
    }
    else if (command == "ir_receive_on")
    {
        processIROnMessage(request, response, wsClient);
    }
    else if (command == "ir_receive_off")
    {
        processIROffMessage(request, response);
    }
    else if (command == "remote_charged")
    {
        // DO NOTHING BUT REPLY (for now)
        api_fillDefaultResponseFields(request, response);
    }
    else if (command == "remote_lowbattery")
    {
        // DO NOTHING BUT REPLY (for now)
        api_fillDefaultResponseFields(request, response);
    }
    else if (command == "remote_normal")
    {
        // DO NOTHING BUT REPLY (for now)
        api_fillDefaultResponseFields(request, response);
    }
    else if (command == "set_brightness")
    {
        // set and store new brightness values
        api_fillDefaultResponseFields(request, response);
        processSetBrightness(request, response);
    }
    else if (command == "set_logging")
    {
        // DO NOTHING BUT REPLY (for now)
        api_fillDefaultResponseFields(request, response);
    }
    else if (command == "reboot")
    {
        api_fillDefaultResponseFields(request, response, 200, true);
        // reboot is done after sending response
    }
    else if (command == "reset")
    {
        api_fillDefaultResponseFields(request, response, 200, true);
        Config::getInstance().reset();
        // reboot is done after sending response
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported command %s", command.c_str());
        api_replyWithError(request, response, 400, "Unsupported command");
    }
}

void api_processData(JsonDocument &request, JsonDocument &response, AsyncWebSocketClient *wsClient)
{

    String type;

    if (request.containsKey("type"))
    {
        type = request["type"].as<String>();
    }

    if (type == "dock")
    {
        processDockMessage(request, response, wsClient);
    }
    else if (type == "auth")
    {
        processAuthMessage(request, response);
    }
    else
    {
        ESP_LOGE(TAG, "Unknown message type %s", type.c_str());
        api_fillDefaultResponseFields(request, response, 400);
    }
}
