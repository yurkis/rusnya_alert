#include "alert_api.h"

#include "esp_err.h"
#include "esp_log.h"
#include "lwip/inet.h"
#include "lwip/ip4_addr.h"
#include "lwip/dns.h"
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

typedef struct _SAlertZone
{
    ERegionState state;
    cbAlertRegionStatusChanged cb;
}SAlertZone;

static SAlertZone zones[MAX_ALERT_ZONES];

bool alertConnect()
{
    //resolve host
    ESP_LOGI(TAG,"Connecting to backend...");

    memset(zones, 0, sizeof(zones));

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
        return false;
    }

    res = recv(sfd, buff, sizeof(buff), 0);
    if (res < 0){
        ESP_LOGE(TAG, "Can't resceive");
        freeaddrinfo(result);
        return false;
    }
    printf("%s", buff);

    //TODO: check for "a:ok"

    freeaddrinfo(result);
    connected = true;
    return true;
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
    ESP_LOGI(TAG, "Ping packet %i", num);
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
        ESP_LOGI(TAG," Zone %i state changed from %s to %s", zone_num,
                                                             alertStateToStr(zones[zone].state),
                                                             alertStateToStr(state));
        if (zones[zone].cb) {
            zones[zone].cb((uint8_t)zone_num, zones[zone].state, state);
        }
        zones[zone].state = state;
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
