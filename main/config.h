#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PARTITION_NAME   "storage"

#define APP_WIFI_AP     CONFIG_ESP_WIFI_SSID
#define APP_WIFI_PASSWD CONFIG_ESP_WIFI_PASSWORD
#define APP_WIFI_TRIES  CONFIG_ESP_MAXIMUM_RETRY

#define OTA_HTTPS_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/rusnya.bin"
#define OTA_HTTPS_DATA_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/storage.bin"
#define DATA_PARTITION "storage"
#define OTA_CHECK_INTERVAL (60*60)

extern const uint8_t ALARM_SOUND_START[] asm("_binary_alarm_wav_start");
extern const uint8_t ALARM_SOUND_END[]   asm("_binary_alarm_wav_end");
#define ALARM_SOUND_SIZE (ALARM_SOUND_END - ALARM_SOUND_START)
extern const uint8_t SAFE_SOUND_START[] asm("_binary_safe_wav_start");
extern const uint8_t SAFE_SOUND_END[]   asm("_binary_safe_wav_end");
#define SAFE_SOUND_SIZE (SAFE_SOUND_END - SAFE_SOUND_START)
extern const uint8_t CONNECTED_SOUND_START[] asm("_binary_connected_wav_start");
extern const uint8_t CONNECTED_SOUND_END[]   asm("_binary_connected_wav_end");
extern const uint8_t DISCONNECTED_SOUND_START[] asm("_binary_disconnected_wav_start");
extern const uint8_t DISCONNECTED_SOUND_END[]   asm("_binary_disconnected_wav_end");

#define LED_GPIO_NUM (GPIO_NUM_23)
#define LED_NOT_CONNECTED_PATTERN   {.pattern =0b10, .bits = 2, .time = 500}
#define LED_NORMAL_PATTERN          {.pattern =0b000000000001, .bits = 16, .time = 2000}
#define LED_OTA_PATTERN             {.pattern =0b10101000010101, .bits = 16, .time = 2000};
#define LED_SETUP_PATTERN           {.pattern =0b01, .bits = 16, .time = 200};
#define LED_ALERT_PATTERN            LED_PATTERN_ON;

#define OTA_HTTPS_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/rusnya.bin"
#define OTA_HTTPS_DATA_URL "https://raw.githubusercontent.com/yurkis/rusnya_alert_bin/main/storage.bin"
#define DATA_PARTITION "storage"
#define OTA_CHECK_INTERVAL (60*60)
