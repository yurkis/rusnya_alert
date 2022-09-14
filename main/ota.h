#pragma once

#include <stdint.h>

typedef struct _SVersion
{
    uint16_t build;
    uint8_t  minor;
    uint8_t  major;
}SVersion;

typedef uint32_t version_t;

SVersion otaParseVersionStr(const char* str);

void otaSetCurrentVersion(SVersion ver);
