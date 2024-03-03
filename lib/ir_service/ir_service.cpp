// Copyright 2024 Craig Petchell

#include <Arduino.h>
#include "ir_service.h"
#include "ir_queue.h"
#include "ir_message.h"

#include <api_service.h>
#include <IRutils.h>

static const char * TAG = "irservice";


#define MAX_IR_TEXT_CODE_LENGTH 2048
#define MAX_IR_FORMAT_TYPE 50

// Current text ir code
char irCode[MAX_IR_TEXT_CODE_LENGTH] = "";
char irFormat[MAX_IR_FORMAT_TYPE] = "";

bool irLearningActive=false;

void buildProntoMessage(ir_message_t &message)
{
    const int strCodeLen = strlen(irCode);

    char workingCode[strCodeLen + 1] = "";
    char *workingPtr;
    message.codeLen = (strCodeLen + 1) / 5;
    uint16_t offset = 0;

    strcpy(workingCode, irCode);
    
    char delimiter[2] = {0,0};
    delimiter[0] = workingCode[4];
    if((delimiter[0] != ' ') && (delimiter[0] != ','))
    {
        ESP_LOGE(TAG, "Pronto delimiter not recognized. Prontocode: %s", workingCode);
    }
    
    char *hexCode = strtok_r(workingCode, delimiter, &workingPtr);
    while (hexCode)
    {
        message.code16[offset++] = strtoul(hexCode, NULL, 16);
        hexCode = strtok_r(NULL, delimiter, &workingPtr);
    }

    message.format = pronto;
    message.action = send;
    message.decodeType = PRONTO;
}

void buildHexMessage(ir_message_t &message)
{
    // Split the UC_CODE parameter into protocol / code / bits / repeats
    char workingCode[MAX_IR_TEXT_CODE_LENGTH];
    char *parts[4];
    int partcount = 0;

    strcpy(workingCode, irCode);
    parts[partcount++] = workingCode;

    char *ptr = workingCode;
    while (*ptr && partcount < 4)
    {
        if (*ptr == ';')
        {
            *ptr = 0;
            parts[partcount++] = ptr + 1;
        }
        ptr++;
    }

    if (partcount != 4)
    {
        ESP_LOGE(TAG, "Unvalid UC code");
        return;
    }

    message.decodeType = strToDecodeType(parts[0]);

    switch (message.decodeType)
    {
    // Unsupported types
    case decode_type_t::UNUSED:
    case decode_type_t::UNKNOWN:
    case decode_type_t::GLOBALCACHE:
    case decode_type_t::PRONTO:
    case decode_type_t::RAW:
        ESP_LOGE(TAG, "The protocol specified is not supported by this program.");
        return;
    default:
        break;
    }

    uint16_t nbits = static_cast<uint16_t>(std::stoul(parts[2]));
    if (nbits == 0 && (nbits <= kStateSizeMax * 8))
    {
        ESP_LOGE(TAG, "No of bits %s is invalid", parts[2]);
        return;
    }

    uint16_t stateSize = nbits / 8;

    uint16_t repeats = static_cast<uint16_t>(std::stoul(parts[3]));
    if (repeats > 20)
    {
        ESP_LOGE(TAG, "Repeat count is too large: %d. Maximum is 20.", repeats);
        return;
    }

    if (!hasACState(message.decodeType))
    {
        message.code64 = std::stoull(parts[1], nullptr, 16);
        message.codeLen = nbits;
    }
    else
    {

        String hexstr = String(parts[1]);
        uint64_t hexstrlength = hexstr.length();
        uint64_t strOffset = 0;
        if (hexstrlength > 1 && hexstr[0] == '0' &&
            (hexstr[1] == 'x' || hexstr[1] == 'X'))
        {
            // Skip 0x/0X
            strOffset = 2;
        }

        hexstrlength -= strOffset;

        memset(&message.code8, 0, stateSize);
        message.codeLen = stateSize;

        // Ptr to the least significant byte of the resulting state for this
        // protocol.
        uint8_t *statePtr = &message.code8[stateSize - 1];

        // Convert the string into a state array of the correct length.
        for (uint64_t i = 0; i < hexstrlength; i++)
        {
            // Grab the next least sigificant hexadecimal digit from the string.
            uint8_t c = tolower(hexstr[hexstrlength + strOffset - i - 1]);
            if (isxdigit(c))
            {
                if (isdigit(c))
                    c -= '0';
                else
                    c = c - 'a' + 10;
            }
            else
            {
                ESP_LOGE(TAG, "Code %s contains non-hexidecimal characters.", parts[1]);
                return;
            }
            if (i % 2 == 1)
            { // Odd: Upper half of the byte.
                *statePtr += (c << 4);
                statePtr--; // Advance up to the next least significant byte of state.
            }
            else
            { // Even: Lower half of the byte.
                *statePtr = c;
            }
        }
    }

    message.format = hex;
    message.action = send;
}

