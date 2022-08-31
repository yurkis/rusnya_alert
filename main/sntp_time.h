#pragma once

#include <stdbool.h>

/* Thanks to StackOverflow: $ strings /etc/localtime | tail -n 1 */
#define UA_TZ  "EET-2EEST,M3.5.0/3,M10.5.0/4"


void timeSetTimezone(const char* TZ);
bool timeSync();
bool timeIsSynced();
