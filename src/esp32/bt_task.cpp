// Copyright 2024 Alex Koessler

#include <Arduino.h>
#include "bt_task.h"

#include <freertos/FreeRTOS.h>

#include <bt_service.h>

#include <esp_log.h>

static const char *TAG = "bttask";
extern void setLedStateBTDiscovery();

void TaskBT(void *pvParameters)
{
    ESP_LOGD(TAG, "TaskBT running on core %d", xPortGetCoreID());

    BluetoothService *btService = new BluetoothService();

    // initialize Bluetooth
    btService->init();
    setLedStateBTDiscovery();
    
    for (;;)
    {
        btService->handle();
        // TODO: blink the led in the discovery pattern!
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}