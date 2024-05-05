// Copyright 2024 Craig Petchell
// Contributions by Alex Koessler

#include <Arduino.h>
#include "ir_task.h"
#include "web_task.h"
#include "bt_task.h"
#include "led_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <WiFi.h>
#include <IRsend.h>

#include <libconfig.h>
#include <mdns_service.h>
#include <wifi_service.h>
#include <ir_message.h>
#include <ir_queue.h>

#include <eth_service.h>
#include <button_service.h>
#include <ota_service.h>
#include <fs_service.h>


#include <esp_log.h>

static const char *TAG = "main";

QueueHandle_t irQueueHandle;

void setup()
{
    BaseType_t taskCreate;

    Serial.begin(115200);

    esp_log_level_set("*", ESP_LOG_INFO);

    Config &config = Config::getInstance();
    WifiService &wifiSrv = WifiService::getInstance();
    wifiSrv.connect();

    EthService &ethSrv = EthService::getInstance();
    ethSrv.connect();

    ButtonService &btnSrv = ButtonService::getInstance();
    btnSrv.init();
    
    SPIFFSService &fsSrv = SPIFFSService::getInstance();
    fsSrv.init();


// Task for controlling the LED is only required if indicator led is available
#if BLASTER_INDICATOR_MODE != INDICATOR_OFF
    TaskHandle_t *ledTaskHandle = NULL;
    taskCreate = xTaskCreatePinnedToCore(
        TaskLed, "Task Led Control",
        8192, NULL, 2, ledTaskHandle, 0);

    if (taskCreate != pdPASS)
    {
        ESP_LOGE(TAG, "Creation of led task failed. Returnvalue: %d.\n", taskCreate);
    }

#endif



    if (!wifiSrv.isActive() && !ethSrv.isActive()){
        //waiting 5 secs for eth or wifi to get ready
        int MAX_NETWORK_WAIT = 10;
        for(int i = 0; i < MAX_NETWORK_WAIT; ++i){
            ESP_LOGD(TAG, "Waiting for network connection %d/%ds ...", i+1, MAX_NETWORK_WAIT);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            if (wifiSrv.isActive() || ethSrv.isActive())
            {
                //we are connected.
                ESP_LOGI(TAG, "Network connection established!");
                break;
            }
        }    
    }




    if (wifiSrv.isActive() || ethSrv.isActive())
    {

        // Create the queue which will have <QueueElementSize> number of elements, each of size `message_t` and pass the address to <QueueHandle>.
        irQueueHandle = xQueueCreate(IR_QUEUE_SIZE, sizeof(ir_message_t));

        // Check if the queue was successfully created
        if (irQueueHandle == NULL)
        {
            ESP_LOGE(TAG, "Queue could not be created. Halt.");
            while (1)
                delay(1000); // Halt at this point as is not possible to continue
        }

        // tasks for controlling the ir output via wifi
        TaskHandle_t *webTaskHandle = NULL;
        taskCreate = xTaskCreatePinnedToCore(
            TaskWeb, "Task Web/Websocket server",
            16384, NULL, 2, webTaskHandle, 0);
        if (taskCreate != pdPASS)
        {
            ESP_LOGE(TAG, "Creation of web task failed. Returnvalue: %d.\n", taskCreate);
        }
        TaskHandle_t *irTaskHandle = NULL;
        taskCreate = xTaskCreatePinnedToCore(
            TaskIR, "Task IR send/receive",
            32768, NULL, 3 /* highest priority */, irTaskHandle, 1);
        if (taskCreate != pdPASS)
        {
            ESP_LOGE(TAG, "Creation of IR task failed. Returnvalue: %d.\n", taskCreate);
        }

        if (BLASTER_ENABLE_OTA){
            OTAService::getInstance().startService();
        }
    }
    else
    {
        // fallback to bluetooth discovery
        TaskHandle_t *btTaskHandle = NULL;
        taskCreate = xTaskCreatePinnedToCore(
            TaskBT, "Task Bluetooth",
            32768, NULL, 2, btTaskHandle, 0);
        if (taskCreate != pdPASS)
        {
            ESP_LOGE(TAG, "Creation of bt task failed. Returnvalue: %d.\n", taskCreate);
        }
    }
}

void loop()
{
    WifiService::getInstance().loop();
    ButtonService::getInstance().loop();
    OTAService::getInstance().loop();
    vTaskDelay(20 / portTICK_PERIOD_MS);
}