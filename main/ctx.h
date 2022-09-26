#pragma once

#include "led_pattern.h"
#include "settings.h"

typedef struct _SRusnyaCTX
{
    led_pattern_t led_pattern;
    SAppSettings settings;
}SRusnyaCTX;

extern SRusnyaCTX ctx;
