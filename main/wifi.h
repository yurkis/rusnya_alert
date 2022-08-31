#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Sorry I'm too lazy to port my C++ classes so IDF WiFi example here */

typedef void (*cbOnWiFiSTAConnected)();
typedef void (*cbOnWiFiConnectFail)();

typedef struct _SWiFiCallbacks {
    cbOnWiFiSTAConnected onWiFiSTAConnected;
    cbOnWiFiConnectFail  onWiFiConnectFail;
}SWiFiCallbacks;

typedef struct _SWiFiSTASettings {
    const char* ap;
    const char* passwd;
    uint16_t    tries;
}SWiFiSTASettings;

bool wifiInit();
bool wifiSTAInit();
void wifiSTASetCallbacks(SWiFiCallbacks callbacks);
bool wifiSTAConnect(SWiFiSTASettings s);
bool wifiIsConnected();
bool wifiWaitForWifi();
