#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
//#include "esp_app_desc.h"

#include "sound.h"
#include "wifi.h"
#include "alert_api.h"
#include "sntp_time.h"
#include "version.h"
#include "ota.h"
#include "ota_https_ssl.h"

static const char* TAG = "mainapp";

#define PARTITION_NAME   "storage"

#define APP_WIFI_AP     CONFIG_ESP_WIFI_SSID
#define APP_WIFI_PASSWD CONFIG_ESP_WIFI_PASSWORD
#define APP_WIFI_TRIES  CONFIG_ESP_MAXIMUM_RETRY

#define OTA_HTTPS_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/rusnya.bin"
#define OTA_CHECK_INTERVAL (60*60)

static SOTAConfig ota_conf = {
    .url = OTA_HTTPS_URL,
    .server_cert = OTA_HTTPS_CERT,
    //.project_name = "rusnya",
};

static AlertRegionID_t my_alert_region = 9; /* Hardcoded Kyiv oblast */
static uint8_t quit_start_hr = 22;
static uint8_t quit_end_hr = 9;

SVersion my_version;

static SWiFiSTASettings wifi_settings = {
    .ap = APP_WIFI_AP,
    .passwd = APP_WIFI_PASSWD,
    .tries = APP_WIFI_TRIES,
};

//const esp_app_desc_t * app_desc;

void initSPIFFS()
{
    esp_vfs_spiffs_conf_t conf = {
      .base_path = "/fs",
      .partition_label = NULL,
      .max_files = 10,
      .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }

}

bool initNVS()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
          ESP_ERROR_CHECK(nvs_flash_erase());
          ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return false;
}

void app_init()
{
    ESP_LOGI(TAG,"\nApp ver: %s\n",APP_VERSION_STR);

    initSPIFFS();
    soundSetup();
    initNVS();

    wifiInit();
    wifiSTAInit();
    wifiSTAConnect(wifi_settings);
    wifiWaitForWifi();
    timeSetTimezone(UA_TZ);
    timeSync();

    my_version = otaParseVersionStr(APP_VERSION_STR);
    otaSetCurrentVersion(my_version);
    otaSetConfig(ota_conf);
}

void play_unsafe()
{
    for (int i=0;i<3;i++){
        soundPlayFile("/fs/ALARM.wav");
        soundPlayFile("/fs/01.wav");
    }
}

void play_safe()
{
    for (int i=0;i<3;i++){
        soundPlayFile("/fs/SAFE.wav");
    }
}

void check_volume()
{
    uint8_t volume_div = SOUND_VOLUME_NO_DIV;
    if (timeIsSynced()) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);
        int hr = timeinfo.tm_hour;
        bool is_quiet = (quit_start_hr>quit_end_hr)?((hr>=quit_start_hr) && (hr>=quit_end_hr)) :
                                                    ((hr>=quit_start_hr) && (hr<=quit_end_hr));
        is_quiet &= (quit_start_hr!=quit_end_hr);
        if (is_quiet) {
            volume_div = SOUND_VOLUME_DIV4X;
        }
    } else {
        ESP_LOGI(TAG, "No time data, Full sound volume");
    }
    soundSetVolumeDiv(volume_div);
}

void alert_callback(AlertRegionID_t region, ERegionState old, ERegionState new)
{
    ESP_LOGI(TAG, "CHANGED SAFE STATE from '%s' to '%s'", alertStateToStr(old), alertStateToStr(new));

    check_volume();

    if (new == eZSUnsafe) {
        play_unsafe();
        return;
    } else {
        if (old == eZSUnsafe) play_safe();
        else {
            if (!timeIsSynced()){
                soundSetVolumeDiv(SOUND_VOLUME_DIV4X);
            }
            soundPlayFile("/fs/start.wav");
        }
    }
}

void app_main(void)
{
    bool connection_lost = false;
    app_init();
    alertConnect();
    alertSetObserver(my_alert_region, alert_callback);
    while(true)
    {
        if (wifiIsConnected()) {
            connection_lost = false;
            if (!alertIsConnected())
            {
                alertConnect();
            }
        } else {
            if (!connection_lost) {
                connection_lost = true;
                check_volume();
                soundPlayFile("/fs/nowifi.wav");
            }
            wifiSTAConnect(wifi_settings);
            wifiWaitForWifi();
            continue;
        }
        alertCheckSync();

        if (timeIsSynced()) {
            static time_t now;
            static time_t last = {0};
            time(&now);

            if ((eZSSafe == alertState(my_alert_region)) && (OTA_CHECK_INTERVAL <= difftime(now, last))) {
                last = now;
                SVersion ota_v;
                if (OTA_PERFORMED == otaPerform(&ota_v))
                {
                    ESP_LOGI(TAG, "OTA Performed. Restarting...");
                    check_volume();
                    soundPlayFile("/fs/nowifi.wav");
                    esp_restart();
                }
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
