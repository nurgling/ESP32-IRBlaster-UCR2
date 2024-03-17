// Copyright 2024 Craig Petchell
// LerningIR codes part added by Alex Koessler 2024

#include <Arduino.h>
#include "ir_task.h"

#include <freertos/FreeRTOS.h>

#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

#include <ir_message.h>
#include <ir_queue.h>
#include <libconfig.h>
#include <api_service.h>
#include <blaster_config.h>

#include <esp_log.h>

static const char *TAG = "irtask";

uint16_t irRepeat = 0;
ir_message_t repeatMessage;
IRsend irsend(true, 0);

#if BLASTER_ENABLE_IR_LEARN == true
const uint16_t irRecvBufferSize = 1024;
IRrecv irrecv(BLASTER_PIN_IR_LEARN, irRecvBufferSize, 15, true);
#endif

bool repeatCallback()
{
    if (irRepeat > 0)
    {
        irRepeat--;
        return true;
    }
    return false;
}

void irSetup()
{
    ESP_LOGD(TAG, "Setting up Pins for IR Sending");

#if BLASTER_ENABLE_IR_INTERNAL == true
    pinMode(BLASTER_PIN_IR_INTERNAL, OUTPUT);
#endif
#if BLASTER_ENABLE_IR_OUT_1 == true
    pinMode(BLASTER_PIN_IR_OUT_1, OUTPUT);
#endif
#if BLASTER_ENABLE_IR_OUT_2 == true
    pinMode(BLASTER_PIN_IR_OUT_2, OUTPUT);
#endif

#if BLASTER_ENABLE_IR_LEARN == true
    ESP_LOGD(TAG, "Setting up Pin for IR Lerning");
    irrecv.setUnknownThreshold(1000);
    irrecv.enableIRIn();
    irrecv.pause();
#endif

    irsend.setRepeatCallback(repeatCallback);
    irsend.begin();
}

void sendProntoCode(ir_message_t &message)
{
    irsend.sendPronto(message.code16, message.codeLen, message.repeat);
    irRepeat = 0;
}

void sendHexCode(ir_message_t &message)
{
    // Copy so not overritten on next message
    memcpy(&repeatMessage, &message, sizeof(ir_message_t));
    irRepeat = message.repeat;
    if (hasACState(message.decodeType))
    {
        irsend.send(message.decodeType, message.code8, message.codeLen);
    }
    else
    {
        irsend.send(message.decodeType, message.code64, message.codeLen, irRepeat);
    }
}

#if BLASTER_ENABLE_IR_LEARN == true
String receiveIR()
{
    String code = "";
    decode_results irRes;

    if (irrecv.decode(&irRes))
    {
        irrecv.pause();

        ESP_LOGV(TAG, resultToHumanReadableBasic(&irRes).c_str());
        code += irRes.decode_type;
        code += ";";
        code += resultToHexidecimal(&irRes);
        code += ";";
        code += irRes.bits;
        code += ";";
        code += irRes.repeat;
        ESP_LOGD(TAG, "Learned IR code in UC format: %d", code.c_str());
    }
    return code;
}
#endif

bool receiveIRState = false;
// TODO: this definitely needs to be done nicer
extern AsyncWebSocketClient *learningClient;

void TaskIR(void *pvParameters)
{
    ESP_LOGD(TAG, "TaskIR running on core %d", xPortGetCoreID());

    irSetup();
    ir_message_t message;
    for (;;)
    {

#if BLASTER_ENABLE_IR_LEARN == true        
        if (receiveIRState)
        {
            String code = receiveIR();
            if (code != "")
            {
                // we learned an IR code. prepare for sending via websocket connection
                JsonDocument eventMsg;
                api_buildIRCodeEvent(eventMsg, code);
                api_sendJsonReply(eventMsg, learningClient);
                receiveIRState = false;
            }
        }
#endif

        if (irQueueHandle != NULL)
        {
            int ret = xQueueReceive(irQueueHandle, &message, 0);
            // int ret = xQueueReceive(irQueueHandle, &message, portMAX_DELAY);
            if (ret == pdPASS)
            {
                switch (message.action)
                {
                case send:
                {
                    // pin indicator was removed from the ir mask.
                    // TODO: trigger some short flashing once a ir command is sent.
                    uint32_t ir_pin_mask = 0;
                    if(message.ir_internal)
                    {
                        if(BLASTER_ENABLE_IR_INTERNAL == true)
                        {
                            ir_pin_mask |= message.ir_internal << BLASTER_PIN_IR_INTERNAL;    
                        }
                        else
                        {
                            ESP_LOGD(TAG, "Internal IR channel requested but not available in dock configuration");
                        }
                    }
                    if(message.ir_ext1)
                    {
                        if(BLASTER_ENABLE_IR_OUT_1 == true)
                        {
                            ir_pin_mask |= message.ir_ext1 << BLASTER_PIN_IR_OUT_1;    
                        }
                        else
                        {
                            ESP_LOGD(TAG, "External IR channel 1 requested but not available in dock configuration");
                        }
                    }
                    if(message.ir_ext2)
                    {
                        if(BLASTER_ENABLE_IR_OUT_2 == true)
                        {
                            ir_pin_mask |= message.ir_ext2 << BLASTER_PIN_IR_OUT_2;
                        }
                        else
                        {
                            ESP_LOGD(TAG, "External IR channel 2 requested but not available in dock configuration");
                        }
                    }

                    // TODO: do we need to report back, if we are not sending the command?
                    // at least log a warning!
                    if (ir_pin_mask == 0)
                    {
                        ESP_LOGE(TAG, "IR command could not be sent. All IR channels requested for sending are not available in the dock configuration");
                        ESP_LOGD(TAG, "Requested channels: internal=%s, out_1=%s, out_2=%s", message.ir_internal?"true ":"false", message.ir_ext1?"true ":"false", message.ir_ext2?"true ":"false");
                        ESP_LOGD(TAG, "Available channels: internal=%s, out_1=%s, out_2=%s", BLASTER_ENABLE_IR_INTERNAL?"true ":"false", BLASTER_ENABLE_IR_OUT_1?"true ":"false", BLASTER_ENABLE_IR_OUT_2?"true ":"false");
                    }
                    else
                    {
                        irsend.setPinMask(ir_pin_mask);

                        switch (message.format)
                        {
                        case pronto:
                            sendProntoCode(message);
                            break;
                        case hex:
                            sendHexCode(message);
                            break;
                        }
                    }
                    break;
                }
                case repeat:
                {
                    irRepeat += message.repeat;
                    break;
                }
                case learn_start:
                {
                    receiveIRState = true;
                    ESP_LOGI(TAG, "Starting IR learning");
                    break;
                }
                case learn_stop:
                {
                    receiveIRState = false;
                    ESP_LOGI(TAG, "Stopping IR learning");
                    break;
                }
                case stop:
                default:
                {
                    irRepeat = 0;
                    break;
                }
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}