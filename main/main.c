#include "app_main.h"
#include "settings.h"
#include "ctx.h"
#include "config.h"
#include "sound.h"
#include "wifi.h"
#include "alert_api.h"
#include "sntp_time.h"
//#include "version.h"
#include "ota.h"
//#include "ota_https_ssl.h"
#include "led_pattern.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_console.h"

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <time.h>

static const char* TAG = "main";

SRusnyaCTX ctx;

static  SLedPattern LED_NOT_CONNECTED = LED_NOT_CONNECTED_PATTERN;

static void initSPIFFS()
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

static bool initConsole()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "alert"">";
    repl_config.max_cmdline_length = 256;

    esp_console_register_help_command();
    wifiRegisterConsole();
    alertRegisterConsole();
    otaRegisterConsole();
    settingsRegisterConsole();

    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    return true;
}

static bool init()
{
    ctx.led_pattern = lpCreate(LED_GPIO_NUM);
    lpSetPattern(ctx.led_pattern, &LED_NOT_CONNECTED);

    initSPIFFS();

    soundSetup();
    nvsInit();

    initConsole();
    return true;
}

static void main_task(void *userData)
{
    ESP_LOGI(TAG, "Running main app");
    run_main();
}

void app_main(void)
{
    init();
    BaseType_t ret = xTaskCreate(main_task, "main", 6144, NULL, 2, NULL);
    if (pdPASS != ret) {
        ESP_LOGE(TAG, "xTaskCreate fail");
    }
    while(true){
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
