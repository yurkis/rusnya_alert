#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "sound.h"

#if CONFIG_IDF_TARGET_ESP32

static const char* TAG = "mainapp";

#define PARTITION_NAME   "storage"

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

void app_main(void)
{
    //initI2S();
    initSPIFFS();
    soundSetup();
    
    for (int i=0;i<1;i++){
        soundPlayFile("/fs/ALARM.wav");
        soundPlayFile("/fs/01.wav");
    }

    for (int i = 3; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    
    for (int i=0;i<3;i++){
        soundPlayFile("/fs/SAFE.wav");
    }
}
#endif
