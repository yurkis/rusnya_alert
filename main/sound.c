#include "sound.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2s.h"

#define SAMPLE_RATE     (8000)
#define DMA_BUF_LEN     (512)
#define DMA_NUM_BUF     (2)
#define I2S_NUM         (0)

//TODO: Wav header parsing
#define WAV_HEADER_SZ (44)

static const char* TAG = "snd";

typedef struct _SAudionInfo
{
    uint16_t SampleRate;
    uint16_t BitsPerSample;
    uint16_t Multiplier;
}SAudioInfo;

typedef struct _SI2SBuff
{
    size_t size;
    uint16_t buff [DMA_BUF_LEN];
}SI2SBuff;

typedef struct _SI2SBuffers
{
    uint8_t current;
    SI2SBuff buffers [2];
}SI2SBuffers;

static SI2SBuffers buffers = {.current=0};


static SAudioInfo ai = {
   .SampleRate = 16000,
   .BitsPerSample = 8,
   .Multiplier = 4,
};

static QueueHandle_t xPlayBufferQueue = NULL;
static QueueHandle_t xTXDoneQueue = NULL;

static void i2s_play_task(void *userData)
{
    static SI2SBuff* curr = NULL;
    static size_t bytes_written;
    while(1) {
         if(( xQueueReceive( xPlayBufferQueue,
                         &( curr ),
                         portMAX_DELAY ) == pdPASS) && (curr))
      {
         
          i2s_write(I2S_NUM, curr->buff, curr->size, &bytes_written, portMAX_DELAY);
          xQueueSend(xTXDoneQueue, ( void * ) &bytes_written, ( TickType_t ) 0);
      }else{
            ESP_LOGE(TAG, "No buff");
            vTaskDelete( NULL );
      }      
    }
}

static bool read_chunk(FILE* f)
{
    size_t size = DMA_BUF_LEN / ai.Multiplier;
    
    
    static unsigned char file_buff[DMA_BUF_LEN];
    size_t got = fread(file_buff, 1, size, f);
    if (got ==0){
        //ESP_LOGI(TAG, "Play finished");
        vTaskDelay(50 / portTICK_PERIOD_MS);
        i2s_stop(I2S_NUM);
        return false;
    }
    //ESP_LOGI(TAG, "Got: %d", got);
    static SI2SBuff* curr;
    curr = &buffers.buffers[buffers.current];
        
    size_t i2s_len = 0;
    for(int i=0; i<got;i++)
    {
        curr->buff[i2s_len*2] = curr->buff[i2s_len*2+1] = file_buff[i]<<8&0xff00;
        i2s_len++;
    }
    curr->size = got*4;
    xQueueSend(xPlayBufferQueue,( void * ) &curr,( TickType_t ) 0 );
    buffers.current = (buffers.current==0)?1:0;
    return true;
}

bool soundSetup()
{

    // Setup I2S
    i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN,
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_MSB,
        .dma_buf_count = DMA_NUM_BUF,
        .dma_buf_len = DMA_BUF_LEN,
        .use_apll = true,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL2
    };
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);

    i2s_set_pin(I2S_NUM, NULL); 

    // Perepare tasks
    xPlayBufferQueue = xQueueCreate(2, sizeof(void*) );
    xTXDoneQueue = xQueueCreate(2, sizeof(size_t) );
    if ((!xPlayBufferQueue) || (!xTXDoneQueue) ) {
            ESP_LOGE(TAG, "Failed to create Queues");
    }
    xTaskCreate(i2s_play_task, "i2s_play", 1024, NULL, configMAX_PRIORITIES - 1, NULL);
    
    return true;
}

bool soundPlayFile(const char* filename)
{
    size_t i2s_sent;
    ESP_LOGI(TAG, "Playing file %s", filename);
    FILE* f = fopen(filename, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        return false;
    }
    
    unsigned char wav_hdr[WAV_HEADER_SZ];
    fread(wav_hdr, 1, WAV_HEADER_SZ, f);
    
    i2s_start(I2S_NUM);
    
    if (!read_chunk(f)) return false;
    static SI2SBuff* curr;
    curr = &buffers.buffers[buffers.current];
    
    while(read_chunk(f))
    {
        //ESP_LOGI(TAG, "current: %d", buffers.current);
        xQueueReceive(xTXDoneQueue, &( i2s_sent ), portMAX_DELAY );
    }
    return true;
}