bool queueIRMessage(ir_message_t &message, int waitingTime_ms=0)
{

    if (irQueueHandle != NULL)
    {
        int ret = xQueueSend(irQueueHandle, (void *)&message, waitingTime_ms / portTICK_PERIOD_MS);
        if (ret == pdTRUE)
        {
            // The message was successfully sent.
            ESP_LOGD(TAG, "Action successfully sent to the IR Queue");
            return true;
        }
        else if (ret == errQUEUE_FULL)
        {
            ESP_LOGE(TAG, "The `TaskWeb` was unable to send message to IR Queue");
        }
    }
    else
    {
        ESP_LOGE(TAG, "The `TaskWeb` was unable to send message to IR Queue; no queue defined");
    }
    return false;
}

AsyncWebSocketClient *learningClient;
void learnIRStart(JsonDocument &input, JsonDocument &output, AsyncWebSocketClient *wsClient)
{
    //backup old learning state
    bool irLearningOld = irLearningActive;

    // block other potential ir commands
    // TODO: is this really safe, or do we need stronger synchronization mechanisms between parallel requests?
    irLearningActive = true;

    ir_message_t message;
    message.action = learn_start;
    if(!queueIRMessage(message, 500)){
        api_replyWithError(input, output, 503, "IR learning could not be triggered");
        ESP_LOGE(TAG, "IR learning could not be triggered");
        //restore learning
        irLearningActive = irLearningOld;
    } 
    else 
    {
        //save ws Client to respond the learned IR code
        learningClient = wsClient;
    }
}

void learnIRStop(JsonDocument &input, JsonDocument &output)
{
    ir_message_t message;
    message.action = learn_stop;
    if(!queueIRMessage(message, 500)){
        api_replyWithError(input, output, 503, "IR learning could not be released");
        ESP_LOGE(TAG, "IR learning could not be released");
    } else {
        irLearningActive = false;
        learningClient = NULL;
    }
}


void queueIR(JsonDocument &input, JsonDocument &output)
{
    const char *newCode = input["code"];
    const char *newFormat = input["format"];
    const uint16_t newRepeat = input["repeat"];
    const bool ir_internal = input["int_side"] || input["int_top"];
    const bool ir_ext1 = input["ext1"];
    const bool ir_ext2 = input["ext2"];
    ir_message_t message;

    message.action = send;
    message.ir_internal = ir_internal;
    message.ir_ext1 = ir_ext1;
    message.ir_ext2 = ir_ext2;
    message.repeat = newRepeat;

    if(irLearningActive){
        api_replyWithError(input, output, 503, "Canot send IR command. IR learning in progress.");
        ESP_LOGE(TAG, "Canot send IR command. IR learning in progress.");
        return;
    }


    if (uxQueueMessagesWaiting(irQueueHandle) != 0) 
    {
        // Message being processed
        if(irCode[0] && (strcmp(newCode, irCode) == 0) && (strcmp(newFormat, irFormat) == 0))
        {
            // Same message. send repeat command
            api_fillDefaultResponseFields(input, output, 202);
            message.action = repeat;
            queueIRMessage(message);
            return;
        }
        else
        {
            // Different message - reject
            api_fillDefaultResponseFields(input, output, 429);
            return;
        }
    }

    if (strlen(newCode)+1 > sizeof(irCode))
    {
        ESP_LOGE(TAG, "Length of sent code is longer than allocated buffer. Length = %u; Max = %u", strlen(newCode), sizeof(irCode));
        api_replyWithError(input, output, 400, "Length of IR code exceeds buffer.");
        return;
    }

    if (strlen(newFormat)+1 > sizeof(irFormat))
    {
        ESP_LOGE(TAG, "Length of sent format is longer than allocated buffer. Length = %u; Max = %u", strlen(newFormat), sizeof(irFormat));
        api_replyWithError(input, output, 400, "Length of IR code format exceeds buffer.");
        return;
    }

    strcpy(irCode, newCode);
    strcpy(irFormat, newFormat);

    if (strcmp("hex", newFormat) == 0)
    {
        buildHexMessage(message);
        queueIRMessage(message);
        api_fillDefaultResponseFields(input, output);
    }
    else if (strcmp("pronto", newFormat) == 0)
    {
        buildProntoMessage(message);
        queueIRMessage(message);
        api_fillDefaultResponseFields(input, output);
    }
    else
    {
        ESP_LOGE(TAG, "Unknown IR format %s", newFormat);
        api_replyWithError(input, output, 400, "Unknown IR format");

        irCode[0] = 0;
        irFormat[0] = 0;
    }
}

void stopIR(JsonDocument &input, JsonDocument &output)
{
    ir_message_t message;
    message.action = stop;

    queueIRMessage(message);
    api_fillDefaultResponseFields(input, output);
}
