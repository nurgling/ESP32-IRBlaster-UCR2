// Copyright 2024 Alex Koessler

#include <Arduino.h>
#include "led_task.h"

#include <freertos/FreeRTOS.h>

#include <blaster_config.h>
#include <libconfig.h>

#include <esp_log.h>

#define FASTLED_RMT_MAX_CHANNELS 1
#include <FastLED.h>

// for pixel led
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
// for pwm led
const int ledChannel = 0;

static const char *TAG = "ledtask";

// normal: breath (white)

// remote_lowbattery: blink fast

// remote_charged: breath (white)

// charging --> on

// identify --> blink fast

// setup --> blink slow

enum ledState
{
    off,
    identify,
    normal,
    learn,
    reset,
    factoryreset,
    none,
};

ledState l_state = off;
ledState triggerState = none;

// current led color value
CHSV current_color = CHSV(0, 0, 0);

// update interval of indicator led in ms
#define UPDATERATE_MS 10

// resolution of single color pwm output
#define RESOLUTION 10

// converts percentage value (0-100) to 8 bit value (0-255)
uint8_t prc2value8(uint8_t prc)
{
    return (prc * 255 / 100);
}

uint8_t scalebrightness(uint8_t val)
{
    return ((val * Config::getInstance().getLedBrightness()) / 100);
}

uint16_t scalebrightness2pwm(uint8_t val)
{
    return (map(val, 0, 255, 0, ((1 << RESOLUTION) - 1)));
}
uint16_t scaleprc2pwm(uint8_t prc)
{
    return (map(prc, 0, 100, 0, ((1 << RESOLUTION) - 1)));
}

void transferColor(CHSV start, CHSV goal, uint32_t duration_ms = 500)
{
    uint32_t numsteps = duration_ms / UPDATERATE_MS;

    if (numsteps > 1)
    {
        // perform intermediate steps towards goal
        for (uint32_t i = 0; i < numsteps; ++i)
        {
            long hue = map(i, 0, numsteps, start.hue, goal.hue); // Map hue based on current step
            long sat = map(i, 0, numsteps, start.sat, goal.sat); // Map sat based on current step
            long val = map(i, 0, numsteps, start.val, goal.val); // Map val based on current step

            // save only the undimmed values for transition calculation
            current_color = CHSV(hue, sat, val);

            // output the dimmed values
            CHSV out = CHSV(hue, sat, scalebrightness(val));

            // ESP_LOGD(TAG, "Led Val: %d", out.val);

#if (BLASTER_INDICATOR_MODE == INDICATOR_LED)
            // output single color led
            ledcWrite(ledChannel, scalebrightness2pwm(out.val));
#elif (BLASTER_INDICATOR_MODE == INDICATOR_PIXEL)
            // output neopixel
            FastLED.showColor(out);
#else
            ESP_LOGE(TAG, "Unsupported BLASTER_INDICATOR_MODE (%d)!", BLASTER_INDICATOR_MODE);
#endif

            // delay
            vTaskDelay(UPDATERATE_MS / portTICK_PERIOD_MS);
        }
    }
    // perform last step to the final goal
    current_color = goal;
    // output the dimmed values
    CHSV out = CHSV(current_color.hue, current_color.sat, scalebrightness(current_color.val));

#if (BLASTER_INDICATOR_MODE == INDICATOR_LED)
    // output single color led
    ledcWrite(ledChannel, scalebrightness2pwm(out.val));
#elif (BLASTER_INDICATOR_MODE == INDICATOR_PIXEL)
    // output neopixel
    FastLED.showColor(out);
#else
    ESP_LOGE(TAG, "Unsupported BLASTER_INDICATOR_MODE (%d)!", BLASTER_INDICATOR_MODE);
#endif
}

void set_led_off(uint32_t duration_ms = 50)
{
    // transition(&current_pwm, 0, duration_ms, hue, sat);

    CHSV g = CHSV(current_color.hue, current_color.sat, 0);
    //ESP_LOGD(TAG, "LedOFF: {%d,%d,%d}->{%d,%d,%d} [%d ms]", current_color.h, current_color.s, current_color.v, g.h, g.s, g.v, duration_ms);
    transferColor(current_color, g, duration_ms);
}

void set_led_on(uint32_t duration_ms = 50, uint8_t hue = 0, uint8_t sat = 0)
{
    uint8_t goalval = prc2value8(100);
    CHSV g = CHSV(hue, sat, goalval);
    CHSV s = CHSV(hue, sat, 0);
    // transition(&current_pwm, onpwm, duration_ms, hue, sat);
    //ESP_LOGD(TAG, "LedON: {%d,%d,%d}->{%d,%d,%d} [%d ms]", s.h, s.s, s.v, g.h, g.s, g.v, duration_ms);
    transferColor(s, g, duration_ms);
}

