#include "alert_api.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
#include "esp_console.h"
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
static const char *TAG = "alert_api";

#define MAX_ALERT_ZONES (25)

const char* const API_HOST = "tcp.alerts.com.ua";
const char* const API_PORT = "1024";
static bool connected = false;
static int sfd = -1;
static char buff [250];

static const char* CONNECT_STR = CONFIG_BACKEND_API_KEY"\n";
static const char* const OK_RESPONSE = "a:ok\n";

typedef struct _SAlertZone
{
    ERegionState state;
    cbAlertRegionStatusChanged cb;
    const char* name;
}SAlertZone;

static SAlertZone zones[MAX_ALERT_ZONES] = {
/* 1*/    {.name = "Вінницька обл."},
/* 2*/    {.name = "Волинська обл."},
/* 3*/    {.name = "Дніпропетровська обл."},
/* 4*/    {.name = "Донецька обл."},
/* 5*/    {.name = "Житомирська обл."},
/* 6*/    {.name = "Закарпатська обл."},
/* 7*/    {.name = "Запорізька обл."},
/* 8*/    {.name = "Івано-Франківська обл."},
/* 9*/    {.name = "Київська обл."},
/*10*/    {.name = "Кіровоградська обл."},
/*11*/    {.name = "Луганська обл."},
/*12*/    {.name = "Львівська обл."},
/*13*/    {.name = "Миколаївська обл."},
/*14*/    {.name = "Одеська обл."},
/*15*/    {.name = "Полтавська обл."},
/*16*/    {.name = "Рівненська обл."},
/*17*/    {.name = "Сумська обл."},
/*18*/    {.name = "Тернопільська обл."},
/*19*/    {.name = "Харківська обл."},
/*20*/    {.name = "Херсонська обл."},
/*21*/    {.name = "Хмельницька обл."},
/*22*/    {.name = "Черкаська обл."},
/*23*/    {.name = "Чернівецька обл."},
/*24*/    {.name = "Чернігівська обл."},
/*25*/    {.name = "м. Київ"},
};

bool alertConnect()
{
    //resolve host
    ESP_LOGI(TAG,"Connecting to backend...");

    for(int i=0; i<MAX_ALERT_ZONES; i++) {
        zones[i].state = eZSUnknown;
    }

    const struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_STREAM,
        };
    struct addrinfo *result;
    int res;
    res = getaddrinfo(API_HOST, API_PORT, &hints, &result);
    if(res != 0){
        ESP_LOGE(TAG, "getaddrinfo failed with %i", res);
        return false;
    }
    static char ip_str[20];
    struct sockaddr_in  *ipv4 = NULL;
    if(result->ai_family != AF_INET)
    {
        ESP_LOGE(TAG, "IPv4 error");
        freeaddrinfo(result);
        return false;
    }

    ipv4 = (struct sockaddr_in *)result->ai_addr;
    inet_ntop(result->ai_family, &ipv4->sin_addr, ip_str, sizeof(ip_str));
    ESP_LOGI(TAG, "Resolved IP addr: %s", ip_str);
    //printf("[IPv4-%d]%s [port]%d \n",ipv4_cnt,buf,ntohs(ipv4->sin_port));

    if (sfd < 1) {
        sfd = socket(AF_INET, SOCK_STREAM, 0);
    }
    if (sfd < 1) {
        ESP_LOGE(TAG, "Can't create socket");
        freeaddrinfo(result);
        return false;
    }
    res = connect(sfd, (const struct sockaddr *)ipv4, sizeof(*ipv4));
    if (res>0) {
        close(sfd);
        sfd=-1;
    }
    if (res < 0){
        ESP_LOGE(TAG, "Can't connect");
        freeaddrinfo(result);
        return false;
    }

    res = send(sfd, CONNECT_STR, strlen(CONNECT_STR),0);
    if (res < 0){
        ESP_LOGE(TAG, "Can't send");
        freeaddrinfo(result);
        close(sfd);
        connected = false;
        return false;
    }

    size_t ok_len = strlen(OK_RESPONSE);
    res = recv(sfd, buff, ok_len, 0);
    if (res < 0){
        ESP_LOGE(TAG, "Can't resceive");
        freeaddrinfo(result);
        close(sfd);
        connected = false;
        return false;
    }
    buff[ok_len] = 0;

    //TODO: check for "a:ok"

    freeaddrinfo(result);
    connected = true;
    return (0 == strcmp(buff, OK_RESPONSE));
}

const char* alertStateToStr(ERegionState state)
{
    switch (state) {
        case eZSUnknown: return "Unknown";
        case eZSSafe: return "Safe";
        case eZSUnsafe: return "UNSAFE";
    }
    return "WRONG_STATE";
}

static void parse_ping(char* line, size_t len)
{
    // Ex: "p:2514"
    int num = atoi(line+2);
    //ESP_LOGI(TAG, "Ping packet %i", num);
}

static void parse_state(char* line, size_t len)
{
    // Ex: s:25=0
    line = line+2;
    // Ex: 25=0
    size_t idx=0;
    for(idx=0; idx<len; idx++)
    {
        if (line[idx] == '=') break;
    }
    if (idx== len) {
        ESP_LOGE(TAG, "Status parse error (no state): %s", line);
        return;
    }

    line[idx] = 0;
    size_t zone = 0;
    size_t zone_num = atoi(line);
    if ((zone_num < 1 ) || (zone_num > MAX_ALERT_ZONES)) {
        ESP_LOGI(TAG, "Unsupported zone %i", zone_num);
    }
    zone = zone_num - 1;
    ERegionState state;
    switch(*(line+idx+1)){
        case '0': state = eZSSafe; break;
        case '1': state = eZSUnsafe; break;
        default:
            ESP_LOGE(TAG, "Unknown state");
            return;
    }

    if (zones[zone].state != state) {
        ESP_LOGI(TAG,"(%i) %s: %s -> %s", zone_num, alertRegionToStr(zone_num),
                                            alertStateToStr(zones[zone].state),
                                            alertStateToStr(state));
        ERegionState old_state = zones[zone].state;
        zones[zone].state = state;
        if (zones[zone].cb) {
            zones[zone].cb((uint8_t)zone_num, old_state, state);
        }
    }
}

