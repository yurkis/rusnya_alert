#include "led_pattern.h"

#include "esp_err.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <stdlib.h>

#define TAG "blink"

typedef struct _SLPInstance
{
    gpio_num_t gpio;
    QueueHandle_t xPatternQueue;
}SLPInstance;

static void pattern_task(void *userData)
{
    ESP_LOGI(TAG, "Task started");

    SLedPattern lp = {0,0,0};
    SLPInstance* instance = (SLPInstance*)userData;

    assert(instance);

    while(1)
    {
        SLedPattern plp;
        if ((lp.pattern == 0xff) || (lp.pattern == 0x0) || (!lp.time) || (!lp.bits)) {
            gpio_set_level(instance->gpio, (lp.pattern != 0)?1:0);
            ESP_LOGI(TAG,"Static value");
            xQueueReceive( instance->xPatternQueue, &( plp ), portMAX_DELAY );
        } else {
            xQueueReceive( instance->xPatternQueue, &( plp ), 1 );
        }
        /*if (!plp) {
            vTaskDelete( NULL );
            return;
        }*/
        lp = plp;
        if((!lp.time) || (!lp.bits) ) continue;
        uint32_t tm = lp.time / lp.bits / portTICK_PERIOD_MS;
        for(int i=0; i<lp.bits; i++) {
            gpio_set_level(instance->gpio, ((lp.pattern>>i)&0x01));
            vTaskDelay(tm);
        }
    }
}

led_pattern_t lpCreate(gpio_num_t gpio)
{
    esp_err_t err  = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    if (ESP_OK != err) {
        ESP_LOGE(TAG, "Can not configure GPIO");
    }
    assert(ESP_OK == err);
    SLPInstance* instance = calloc(1, sizeof(SLPInstance));
    if (!instance) {
        ESP_LOGE(TAG, "calloc fail");
    }
    assert(instance);
    instance->gpio = gpio;
    instance->xPatternQueue = xQueueCreate(2, sizeof(SLedPattern) );
    assert(instance->xPatternQueue);
    BaseType_t ret = xTaskCreate(pattern_task, "blink", 2048, instance, 2, NULL);
    /*if (pdPASS != ret) {
        ESP_LOGE(TAG, "xTaskCreate fail");
    }
    assert(pdPASS == ret);*/
    return instance;
}

bool lpSetPattern(led_pattern_t instance, SLedPattern* pattern)
{
    assert(instance);
    xQueueSend(((SLPInstance*)instance)->xPatternQueue, pattern,( TickType_t ) 0);
    return true;
}