void breath_once(uint32_t dur_ms = 6000, uint8_t hue = 0, uint8_t sat = 0)
{
    set_led_on(dur_ms / 2, hue, sat);
    set_led_off(dur_ms / 2);
}

void setPWMState(ledState nextState)
{
    if (l_state == nextState)
    {
        return;
    }
    if (l_state != off)
    {
        // transition to intermediate off state
        set_led_off(100);
    }
    l_state = nextState;
}

void setLedStateIdentify()
{
    triggerState = identify;
}
void setLedStateLearn()
{
    triggerState = learn;
}
void setLedStateNormal()
{
    triggerState = normal;
}
void setLedStateReset()
{
    triggerState = reset;
}
void setLedStateFactoryReset()
{
    triggerState = factoryreset;
}


void setupLedOutput()
{
    pinMode(BLASTER_PIN_INDICATOR, OUTPUT);

#if BLASTER_INDICATOR_MODE == INDICATOR_LED
    // configure LED PWM functionalitites
    ledcSetup(ledChannel, 5000, RESOLUTION);
    // attach the channel to the GPIO2 to be controlled
    ledcAttachPin(BLASTER_PIN_INDICATOR, ledChannel);
    // turn off led
    ledcWrite(ledChannel, 0);
#elif BLASTER_INDICATOR_MODE == INDICATOR_PIXEL
    // setup pixel led
    //FastLED.addLeds<WS2812B, BLASTER_PIN_INDICATOR, RGB>(leds, NUM_LEDS);
    FastLED.addLeds<WS2812B, BLASTER_PIN_INDICATOR, GRB>(leds, NUM_LEDS);
    FastLED.clear(true);
#else
    ESP_LOGE(TAG, "Unsupported BLASTER_INDICATOR_MODE (%d)!", BLASTER_INDICATOR_MODE);
#endif

    l_state = normal;
}

// TODO: rework using led lib to support rgb leds (remark to myself: check out ledwriter)!
void TaskLed(void *pvParameters)
{
    ESP_LOGD(TAG, "TaskLed running on core %d", xPortGetCoreID());

    setupLedOutput();

    int i = 0;
    unsigned long lastMillis = 0;
    unsigned long currentMillis = 0;

    const uint task_idle_time = 20;
    const uint pause_off = 20;
    const uint pause_on = 20;
    const uint transfer_time = 100;

    for (;;)
    {
        if (triggerState != none)
        {
            setPWMState(triggerState);
            triggerState = none;
        }
        switch (l_state)
        {
        case normal:
            currentMillis = millis();
            if ((lastMillis == 0) || (currentMillis > lastMillis + 10000))
            {
                lastMillis = currentMillis;

                breath_once(3000, HSVHue::HUE_GREEN, 255); // breath green
            }
            break;

        case identify:
        {
            for (i = 0; i < 2; ++i)
            {
                set_led_on(transfer_time, HSVHue::HUE_BLUE, 255);
                vTaskDelay(pause_on / portTICK_PERIOD_MS);
                set_led_off(transfer_time);
                vTaskDelay(pause_off / portTICK_PERIOD_MS);
                set_led_on(transfer_time, HSVHue::HUE_ORANGE, 255);
                vTaskDelay(pause_on / portTICK_PERIOD_MS);
                set_led_off(transfer_time);
                vTaskDelay(pause_off / portTICK_PERIOD_MS);
                set_led_on(transfer_time, HSVHue::HUE_GREEN, 255);
                vTaskDelay(pause_on / portTICK_PERIOD_MS);
                set_led_off(transfer_time);
                vTaskDelay(pause_off / portTICK_PERIOD_MS);
                set_led_on(transfer_time, HSVHue::HUE_RED, 255);
                vTaskDelay(pause_on / portTICK_PERIOD_MS);
                set_led_off(transfer_time);
                vTaskDelay(pause_off / portTICK_PERIOD_MS);
            }
            l_state = normal;
        }
        break;

        case learn:
        {
            transferColor(current_color, CHSV(HSVHue::HUE_RED, 200, 255), transfer_time);
        }
        break;

        case reset:
        {
            transferColor(current_color, CHSV(HSVHue::HUE_BLUE, 200, 255), transfer_time);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            set_led_off(transfer_time);
            vTaskDelay((200 - task_idle_time) / portTICK_PERIOD_MS);
        }
        break;

        case factoryreset:
        {
            transferColor(current_color, CHSV(HSVHue::HUE_RED, 200, 255), transfer_time);
            vTaskDelay(50 / portTICK_PERIOD_MS);
            set_led_off(transfer_time);
            vTaskDelay((50 - task_idle_time) / portTICK_PERIOD_MS);
        }
        break;

        default:
            break;
        }
        // TODO: blink the led in the discovery pattern!

        vTaskDelay(task_idle_time / portTICK_PERIOD_MS);
    }
}
