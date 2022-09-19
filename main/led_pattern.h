#pragma once

#include "driver/gpio.h"
#include <stdbool.h>
#include <stdint.h>

typedef void* led_pattern_t;
typedef struct _SLedPattern
{
    uint32_t pattern;
    size_t   bits;
    uint32_t time;
}SLedPattern;

#define LED_PATTERN_ON  {.pattern=0xff, .bits=8, .time=1000}
#define LED_PATTERN_OFF {.pattern=0x00, .bits=8, .time=1000}

led_pattern_t lpCreate(gpio_num_t gpio);
bool          lpDestroy(led_pattern_t instance);
bool lpSetPattern(led_pattern_t instance, SLedPattern *pattern);

