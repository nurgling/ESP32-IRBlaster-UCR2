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
#define INDICATOR_RGB 2

#ifdef BLASTER_INDICATOR_MODE
#if BLASTER_INDICATOR_MODE == INDICATOR_RGB
#warning Using RGB led as indicator is currently not supported. Disabling indicator led!
#undef BLASTER_INDICATOR_MODE
#define BLASTER_INDICATOR_MODE INDICATOR_OFF
#endif
#endif




#ifdef __cplusplus
}
#endif

#endif