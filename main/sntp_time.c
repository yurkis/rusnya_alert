#include "sntp_time.h"

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_sntp.h"
static const char *TAG = "sntp_time";

void timeSetTimezone(const char *TZ)
{
    setenv("TZ", TZ, 1);
    tzset();
}

static void time_sync_notification_cb(struct timeval *tv)
{
    static char strftime_buf[64];
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Synced! The current date/time is: %s", strftime_buf);
}

bool timeSync()
{
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
    ESP_LOGI(TAG, "Sync started");
    return true;
}

bool timeIsSynced()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        return false;
    }
    return true;
}
