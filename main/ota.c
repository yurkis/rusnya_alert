#include "esp_err.h"
#include "esp_log.h"

#include "esp_app_format.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"

#include "ota.h"
#include <string.h>

static const char* TAG = "ota";

static version_t curr_version = 0;
static SOTAConfig conf ={0};

#define esp_app_desc_t_offset (sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t))
static char BUFF [esp_app_desc_t_offset + sizeof(esp_app_desc_t)];

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

void otaSetConfig(SOTAConfig config)
{
    conf = config;
}

int otaCheck(SVersion *ota_ver)
{
    ESP_LOGI(TAG, "Checking...");
    if (ota_ver) {
        memset(ota_ver, 0, sizeof(*ota_ver));
    }
    esp_err_t err;
    esp_http_client_config_t config = {
            .url = conf.url,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = conf.server_cert,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if ((err = esp_http_client_open(client, 0)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
            return OTA_ERR_HTTP_CONNECT;
        }
    int content_length =  esp_http_client_fetch_headers(client);
    int read_len;
    read_len = esp_http_client_read_response(client, BUFF, sizeof(BUFF));
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    esp_app_desc_t* app_desc = (esp_app_desc_t*)(BUFF + esp_app_desc_t_offset);

    if (ESP_APP_DESC_MAGIC_WORD != app_desc->magic_word) {
        ESP_LOGE(TAG, "Wrong binary format");
        return OTA_ERR_BIN_FORMAT;
    }

    //TODO: something with comparsion here
    if (conf.project_name) {
        if (0 == strncmp(conf.project_name, app_desc->project_name, sizeof(*app_desc->project_name))) {
            ESP_LOGE(TAG, "Wrong project name. Expected: '%s', Got: '%s'", conf.project_name, app_desc->project_name);
            return OTA_ERR_PROJECT_NAME;
        }
    } else {
        ESP_LOGI(TAG, "Project name check skipped");
    }

    SVersion remote_ver = otaParseVersionStr(app_desc->version);
    ESP_LOGI(TAG," Remote version is %i.%i.%i Length: %i",(uint32_t)remote_ver.major,
                                                          (uint32_t)remote_ver.minor,
                                                          (uint32_t)remote_ver.build,
                                                          content_length);
    if (ota_ver) {
        *ota_ver = remote_ver;
    }
    version_t remote_v = version2raw(remote_ver);
    if (curr_version<remote_v) {
        ESP_LOGI(TAG, "Update found %x -> %x", curr_version, remote_v);
        return OTA_UPDATE_FOUND;
    }

    return OTA_NO_UPDATE;
}

int otaPerform(SVersion *ota_ver)
{
    int ret = otaCheck(ota_ver);
    if (OTA_UPDATE_FOUND != ret) {
        return ret;
    }
    esp_http_client_config_t config = {
            .url = conf.url,
            .transport_type = HTTP_TRANSPORT_OVER_SSL,
            .cert_pem = conf.server_cert,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
    ESP_LOGI(TAG, "Performing OTA update...");
    esp_err_t err = esp_https_ota(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA update failed with %i", err);
        return OTA_FAILED;
    }
    return OTA_PERFORMED;
}
