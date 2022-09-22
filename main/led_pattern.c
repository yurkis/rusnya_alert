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
    SLedPattern lp = {.pattern=0, .bits=1};
    SLPInstance* instance = (SLPInstance*)userData;
    gpio_num_t gpio = instance->gpio;
    assert(instance);

    while(1)
    {
        if ((!lp.bits) && (!lp.time)) {
            break;
        }
        if ((lp.pattern == 0xff) || (lp.pattern == 0x0) || (!lp.time) || (!lp.bits)) {
            gpio_set_level(instance->gpio, (lp.pattern != 0)?1:0);
            xQueueReceive( instance->xPatternQueue, &( lp ), portMAX_DELAY );
        }
        if((!lp.time) || (!lp.bits) ) continue;
        uint32_t tm = lp.time / lp.bits / portTICK_PERIOD_MS;
        for(int i=0; i<lp.bits; i++) {
            if(pdTRUE == xQueueReceive( instance->xPatternQueue, &( lp ), 1 )) {
                break;
            }
            gpio_set_level(instance->gpio, ((lp.pattern>>i)&0x01));
            vTaskDelay(tm);
        }
    }
    gpio_set_level(gpio, 0);
    vTaskDelete( NULL );
}

led_pattern_t lpCreate(gpio_num_t gpio)
{
    esp_err_t err  = gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    assert(ESP_OK == err);
    SLPInstance* instance = calloc(1, sizeof(SLPInstance));
    assert(instance);
    instance->gpio = gpio;
    instance->xPatternQueue = xQueueCreate(2, sizeof(SLedPattern) );
    assert(instance->xPatternQueue);
    BaseType_t ret = xTaskCreate(pattern_task, "blink", 1024, instance, 2, NULL);
    if (pdPASS != ret) {
        ESP_LOGE(TAG, "xTaskCreate fail");
    }
    assert(pdPASS == ret);
    return instance;
}

bool lpSetPattern(led_pattern_t instance, SLedPattern* pattern)
{
    assert(instance);
    xQueueSend(((SLPInstance*)instance)->xPatternQueue, pattern,( TickType_t ) 0);
    return true;
}

void lpDestroy(led_pattern_t instance)
{
    static SLedPattern pattern = {.time=0, .bits=0};
    xQueueSend(((SLPInstance*)instance)->xPatternQueue, &pattern, portMAX_DELAY);
    free(instance);
}
