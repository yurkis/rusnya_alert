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

#define OTA_HTTPS_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/rusnya.bin"
#define OTA_HTTPS_DATA_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/storage.bin"
#define DATA_PARTITION "storage"
#define OTA_CHECK_INTERVAL (60*60)

static SOTAConfig ota_conf = {
    .url = OTA_HTTPS_URL,
    .data_url = OTA_HTTPS_DATA_URL,
    .data_partition_name = DATA_PARTITION,
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

#define LED_GPIO_NUM (GPIO_NUM_23)
static led_pattern_t led_pattern;
static  SLedPattern LED_NOT_CONNECTED = {.pattern =0b10, .bits = 2, .time = 500};
static  SLedPattern LED_NORMAL = {.pattern =0b000000000001, .bits = 16, .time = 2000};
static  SLedPattern LED_OTA = {.pattern =0b10101000010101, .bits = 16, .time = 2000};
static  SLedPattern LED_SETUP = {.pattern =0b01, .bits = 16, .time = 200};
static const SLedPattern LED_ALERT = LED_PATTERN_ON;

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

bool initConsole()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "alert"">";
    repl_config.max_cmdline_length = 256;

    esp_console_register_help_command();
    wifiRegisterConsole();
    alertRegisterConsole();
    otaRegisterConsole();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    return true;
}

void app_init()
{
    led_pattern = lpCreate(LED_GPIO_NUM);
    lpSetPattern(led_pattern, &LED_NOT_CONNECTED);

    ESP_LOGI(TAG,"\nApp ver: %s\n",APP_VERSION_STR);

    initSPIFFS();

    static char data_ver[64] = {0};
    FILE* f = fopen("/fs/version.txt", "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Can't check data version");
    } else {
        fread(data_ver, 1, sizeof(data_ver)-1, f);
        ESP_LOGI(TAG,"Data version: %s", data_ver);
    }

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

    initConsole();
}

void play_unsafe()
{
    lpSetPattern(led_pattern, &LED_ALERT);
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
    lpSetPattern(led_pattern, &LED_NORMAL);
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
            lpSetPattern(led_pattern, &LED_NORMAL);
            //lpDestroy(led_pattern);
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
                lpSetPattern(led_pattern, &LED_NOT_CONNECTED);
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
                    lpSetPattern(led_pattern, &LED_OTA);
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
