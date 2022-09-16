#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef size_t AlertRegionID_t;
/* Region ID:
1:  "Вінницька область"   "Vinnytsia oblast"
2:  "Волинська область"   "Volyn oblast"
3:  "Дніпропетровська область"   "Dnipropetrovsk oblast"
4:  "Донецька область"   "Donetsk oblast"
5:  "Житомирська область"   "Zhytomyr oblast"
6:  "Закарпатська область"   "Zakarpattia oblast"
7:  "Запорізька область"   "Zaporizhzhia oblast"
8:  "Івано-Франківська область"   "Ivano-Frankivsk oblast"
9:  "Київська область"   "Kyiv oblast"
10: "Кіровоградська область"   "Kirovohrad oblast"
11: "Луганська область"   "Luhansk oblast"
12: "Львівська область"   "Lviv oblast"
13: "Миколаївська область"   "Mykolaiv oblast"
14: "Одеська область"   "Odesa oblast"
15: "Полтавська область"   "Poltava oblast"
16: "Рівненська область"   "Rivne oblast"
17: "Сумська область"   "Sumy oblast"
18: "Тернопільська область"   "Ternopil oblast"
19: "Харківська область"   "Kharkiv oblast"
20: "Херсонська область"   "Kherson oblast"
21: "Хмельницька область"   "Khmelnytskyi oblast"
22: "Черкаська область"   "Cherkasy oblast"
23: "Чернівецька область"   "Chernivtsi oblast"
24: "Чернігівська область"   "Chernihiv oblast"
25: "м. Київ"   "Kyiv"
*/

typedef enum {
    eZSUnknown = 0,
    eZSSafe,
    eZSUnsafe,
}ERegionState;
typedef void (*cbAlertRegionStatusChanged)(AlertRegionID_t region, ERegionState old, ERegionState new);

bool alertConnect();
bool alertCheckSync();
bool alertSetObserver(AlertRegionID_t region, cbAlertRegionStatusChanged cb);
bool alertIsConnected();
ERegionState alertState(AlertRegionID_t region);
const char* alertStateToStr(ERegionState state);
const char *alertRegionToStr(AlertRegionID_t region);

