#include "esp_err.h"
#include "esp_log.h"

#include "ota.h"
#include <string.h>

static const char* TAG = "ota";

static version_t curr_version = 0;

// I could use packed union but prefer to handle bits by my own
static version_t version2raw(SVersion ver)
{
    return (version_t) (ver.major<<24 | ver.minor<<16 | ver.build);
}

static SVersion raw2version(version_t raw)
{
   SVersion v;
   v.major = (raw<<24) & 0xff;
   v.minor = (raw<<16) & 0xff;
   v.build = raw & 0xffff;
   return v;
}

SVersion otaParseVersionStr(const char *str)
{
    static char curr_str [16];
    SVersion v;
    memset(&v, 0, sizeof(v));
    size_t curr=0;
    size_t str_size = strlen(str);
    memset(curr_str, 0, sizeof(curr_str));
    size_t i=0;
    while((curr<str_size) && (str[curr]!='.')) {
        curr_str[i++] = str[curr++];
    }
    if (i) {
        curr_str[i]=0;
        v.major = (uint8_t)atoi(curr_str);
    }

    memset(curr_str, 0, sizeof(curr_str));
    i=0;curr++;
    while((curr<str_size) && (str[curr]!='.')) {
        curr_str[i++] = str[curr++];
    }
    if (i) {
        curr_str[i]=0;
        v.minor = (uint8_t)atoi(curr_str);
    }

    memset(curr_str, 0, sizeof(curr_str));
    i=0;curr++;
    while((curr<str_size) && (str[curr]!='.')) {
        curr_str[i++] = str[curr++];
    }
    if (i) {
        curr_str[i]=0;
        v.build = (uint16_t)atoi(curr_str);
    }
    return v;
}

void otaSetCurrentVersion(SVersion ver)
{
    curr_version = version2raw(ver);
    ESP_LOGI(TAG,"Current app version is %i.%i.%i (%x)", (uint32_t)ver.major,
                                                         (uint32_t)ver.minor,
                                                         (uint32_t)ver.build,
                                                         curr_version);
}
