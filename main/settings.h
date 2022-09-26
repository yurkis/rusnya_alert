#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "alert_api.h"

typedef struct _SAppSettings{
    char ap_name[96];
    char ap_pass[96];
    AlertRegionID_t alert_region;
    uint8_t quit_start_hr;
    uint8_t quit_end_hr;
}SAppSettings;

bool nvsInit();
bool settingsRead(SAppSettings* s);
bool settingsWrite(SAppSettings* s);
void settingsRegisterConsole();

