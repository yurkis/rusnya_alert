#include "settings.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_console.h"

#include <stdio.h>
#include <string.h>

#define NVS_NAME "data"
#define AP_NAME "ap"
#define AP_PASSWD "passwd"
#define REGION "region"
#define Q_START "q_start"
#define Q_END   "q_end"

#define TAG "app_nvs"

bool nvsInit()
{
    bool ret_val = true;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
          ESP_ERROR_CHECK(nvs_flash_erase());
          ret = nvs_flash_init();
          ret_val = false;
    }
    ESP_ERROR_CHECK(ret);
    return ret_val;
}

#define GET_STRING_VAL(name, field)\
    err = nvs_get_str(handle, name, NULL, &length);\
    if (ESP_OK != err){\
        ESP_LOGE(TAG, "Can not get '%s' (stettings incomplete?)", name);\
        nvs_close(handle);\
        return false;\
    }\
    if (length>=sizeof(field)) {\
        ESP_LOGE(TAG, "Stored field %s is too long!", name);\
        nvs_close(handle);\
        return false;\
    }\
    err = nvs_get_str(handle, name, field, &length);\
    if (ESP_OK != err){\
        ESP_LOGE(TAG, "Could not get '%s' (stettings incomplete?)", name);\
        nvs_close(handle);\
        return false;\
    }

bool settingsRead(SAppSettings *s)
{
    nvs_handle handle;
    size_t length;
    uint8_t val;
    esp_err_t err = nvs_open(NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error open NVS");
        return false;
    }

    GET_STRING_VAL(AP_NAME, s->ap_name);
    GET_STRING_VAL(AP_PASSWD, s->ap_pass);

    err = nvs_get_u8(handle, REGION, &val);
    if (ESP_OK != err){
        ESP_LOGE(TAG, "Can not get '%s' (stettings incomplete?)", REGION);
        nvs_close(handle);
        return false;
    }
    s->alert_region = (AlertRegionID_t)val;

    err = nvs_get_u8(handle, Q_START, &s->quit_start_hr);
    if (ESP_OK != err){
        ESP_LOGE(TAG, "Can not get '%s' (using default)", Q_START);
        s->quit_start_hr = 0;
    }

    err = nvs_get_u8(handle, Q_END, &s->quit_end_hr);
    if (ESP_OK != err){
        ESP_LOGE(TAG, "Can not get '%s' (using default)", Q_END);
        s->quit_end_hr = 0;
    }

    return true;
}

bool settingsWrite(SAppSettings *s)
{
    nvs_handle handle;

    esp_err_t err = nvs_open(NVS_NAME, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error open NVS");
        return false;
    }
    err = nvs_set_str(handle, AP_NAME, s->ap_name);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Can not save '%s'", AP_NAME);
    }
    err = nvs_set_str(handle, AP_PASSWD, s->ap_pass);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Can not save '%s'", AP_PASSWD);
    }
    err = nvs_set_u8(handle, REGION, (uint8_t)s->alert_region);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Can not save '%s'", REGION);
    }
    err = nvs_set_u8(handle, Q_START, s->quit_start_hr);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Can not save '%s'", Q_START);
    }
    err = nvs_set_u8(handle, Q_END, s->quit_end_hr);
    if (ESP_OK != err) {
        ESP_LOGI(TAG, "Can not save '%s'", Q_END);
    }
    return true;
}

////////////////// Console //////////////////////
static SAppSettings s;
static int cliGet(int argc, char **argv)
{
    if (!settingsRead(&s)) {
        ESP_LOGI(TAG, "Can not read settings");
        return 0;
    }
    printf("AP name:\t%s\n",   s.ap_name);
    printf("AP passwd:\t%s\n", s.ap_pass);
    printf("Alert region:\t%i (%s)\n", s.alert_region, alertRegionToStr(s.alert_region));
    return 0;
}

static int cliSet(int argc, char **argv)
{
    if (argc<4){
        printf("Usage: %s <ap_name> <ap_passwd> <alert_region> [silent_hr_start] [silent_hr_end]\n", argv[0]);
        return 0;
    }
    strncpy(s.ap_name, argv[1], sizeof(s.ap_name));
    strncpy(s.ap_pass, argv[2], sizeof(s.ap_pass));
    s.alert_region = (AlertRegionID_t) atoi (argv[3]);
    s.quit_start_hr = (uint8_t)(argc>4)?(uint8_t)atoi(argv[4]):0;
    s.quit_end_hr = (uint8_t)(argc>5)?(uint8_t)atoi(argv[5]):0;
    if (!settingsWrite(&s)){
        ESP_LOGE(TAG, "Can not write settings");
    }
    return 0;
}

typedef struct _SCLISubCmd{
    const char* cmd;
    const char* desc;
    int (*handler)(int argc, char **argv);
}SCLISubCmd;

static SCLISubCmd cmds[]= {
    {"get",    "get stored settings",cliGet},
    {"set",    "<ap_name> <ap_passwd> <alert_region> [silent_hr_start] [silent_hr_end]", cliSet},
    {0,0,0}
};

static void cliBanner()
{
    printf("Subcommands:\n");
    for(size_t i=0; (cmds[i].cmd && cmds[i].handler); i++){
        printf("\t%s\t%s\n", cmds[i].cmd, cmds[i].desc);
    }
}

static int cliSettings(int argc, char **argv)
{
    size_t i=0;
    if (argc<2){
        cliBanner();
        return 0;
    }
    for(i=0; (cmds[i].cmd && cmds[i].handler); i++){
        if (!strcmp(argv[1], cmds[i].cmd)){
            printf("\n");
            int ret = cmds[i].handler(argc-1, argv+1);
            printf("\n");
            return ret;
        }
    }
    cliBanner();
    return 0;
}

void settingsRegisterConsole()
{
    const esp_console_cmd_t cmd = {
            .command = "nvs",
            .help = "Setings storage commands set",
            .hint = NULL,
            .func = &cliSettings,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
