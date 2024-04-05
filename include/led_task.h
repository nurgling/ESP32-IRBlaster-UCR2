// Copyright 2024 Alex Koessler

#ifndef LED_TASK_H_
#define LED_TASK_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <Arduino.h>

void TaskLed(void *pvParameters);


#define INDICATOR_OFF 0
#define INDICATOR_LED 1
#define INDICATOR_PIXEL 2





#ifdef __cplusplus
}
#endif

#endif