// Copyright 2024 Craig Petchell
// Contributions by Alex Koessler

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Arduino.h>
#include <WiFi.h>
#include <IRsend.h>

#include <libconfig.h>
#include <mdns_service.h>
#include <wifi_service.h>

#include <ir_message.h>
#include <ir_queue.h>

#include "ir_task.h"
#include "web_task.h"
#include "bt_task.h"
#include "led_task.h"

#include <esp_log.h>

static const char *TAG = "main";

QueueHandle_t irQueueHandle;

void setup()
{
    Serial.begin(115200);

    esp_log_level_set("*", ESP_LOG_INFO);

    Config &config = Config::getInstance();
    WifiService &wifiSrv = WifiService::getInstance();

    wifiSrv.connect();

    // Task for controlling the LED is always required
    TaskHandle_t *ledTaskHandle = NULL;
    BaseType_t taskCreate;
    taskCreate = xTaskCreatePinnedToCore(
        TaskLed, "Task Led Control",
        8192, NULL, 2, ledTaskHandle, 0);

    if (taskCreate != pdPASS)
    {
        ESP_LOGE(TAG, "Creation of led task failed. Returnvalue: %d.\n", taskCreate);
    }

    if (wifiSrv.isActive())
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
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}