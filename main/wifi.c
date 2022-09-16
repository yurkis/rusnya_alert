#include "wifi.h"

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_console.h"

#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

static SWiFiCallbacks cbs = {0};
static SWiFiSTASettings sta_setings = {};
static bool is_connected = false;

static int s_retry_num = 0;

static const char *TAG = "wifi_sta";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        is_connected = false;
        if (s_retry_num < sta_setings.tries) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            if (cbs.onWiFiConnectFail) {
               cbs.onWiFiConnectFail();
            }
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        is_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        if (cbs.onWiFiSTAConnected) {
            cbs.onWiFiSTAConnected();
        }
    }
}

bool wifiInit()
{
    return true;
}

bool wifiSTAInit()
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
    ESP_LOGI(TAG, "Init done");
    return true;
}

void wifiSTASetCallbacks(SWiFiCallbacks callbacks)
{
    //TODO: Sync here
    cbs = callbacks;
}

bool wifiSTAConnect(SWiFiSTASettings s)
{
    sta_setings = s;
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config));
    strncpy((char*)wifi_config.sta.ssid, sta_setings.ap, sizeof(wifi_config.sta.ssid));
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid)-1] = 0;
    strncpy((char*)wifi_config.sta.password, sta_setings.passwd, sizeof(wifi_config.sta.password));
    wifi_config.sta.password[sizeof(wifi_config.sta.password)-1] = 0;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required= false;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifiSTAConnect finished.");
    return true;
}

bool wifiIsConnected()
{
    //TODO: atomic
    return is_connected;
}

bool wifiWaitForWifi()
{
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                pdFALSE,
                pdFALSE,
                portMAX_DELAY);
    return (bits & WIFI_CONNECTED_BIT);
}

bool wifiSTADisconnect()
{
    esp_wifi_disconnect();
    return true;
}

/////////////// Console
static int cliDisconnect(int argc, char **argv)
{
    wifiSTADisconnect();
    return 0;
}

typedef struct _SCLISubCmd{
    const char* cmd;
    int (*handler)(int argc, char **argv);
}SCLISubCmd;

static SCLISubCmd cmds[]= {
    {"disconnect", cliDisconnect},
    {0,0}
};

static void cliWiFiBanner()
{
    printf("Subcommands:\n");
    for(size_t i=0; (cmds[i].cmd && cmds[i].handler); i++){
        printf("\t%s\n", cmds[i].cmd);
    }
}

static int cliWifi(int argc, char **argv)
{
    size_t i=0;
    if (argc<2){
        cliWiFiBanner();
        return 0;
    }
    for(i=0; (cmds[i].cmd && cmds[i].handler); i++){
        if (!strcmp(argv[1], cmds[i].cmd)){
            return cmds[i].handler(argc-1, argv+1);
        }
    }
    cliWiFiBanner();
    return 0;
}

void wifiRegisterConsole()
{
    const esp_console_cmd_t cmd = {
            .command = "wifi",
            .help = "Get version of chip and SDK",
            .hint = NULL,
            .func = &cliWifi,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}