static void parse_line(char* line)
{
    //printf("\n>>%s\n", line);
    size_t len = strlen(line);
    if (len<2){
        ESP_LOGI(TAG, "Line too short: %s", line);
        return;
    }
    if (line[1] != ':') {
        ESP_LOGE(TAG, "Wrong format (delim) : %s", line);
        return;
    }
    switch (line[0]){
        case 'p': parse_ping(line, len); break;
        case 's': parse_state(line, len); break;
        default:
            ESP_LOGE(TAG," Unsupported packet: %s", line);
    }
}

bool alertCheckSync()
{
    memset (buff, 0, sizeof(buff));
    int res = recv(sfd, buff, sizeof(buff), 0);
    if (res < 0){
        ESP_LOGE(TAG, "Can't resceive");
        close(sfd);
        connected = false;
        return false;
    }
    char* p = buff;
    static char curr_line[64];
    memset(curr_line, 0, sizeof(curr_line));
    size_t curr_size=0;
//    printf("\n%s", buff);
    while(*p)
    {
        if (curr_size >= sizeof(curr_line)){
            curr_line[curr_size] = 0;
            ESP_LOGE(TAG, "Parsing error:\n>>>%s\n%s", curr_line, buff);
            curr_size=0;
        }
        if (*p == '\n') {
            curr_line[curr_size] = 0;
            parse_line(curr_line);
            curr_size = 0;
        } else {
            curr_line[curr_size++] = *p;
        }
        p++;
    }
    return true;
}

bool alertIsConnected()
{
    return connected;
}

bool alertSetObserver(AlertRegionID_t region, cbAlertRegionStatusChanged cb)
{
    if ((region < 1) || (region > MAX_ALERT_ZONES)) {
        ESP_LOGE(TAG, "alertSetObserver: wrong region");
        return false;
    }
    zones[region-1].cb = cb;
    return true;
}

ERegionState alertState(AlertRegionID_t region)
{
    if ((region < 1) || (region > MAX_ALERT_ZONES)) {
        ESP_LOGE(TAG, "alertState: wrong region");
        return eZSUnknown;
    }
    return zones[region-1].state;
}

const char *alertRegionToStr(AlertRegionID_t region)
{
    static const char* UNKNOWN = "UNKNOWN";
    if ((region < 1) || (region > MAX_ALERT_ZONES)) {
        ESP_LOGE(TAG, "alertRegionToStr: wrong region");
        return UNKNOWN;
    }
    if (!zones[region-1].name) {
        return UNKNOWN;
    }
    return zones[region-1].name;
}

/////////////////////// CLI ///////////////////////////
static int cliList(int argc, char **argv)
{
    printf("ID:\t|State:\t|Name:\n");
    for(int i=0; i<MAX_ALERT_ZONES; i++){
        printf("#%i\t %s\t %s\n", i+1, alertStateToStr(zones[i].state), zones[i].name);
    }
    return 0;
}


static int cliSet(int argc, char **argv)
{
    static const char* cliSet__state_help = "\t\t<state> (-1) - unknown, (0) - safe, (1) - unsafe\n";
    if (argc<3){
        printf("Usage: %s <region_id> <state>\n", argv[0]);
        printf(cliSet__state_help);
        return 0;
    }
    size_t reg_id = atoi(argv[1]);
    size_t int_state = atoi(argv[2]);
    ERegionState state = eZSUnknown;
    switch (int_state){
        case -1: state = eZSUnknown; break;
        case  0: state = eZSSafe; break;
        case  1: state = eZSUnsafe; break;
        default: printf(cliSet__state_help); return 0;
    }
    if ((reg_id<1) || (reg_id>=MAX_ALERT_ZONES)) {
        printf("Wrong region id\n");
        return 0;
    }
    if (zones[reg_id-1].state != state) {
        ERegionState old_state = zones[reg_id-1].state;
        zones[reg_id-1].state = state;
        if (zones[reg_id-1].cb) {
            zones[reg_id-1].cb((uint8_t)reg_id, old_state, state);
        }
    }
    return 0;
}

typedef struct _SCLISubCmd{
    const char* cmd;
    const char* desc;
    int (*handler)(int argc, char **argv);
}SCLISubCmd;

static SCLISubCmd cmds[]= {
    {"list", "List regions with IDs and states",cliList},
    {"set",  "<region> <state> Set region state",cliSet},
    {0,0,0}
};

static void cliBanner()
{
    printf("Subcommands:\n");
    for(size_t i=0; (cmds[i].cmd && cmds[i].handler); i++){
        printf("\t%s\t%s\n", cmds[i].cmd, cmds[i].desc);
    }
}

static int cliAlert(int argc, char **argv)
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

void alertRegisterConsole()
{
    const esp_console_cmd_t cmd = {
            .command = "alert",
            .help = "Alert protocol commands set",
            .hint = NULL,
            .func = &cliAlert,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&cmd) );
}
