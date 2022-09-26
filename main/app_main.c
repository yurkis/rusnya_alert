#include "ctx.h"
#include "config.h"

#include "sound.h"
#include "wifi.h"
#include "alert_api.h"
#include "sntp_time.h"
#include "version.h"
#include "ota.h"
#include "ota_https_ssl.h"
#include "led_pattern.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_console.h"

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>

static const char* TAG = "mainapp";

#define PARTITION_NAME   "storage"

#define APP_WIFI_AP     CONFIG_ESP_WIFI_SSID
#define APP_WIFI_PASSWD CONFIG_ESP_WIFI_PASSWORD
#define APP_WIFI_TRIES  CONFIG_ESP_MAXIMUM_RETRY

static AlertRegionID_t my_alert_region = 9; /* Hardcoded Kyiv oblast */
static uint8_t quit_start_hr = 22;
static uint8_t quit_end_hr = 9;

static  SLedPattern LED_NOT_CONNECTED = LED_NOT_CONNECTED_PATTERN;
static  SLedPattern LED_NORMAL = LED_NORMAL_PATTERN;
static  SLedPattern LED_OTA = LED_OTA_PATTERN;
//static  SLedPattern LED_SETUP = {.pattern =0b01, .bits = 16, .time = 200};
static  SLedPattern LED_ALERT = LED_ALERT_PATTERN;

static SWiFiSTASettings wifi_settings = {
    .ap = APP_WIFI_AP,
    .passwd = APP_WIFI_PASSWD,
    .tries = APP_WIFI_TRIES,
};

static SOTAConfig ota_conf = {
    .url = OTA_HTTPS_URL,
    .data_url = OTA_HTTPS_DATA_URL,
    //.data_partition_name = DATA_PARTITION,
    .server_cert = (const char*) OTA_HTTPS_CERT,
    //.project_name = "rusnya",
};

void app_init()
{
    wifiInit();
    wifiSTAInit();
    wifiSTAConnect(wifi_settings);
    wifiWaitForWifi();
    timeSetTimezone(UA_TZ);
    timeSync();

    SVersion my_version = otaParseVersionStr(APP_VERSION_STR);
    otaSetCurrentVersion(my_version);
    otaSetConfig(ota_conf);
}

void play_unsafe()
{
    lpSetPattern(ctx.led_pattern, &LED_ALERT);
    for (int i=0;i<3;i++){
        //soundPlayFile("/fs/ALARM.wav");
        //soundPlayFile("/fs/01.wav");
        soundPlayBuffer(ALARM_SOUND_START, ALARM_SOUND_SIZE);
    }
}

void play_safe()
{
    for (int i=0;i<3;i++){
        //soundPlayFile("/fs/SAFE.wav");
        soundPlayBuffer(SAFE_SOUND_START, SAFE_SOUND_SIZE);
    }
    lpSetPattern(ctx.led_pattern, &LED_NORMAL);
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
            lpSetPattern(ctx.led_pattern, &LED_NORMAL);
            //lpDestroy(led_pattern);
            soundPlayFile("/fs/start.wav");
        }
    }
}

void run_main(void)
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
                lpSetPattern(ctx.led_pattern, &LED_NOT_CONNECTED);
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
                if (OTA_UPDATE_FOUND == otaCheck(&ota_v)){
                    ESP_LOGI(TAG, "OTA package found");
                    lpSetPattern(ctx.led_pattern, &LED_OTA);
                    if (OTA_PERFORMED == otaPerform(&ota_v))
                    {
                        ESP_LOGI(TAG, "OTA Performed. Restarting...");
                        check_volume();
                        soundPlayFile("/fs/nowifi.wav");
                        esp_restart();
                    }
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
