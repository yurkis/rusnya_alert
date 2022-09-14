#pragma once

#include <stdint.h>

typedef struct _SVersion
{
    uint16_t build;
    uint8_t  minor;
    uint8_t  major;
}SVersion;

typedef struct _SOTAConfig
{
    const char* url;
    const char* server_cert;
    const char* project_name;
}SOTAConfig;

typedef uint32_t version_t;

SVersion otaParseVersionStr(const char* str);

#define OTA_UPDATE_FOUND (1)
#define OTA_NO_UPDATE    (0)
#define OTA_PERFORMED    (1)

#define OTA_ERR_HTTP_CONNECT (-1)
#define OTA_ERR_BIN_FORMAT   (-2)
#define OTA_ERR_PROJECT_NAME (-3)
#define OTA_FAILED           (-9)


void otaSetCurrentVersion(SVersion ver);
void otaSetConfig(SOTAConfig config);
int  otaCheck(SVersion* ota_ver);
int  otaPerform(SVersion *ota_ver);
